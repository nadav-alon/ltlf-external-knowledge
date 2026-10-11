#!/usr/bin/env python3
"""Markdown report of an ltlf-ek-bench sweep JSON, one section per family.

    python3 scripts/sweep_report.py SWEEP.json [--subject otf-mtdfa-product] > report.md

Every slippery-* arm is one family. Per family and per N the report gives the
prune ratio (reachable product states with the knowledge over without:
dfa-product / dfa-product-unpruned; the same over dfa-product-nk is the
column "vs -nk", the task-equivalent baseline), the EK and ltlfsynt wall-clock, and the
speedup over ltlfsynt. A cell is one (arm, realizable) pair; the figure for a
family at N is the median over its cells. The headline per family is the
speedup at the largest N where at least one cell finished on both sides.
ltlfsynt's startup floor (~4 ms) is not subtracted.
"""
import argparse
import json
import statistics
import sys
from collections import defaultdict

EK_SUBJECT = "otf-mtdfa-product"
EK_UNPRUNED = "dfa-product-unpruned"
EK_PRODUCT = "dfa-product"
EK_NK = "dfa-product-nk"


def family_group(name):
    return "slippery" if name.startswith("slippery-") else name


def _finished_wall(timings):
    """(family, n, realizable, subject) -> (min_ns, median_ns), finished cells only."""
    out, failed = {}, set()
    for r in timings:
        key = (r["family"], r["n"], r["realizable"], r["subject"])
        if r["stage"] == "TIMEOUT" or r["stage"].startswith("FAILED_"):
            failed.add(key)
        elif r["stage"] == "wall_total" and r.get("median_ns") is not None:
            out[key] = (r["ns"], r["median_ns"])
    for key in failed:
        out.pop(key, None)
    return out


def _product_states(structural, subject):
    return {
        (r["family"], r["n"], r["realizable"]): r["value"]
        for r in structural
        if r["subject"] == subject and r["metric"] == "product_states"
    }


def build_rows(sweep, subject=EK_SUBJECT):
    """{group: {n: {'cells', 'prune', 'prune_nk', 'ek_med', 'ek_min', 'lt_med', 'lt_min',
    'speedup_med', 'speedup_min'}}}; a figure is None when no cell has it."""
    wall = _finished_wall(sweep["timings"])
    with_k = _product_states(sweep["structural"], EK_PRODUCT)
    without_k = _product_states(sweep["structural"], EK_UNPRUNED)
    nk_k = _product_states(sweep["structural"], EK_NK)
    lt = {
        (r["family"], r["n"], r["realizable"]): r
        for r in sweep["ltlfsynt"]
        if r["status"] == "ok" and r.get("ltlfsynt_median_ns") is not None
    }

    cells = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))
    keys = {(r["family"], r["n"], r["realizable"]) for r in sweep["timings"]}
    for fam, n, real in sorted(keys):
        c = cells[family_group(fam)][n]
        if (fam, n, real) in with_k and without_k.get((fam, n, real)):
            c["prune"].append(with_k[(fam, n, real)] / without_k[(fam, n, real)])
        if (fam, n, real) in with_k and nk_k.get((fam, n, real)):
            c["prune_nk"].append(with_k[(fam, n, real)] / nk_k[(fam, n, real)])
        ek = wall.get((fam, n, real, subject))
        row = lt.get((fam, n, real))
        if ek:
            c["ek_min"].append(ek[0])
            c["ek_med"].append(ek[1])
        if row:
            c["lt_min"].append(row["ltlfsynt_ns"])
            c["lt_med"].append(row["ltlfsynt_median_ns"])
        if ek and row:
            c["speedup_med"].append(row["ltlfsynt_median_ns"] / ek[1])
            c["speedup_min"].append(row["ltlfsynt_ns"] / ek[0])

    def med(xs):
        return statistics.median(xs) if xs else None

    return {
        g: {n: {k: med(v) for k, v in by_n[n].items()} | {"cells": len(by_n[n]["ek_med"])}
            for n in sorted(by_n)}
        for g, by_n in cells.items()
    }


def _fmt(x, fmt):
    return "n/a" if x is None else fmt.format(x)


def render(sweep, subject=EK_SUBJECT):
    prov = sweep["provenance"]
    rows = build_rows(sweep, subject)
    out = [
        "# Sweep report",
        "",
        f"- build `{prov['cmake_build_type']}`, Spot {prov['spot_version']}, "
        f"commit `{prov['repo_commit'][:7]}`, repeat={prov['repeat']}, "
        f"timeout={prov['timeout_seconds']} s",
        f"- EK subject: `{subject}` (wall-clock, median of the repeats); "
        "ltlfsynt on the monolithic `psi_in -> phi` encoding",
        "- prune ratio: reachable product states with knowledge over without "
        "(`dfa-product` / `dfa-product-unpruned`)",
        "- prune ratio (vs -nk): the same over `dfa-product-nk` (psi_in folded into "
        "the Goal; the same decision problem ltlfsynt is given)",
        "- a family's figure at N is the median over its cells (arm x realizable); "
        "all slippery-* arms are one family",
        "- ltlfsynt startup floor (~4 ms) is noted, not subtracted",
        f"- stopped early: {prov['stopped_early']}; verdict mismatches: "
        f"{prov['verdict_mismatch_count']}",
        "",
    ]
    for group in sorted(rows):
        out += [f"## {group}", ""]
        out += ["| N | cells | prune ratio | prune ratio (vs -nk) | EK med (ms) | EK min (ms) | "
                "ltlfsynt med (ms) | speedup (med) | speedup (min) |",
                "|---|---|---|---|---|---|---|---|---|"]
        for n, r in rows[group].items():
            out.append(
                f"| {n} | {r['cells']} | {_fmt(r.get('prune'), '{:.3f}')} "
                f"| {_fmt(r.get('prune_nk'), '{:.3f}')} "
                f"| {_fmt(r.get('ek_med') and r['ek_med'] / 1e6, '{:.2f}')} "
                f"| {_fmt(r.get('ek_min') and r['ek_min'] / 1e6, '{:.2f}')} "
                f"| {_fmt(r.get('lt_med') and r['lt_med'] / 1e6, '{:.2f}')} "
                f"| {_fmt(r.get('speedup_med'), '{:.2f}x')} "
                f"| {_fmt(r.get('speedup_min'), '{:.2f}x')} |")
        raced = [n for n, r in rows[group].items() if r.get("speedup_med") is not None]
        out.append("")
        if raced:
            n = max(raced)
            out.append(f"**Speedup over ltlfsynt at the largest N finished "
                       f"(N={n}): {rows[group][n]['speedup_med']:.2f}x (median).**")
        else:
            out.append("**No speedup over ltlfsynt: no cell finished on both sides "
                       "(ltlfsynt is n/a for a family without a declared psi_in).**")
        out.append("")
    return "\n".join(out)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("sweep")
    ap.add_argument("--subject", default=EK_SUBJECT)
    args = ap.parse_args(argv)
    with open(args.sweep) as f:
        sweep = json.load(f)
    sys.stdout.write(render(sweep, args.subject) + "\n")


if __name__ == "__main__":
    main()
