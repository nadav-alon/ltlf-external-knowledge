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


def _ms(ms):
    return "n/a" if ms is None else f"{ms:.2f}"


def _x(x):
    return "n/a" if x is None else f"{x:.2f}$\\times$"


def _tabular(head, rows):
    cols = "l" + "r" * (len(head) - 1)
    lines = [f"\\begin{{tabular}}{{{cols}}}", "\\toprule", " & ".join(head) + " \\\\", "\\midrule"]
    lines += [" & ".join(r) + " \\\\" for r in rows]
    return "\n".join(lines + ["\\bottomrule", "\\end{tabular}"]) + "\n"


def _ltlfsynt_ns(sweep):
    """(group, n) -> (median full-synthesis ms, median internal-stage ms) over its ok cells."""
    acc = defaultdict(lambda: ([], []))
    for r in sweep["ltlfsynt"]:
        if r["status"] != "ok":
            continue
        full, internal = acc[(family_group(r["family"]), r["n"])]
        if r.get("ltlfsynt_median_ns") is not None:
            full.append(r["ltlfsynt_median_ns"] / 1e6)
        if r.get("ltlfsynt_internal_ns") is not None:
            internal.append(r["ltlfsynt_internal_ns"] / 1e6)
    return {k: (_med(f), _med(i)) for k, (f, i) in acc.items()}


def _groups(sweep):
    return sorted({family_group(r["family"]) for r in sweep["timings"]})


def rq2(sweep):
    """Per family at the sweep's largest N: knowledge vs no knowledge, and ltlfsynt as full synthesis."""
    n = sweep["provenance"]["n_max"]
    pairs = sweep_pairs.build_rows(sweep, SIMP, SIMP_NK)
    lt = _ltlfsynt_ns(sweep)
    rows = []
    for g in _groups(sweep):
        p = pairs.get((g, n))
        if p is None:
            continue
        full, internal = lt.get((g, n), (None, None))
        rows.append([g, f"{n}", _ms(p["ek_ms"]), _ms(p["formula_ms"]),
                     _x(p["speedup"]), _ms(full), _ms(internal)])
    head = ["family", "$N$", "knowledge (ms)", "no knowledge (ms)", "knowledge speed-up",
            "\\tool{ltlfsynt} (ms)", "\\tool{ltlfsynt} stages (ms)"]
    return _tabular(head, rows)


def render(sweep):
    return {"rq2.tex": rq2(sweep)}


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
            out.write(text)


if __name__ == "__main__":
    main()
