#!/usr/bin/env python3
"""LaTeX fragments for summary-paper §8.1-8.2, drawn from one committed HPC sweep JSON only.

    python3 scripts/summary_paper_tables.py SWEEP.json OUT_DIR

Writes OUT_DIR/rq1.tex, rq2.tex (tabular bodies for tab:rq1, tab:rq2) and prune-ratio.tex,
speedup.tex (pgfplots series for fig:prune, fig:speedup). The knowledge / no-knowledge pair is
`otf-mtdfa-product-otfsolve-simp` against `otf-mtdfa-product-nk-otfsolve-simp`; ltlfsynt is full
synthesis, with the summed stage times it prints as its internal time.
"""
import argparse
import json
import os
import statistics
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sweep_pairs  # noqa: E402
import sweep_report  # noqa: E402
from sweep_report import family_group  # noqa: E402

SIMP = "otf-mtdfa-product-otfsolve-simp"
SIMP_NK = "otf-mtdfa-product-nk-otfsolve-simp"
# RQ1 alone also lists the explicit and MTDFA variants; each is a (knowledge, no-knowledge) pair.
RQ1_VARIANTS = [
    ("dfa", "dfa-product", "dfa-product-nk"),
    ("nfa", "nfa-product", "nfa-product-nk"),
    ("mtdfa", "mtdfa-product", "mtdfa-product-nk"),
    ("mtnfa", "mtnfa-product", "mtnfa-product-nk"),
    ("otf-solve-simp", SIMP, SIMP_NK),
]


def _med(xs):
    return statistics.median(xs) if xs else None


def _fmt_ms(ms):
    return "n/a" if ms is None else f"{ms:.2f}"


def _fmt_speedup(x, bound=False):
    if x is None:
        return "n/a"
    return f"{x:.2f}$\\times$" if not bound else f"$\\ge$ {x:.2f}$\\times$"


def _tabular(head, rows):
    cols = "l" + "r" * (len(head) - 1)
    lines = [f"\\begin{{tabular}}{{{cols}}}", "\\toprule", " & ".join(head) + " \\\\", "\\midrule"]
    lines += [" & ".join(r) + " \\\\" for r in rows]
    return "\n".join(lines + ["\\bottomrule", "\\end{tabular}"]) + "\n"


def _ltlfsynt_ns(sweep, group=family_group):
    """(group, n) -> (median full-synthesis ms, median internal-stage ms) over its ok cells."""
    acc = defaultdict(lambda: ([], []))
    for r in sweep["ltlfsynt"]:
        if r["status"] != "ok":
            continue
        full, internal = acc[(group(r["family"]), r["n"])]
        if r.get("ltlfsynt_median_ns") is not None:
            full.append(r["ltlfsynt_median_ns"] / 1e6)
        if r.get("ltlfsynt_internal_ns") is not None:
            internal.append(r["ltlfsynt_internal_ns"] / 1e6)
    return {k: (_med(f), _med(i)) for k, (f, i) in acc.items()}


def rq2(sweep):
    """Per family (slippery by arm) at the sweep's largest N: knowledge vs no knowledge, and ltlfsynt.

    A no-knowledge side that did not finish reads `t/o`, and the speed-up becomes the lower bound
    sweep_pairs counts at timeout over the knowledge median.
    """
    n = sweep["provenance"]["n_max"]
    pairs = sweep_pairs.build_rows(sweep, SIMP, SIMP_NK, arms=True)
    ltlfsynt_ms = _ltlfsynt_ns(sweep, group=lambda family: family)
    rows = []
    for g in sorted({g for g, _ in pairs}):
        p = pairs.get((g, n))
        if p is None:
            continue
        full, internal = ltlfsynt_ms.get((g, n), (None, None))
        no_knowledge = _fmt_ms(p["formula_ms"])
        if p["bound"] is None:
            speedup = _fmt_speedup(p["speedup"])
        else:
            speedup = _fmt_speedup(p["bound"], bound=True)
            no_knowledge = "t/o" if p["formula_ms"] is None else no_knowledge
        rows.append([g, f"{n}", _fmt_ms(p["ek_ms"]), no_knowledge, speedup, _fmt_ms(full), _fmt_ms(internal)])
    head = ["family", "$N$", "knowledge (ms)", "no knowledge (ms)", "knowledge speed-up",
            "\\tool{ltlfsynt} (ms)", "\\tool{ltlfsynt} stages (ms)"]
    return _tabular(head, rows)


