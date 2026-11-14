#!/usr/bin/env python3
"""Race EK against NK and ltlfsynt on the hard instances of the output-dependency race.

    python3 scripts/hard_race.py [--specs DIR] [--out FILE.json] [--timeout S] [--family F] [--n-min N] [--n-max N]

Every instance of DIR (tests/data/hard-instances: NAME.ltlf, NAME.part, and NAME.tout where a
hand-written compact Tout exists) is run under four arms, each a cell of the JSON:

  nk           ltlf-ek-synth, no knowledge (the instance's own part file)
  ltlfsynt     Spot's ltlfsynt, full synthesis (the controller is built, then discarded), same formula
  ek-derived   ltlf-ek-deps --direction out emits the part file and the Tout, then ltlf-ek-synth
               reads them; extract_s is the deps stage, synth_s the synthesis, wall_s their sum
  ek-compact   ltlf-ek-synth with the hand-written Tout and every output known

ltlf-ek-synth runs --otf-mtdfa-product --otf-solve --simplify-formula in every EK and NK cell. A
cell names its arm and ends as ok (verdict REALIZABLE / UNREALIZABLE), timeout, error, or none
(ek-derived only: no output dependency found, so there is no Tout to race). The run's
verdict_mismatches lists every instance on which two finished arms disagree.
"""
import argparse
import datetime
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
DEFAULT_SPECS = os.path.join(REPO, "tests", "data", "hard-instances")
SYNTH_FLAGS = ["--otf-mtdfa-product", "--otf-solve", "--simplify-formula", "--realizable"]
ARMS = ("nk", "ltlfsynt", "ek-derived", "ek-compact")
MEM_KIB = 8 * 1024 * 1024
VERDICTS = {0: "REALIZABLE", 20: "UNREALIZABLE"}


def instance_key(name):
    """(family, n): `mux-16` -> ("mux", 16); a name without a size is its own family, n None."""
    m = re.fullmatch(r"(.+)-(\d+)", name)
    return (m.group(1), int(m.group(2))) if m else (name, None)


def read_part(path):
    """The four keys of a part file as lists."""
    keys = {}
    with open(path) as f:
        for line in f:
            k, _, v = line.partition(":")
            keys[k.strip()] = v.split()
    return keys


