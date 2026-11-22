#!/usr/bin/env python3
"""SYNTCOMP `tlsf-fin` prune-ratio sweep driver (docs/research/
2026-11-22-syntcomp-fin-prune-ratio.md).

Runs build/prune-ratio-probe (scripts/prune_ratio_probe.cpp, which carries its
compile line) once per instance, in its own process under `ulimit -v` and
`timeout`, and appends one JSON line per instance to --out.  Re-running with the
same --out skips instances already present, so a stopped sweep resumes.

Order: tier 1 (n_ap <= 16) then tier 2, each in a seeded shuffle, so a
truncated run is an unbiased subsample of the tier it stopped in.

  prune_ratio_sweep.py CORPUS_DIR --out rows.jsonl [--workers 6]
        [--cap-gib 2] [--timeout1 60] [--timeout2 20] [--deadline-min M]
  prune_ratio_sweep.py --summarize rows.jsonl     # the ratio < 0.5 list
"""
import argparse
import concurrent.futures
import json
import os
import random
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tlsf_adapter  # noqa: E402

PROBE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build",
                     "prune-ratio-probe")
TIER1_MAX_AP = 16
THRESHOLD = 0.5

# Row outcomes: one each, never folded into another.
MEASURED = "MEASURED"
NO_KNOWLEDGE = "NO_KNOWLEDGE"
PARTIAL = "PARTIAL"
TIMEOUT = "TIMEOUT"
MEMOUT = "MEMOUT"


def prune_ratio(row):
    """K/U, or None when the row has no two measured product sizes."""
    k, u = row.get("k"), row.get("u")
    if not k or not u or not k.get("product_states") or not u.get("product_states"):
        return None
    return k["product_states"] / u["product_states"]


def classify(row):
    """One outcome per row: a TIMEOUT or MEMOUT row is never folded into a measured one."""
    st = row.get("status")
    if st != "OK":
        return st
    if row.get("known_outputs") == 0:
        return NO_KNOWLEDGE
    if prune_ratio(row) is None:
        return PARTIAL  # extraction done, a product did not finish
    return MEASURED


def candidates(rows):
    """Measured rows with ratio < THRESHOLD, smallest ratio first."""
    out = [(prune_ratio(r), r) for r in rows if classify(r) == MEASURED]
    return sorted([(q, r) for q, r in out if q < THRESHOLD],
                  key=lambda qr: (qr[0], qr[1]["file"]))


def n_ap(conv):
    return len(conv["inputs"]) + len(conv["outputs"])


def last_json_line(lines):
    """The last `{` line parsed, or None when there is none or it is truncated."""
    if not lines:
        return None
    try:
        return json.loads(lines[-1])
    except ValueError:
        return None


def run_one(rel, tier, cap_kib, timeout_s, conv):
    """`conv` is the tlsf_adapter.convert result, or its NeedsTlsf exception."""
    row = {"file": rel, "tier": tier}
    if isinstance(conv, tlsf_adapter.NeedsTlsf):
        return dict(row, status="NEEDS_TLSF", what=str(conv))
    row["n_ap"] = n_ap(conv)
    with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as d:
        fpath, ppath = os.path.join(d, "f.ltlf"), os.path.join(d, "p.part")
        with open(fpath, "w") as f:
            f.write(conv["formula"])
        with open(ppath, "w") as f:
            f.write(tlsf_adapter.part_file_text(conv))
        cmd = ["bash", "-c", 'ulimit -v %d; exec timeout -s KILL %d "$@"'
               % (cap_kib, timeout_s), "_", PROBE, fpath, ppath]
        t0 = time.time()
        p = subprocess.run(cmd, capture_output=True, text=True)
        row["wall"] = round(time.time() - t0, 3)
    lines = [ln for ln in p.stdout.splitlines() if ln.startswith("{")]
    if p.returncode == 137 or (p.returncode < 0):
        got = last_json_line(lines) or {}  # a partial line is dropped
        return dict(row, status=TIMEOUT, **{k: v for k, v in got.items()
                                              if k != "status"})
    if not lines:
        why = MEMOUT if "bad_alloc" in p.stderr else "ERR"
        return dict(row, status=why, what=p.stderr.strip()[-200:])
    got = last_json_line(lines)
    if got is None:
        return dict(row, status="ERR", what="unparsable probe output")
    if got.get("status") == "ERR" and "bad_alloc" in got.get("what", ""):
        got["status"] = MEMOUT
    return dict(row, **got)


def summarize(rows):
    from collections import Counter
    for tier in (1, 2):
        rs = [r for r in rows if r["tier"] == tier]
        print("tier %d: %d rows %s" % (tier, len(rs),
              dict(Counter(classify(r) for r in rs))))
    print("ratio < %.1f:" % THRESHOLD)
    for q, r in candidates(rows):
        print("  %.3f  K=%d U=%d  known %d/%d  %s" % (
            q, r["k"]["product_states"], r["u"]["product_states"],
            r["known_outputs"], r["outputs"], r["file"]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("corpus", nargs="?")
    ap.add_argument("--out")
    ap.add_argument("--summarize")
    ap.add_argument("--workers", type=int, default=6)
    ap.add_argument("--cap-gib", type=float, default=2)
    ap.add_argument("--timeout1", type=int, default=60)
    ap.add_argument("--timeout2", type=int, default=20)
    ap.add_argument("--deadline-min", type=float, default=None)
    ap.add_argument("--seed", type=int, default=20261122)
    a = ap.parse_args()
    if a.summarize:
        with open(a.summarize) as f:
            summarize([json.loads(ln) for ln in f if ln.strip()])
        return 0
    done = set()
    if os.path.exists(a.out):
        with open(a.out) as f:
            done = {json.loads(ln)["file"] for ln in f if ln.strip()}
    files = []
    for root, _, names in os.walk(a.corpus):
        files += [os.path.join(root, n) for n in names if n.endswith(".tlsf")]
    files = sorted(files)
    tiers = {1: [], 2: []}
    for p in files:
        rel = os.path.relpath(p, a.corpus)
        if rel in done:
            continue
        try:
            with open(p) as f:
                c = tlsf_adapter.convert(f.read())
        except tlsf_adapter.NeedsTlsf as e:
            tiers[1].append((p, rel, e))  # run_one turns the exception into a row
            continue
        tiers[1 if n_ap(c) <= TIER1_MAX_AP else 2].append((p, rel, c))
    rng = random.Random(a.seed)
    order = []
    for t in (1, 2):
        rng.shuffle(tiers[t])
        order += [(p, rel, t, c) for p, rel, c in tiers[t]]
    deadline = time.time() + a.deadline_min * 60 if a.deadline_min else None
    cap_kib = int(a.cap_gib * 1024 * 1024)
    with open(a.out, "a") as out, \
            concurrent.futures.ThreadPoolExecutor(a.workers) as ex:
        it = iter(order)
        pending = set()
        while True:
            while len(pending) < a.workers * 2 and not (deadline and time.time() > deadline):
                nxt = next(it, None)
                if nxt is None:
                    break
                p, rel, t, c = nxt
                pending.add(ex.submit(run_one, rel, t, cap_kib,
                                      a.timeout1 if t == 1 else a.timeout2, c))
            if not pending:
                break
            fin, pending = concurrent.futures.wait(
                pending, return_when=concurrent.futures.FIRST_COMPLETED)
            for fut in fin:
                out.write(json.dumps(fut.result()) + "\n")
                out.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main())