def rq1(sweep):
    """Per family at the sweep's largest N: each variant's median wall time with / without knowledge.

    A cell with any unfinished repeat is left out; a variant with no finished cell reads `t/o`, and
    one the family never ran (no psi_in, so no -nk subject) reads `n/a`.
    The last column is the family's ltlfsynt verdict mismatches, over every N.
    """
    n = sweep["provenance"]["n_max"]
    groups = sorted({family_group(r["family"]) for r in sweep["timings"]})
    wall = sweep_report.finished_wall(sweep["timings"])
    meds = defaultdict(list)
    for (fam, cell_n, _, subject), (_, med) in wall.items():
        if cell_n == n:
            meds[(family_group(fam), subject)].append(med / 1e6)
    ran = {(family_group(r["family"]), r["subject"]) for r in sweep["timings"] if r["n"] == n}
    mismatches = defaultdict(int)
    for r in sweep["ltlfsynt"]:
        mismatches[family_group(r["family"])] += bool(r.get("verdict_mismatch"))

    def cell(g, subject):
        m = _med(meds[(g, subject)])
        if m is None:
            return "t/o" if (g, subject) in ran else "n/a"
        return _fmt_ms(m)

    rows = [[g] + [f"{cell(g, k)} / {cell(g, nk)}" for _, k, nk in RQ1_VARIANTS] + [str(mismatches[g])]
            for g in groups]
    head = ["family"] + [name for name, _, _ in RQ1_VARIANTS] + ["mismatches"]
    return _tabular(head, rows)


def _plot(rows, field):
    """pgfplots series, one per family, of `field` against N; a family with no figure is omitted."""
    lines = []
    for g in sorted(rows):
        points = [(n, r[field]) for n, r in sorted(rows[g].items()) if r.get(field) is not None]
        if points:
            coords = " ".join(f"({n},{v:.4g})" for n, v in points)
            lines.append(f"\\addplot coordinates {{{coords}}};\n\\addlegendentry{{{g}}}")
    return "\n".join(lines) + "\n"


def prune_ratio(sweep):
    """fig:prune: explored product roots of the otf-solve-simp pair, with knowledge over without."""
    rows = defaultdict(dict)
    for (g, n), r in sweep_pairs.build_rows(sweep, SIMP, SIMP_NK).items():
        rows[g][n] = r
    return _plot(rows, "roots")


def speedup(sweep):
    """fig:speedup: ltlfsynt full synthesis over the knowledge subject, per family."""
    return _plot(sweep_report.build_rows(sweep, SIMP), "speedup_med")


def render(sweep):
    return {"rq1.tex": rq1(sweep), "rq2.tex": rq2(sweep),
            "prune-ratio.tex": prune_ratio(sweep), "speedup.tex": speedup(sweep)}


def _header(sweep_path, out_dir):
    return f"% generated by python3 scripts/summary_paper_tables.py {sweep_path} {out_dir}; do not edit\n"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("sweep")
    ap.add_argument("out_dir")
    args = ap.parse_args(argv)
    with open(args.sweep) as f:
        sweep = json.load(f)
    os.makedirs(args.out_dir, exist_ok=True)
    for name, text in render(sweep).items():
        with open(os.path.join(args.out_dir, name), "w") as out:
            out.write(_header(args.sweep, args.out_dir) + text)


if __name__ == "__main__":
    main()
