#!/usr/bin/env python3
"""Merge the per-family sweep JSONs of a SLURM job array into one sweep JSON.

    python3 scripts/merge_sweep.py PART.json [PART.json ...] > sweep.json

Each part is one ltlf-ek-bench invocation over a subset of the families. The
row lists (timings, structural, ltlfsynt, and any other top-level list) are
concatenated; the provenance of the first part stands, with the family list
unioned, stopped_early OR-ed, verdict_mismatch_count summed and
sweep_wall_seconds the longest part (the parts run concurrently). Parts that
disagree on build type, Spot version, commit, repeat or timeout were not one
sweep, so the merge refuses them.
"""
import json
import sys

SAME = ("cmake_build_type", "spot_version", "repo_commit", "repeat", "timeout_seconds")


def merge(parts):
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
    prov["stopped_early"] = any(p["provenance"]["stopped_early"] for p in parts)
    prov["verdict_mismatch_count"] = sum(p["provenance"]["verdict_mismatch_count"] for p in parts)
    prov["sweep_wall_seconds"] = max(p["provenance"].get("sweep_wall_seconds", 0) for p in parts)
    merged = {"provenance": prov}
    for part in parts:
        for key, value in part.items():
            if isinstance(value, list):
                merged.setdefault(key, []).extend(value)
    return merged


def main(argv=None):
    paths = (sys.argv[1:] if argv is None else argv)
    if not paths:
        sys.exit(__doc__)
    parts = []
    for path in paths:
        with open(path) as f:
            parts.append(json.load(f))
    json.dump(merge(parts), sys.stdout)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
