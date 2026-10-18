#!/usr/bin/env python3
"""Markdown table of one EK subject against one formula subject, per family and N, from a sweep JSON.

    python3 scripts/sweep_pairs.py SWEEP.json EK_SUBJECT FORMULA_SUBJECT [--arms] > table.md

Every figure is a median over the cells it names. A speedup counts only cells where both sides
finished; the lower-bound column also counts cells where EK finished and the other side did not,
at timeout / EK median, so it never hides an unfinished cell.
"""
import argparse
import json
import os
import statistics
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep_report import PRODUCT_MTDFA_ROOTS, _finished_wall, _metric_by_cell, family_group  # noqa: E402


def _cells(sweep, subject):
    return {(r["family"], r["n"], r["realizable"]) for r in sweep["timings"] if r["subject"] == subject}


def build_rows(sweep, ek, formula, arms=False):
    """(group, n) -> figures; group is the family, or the arm with arms=True."""
    timeout_ns = sweep["provenance"]["timeout_seconds"] * 1e9
    wall = _finished_wall(sweep["timings"])
    roots_ek = _metric_by_cell(sweep["structural"], ek, PRODUCT_MTDFA_ROOTS)
    roots_f = _metric_by_cell(sweep["structural"], formula, PRODUCT_MTDFA_ROOTS)
    lt = {(r["family"], r["n"], r["realizable"]): r for r in sweep["ltlfsynt"]}
    raced = _cells(sweep, formula)
    acc = defaultdict(lambda: defaultdict(list))
    for key in sorted(_cells(sweep, ek)):
        fam, n, _ = key
        c = acc[(fam if arms else family_group(fam), n)]
        c["cells"].append(1)
        e = wall.get(key + (ek,))
        if not e:
            continue
        c["ek"].append(e[1])
        f = wall.get(key + (formula,))
        if key in raced:
            if f:
                c["formula"].append(f[1])
                c["speedup"].append(f[1] / e[1])
                c["bound"].append(f[1] / e[1])
            else:
                c["unfinished"].append(1)
                c["bound"].append(timeout_ns / e[1])
        if roots_ek.get(key) and roots_f.get(key):
            c["roots"].append(roots_ek[key] / roots_f[key])
        row = lt.get(key)
        if row and row["status"] == "ok" and row.get("ltlfsynt_internal_ns"):
            c["lt"].append(row["ltlfsynt_internal_ns"] / e[1])
            c["lt_bound"].append(row["ltlfsynt_internal_ns"] / e[1])
        elif row and row["status"] == "timeout":
            c["lt_timeout"].append(1)
            c["lt_bound"].append(timeout_ns / e[1])
    med = lambda xs: statistics.median(xs) if xs else None  # noqa: E731
    return {
        k: {
            "cells": len(c["cells"]), "ek_finished": len(c["ek"]),
            "both": len(c["speedup"]), "unfinished": len(c["unfinished"]),
            "ek_ms": med([x / 1e6 for x in c["ek"]]),
            "formula_ms": med([x / 1e6 for x in c["formula"]]),
            "speedup": med(c["speedup"]), "bound": med(c["bound"]) if c["unfinished"] else None,
            "roots": med(c["roots"]),
            "lt": med(c["lt"]), "lt_n": len(c["lt"]), "lt_timeouts": len(c["lt_timeout"]),
            "lt_bound": med(c["lt_bound"]) if c["lt_timeout"] else None,
        }
        for k, c in acc.items()
    }


def _f(x, fmt):
    return "n/a" if x is None else fmt.format(x)


def render(sweep, ek, formula, arms=False):
    rows = build_rows(sweep, ek, formula, arms)
    out = [
        f"EK `{ek}` vs formula `{formula}`, both timed in process; ltlfsynt is its summed `--verbose` stage time.",
        "",
        "| family | N | cells (EK finished / all) | EK med (ms) | formula med (ms) | speedup, cells both finished | "
        "speedup lower bound, unfinished at timeout | explored-roots ratio | ltlfsynt internal speedup (cells) | "
        "ltlfsynt lower bound, timeouts at timeout |",
        "|---|---|---|---|---|---|---|---|---|---|",
    ]
    for (g, n) in sorted(rows):
        r = rows[(g, n)]
        sp = "n/a" if r["speedup"] is None else f"{r['speedup']:.2f}x ({r['both']})"
        bound = "—" if r["bound"] is None else f"≥ {r['bound']:.2f}x ({r['unfinished']} unfinished)"
        lt = "n/a" if r["lt"] is None else f"{r['lt']:.2f}x ({r['lt_n']})"
        ltb = "—" if r["lt_bound"] is None else f"≥ {r['lt_bound']:.2f}x ({r['lt_timeouts']} timed out)"
        out.append(
            f"| {g} | {n} | {r['ek_finished']} / {r['cells']} | {_f(r['ek_ms'], '{:.2f}')} | "
            f"{_f(r['formula_ms'], '{:.2f}')} | {sp} | {bound} | {_f(r['roots'], '{:.3f}')} | {lt} | {ltb} |"
        )
    return "\n".join(out) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("sweep")
    ap.add_argument("ek")
    ap.add_argument("formula")
    ap.add_argument("--arms", action="store_true", help="one row per slippery arm instead of one per family")
    args = ap.parse_args(argv)
    with open(args.sweep) as f:
        sys.stdout.write(render(json.load(f), args.ek, args.formula, args.arms))


if __name__ == "__main__":
    main()
