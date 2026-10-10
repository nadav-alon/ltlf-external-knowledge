#!/usr/bin/env python3
"""Charts for the 2026-08-12 presentation, from a ltlf-ek-bench report.

Reads the JSON report (not the workbook -- same numbers, no openpyxl in the
loop) and writes one vector PDF per figure into figures/.  Run via `make figs`.

The comparable families are plotted, without `parity-t3`: it is barred from the
comparison tables by the benchmark-suite PRD's Stop-list 1, and its declared
`expected_realizable` is known wrong (see docs/runs/2026-08-12-benchmark-numbers.md).
"""

import json
import os
import pathlib
import sys

os.environ.setdefault("MPLCONFIGDIR",
                      str(pathlib.Path(__file__).parent / ".mplcache"))

import matplotlib
matplotlib.use("pdf")
import matplotlib.pyplot as plt
from matplotlib.ticker import LogLocator, NullFormatter

HERE = pathlib.Path(__file__).parent
REPO = HERE.parent.parent.parent
REPORT = REPO / "docs/runs/2026-08-11-benchmarks-release.json"
FIGDIR = HERE / "figures"

# The four citable families, in the order the deck talks about them.
FAMILIES = ["cons-prunes", "cons-inert", "mirror-small", "mirror-degenerate"]

# The knowledge-size pair (tier t2: aperiodic T_in, no psi_in supplied, so no
# ltlfsynt race).  Held separate from FAMILIES because they answer a different
# question -- what large knowledge costs -- and because the four above all pin
# |T_in| = 1, which is why their product can never exceed their goal.
KNOWLEDGE_FAMILIES = ["knowledge-chain", "knowledge-chain-inert"]

# Categorical slots 1, 2, 3, 7 of the validated reference palette.  Slot 4
# (yellow) is deliberately skipped -- it fails the all-pairs floors beside
# slot 2 (orange).  Validated all-pairs, light mode: worst CVD dE 9.2,
# worst normal-vision dE 16.3.  Order is fixed and never cycled; the colour
# follows the method, not its rank in any given panel.
METHODS = [
    ("otf-mtdfa-product", "OtfMtdfaProduct", "#2a78d6", "o"),
    ("dfa-product",       "DfaProduct",      "#eb6834", "s"),
    ("nfa-product",       "NfaProduct",      "#1baf7a", "^"),
    ("mtnfa-product",     "MtnfaProduct",    "#4a3aa7", "D"),
]
# Goal-DFA / product-DFA colours of the knowledge figures; same hues as slots 1, 2.
GOAL_C, PROD_C = "#2a78d6", "#eb6834"
# MtdfaProduct is the baseline every ratio is taken against, so it is drawn as
# a reference rule rather than spending a categorical slot on it.
BASELINE = "mtdfa-product"

INK = "#0b0b0b"
INK_2 = "#52514e"
INK_MUTED = "#8a8984"
SURFACE = "#fcfcfb"
GRID = "#e3e2de"

# The polarity the workbook's `summary` sheet uses, so a number read off a
# chart here matches a number read out of the workbook.
POLARITY = True


def load():
    with open(REPORT) as fh:
        return json.load(fh)


def style():
    plt.rcParams.update({
        "figure.facecolor": SURFACE,
        "axes.facecolor": SURFACE,
        "savefig.facecolor": SURFACE,
        "font.size": 8,
        "font.family": "sans-serif",
        "text.color": INK,
        "axes.labelcolor": INK_2,
        "axes.edgecolor": GRID,
        "axes.linewidth": 0.8,
        "axes.titlesize": 8.5,
        "axes.titleweight": "bold",
        "axes.titlecolor": INK,
        "xtick.color": INK_2,
        "ytick.color": INK_2,
        "xtick.labelsize": 7,
        "ytick.labelsize": 7,
        "legend.fontsize": 7.5,
        "legend.frameon": False,
        "grid.color": GRID,
        "grid.linewidth": 0.6,
        "savefig.bbox": "tight",
        "savefig.pad_inches": 0.02,
        "pdf.fonttype": 42,
    })


def recede(ax):
    """Grid and axes stay behind the data and out of the way."""
    ax.grid(True, which="major", axis="y", zorder=0)
    ax.set_axisbelow(True)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)


def wall_totals(d):
    """(family, n, subject) -> wall_total ns, at the summary sheet's polarity."""
    out = {}
    for r in d["timings"]:
        if r["stage"] != "wall_total" or r["realizable"] is not POLARITY:
            continue
        if r["timed_out"]:
            continue
        out[(r["family"], r["n"], r["subject"])] = r["ns"]
    return out


