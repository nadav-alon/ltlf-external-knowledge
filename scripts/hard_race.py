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
  ek-derived-fixed, ek-compact-fixed
               the same two, with --fix-dependent-outputs (experimental): the known outputs leave
               the controller's choice. A fixed cell that finishes REALIZABLE is run again with
               --model-check, which verifies its controller against the formula (verify_s). An
               unsafe controller makes the cell an error; an unverifiable one (the verifier ran out
               of time or memory, recorded in verify_limit) leaves the cell ok. A fixed cell that
               finishes takes its instance into the bar (below), and a verdict that differs from
               NK's or ltlfsynt's fails the run.

ltlf-ek-synth runs --otf-mtdfa-product --otf-solve --simplify-formula --realizable in every EK and
NK cell; --realizable only suppresses printing the controller, which is still built. A
cell names its arm and ends as ok (verdict REALIZABLE / UNREALIZABLE), timeout, error, or none
(ek-derived only: no output dependency found, so there is no Tout to race). The run's
verdict_mismatches lists every instance on which two finished arms disagree, and its `bar` says
whether the bar is met: on at least one hard family (mux n>=16, shift n>=20), a fixed arm is
at least 2x faster than both NK and ltlfsynt (geometric mean over the family's instances where
that fixed arm finished; a baseline that timed out counts as its timeout, a lower bound; an
instance whose baseline errored is left out), with 0 verdict mismatches and 0 fixed failures.
"""
import argparse
import datetime
import json
import math
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time

import hard_instances

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
DEFAULT_SPECS = os.path.join(REPO, "tests", "data", "hard-instances")
SYNTH_FLAGS = ["--otf-mtdfa-product", "--otf-solve", "--simplify-formula", "--realizable"]
ARMS = ("nk", "ltlfsynt", "ek-derived", "ek-compact", "ek-derived-fixed", "ek-compact-fixed")
FIX_FLAG = "--fix-dependent-outputs"
BAR_SPEEDUP = 2.0
HARD_FAMILIES = {"mux": 16, "shift": 20}  # family -> smallest n of the hard range
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


def cell(arm, status, wall, verdict=None, **extra):
    """One cell of the run JSON: the only place that knows its keys."""
    return {"arm": arm, "status": status, "verdict": verdict, "wall_s": wall, **extra}


def synth_cell(arm, synth, formula, args, timeout, flags=()):
    rc, out, wall = run([synth, *SYNTH_FLAGS, *flags, "--formula", formula, *args], timeout)
    if rc is None:
        return cell(arm, "timeout", wall)
    if rc in VERDICTS:
        return cell(arm, "ok", wall, VERDICTS[rc])
    return cell(arm, "error", wall, rc=rc)


def verification(rc, out):
    """`--model-check`'s outcome: verified (SAFE), unsafe (a counterexample), else unverifiable (the verifier
    itself ran out of time or memory)."""
    if rc == 0 and out.strip() == "SAFE":
        return "verified"
    return "unsafe" if out.startswith("UNSAFE") else "unverifiable"


def verified_cell(arm, synth, formula, args, timeout):
    """A fixed arm: the timed synthesis, then (on REALIZABLE) a --model-check of its controller under the same timeout."""
    got = synth_cell(arm, synth, formula, args, timeout, [FIX_FLAG])
    if got["status"] != "ok" or got["verdict"] != "REALIZABLE":
        return got
    flags = [f for f in SYNTH_FLAGS if f != "--realizable"] + [FIX_FLAG, "--model-check"]
    rc, out, verify = run([synth, *flags, "--formula", formula, *args], timeout)
    got["verify_s"] = verify
    got["controller"] = verification(rc, out)
    if got["controller"] == "unverifiable":
        got["verify_limit"] = "timeout" if rc is None else "rc %d" % rc
    if got["controller"] == "unsafe":
        got.update(status="error", rc=rc)
    return got


def ltlfsynt_cell(ltlfsynt, formula, part, timeout):
    rc, out, wall = run([ltlfsynt, "--ins=" + ",".join(part["input_free"]),
                         "--outs=" + ",".join(part["output_free"]), "-f", formula], timeout)
    if rc is None:
        return cell("ltlfsynt", "timeout", wall)
    verdict = out.split("\n", 1)[0].strip()
    if rc in (0, 1) and verdict in VERDICTS.values():
        return cell("ltlfsynt", "ok", wall, verdict)
    return cell("ltlfsynt", "error", wall, rc=rc)


def derived_cell(build, formula, part_path, scratch, timeout):
    emitted, tout = os.path.join(scratch, "derived.part"), os.path.join(scratch, "derived.tout")
    rc, out, extract = run([os.path.join(build, "ltlf-ek-deps"), "--direction", "out", "--formula", formula,
                            "--part-file", part_path, "--emit-part", emitted, "--transducer", tout], timeout)
    if rc is None:
        return [cell("ek-derived", "timeout", extract, extract_s=extract)]
    if rc != 0:
        return [cell("ek-derived", "error", extract, extract_s=extract, rc=rc)]
    if not os.path.exists(tout):
        return [cell("ek-derived", "none", extract, extract_s=extract)]
    synth = os.path.join(build, "ltlf-ek-synth")
    args = ["--part-file", emitted, "--known-output-transducer", tout]
    cells = []
    for arm, make in (("ek-derived", synth_cell), ("ek-derived-fixed", verified_cell)):
        synthed = make(arm, synth, formula, args, max(timeout - extract, 1))
        synthed.update(extract_s=extract, synth_s=synthed["wall_s"], wall_s=extract + synthed["wall_s"])
        cells.append(synthed)
    return cells


def compact_cell(build, formula, part, tout_path, scratch, timeout):
    known = os.path.join(scratch, "compact.part")
    with open(known, "w") as f:
        f.write(hard_instances.part_text(part["input_free"], part["output_free"], known=part["output_free"]))
    synth = os.path.join(build, "ltlf-ek-synth")
    args = ["--part-file", known, "--known-output-transducer", tout_path]
    return [synth_cell("ek-compact", synth, formula, args, timeout),
            verified_cell("ek-compact-fixed", synth, formula, args, timeout)]


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
        got = derived_cell(build, formula, base + ".part", scratch, timeout)
        cells += got
        if os.path.exists(base + ".tout"):
            cells += compact_cell(build, formula, part, base + ".tout", scratch, timeout)
    return [dict(c, instance=name, family=family, n=n) for c in cells]


def verdict_mismatches(cells):
    """Instances on which two finished arms disagree on realizability."""
    seen = {}
    for c in cells:
        if c["status"] == "ok":
            seen.setdefault(c["instance"], {})[c["arm"]] = c["verdict"]
    return {i: v for i, v in sorted(seen.items()) if len(set(v.values())) > 1}


def geomean(xs):
    return math.exp(sum(math.log(x) for x in xs) / len(xs))


def bar(cells, timeout, mismatches, failures):
    """Whether the bar is met, with the evidence per (fixed arm, hard family).

    speedup of an instance = min(NK wall, ltlfsynt wall) / fixed wall, a timed-out baseline counting as
    `timeout` and an instance with an errored baseline left out; a family's speedup is the geometric mean
    over the instances where the fixed arm finished. `unverifiable` counts the evidence instances whose
    controller the verifier could not finish, and `fixed_unfinished` the family's instances the arm did not finish.
    """
    by = {}
    for c in cells:
        by.setdefault(c["instance"], {})[c["arm"]] = c
    evidence = []
    for arm in ("ek-derived-fixed", "ek-compact-fixed"):
        for family, n_min in HARD_FAMILIES.items():
            speedups, unverifiable, unfinished = [], 0, 0
            for inst, got in by.items():
                fam, n = instance_key(inst)
                fixed = got.get(arm)
                if fam != family or n is None or n < n_min or not fixed:
                    continue
                if fixed["status"] != "ok":
                    unfinished += 1
                    continue
                base_cells = [got.get(a) for a in ("nk", "ltlfsynt")]
                if any(not b or b["status"] not in ("ok", "timeout") for b in base_cells):
                    continue
                base = [b["wall_s"] if b["status"] == "ok" else timeout for b in base_cells]
                unverifiable += fixed.get("controller") == "unverifiable"
                speedups.append(min(base) / max(fixed["wall_s"], 1e-3))
            if speedups:
                evidence.append({"arm": arm, "family": family, "instances": len(speedups),
                                 "speedup_geomean": geomean(speedups),
                                 "unverifiable": unverifiable, "fixed_unfinished": unfinished})
    met = not mismatches and not failures and any(e["speedup_geomean"] >= BAR_SPEEDUP for e in evidence)
    return {"met": met, "required_speedup": BAR_SPEEDUP, "mismatches": len(mismatches), "fixed_failures": len(failures),
            "evidence": evidence}


def fixed_failures(cells):
    """Fixed-arm cells that errored: an unsafe controller, or a Tout whose lambda leaves a known output free."""
    return [c["instance"] + ":" + c["arm"] for c in cells if c["arm"].endswith("-fixed") and c["status"] == "error"]


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
        "fixed_failures": fixed_failures(cells),
    }
    result["bar"] = bar(cells, args.timeout, result["verdict_mismatches"], result["fixed_failures"])
    with open(args.out, "w") as f:
        json.dump(result, f, indent=1)
        f.write("\n")
    return 1 if result["verdict_mismatches"] or result["fixed_failures"] else 0


if __name__ == "__main__":
    sys.exit(main())
