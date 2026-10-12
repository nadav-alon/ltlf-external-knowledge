#!/usr/bin/env python3
"""Merge the per-family sweep JSONs of a SLURM job array into one sweep JSON.

    python3 scripts/merge_sweep.py [--expect-families=A,B,...] PART.json [PART.json ...] > sweep.json

Each part is one ltlf-ek-bench invocation over a subset of the families. The
row lists (timings, structural, ltlfsynt, and any other top-level list) are
concatenated; the provenance of the first part stands, with the family list
unioned, stopped_early OR-ed, verdict_mismatch_count summed and
sweep_wall_seconds the sum of the parts (they run one after another on one
node). Parts that disagree on machine, build type, Spot version, commit, repeat,
timeout, grid bounds, budget, subjects or resolved Spot path were not one sweep, so
the merge refuses them. With --expect-families, a merged family set that is not
exactly the expected one is refused too.
"""
import json
import sys

SAME = ("machine", "cmake_build_type", "spot_version", "repo_commit", "repeat", "timeout_seconds",
        "n_min", "n_max", "subjects", "budget_seconds", "spot_resolved_path")


def merge(parts, expect_families=None):
    if not parts:
        raise ValueError("merge_sweep: no parts")
    prov = dict(parts[0]["provenance"])
    for i, part in enumerate(parts[1:], 1):
        for key in SAME:
            if part["provenance"].get(key) != prov.get(key):
                raise ValueError(f"merge_sweep: part {i} differs on {key}: "
                                 f"{part['provenance'].get(key)!r} vs {prov.get(key)!r}")
    seen = [f for p in parts for f in p["provenance"].get("families", [])]
    prov["families"] = list(dict.fromkeys(seen))
    if expect_families is not None and set(prov["families"]) != set(expect_families):
        raise ValueError(f"merge_sweep: families {sorted(prov['families'])} are not the "
                         f"expected {sorted(expect_families)}")
    prov["stopped_early"] = any(p["provenance"]["stopped_early"] for p in parts)
    prov["verdict_mismatch_count"] = sum(p["provenance"]["verdict_mismatch_count"] for p in parts)
    prov["sweep_wall_seconds"] = sum(p["provenance"].get("sweep_wall_seconds", 0) for p in parts)
    merged = {"provenance": prov}
    for part in parts:
        for key, value in part.items():
            if isinstance(value, list):
                merged.setdefault(key, []).extend(value)
    return merged


def main(argv=None):
    args = sys.argv[1:] if argv is None else argv
    expect = None
    paths = []
    for arg in args:
        if arg.startswith("--expect-families="):
            expect = [f for f in arg.split("=", 1)[1].split(",") if f]
        else:
            paths.append(arg)
    if not paths:
        sys.exit(__doc__)
    parts = []
    for path in paths:
        with open(path) as f:
            parts.append(json.load(f))
    try:
        merged = merge(parts, expect)
    except ValueError as e:
        sys.exit(str(e))
    json.dump(merged, sys.stdout)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