def ltlfsynt_times(d):
    """(family, n) -> ltlfsynt wall ns, comparable rows only.

    Only `status == "ok"` rows exist for the t1 families; t3 is recorded as
    "n/a -- by expressibility" and never contacted, so it simply has no key.
    """
    return {(r["family"], r["n"]): r["ltlfsynt_ns"]
            for r in d["ltlfsynt"]
            if r["status"] == "ok" and r["realizable"] is POLARITY}


def structural(d):
    """(family, n, subject, metric) -> value, at the summary sheet's polarity."""
    out = {}
    for r in d["structural"]:
        if r["realizable"] is not POLARITY:
            continue
        out[(r["family"], r["n"], r["subject"], r["metric"])] = r["value"]
    return out


# ---------------------------------------------------------------------------
# Figure 1 -- speedup vs n, one panel per family.
# Job: identity (which method) over change-over-time (n) => multi-series line,
# small multiples so four families do not become a twenty-line spaghetti plot.
# ---------------------------------------------------------------------------
def fig_speedup(d):
    wt = wall_totals(d)
    ns = sorted({r["n"] for r in d["timings"]})

    # All six comparable families, 2x3.  The t2 pair belongs here: tier
    # governs the *external* ltlfsynt claim, and a cross-method ratio makes no
    # expressibility claim at all.  parity-t3 stays out -- its declared
    # realizability is known wrong, which is a different objection.
    fams = FAMILIES + KNOWLEDGE_FAMILIES
    fig, axgrid = plt.subplots(2, 3, figsize=(9.6, 4.6), sharey=True)
    axes = axgrid.flatten()
    for i, (ax, fam) in enumerate(zip(axes, fams)):
        recede(ax)
        ax.axhline(1.0, color=INK_MUTED, lw=1.0, ls=(0, (4, 3)), zorder=1)
        for key, label, colour, marker in METHODS:
            xs, ys = [], []
            for n in ns:
                base = wt.get((fam, n, BASELINE))
                mine = wt.get((fam, n, key))
                if base and mine:
                    xs.append(n)
                    ys.append(base / mine)
            ax.plot(xs, ys, color=colour, lw=1.8, marker=marker, ms=3.4,
                    mew=0, zorder=3, label=label, clip_on=False)
        ax.set_yscale("log")
        ax.set_title(fam)
        ax.set_xlabel("$n$")
        ax.set_xticks(ns[::2])
        ax.yaxis.set_minor_formatter(NullFormatter())
        # The baseline is a rule rather than a series, so it is named on the
        # plot instead of spending a categorical slot in the legend -- and only
        # on the one panel with clear space at y=1, since the surface-coloured
        # relief box would otherwise erase data on the other three.
        if i == 0:
            ax.annotate("MtdfaProduct = 1", xy=(0.5, 1.0),
                        xycoords=("axes fraction", "data"),
                        ha="center", va="center", color=INK_MUTED, fontsize=6,
                        zorder=4,
                        bbox=dict(boxstyle="round,pad=0.18", fc=SURFACE,
                                  ec="none"))

    for ax in (axes[0], axes[3]):          # left column of each row only
        ax.set_ylabel("speedup vs MtdfaProduct")
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", ncol=4,
               bbox_to_anchor=(0.5, -0.05), columnspacing=2.2)
    fig.subplots_adjust(hspace=0.55)
    fig.savefig(FIGDIR / "speedup.pdf")
    plt.close(fig)


