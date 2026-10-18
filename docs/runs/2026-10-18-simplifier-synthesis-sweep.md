# Simplifier and full-synthesis sweep (2026-10-18)

The four Oct 16 families (knowledge-chain, cons-prunes, co-generated, and slippery's three arms counted as one family) at N=2..6 on the desktop. Every subject is included:
- the on-the-fly-solve pair from #25;
- the same pair with Spot's LTLf formula simplifier (`-simp`, #158);
- the build-then-solve subjects;
- the explicit-DFA subjects.

ltlfsynt runs as **full synthesis**, not `--realizability` (#176). The column that misread its root count is gone (#177).

- commit `cb1eb35` (master after #158, #176 and #177), Release build, Spot 2.15.1 (`~/opt/spot-2.15.1`), repeat=5, timeout=20 s
- machine: desktop (WSL2, 24 cores, 19 GiB), started 2026-10-18 16:40:14Z (19:40 IDT), sweep wall time 2689 s (until about 20:25 IDT), via `mkdir -p build && SPOT_ROOT=~/opt/spot-2.15.1 scripts/run-sweep.sh build/benchout/sweep178`
- verdict mismatches across all subjects: **0**
- raw data: `2026-10-18-simplifier-synthesis-sweep.json`

## 1. Headline

Once the folded formula gets the same cheap simplifier ltlfsynt uses, folding the knowledge into the formula is as fast as the transducer or faster, on cons-prunes and on two of slippery's three arms. EK leads in two places:
- co-generated, where the simplifier blows up the folded formula;
- slippery-onehot at N=6, a single point.

Figures are medians over the cells named; section 2 gives every row with its cell count.

| family | EK vs formula: same on-the-fly solver, both simplified (N=2→6) | EK vs ltlfsynt: internal time, no startup floor (N=2→6) |
|---|---|---|
| cons-prunes | 0.96x → 1.06x, parity (all cells) | 0.95x → 0.71x, ltlfsynt faster (all cells) |
| slippery, binary and binary-compact arms | 0.94x → 0.69–0.74x, formula faster | 0.78x → 0.41–0.46x, ltlfsynt faster |
| slippery, onehot arm | 0.89x → 0.59x (N=4) → 1.75x (N=6) | 0.76x → 0.61x (N=4) → 1.01x (N=5) → **2.39x (N=6)** |
| co-generated | 1.18x → 2256x (N=5) → ≥ 16057x (N=6, formula side unfinished in both cells). The simplifier blows up the folded formula here. | 0.90x → 2.05x (N=4) → 3861x (N=5). N=6: 0.60x in the realizable cell; ltlfsynt timed out in the unrealizable one (≥ 8640x counting it at the timeout). |

- knowledge-chain is not raced: it has no ψ_in, so there is neither a formula side nor an ltlfsynt run. EK finished every one of its cells.
- **#159 (PR #175) corrected:** its in-process wins (slippery up to 159x, co-generated 9.6x) compared EK against a formula solver *without* the simplifier. With it, slippery's formula side goes from 10.8 s to 29 ms at N=5. The simplifier leaves EK's own times unchanged (φ alone is already simple), so the whole difference is on the folded `ψ_in → φ`.
- **Explored roots** (on-the-fly pair, EK over formula): 0.51–0.59 on co-generated and 0.63–0.75 on cons-prunes, with or without the simplifier. EK explores fewer roots, but that does not turn into time on cons-prunes. (The build-then-solve pair in section 3 differs: co-generated is 1.19–1.48 there.)
- **slippery-onehot N=6** is the one cell group where EK beats both the simplified formula and ltlfsynt, after a rising trend (0.61x → 1.01x → 2.39x against ltlfsynt over N=4..6). It is one arm and one N; N=7 would show whether the trend holds.

## 2. Per N

Regenerate each table with `python3 scripts/sweep_pairs.py docs/runs/2026-10-18-simplifier-synthesis-sweep.json EK FORMULA [--arms]`.

### 2a. On-the-fly solve, both simplified

EK `otf-mtdfa-product-otfsolve-simp` vs formula `otf-mtdfa-product-nk-otfsolve-simp`, both timed in process; ltlfsynt is its summed `--verbose` stage time.

| family | N | cells (EK finished / all) | EK med (ms) | formula med (ms) | speedup, cells both finished | speedup lower bound, unfinished at timeout | explored-roots ratio | ltlfsynt internal speedup (cells) | ltlfsynt lower bound, timeouts at timeout |
|---|---|---|---|---|---|---|---|---|---|
| co-generated | 2 | 2 / 2 | 0.76 | 0.88 | 1.18x (2) | — | 0.591 | 0.90x (2) | — |
| co-generated | 3 | 2 / 2 | 0.74 | 1.17 | 1.58x (2) | — | 0.553 | 1.07x (2) | — |
| co-generated | 4 | 2 / 2 | 0.75 | 2.15 | 2.87x (2) | — | 0.529 | 2.05x (2) | — |
| co-generated | 5 | 2 / 2 | 0.83 | 1895.75 | 2255.64x (2) | — | 0.514 | 3861.35x (2) | — |
| co-generated | 6 | 2 / 2 | 1.25 | n/a | n/a | ≥ 16057.30x (2 unfinished) | n/a | 0.60x (1) | ≥ 8640.39x (1 timed out) |
| cons-prunes | 2 | 2 / 2 | 0.71 | 0.68 | 0.96x (2) | — | 0.750 | 0.95x (2) | — |
| cons-prunes | 3 | 2 / 2 | 0.77 | 0.73 | 0.95x (2) | — | 0.700 | 0.87x (2) | — |
| cons-prunes | 4 | 2 / 2 | 0.81 | 0.77 | 0.97x (2) | — | 0.667 | 0.78x (2) | — |
| cons-prunes | 5 | 2 / 2 | 0.79 | 0.80 | 1.02x (2) | — | 0.643 | 0.82x (2) | — |
| cons-prunes | 6 | 2 / 2 | 0.96 | 1.00 | 1.06x (2) | — | 0.625 | 0.71x (2) | — |
| knowledge-chain | 2 | 2 / 2 | 0.74 | n/a | n/a | — | n/a | n/a | — |
| knowledge-chain | 3 | 2 / 2 | 0.73 | n/a | n/a | — | n/a | n/a | — |
| knowledge-chain | 4 | 2 / 2 | 0.75 | n/a | n/a | — | n/a | n/a | — |
| knowledge-chain | 5 | 2 / 2 | 0.77 | n/a | n/a | — | n/a | n/a | — |
| knowledge-chain | 6 | 2 / 2 | 0.98 | n/a | n/a | — | n/a | n/a | — |
| slippery | 2 | 6 / 6 | 1.51 | 1.38 | 0.92x (6) | — | 1.000 | 0.78x (6) | — |
| slippery | 3 | 6 / 6 | 3.58 | 3.00 | 0.84x (6) | — | 1.000 | 0.71x (6) | — |
| slippery | 4 | 6 / 6 | 8.63 | 6.19 | 0.68x (6) | — | 1.000 | 0.58x (6) | — |
| slippery | 5 | 6 / 6 | 42.20 | 28.80 | 0.70x (6) | — | 1.000 | 0.41x (6) | — |
| slippery | 6 | 6 / 6 | 174.26 | 135.12 | 0.77x (6) | — | 1.000 | 0.47x (6) | — |

slippery by arm (`--arms`):

| family | N | cells (EK finished / all) | EK med (ms) | formula med (ms) | speedup, cells both finished | speedup lower bound, unfinished at timeout | explored-roots ratio | ltlfsynt internal speedup (cells) | ltlfsynt lower bound, timeouts at timeout |
|---|---|---|---|---|---|---|---|---|---|
| slippery-binary | 2 | 2 / 2 | 1.41 | 1.32 | 0.94x (2) | — | 1.000 | 0.78x (2) | — |
| slippery-binary | 3 | 2 / 2 | 3.34 | 2.79 | 0.84x (2) | — | 1.000 | 0.74x (2) | — |
| slippery-binary | 4 | 2 / 2 | 8.13 | 5.52 | 0.68x (2) | — | 1.000 | 0.59x (2) | — |
| slippery-binary | 5 | 2 / 2 | 36.75 | 21.47 | 0.59x (2) | — | 1.000 | 0.41x (2) | — |
| slippery-binary | 6 | 2 / 2 | 156.39 | 111.13 | 0.69x (2) | — | 1.000 | 0.46x (2) | — |
| slippery-binary-compact | 2 | 2 / 2 | 1.43 | 1.34 | 0.94x (2) | — | 1.000 | 0.78x (2) | — |
| slippery-binary-compact | 3 | 2 / 2 | 3.39 | 2.92 | 0.86x (2) | — | 1.000 | 0.66x (2) | — |
| slippery-binary-compact | 4 | 2 / 2 | 8.12 | 6.19 | 0.76x (2) | — | 1.000 | 0.57x (2) | — |
| slippery-binary-compact | 5 | 2 / 2 | 40.00 | 28.80 | 0.72x (2) | — | 1.000 | 0.35x (2) | — |
| slippery-binary-compact | 6 | 2 / 2 | 170.54 | 125.51 | 0.74x (2) | — | 1.000 | 0.41x (2) | — |
| slippery-onehot | 2 | 2 / 2 | 1.90 | 1.69 | 0.89x (2) | — | 1.000 | 0.76x (2) | — |
| slippery-onehot | 3 | 2 / 2 | 4.33 | 3.33 | 0.77x (2) | — | 1.000 | 0.71x (2) | — |
| slippery-onehot | 4 | 2 / 2 | 18.05 | 10.57 | 0.59x (2) | — | 1.000 | 0.61x (2) | — |
| slippery-onehot | 5 | 2 / 2 | 119.78 | 92.09 | 0.77x (2) | — | 1.000 | 1.01x (2) | — |
| slippery-onehot | 6 | 2 / 2 | 1160.27 | 2017.40 | 1.75x (2) | — | 1.000 | 2.39x (2) | — |

### 2b. On-the-fly solve, no simplifier

EK `otf-mtdfa-product-otfsolve` vs formula `otf-mtdfa-product-nk-otfsolve`, both timed in process; ltlfsynt is its summed `--verbose` stage time.

| family | N | cells (EK finished / all) | EK med (ms) | formula med (ms) | speedup, cells both finished | speedup lower bound, unfinished at timeout | explored-roots ratio | ltlfsynt internal speedup (cells) | ltlfsynt lower bound, timeouts at timeout |
|---|---|---|---|---|---|---|---|---|---|
| co-generated | 2 | 2 / 2 | 0.75 | 0.87 | 1.17x (2) | — | 0.591 | 0.91x (2) | — |
| co-generated | 3 | 2 / 2 | 0.78 | 1.04 | 1.33x (2) | — | 0.553 | 1.01x (2) | — |
| co-generated | 4 | 2 / 2 | 0.81 | 1.43 | 1.77x (2) | — | 0.529 | 1.90x (2) | — |
| co-generated | 5 | 2 / 2 | 0.81 | 3.10 | 3.88x (2) | — | 0.515 | 4120.63x (2) | — |
| co-generated | 6 | 2 / 2 | 1.25 | 11.11 | 9.15x (2) | — | 0.508 | 0.62x (1) | ≥ 8350.19x (1 timed out) |
| cons-prunes | 2 | 2 / 2 | 0.71 | 0.65 | 0.91x (2) | — | 0.750 | 0.96x (2) | — |
| cons-prunes | 3 | 2 / 2 | 0.78 | 0.71 | 0.92x (2) | — | 0.700 | 0.86x (2) | — |
| cons-prunes | 4 | 2 / 2 | 0.81 | 0.71 | 0.89x (2) | — | 0.667 | 0.79x (2) | — |
| cons-prunes | 5 | 2 / 2 | 0.80 | 0.76 | 0.96x (2) | — | 0.643 | 0.81x (2) | — |
| cons-prunes | 6 | 2 / 2 | 1.03 | 0.99 | 0.97x (2) | — | 0.625 | 0.67x (2) | — |
| knowledge-chain | 2 | 2 / 2 | 0.76 | n/a | n/a | — | n/a | n/a | — |
| knowledge-chain | 3 | 2 / 2 | 0.71 | n/a | n/a | — | n/a | n/a | — |
| knowledge-chain | 4 | 2 / 2 | 0.73 | n/a | n/a | — | n/a | n/a | — |
| knowledge-chain | 5 | 2 / 2 | 0.84 | n/a | n/a | — | n/a | n/a | — |
| knowledge-chain | 6 | 2 / 2 | 1.03 | n/a | n/a | — | n/a | n/a | — |
| slippery | 2 | 6 / 6 | 1.51 | 4.43 | 2.57x (6) | — | 1.000 | 0.77x (6) | — |
| slippery | 3 | 6 / 6 | 3.53 | 23.37 | 6.55x (6) | — | 1.000 | 0.71x (6) | — |
| slippery | 4 | 6 / 6 | 8.68 | 324.02 | 33.87x (6) | — | 1.000 | 0.58x (6) | — |
| slippery | 5 | 6 / 6 | 42.20 | 10774.59 | 154.17x (5) | ≥ 163.28x (1 unfinished) | 1.000 | 0.40x (6) | — |
| slippery | 6 | 6 / 6 | 178.79 | 272.10 | 1.60x (2) | ≥ 17.64x (4 unfinished) | 1.000 | 0.46x (6) | — |

How to read the tables:
- **speedup** = formula median / EK median, over the cells where both finished; the count is in brackets.
- **lower bound** also counts the cells where EK finished and the formula side did not (timed out, or timed out on some repeats), at timeout / EK median.
- **ltlfsynt internal speedup** = ltlfsynt's summed `--verbose` stage times (full synthesis) / that table's EK median. Its lower bound counts ltlfsynt timeouts at the timeout. ltlfsynt's internal time is at most its wall time, so that bound is approximate.
- **explored-roots ratio** = EK `product_mtdfa_roots` / formula `product_mtdfa_roots`.

## 3. Full report (build-then-solve EK)

This is `python3 scripts/sweep_report.py docs/runs/2026-10-18-simplifier-synthesis-sweep.json`, the default EK subject `otf-mtdfa-product`. It carries the structural columns (prune and nk-product ratios), which don't depend on the EK subject. Its in-process and EK-dependent columns refer to build-then-solve EK; section 2 has them for the on-the-fly-solve subjects.

#### Sweep report

- build `Release`, Spot 2.15.1, commit `cb1eb35`, repeat=5, timeout=20 s
- EK subject: `otf-mtdfa-product` (wall-clock, median of the repeats); ltlfsynt on the monolithic `psi_in -> phi` encoding, run as full synthesis (it builds a strategy, not `--realizability`)
- prune ratio: reachable product states with knowledge over without (`dfa-product` / `dfa-product-unpruned`)
- nk-product ratio: `dfa-product` over `dfa-product-nk` (psi_in folded into the Goal; the same decision problem ltlfsynt is given)
- minimized nk-product ratio: the nk-product ratio with both products Moore-minimized before their states are counted (`product_states_min`); the completion sink is counted on neither side (a state with no path to acceptance adds nothing)
- in-process speedup: `otf-mtdfa-product-nk` median over `otf-mtdfa-product` median, both timed in process, whatever --subject says; n/a for a family without a psi_in
- explored-roots ratio: `product_mtdfa_roots` of `otf-mtdfa-product` over that of `otf-mtdfa-product-nk`, whatever --subject says; n/a for a family without a psi_in
- a family's figure at N is the median over its cells (arm x realizable); all slippery-* arms are one family
- a cell with any timed-out or failed repeat, on either side, is left out (its median would cover the surviving repeats only)
- ltlfsynt internal speedup: the sum of the stage times `ltlfsynt --verbose` prints, over the EK median; no process startup floor on the ltlfsynt side. A cell whose stage lines are missing or unparseable is left out and counted in a note; a cell that timed out is left out and counted in the ltlfsynt-timeout note
- ltlfsynt reports no explored size: the `k` in its `translation to MTDFA (k roots, ...)` line counts the formulas it started from
- ltlfsynt startup floor (~4 ms) is noted, not subtracted, in the wall-clock columns
- verdict mismatches: 0

#### co-generated

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 2 | 1.125 | 2.208 | 1.688 | 0.81 | 0.75 | 6.59 | 8.11x | 8.52x | 1.08x | 1.193 | 0.83x |
| 3 | 2 | 2 | 1.188 | 4.325 | 2.708 | 0.90 | 0.83 | 7.39 | 8.31x | 8.33x | 1.26x | 1.332 | 0.89x |
| 4 | 2 | 2 | 1.219 | 8.403 | 4.725 | 1.03 | 0.97 | 8.51 | 8.33x | 8.50x | 1.70x | 1.412 | 1.51x |
| 5 | 2 | 2 | 1.234 | 16.449 | 8.736 | 1.38 | 1.31 | 3262.38 | 2451.30x | 2556.39x | 3.25x | 1.455 | 2441.33x |
| 6 | 2 | 1 | 1.242 | 32.473 | 16.743 | 2.33 | 2.22 | 12.39 | 5.01x | 5.19x | 9.37x | 1.477 | 0.33x |

**Speedup over ltlfsynt at the largest N finished (N=5, 2 of 2 cells): 2451.30x (median); stops at N=5 because N=6 has 1 ltlfsynt timeout.**

ltlfsynt timed out (20 s) at N=6 in 1 of 2 cells where EK finished: speedup at least 9118.94x (median lower bound).

#### cons-prunes

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 2 | 0.678 | 1.000 | 0.900 | 0.74 | 0.70 | 6.79 | 9.21x | 9.19x | 0.88x | 0.875 | 0.92x |
| 3 | 2 | 2 | 0.509 | 1.000 | 0.917 | 0.82 | 0.71 | 6.55 | 8.04x | 9.08x | 0.86x | 0.900 | 0.82x |
| 4 | 2 | 2 | 0.343 | 1.000 | 0.929 | 0.79 | 0.77 | 6.49 | 8.20x | 7.73x | 0.94x | 0.917 | 0.79x |
| 5 | 2 | 2 | 0.214 | 1.000 | 0.938 | 0.86 | 0.80 | 6.60 | 7.71x | 7.71x | 0.92x | 0.929 | 0.75x |
| 6 | 2 | 2 | 0.127 | 1.000 | 0.944 | 1.04 | 1.00 | 6.63 | 6.83x | 6.82x | 0.97x | 0.938 | 0.65x |

**Speedup over ltlfsynt at the largest N finished (N=6, 2 of 2 cells): 6.83x (median); every cell finished.**

#### knowledge-chain

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 0 | 0.833 | n/a | n/a | 0.76 | 0.69 | n/a | n/a | n/a | n/a | n/a | n/a |
| 3 | 2 | 0 | 0.697 | n/a | n/a | 0.76 | 0.69 | n/a | n/a | n/a | n/a | n/a | n/a |
| 4 | 2 | 0 | 0.527 | n/a | n/a | 0.81 | 0.74 | n/a | n/a | n/a | n/a | n/a | n/a |
| 5 | 2 | 0 | 0.356 | n/a | n/a | 0.85 | 0.80 | n/a | n/a | n/a | n/a | n/a | n/a |
| 6 | 2 | 0 | 0.223 | n/a | n/a | 1.05 | 1.02 | n/a | n/a | n/a | n/a | n/a | n/a |

**Not raced against ltlfsynt: the family declares no psi_in, so ltlfsynt has no encoding of it (n/a by expressibility). EK finished 10 of 10 cells.**

#### slippery

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 6 | 6 | 16.000 | 1.882 | 1.882 | 1.51 | 1.43 | 7.88 | 5.30x | 5.40x | 2.58x | 1.000 | 0.79x |
| 3 | 6 | 6 | 64.000 | 1.969 | 1.969 | 3.27 | 3.20 | 10.57 | 3.08x | 2.93x | 7.07x | 1.000 | 0.75x |
| 4 | 6 | 6 | 256.000 | 1.992 | 1.992 | 7.81 | 7.71 | 14.73 | 1.78x | 1.77x | 37.61x | 1.000 | 0.62x |
| 5 | 6 | 6 | 1024.000 | 1.998 | 1.998 | 37.11 | 36.02 | 34.85 | 1.04x | 1.04x | 180.71x | 1.000 | 0.45x |
| 6 | 6 | 6 | 4096.000 | 2.000 | 2.000 | 149.73 | 147.34 | 138.55 | 0.93x | 0.92x | 1.70x | 1.000 | 0.54x |

**Speedup over ltlfsynt at the largest N finished (N=6, 6 of 6 cells): 0.93x (median); every cell finished.**

otf-mtdfa-product-nk timed out (20 s) at N=6 in 4 of 6 cells where otf-mtdfa-product finished: in-process speedup at least 77.11x (median lower bound; the figure in the table leaves those cells out).