def run(argv, timeout):
    """(rc, stdout, wall seconds); rc None on timeout. Memory is capped by a plain ulimit -v."""
    t0 = time.monotonic()
    try:
        p = subprocess.run(["bash", "-c", 'ulimit -v %d; exec "$@"' % MEM_KIB, "bash", *argv],
                           capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return None, "", time.monotonic() - t0
    return p.returncode, p.stdout, time.monotonic() - t0


def synth_cell(arm, synth, formula, args, timeout):
    rc, out, wall = run([synth, *SYNTH_FLAGS, "--formula", formula, *args], timeout)
    if rc is None:
        return {"arm": arm, "status": "timeout", "verdict": None, "wall_s": wall}
    if rc in VERDICTS:
        return {"arm": arm, "status": "ok", "verdict": VERDICTS[rc], "wall_s": wall}
    return {"arm": arm, "status": "error", "verdict": None, "wall_s": wall, "rc": rc}


def ltlfsynt_cell(ltlfsynt, formula, part, timeout):
    rc, out, wall = run([ltlfsynt, "--ins=" + ",".join(part["input_free"]),
                         "--outs=" + ",".join(part["output_free"]), "-f", formula], timeout)
    if rc is None:
        return {"arm": "ltlfsynt", "status": "timeout", "verdict": None, "wall_s": wall}
    verdict = out.split("\n", 1)[0].strip()
    if rc in (0, 1) and verdict in VERDICTS.values():
        return {"arm": "ltlfsynt", "status": "ok", "verdict": verdict, "wall_s": wall}
    return {"arm": "ltlfsynt", "status": "error", "verdict": None, "wall_s": wall, "rc": rc}


def derived_cell(build, formula, part_path, scratch, timeout):
    emitted, tout = os.path.join(scratch, "derived.part"), os.path.join(scratch, "derived.tout")
    rc, out, extract = run([os.path.join(build, "ltlf-ek-deps"), "--direction", "out", "--formula", formula,
                            "--part-file", part_path, "--emit-part", emitted, "--transducer", tout], timeout)
    if rc is None:
        return {"arm": "ek-derived", "status": "timeout", "verdict": None, "wall_s": extract, "extract_s": extract}
    if rc != 0:
        return {"arm": "ek-derived", "status": "error", "verdict": None, "wall_s": extract, "extract_s": extract,
                "rc": rc}
    if not os.path.exists(tout):
        return {"arm": "ek-derived", "status": "none", "verdict": None, "wall_s": extract, "extract_s": extract}
    cell = synth_cell("ek-derived", os.path.join(build, "ltlf-ek-synth"), formula,
                      ["--part-file", emitted, "--known-output-transducer", tout],
                      max(timeout - extract, 1))
    cell.update(extract_s=extract, synth_s=cell["wall_s"], wall_s=extract + cell["wall_s"])
    return cell


def compact_cell(build, formula, part, tout_path, scratch, timeout):
    known = os.path.join(scratch, "compact.part")
    with open(known, "w") as f:
        f.write("input_free: %s\ninput_known:\noutput_free:\noutput_known: %s\n"
                % (" ".join(part["input_free"]), " ".join(part["output_free"])))
    return synth_cell("ek-compact", os.path.join(build, "ltlf-ek-synth"), formula,
                      ["--part-file", known, "--known-output-transducer", tout_path], timeout)


def race_instance(name, specs, build, ltlfsynt, timeout):
    base = os.path.join(specs, name)
    with open(base + ".ltlf") as f:
        formula = f.read().strip()
    part = read_part(base + ".part")
    family, n = instance_key(name)
    synth = os.path.join(build, "ltlf-ek-synth")
    cells = [synth_cell("nk", synth, formula, ["--part-file", base + ".part"], timeout),
             ltlfsynt_cell(ltlfsynt, formula, part, timeout)]
    with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as scratch:
        cells.append(derived_cell(build, formula, base + ".part", scratch, timeout))
        if os.path.exists(base + ".tout"):
            cells.append(compact_cell(build, formula, part, base + ".tout", scratch, timeout))
    return [dict(c, instance=name, family=family, n=n) for c in cells]


def verdict_mismatches(cells):
    """Instances on which two finished arms disagree on realizability."""
    seen = {}
    for c in cells:
        if c["status"] == "ok":
            seen.setdefault(c["instance"], {})[c["arm"]] = c["verdict"]
    return {i: v for i, v in sorted(seen.items()) if len(set(v.values())) > 1}


def instances(specs, family=None, n_min=None, n_max=None):
    names = sorted(f[:-5] for f in os.listdir(specs) if f.endswith(".ltlf"))
    keep = []
    for name in names:
        fam, n = instance_key(name)
        if family and fam != family:
            continue
        if n is not None and ((n_min is not None and n < n_min) or (n_max is not None and n > n_max)):
            continue
        keep.append(name)
    return sorted(keep, key=lambda s: (instance_key(s)[0], instance_key(s)[1] or 0))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--specs", default=DEFAULT_SPECS)
    ap.add_argument("--build", default=os.path.join(REPO, "build"))
    ap.add_argument("--ltlfsynt", default=shutil.which("ltlfsynt") or "ltlfsynt")
    ap.add_argument("--timeout", type=float, default=120.0, help="seconds per cell (default 120)")
    ap.add_argument("--family")
    ap.add_argument("--n-min", type=int)
    ap.add_argument("--n-max", type=int)
    ap.add_argument("--out", default=os.path.join(REPO, "docs", "runs",
                                                  datetime.date.today().isoformat() + "-hard-instance-race.json"))
    args = ap.parse_args(argv)
    cells = []
    for name in instances(args.specs, args.family, args.n_min, args.n_max):
        got = race_instance(name, args.specs, args.build, args.ltlfsynt, args.timeout)
        cells += got
        print(name, " ".join("%s=%s/%.2fs" % (c["arm"], c["verdict"] or c["status"], c["wall_s"]) for c in got),
              file=sys.stderr, flush=True)
    result = {
        "provenance": {
            "date": datetime.date.today().isoformat(), "timeout_seconds": args.timeout, "mem_kib": MEM_KIB,
            "host": platform.node(), "ltlfsynt": subprocess.run([args.ltlfsynt, "--version"], capture_output=True,
                                                                text=True).stdout.splitlines()[0],
            "synth_flags": SYNTH_FLAGS, "arms": ARMS,
            "specs": os.path.relpath(args.specs, REPO),
        },
        "cells": cells,
        "verdict_mismatches": verdict_mismatches(cells),
    }
    with open(args.out, "w") as f:
        json.dump(result, f, indent=1)
        f.write("\n")
    return 1 if result["verdict_mismatches"] else 0


if __name__ == "__main__":
    sys.exit(main())