# ---------------------------------------------------------------------------
# Figure 2 -- why: the goal DFA blows up, the product does not (or does).
# Two series only, so no legend-colour ambiguity; direct-labelled.
# ---------------------------------------------------------------------------
def fig_structural(d):
    st = structural(d)
    ns = sorted({r["n"] for r in d["structural"]})

    fig, axes = plt.subplots(1, 4, figsize=(9.6, 3.0), sharey=True)
    for i, (ax, fam) in enumerate(zip(axes, FAMILIES)):
        recede(ax)
        # On three of the four families these two series are equal at every n,
        # which is the point of the panel -- so the goal curve is drawn as a
        # wide halo and the product as a dashed line on top.  Coincidence then
        # reads as "dashes inside a band" rather than as a missing series.
        series = [
            ("goal DFA", "goal_dfa_states", GOAL_C, "o", 4.0, "solid", 0.0),
            ("product", "product_states", PROD_C, "s", 1.5, (0, (3, 2.4)), 3.6),
        ]
        for label, metric, colour, marker, lw, ls, ms in series:
            xs, ys = [], []
            for n in ns:
                v = st.get((fam, n, "dfa-product", metric))
                if v:
                    xs.append(n)
                    ys.append(v)
            ax.plot(xs, ys, color=colour, lw=lw, ls=ls, marker=marker, ms=ms,
                    mew=0, zorder=3, label=label, clip_on=False,
                    alpha=0.9 if lw > 2 else 1.0,
                    solid_capstyle="round")
        ax.set_yscale("log")
        ax.set_title(fam)
        ax.set_xlabel("$n$")
        ax.set_xticks(ns[::2])
        ax.yaxis.set_minor_formatter(NullFormatter())

    # Only the family where the two curves separate gets direct labels; on the
    # other three they would sit on top of each other.
    a0 = axes[0]
    a0.annotate("goal DFA", xy=(ns[-1], st[(FAMILIES[0], ns[-1], "dfa-product",
                                            "goal_dfa_states")]),
                xytext=(-2, 7), textcoords="offset points", ha="right",
                color=GOAL_C, fontsize=7, fontweight="bold")
    a0.annotate("product", xy=(ns[-1], st[(FAMILIES[0], ns[-1], "dfa-product",
                                           "product_states")]),
                xytext=(-2, -12), textcoords="offset points", ha="right",
                color=PROD_C, fontsize=7, fontweight="bold")

    axes[0].set_ylabel("states, DfaProduct")
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", ncol=2,
               bbox_to_anchor=(0.5, -0.10), columnspacing=2.2)
    fig.savefig(FIGDIR / "structural.pdf")
    plt.close(fig)


# ---------------------------------------------------------------------------
# Figure 3 -- absolute cost against n, all five methods plus the ltlfsynt
# baseline.  Plotted against n rather than as a bar chart at the largest n on
# purpose: ltlfsynt's measured time is flat in n (~5 ms everywhere) while the
# goal DFA grows exponentially, which says the number is dominated by process
# startup rather than by synthesis.  A single-n snapshot would hide that and
# read as a speedup claim; the flat line is the caveat, drawn.
# ---------------------------------------------------------------------------
def fig_cost(d):
    wt = wall_totals(d)
    lt = ltlfsynt_times(d)
    ns = sorted({r["n"] for r in d["timings"]})

    # The baseline takes a neutral dark ink rather than a categorical slot --
    # dark enough to read as data, hueless enough not to claim series identity.
    order = [("mtdfa-product", "MtdfaProduct", INK_2, "v")] + [
        (k, lbl, c, m) for k, lbl, c, m in METHODS]

    fig, axes = plt.subplots(1, 4, figsize=(9.6, 3.0), sharey=True)
    for i, (ax, fam) in enumerate(zip(axes, FAMILIES)):
        recede(ax)
        for key, label, colour, marker in order:
            xs, ys = [], []
            for n in ns:
                v = wt.get((fam, n, key))
                if v:
                    xs.append(n)
                    ys.append(v / 1e6)  # ns -> ms
            ax.plot(xs, ys, color=colour, lw=1.8, marker=marker, ms=3.4,
                    mew=0, zorder=3, label=label, clip_on=False)

        # ltlfsynt is not a subject of the sweep -- it is an external process
        # measured end to end -- so it is drawn as a dashed rule in neutral
        # ink, never as a sixth categorical series.
        xs = [n for n in ns if (fam, n) in lt]
        ys = [lt[(fam, n)] / 1e6 for n in xs]
        ax.plot(xs, ys, color=INK, lw=1.6, ls=(0, (4, 2.5)), zorder=4,
                label="ltlfsynt (whole process)", clip_on=False)

        ax.set_yscale("log")
        ax.set_title(fam)
        ax.set_xlabel("$n$")
        ax.set_xticks(ns[::2])
        ax.yaxis.set_minor_formatter(NullFormatter())

    axes[0].set_ylabel("wall total (ms, log)")
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", ncol=6,
               bbox_to_anchor=(0.5, -0.10), columnspacing=1.6)
    fig.savefig(FIGDIR / "cost.pdf")
    plt.close(fig)


