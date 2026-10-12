#!/usr/bin/env python3
"""Markdown report of an ltlf-ek-bench sweep JSON, one section per family.

    python3 scripts/sweep_report.py SWEEP.json [--subject otf-mtdfa-product] > report.md

Every slippery-* arm is one family. Per family and per N the report gives the
prune ratio (reachable product states with the knowledge over without:
dfa-product / dfa-product-unpruned; the same over dfa-product-nk is the
nk-product ratio, the task-equivalent baseline), the EK and ltlfsynt wall-clock,
and the speedup over ltlfsynt. The cells column counts the cells EK finished,
the raced column those finished on both sides. A cell is one (arm, realizable)
pair; the figure for a family at N is the median over its cells. A cell with any timed-out or
failed repeat, on either side, is not finished: its median would be taken over
the survivors alone, so it is left out. The headline per family is the speedup
at the largest N where every cell finished on both sides, else at the largest N
where some did, with the cell count, and says why it stopped at that N (the first N that
lost a cell to an ltlfsynt error or timeout, or to an unfinished cell).
Where ltlfsynt timed out and EK finished, the speedup is reported as a lower
bound; where it errored, a note names the N and the cell count.
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
    """(family, n, realizable, subject) -> (min_ns, median_ns), cells whose every repeat finished."""
    out, failed = {}, set()
    for r in timings:
        key = (r["family"], r["n"], r["realizable"], r["subject"])
        if r["stage"] == "TIMEOUT" or r["stage"].startswith(
                ("FAILED_", "PARTIAL_TIMEOUT_", "PARTIAL_FAILED_")):
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
    """{group: {n: {'cells', 'both', 'total', 'lt_timeouts', 'lt_bounded', 'lt_errors', 'prune', 'prune_nk', 'ek_med', 'ek_min', 'lt_med', 'lt_min',
    'speedup_med', 'speedup_min', 'bound'}}}; a figure is None when no cell has it."""
    wall = _finished_wall(sweep["timings"])
    with_k = _product_states(sweep["structural"], EK_PRODUCT)
    without_k = _product_states(sweep["structural"], EK_UNPRUNED)
    nk_k = _product_states(sweep["structural"], EK_NK)
    lt = {
        (r["family"], r["n"], r["realizable"]): r
        for r in sweep["ltlfsynt"]
        if r["status"] == "ok" and r.get("ltlfsynt_median_ns") is not None
    }

    lt_timeout = {
        (r["family"], r["n"], r["realizable"])
        for r in sweep["ltlfsynt"]
        if r["status"] in ("timeout", "partial_timeout")
    }

    lt_error = {
        (r["family"], r["n"], r["realizable"])
        for r in sweep["ltlfsynt"]
        if r["status"] == "error"
    }

    cells = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))
    keys = {(r["family"], r["n"], r["realizable"]) for r in sweep["timings"]}
    for fam, n, real in sorted(keys):
        key = (fam, n, real)
        c = cells[family_group(fam)][n]
        c["total"].append(1)
        if key in with_k and without_k.get(key):
            c["prune"].append(with_k[key] / without_k[key])
        if key in with_k and nk_k.get(key):
            c["prune_nk"].append(with_k[key] / nk_k[key])
        ek = wall.get(key + (subject,))
        row = lt.get(key)
        if ek:
            c["ek_min"].append(ek[0])
            c["ek_med"].append(ek[1])
        if row:
            c["lt_min"].append(row["ltlfsynt_ns"])
            c["lt_med"].append(row["ltlfsynt_median_ns"])
        if ek and row:
            c["speedup_med"].append(row["ltlfsynt_median_ns"] / ek[1])
            c["speedup_min"].append(row["ltlfsynt_ns"] / ek[0])
        if key in lt_error:
            c["lt_error"].append(1)
        if key in lt_timeout:
            c["lt_timeout"].append(1)
        if ek and key in lt_timeout:
            c["bound"].append(sweep["provenance"]["timeout_seconds"] * 1e9 / ek[1])

    def med(xs):
        return statistics.median(xs) if xs else None

    return {
        g: {n: {k: med(v) for k, v in by_n[n].items()}
               | {"cells": len(by_n[n]["ek_med"]), "both": len(by_n[n]["speedup_med"]),
                  "total": len(by_n[n]["total"]), "lt_timeouts": len(by_n[n]["lt_timeout"]),
                  "lt_bounded": len(by_n[n]["bound"]),
                  "lt_errors": len(by_n[n]["lt_error"])}
            for n in sorted(by_n)}
        for g, by_n in cells.items()
    }


def _fmt(x, fmt):
    return "n/a" if x is None else fmt.format(x)


def _count(k, noun):
    return f"{k} {noun}{'s' * (k > 1)}"


def _unraced(r):
    """Why a cell at this N did not race on both sides, as 'a, b' (empty when all raced)."""
    parts = []
    if r["lt_errors"]:
        parts.append(_count(r["lt_errors"], "ltlfsynt error"))
    if r["lt_timeouts"]:
        parts.append(_count(r["lt_timeouts"], "ltlfsynt timeout"))
    other = r["total"] - r["both"] - r["lt_errors"] - r["lt_timeouts"]
    if other > 0:
        parts.append(f"{other} unfinished on another side")
    return ", ".join(parts)


def _stop_reason(by_n, n):
    """The clause the headline at N=n ends with, leading '; ' included.

    Names the first N >= n with an unraced cell: 'stops at' when it is above n,
    'partial' when n itself lost cells (no N was complete). Otherwise says every
    cell finished, or, when only an N below n lost cells, that every cell at n did.
    """
    for m in sorted(k for k in by_n if k >= n):
        why = _unraced(by_n[m])
        if why:
            if m == n:
                return f"; partial: N={n} has {why}"
            return f"; stops at N={n} because N={m} has {why}"
    if any(_unraced(r) for r in by_n.values()):
        return f"; every cell at N={n} finished"
    return "; every cell finished"


def _headline(by_n, timeout_s):
    raced = [n for n, r in by_n.items() if r.get("speedup_med") is not None]
    complete = [n for n in raced if by_n[n]["both"] == by_n[n]["total"]]
    lines = []
    if raced:
        n = max(complete or raced)
        r = by_n[n]
        lines.append(f"**Speedup over ltlfsynt at the largest N finished "
                     f"(N={n}, {r['both']} of {r['total']} cells): "
                     f"{r['speedup_med']:.2f}x (median){_stop_reason(by_n, n)}.**")
    else:
        lines.append("**No speedup over ltlfsynt: no cell finished on both sides "
                     "(ltlfsynt is n/a for a family without a declared psi_in).**")
    for n, r in by_n.items():
        if r["lt_bounded"]:
            lines += ["", f"ltlfsynt timed out ({timeout_s} s) at N={n} in "
                          f"{r['lt_bounded']} of {r['total']} cells where EK finished: "
                          f"speedup at least {r['bound']:.2f}x (median lower bound)."]
        if r["lt_errors"]:
            lines += ["", f"ltlfsynt errored at N={n} in {r['lt_errors']} of "
                          f"{r['total']} cells: those cells have no speedup."]
    return lines


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
        "- nk-product ratio: `dfa-product` over `dfa-product-nk` (psi_in folded into "
        "the Goal; the same decision problem ltlfsynt is given)",
        "- a family's figure at N is the median over its cells (arm x realizable); "
        "all slippery-* arms are one family",
        "- a cell with any timed-out or failed repeat, on either side, is left out "
        "(its median would cover the surviving repeats only)",
        "- ltlfsynt startup floor (~4 ms) is noted, not subtracted",
        f"- verdict mismatches: {prov['verdict_mismatch_count']}",
        "",
    ]
    if prov["stopped_early"]:
        out[2:2] = ["**STOPPED EARLY: the sweep hit its budget, so the grid is truncated "
                    "and the largest N rows are partial.**", ""]
    for group in sorted(rows):
        out += [f"## {group}", ""]
        out += ["| N | cells | raced | prune ratio | nk-product ratio | EK med (ms) | EK min (ms) | "
                "ltlfsynt med (ms) | speedup (med) | speedup (min) |",
                "|---|---|---|---|---|---|---|---|---|---|"]
        for n, r in rows[group].items():
            out.append(
                f"| {n} | {r['cells']} | {r['both']} | {_fmt(r.get('prune'), '{:.3f}')} "
                f"| {_fmt(r.get('prune_nk'), '{:.3f}')} "
                f"| {_fmt(r.get('ek_med') and r['ek_med'] / 1e6, '{:.2f}')} "
                f"| {_fmt(r.get('ek_min') and r['ek_min'] / 1e6, '{:.2f}')} "
                f"| {_fmt(r.get('lt_med') and r['lt_med'] / 1e6, '{:.2f}')} "
                f"| {_fmt(r.get('speedup_med'), '{:.2f}x')} "
                f"| {_fmt(r.get('speedup_min'), '{:.2f}x')} |")
        out.append("")
        out += _headline(rows[group], prov["timeout_seconds"])
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
