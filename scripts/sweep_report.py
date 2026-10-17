#!/usr/bin/env python3
"""Markdown report of an ltlf-ek-bench sweep JSON, one section per family.

    python3 scripts/sweep_report.py SWEEP.json [--subject otf-mtdfa-product] > report.md

Column definitions and the handling of unfinished cells are in the legend the report prints.
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
OTF_NK = "otf-mtdfa-product-nk"
PRODUCT_STATES_MIN = "product_states_min"
PRODUCT_MTDFA_ROOTS = "product_mtdfa_roots"


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


def _metric_by_cell(structural, subject, metric="product_states"):
    return {
        (r["family"], r["n"], r["realizable"]): r["value"]
        for r in structural
        if r["subject"] == subject and r["metric"] == metric
    }


def _ratio(c, name, num, den, key):
    """Append num[key] / den[key] to c[name] when both are present and den is nonzero."""
    if key in num and den.get(key):
        c[name].append(num[key] / den[key])


def build_rows(sweep, subject=EK_SUBJECT):
    """{group: {n: {'cells', 'both', 'total', 'lt_timeouts', 'lt_bounded', 'lt_errors', 'prune', 'prune_nk', 'nk_ratio_min', 'inproc_speedup', 'roots_ratio', 'internal_speedup', 'lt_roots_ratio', 'internal_missing', 'lt_roots_missing', 'ek_roots_missing', 'inproc_bound', 'nk_bounded', 'ek_med', 'ek_min', 'lt_med', 'lt_min',
    'speedup_med', 'speedup_min', 'bound'}}}; a figure is None when no cell has it."""
    wall = _finished_wall(sweep["timings"])
    with_k = _metric_by_cell(sweep["structural"], EK_PRODUCT)
    without_k = _metric_by_cell(sweep["structural"], EK_UNPRUNED)
    nk_k = _metric_by_cell(sweep["structural"], EK_NK)
    with_k_min = _metric_by_cell(sweep["structural"], EK_PRODUCT, PRODUCT_STATES_MIN)
    nk_k_min = _metric_by_cell(sweep["structural"], EK_NK, PRODUCT_STATES_MIN)
    roots = _metric_by_cell(sweep["structural"], EK_SUBJECT, PRODUCT_MTDFA_ROOTS)
    roots_nk = _metric_by_cell(sweep["structural"], OTF_NK, PRODUCT_MTDFA_ROOTS)
    # `roots` above is pinned to EK_SUBJECT for the in-process ratio; this one follows --subject.
    subject_roots = _metric_by_cell(sweep["structural"], subject, PRODUCT_MTDFA_ROOTS)
    nk_timeout = {
        (r["family"], r["n"], r["realizable"])
        for r in sweep["timings"]
        if r["subject"] == OTF_NK and r["stage"] in ("TIMEOUT", "PARTIAL_TIMEOUT_wall_total")
    }
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
        _ratio(c, "prune", with_k, without_k, key)
        _ratio(c, "prune_nk", with_k, nk_k, key)
        _ratio(c, "nk_ratio_min", with_k_min, nk_k_min, key)
        ek = wall.get(key + (subject,))
        row = lt.get(key)
        otf_nk = wall.get(key + (OTF_NK,))
        otf = wall.get(key + (EK_SUBJECT,))
        if otf and otf_nk:
            c["inproc_speedup"].append(otf_nk[1] / otf[1])
            _ratio(c, "roots_ratio", roots, roots_nk, key)
        if otf and key in nk_timeout:
            c["inproc_bound"].append(sweep["provenance"]["timeout_seconds"] * 1e9 / otf[1])
        if ek:
            c["ek_min"].append(ek[0])
            c["ek_med"].append(ek[1])
        if row:
            c["lt_min"].append(row["ltlfsynt_ns"])
            c["lt_med"].append(row["ltlfsynt_median_ns"])
        if ek and row:
            c["speedup_med"].append(row["ltlfsynt_median_ns"] / ek[1])
            c["speedup_min"].append(row["ltlfsynt_ns"] / ek[0])
            if row.get("ltlfsynt_internal_ns") is None:
                c["internal_missing"].append(1)
            else:
                c["internal_speedup"].append(row["ltlfsynt_internal_ns"] / ek[1])
        if row:
            if not row.get("ltlfsynt_explored_roots"):
                c["lt_roots_missing"].append(1)  # absent, or 0 (a solve explores at least one root)
            elif key not in subject_roots:
                c["ek_roots_missing"].append(1)
            else:
                c["lt_roots_ratio"].append(subject_roots[key] / row["ltlfsynt_explored_roots"])
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
                  "nk_bounded": len(by_n[n]["inproc_bound"]),
                  "lt_errors": len(by_n[n]["lt_error"]),
                  "internal_missing": len(by_n[n]["internal_missing"]),
                  "lt_roots_missing": len(by_n[n]["lt_roots_missing"]),
                  "ek_roots_missing": len(by_n[n]["ek_roots_missing"])}
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
                     "(the EK subject has no finished cell, or ltlfsynt has no value: it is n/a "
                     "for a family without a declared psi_in).**")
    for n, r in by_n.items():
        if r["lt_bounded"]:
            lines += ["", f"ltlfsynt timed out ({timeout_s} s) at N={n} in "
                          f"{r['lt_bounded']} of {r['total']} cells where EK finished: "
                          f"speedup at least {r['bound']:.2f}x (median lower bound)."]
        if r["nk_bounded"]:
            lines += ["", f"{OTF_NK} timed out ({timeout_s} s) at N={n} in "
                          f"{r['nk_bounded']} of {r['total']} cells where {EK_SUBJECT} finished: "
                          f"in-process speedup at least {r['inproc_bound']:.2f}x "
                          "(median lower bound; the figure in the table leaves those cells out)."]
        if r["internal_missing"]:
            lines += ["", f"ltlfsynt internal time is missing at N={n} in "
                          f"{r['internal_missing']} of {r['total']} cells: those cells have "
                          "no internal speedup."]
        if r["lt_roots_missing"]:
            lines += ["", f"ltlfsynt explored roots are missing at N={n} in "
                          f"{r['lt_roots_missing']} of {r['total']} cells: those cells have "
                          "no ltlfsynt explored-roots ratio."]
        if r["ek_roots_missing"]:
            lines += ["", f"EK `{PRODUCT_MTDFA_ROOTS}` is missing at N={n} in "
                          f"{r['ek_roots_missing']} of {r['total']} cells: those cells have "
                          "no ltlfsynt explored-roots ratio."]
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
        "- minimized nk-product ratio: the nk-product ratio with both products "
        "Moore-minimized before their states are counted (`product_states_min`); "
        "the completion sink is counted on neither side (a state with no path to "
        "acceptance adds nothing)",
        f"- in-process speedup: `{OTF_NK}` median over `{EK_SUBJECT}` median, both timed "
        "in process, whatever --subject says; n/a for a family without a psi_in",
        f"- explored-roots ratio: `{PRODUCT_MTDFA_ROOTS}` of `{EK_SUBJECT}` over that of "
        f"`{OTF_NK}`, whatever --subject says; n/a for a family without a psi_in",
        "- a family's figure at N is the median over its cells (arm x realizable); "
        "all slippery-* arms are one family",
        "- a cell with any timed-out or failed repeat, on either side, is left out "
        "(its median would cover the surviving repeats only)",
        "- ltlfsynt internal speedup: the sum of the stage times `ltlfsynt --verbose` prints, "
        "over the EK median; no process startup floor on the ltlfsynt side. A cell whose "
        "stage lines are missing or unparseable is left out and counted in a note; a cell that "
        "timed out is left out and counted in the ltlfsynt-timeout note",
        "- ltlfsynt explored-roots ratio: EK `product_mtdfa_roots` over the root count of "
        "ltlfsynt's `translation to MTDFA` line; a cell without that count is left out and "
        "counted in a note",
        "- ltlfsynt startup floor (~4 ms) is noted, not subtracted, in the wall-clock columns",
        f"- verdict mismatches: {prov['verdict_mismatch_count']}",
        "",
    ]
    if prov["stopped_early"]:
        out[2:2] = ["**STOPPED EARLY: the sweep hit its budget, so the grid is truncated "
                    "and the largest N rows are partial.**", ""]
    for group in sorted(rows):
        out += [f"## {group}", ""]
        out += ["| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | "
                "ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup | ltlfsynt explored-roots ratio |",
                "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
        for n, r in rows[group].items():
            out.append(
                f"| {n} | {r['cells']} | {r['both']} | {_fmt(r.get('prune'), '{:.3f}')} "
                f"| {_fmt(r.get('prune_nk'), '{:.3f}')} "
                f"| {_fmt(r.get('nk_ratio_min'), '{:.3f}')} "
                f"| {_fmt(r.get('ek_med') and r['ek_med'] / 1e6, '{:.2f}')} "
                f"| {_fmt(r.get('ek_min') and r['ek_min'] / 1e6, '{:.2f}')} "
                f"| {_fmt(r.get('lt_med') and r['lt_med'] / 1e6, '{:.2f}')} "
                f"| {_fmt(r.get('speedup_med'), '{:.2f}x')} "
                f"| {_fmt(r.get('speedup_min'), '{:.2f}x')} "
                f"| {_fmt(r.get('inproc_speedup'), '{:.2f}x')} "
                f"| {_fmt(r.get('roots_ratio'), '{:.3f}')} "
                f"| {_fmt(r.get('internal_speedup'), '{:.2f}x')} "
                f"| {_fmt(r.get('lt_roots_ratio'), '{:.3f}')} |")
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