# ---------------------------------------------------------------------------
# Figure 4 -- the knowledge-size axis.  Answers the question the structural
# figure provokes: why does the product never exceed the goal?  Because those
# four families all carry ONE-state knowledge.  Here |T_in| = n instead.
# ---------------------------------------------------------------------------
def fig_knowledge(d):
    st = structural(d)
    ns = sorted({r["n"] for r in d["structural"]})

    titles = {
        "knowledge-chain": "knowledge-chain  (cons prunes)",
        "knowledge-chain-inert": "knowledge-chain-inert  (nothing to prune)",
    }
    fig, axes = plt.subplots(1, 2, figsize=(9.0, 3.0), sharey=True)
    for ax, fam in zip(axes, KNOWLEDGE_FAMILIES):
        recede(ax)
        for label, metric, colour, marker, lw, ls, ms in [
            ("goal DFA", "goal_dfa_states", GOAL_C, "o", 4.0, "solid", 0.0),
            ("product", "product_states", PROD_C, "s", 1.5, (0, (3, 2.4)), 3.6),
        ]:
            xs, ys = [], []
            for n in ns:
                v = st.get((fam, n, "dfa-product", metric))
                if v:
                    xs.append(n)
                    ys.append(v)
            ax.plot(xs, ys, color=colour, lw=lw, ls=ls, marker=marker, ms=ms,
                    mew=0, zorder=3, label=label, clip_on=False,
                    alpha=0.9 if lw > 2 else 1.0, solid_capstyle="round")
        ax.set_yscale("log")
        ax.set_title(titles.get(fam, fam))
        ax.set_xlabel("$n$   (= number of states in $T_{in}$)")
        ax.set_xticks(ns)
        ax.yaxis.set_minor_formatter(NullFormatter())

    axes[0].set_ylabel("states, DfaProduct")
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", ncol=2,
               bbox_to_anchor=(0.5, -0.10), columnspacing=2.2)
    fig.savefig(FIGDIR / "knowledge.pdf")
    plt.close(fig)


def report(d):
    """Numbers the slide text quotes, printed so the prose can never drift."""
    wt = wall_totals(d)
    n = max(r["n"] for r in d["timings"])
    print(f"-- speedup vs {BASELINE} at n={n} (realizable={POLARITY})")
    for fam in FAMILIES:
        base = wt.get((fam, n, BASELINE))
        cells = []
        for key, label, _, _ in METHODS:
            v = wt.get((fam, n, key))
            cells.append(f"{label}={base / v:.2f}" if (base and v) else f"{label}=--")
        print(f"   {fam:20} " + "  ".join(cells))
    ok = [r for r in d["ltlfsynt"] if r["status"] == "ok"]
    print(f"-- ltlfsynt race: {len(ok)} t1 rows, "
          f"{sum(1 for r in ok if r['verdict_mismatch'])} mismatches, "
          f"{len(d['ltlfsynt']) - len(ok)} non-t1 rows skipped by "
          f"expressibility (t2 + t3)")

    # The flatness is the caveat on the cost figure, so it gets stated as a
    # number rather than left to the eye.
    lt = ltlfsynt_times(d)
    lo = min(lt.values()) / 1e6
    hi = max(lt.values()) / 1e6
    n_lo, n_hi = min(r["n"] for r in d["timings"]), n
    growth = []
    for fam in FAMILIES:
        a, b = lt.get((fam, n_lo)), lt.get((fam, n_hi))
        if a and b:
            growth.append(b / a)
    mt_lo = wt[(FAMILIES[0], n_lo, BASELINE)]
    mt_hi = wt[(FAMILIES[0], n_hi, BASELINE)]
    print(f"-- ltlfsynt wall: {lo:.2f}..{hi:.2f} ms over every family and "
          f"n={n_lo}..{n_hi}; grows {min(growth):.2f}-{max(growth):.2f}x "
          f"across that range, vs {mt_hi / mt_lo:.1f}x for MtdfaProduct "
          f"=> startup-dominated, not a synthesis-cost comparison")
    p = d["provenance"]
    print(f"-- provenance: {p['cmake_build_type']} build, spot {p['spot_version']}, "
          f"repeat {p['repeat']}, n {p['n_min']}..{p['n_max']}")


def main():
    if not REPORT.exists():
        sys.exit(f"missing benchmark report: {REPORT}")
    FIGDIR.mkdir(exist_ok=True)
    style()
    d = load()
    fig_speedup(d)
    fig_structural(d)
    fig_cost(d)
    fig_knowledge(d)
    report(d)
    print(f"wrote {len(list(FIGDIR.glob('*.pdf')))} figures to {FIGDIR}")


if __name__ == "__main__":
    main()
