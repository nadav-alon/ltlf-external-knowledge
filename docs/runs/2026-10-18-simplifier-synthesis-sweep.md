# Simplifier and full-synthesis sweep (2026-10-18)

The four Oct 16 families (the three slippery arms as one, knowledge-chain, cons-prunes, co-generated) at N=2..6 on the desktop, with every subject: the on-the-fly-solve pair from #25, the same pair with Spot's LTLf formula simplifier (`-simp`, #158), the build-then-solve subjects and the explicit-DFA subjects. ltlfsynt runs as **full synthesis**, not `--realizability` (#176), and the column that misread its root count is gone (#177).

- commit `cb1eb35` (master after #158, #176 and #177), Release build, Spot 2.15.1 (`~/opt/spot-2.15.1`), repeat=5, timeout=20 s
- machine: desktop (WSL2, 24 cores, 19 GiB), run 2026-10-18 19:40–20:22 IDT, via `mkdir -p build && SPOT_ROOT=~/opt/spot-2.15.1 scripts/run-sweep.sh build/benchout/sweep178`
- verdict mismatches across all subjects: **0**
- raw data: `2026-10-18-simplifier-synthesis-sweep.json`

## 1. Headline

Once the formula side gets the same cheap simplifier ltlfsynt uses, folding the knowledge into the formula is as fast as the transducer or faster. The one exception is where the simplifier itself blows up.

| family | EK vs formula, same on-the-fly solver, both simplified (N=2→6) | EK vs ltlfsynt, internal time, no startup floor (N=2→6) |
|---|---|---|
| cons-prunes | 0.96x → 1.06x (parity) | 0.95x → 0.71x (ltlfsynt faster) |
| slippery | 0.92x → 0.77x (formula faster) | 0.78x → 0.47x (ltlfsynt faster) |
| co-generated | 1.18x → 2256x at N=5; the formula side is unfinished in both N=6 cells. The simplifier blows up the folded formula here, the same pathology as ltlfsynt's erratic cells. | 0.90x → 0.60x, with ltlfsynt's erratic N=5 cell (3861x) |

**#159 (PR #175) corrected:** its in-process wins (slippery up to 159x, co-generated 9.6x) compared EK against a formula solver *without* the simplifier. With it, slippery's formula side goes from 10.8 s to 29 ms at N=5. The simplifier leaves EK's own times unchanged (φ alone is already simple), so the whole difference sits on the folded `ψ_in → φ`.

The explored-roots ratio, below 1 on co-generated (0.51–0.59) and cons-prunes (0.63–0.75), is unchanged, but it doesn't turn into time: the formula side does more, cheaper steps.

## 2. Per N


### On-the-fly solve, no simplifier

| family | N | EK med (ms) | formula med (ms) | in-process speedup | explored-roots ratio | formula unfinished (EK finished) | ltlfsynt internal speedup vs this EK |
|---|---|---|---|---|---|---|---|
| co-generated | 2 | 0.75 | 0.87 | 1.17x | 0.591 | 0 | 0.91x |
| co-generated | 3 | 0.78 | 1.04 | 1.33x | 0.553 | 0 | 1.01x |
| co-generated | 4 | 0.81 | 1.43 | 1.77x | 0.529 | 0 | 1.90x |
| co-generated | 5 | 0.81 | 3.10 | 3.88x | 0.515 | 0 | 4120.63x |
| co-generated | 6 | 1.25 | 11.11 | 9.15x | 0.508 | 0 | 0.62x |
| cons-prunes | 2 | 0.71 | 0.65 | 0.91x | 0.750 | 0 | 0.96x |
| cons-prunes | 3 | 0.78 | 0.71 | 0.92x | 0.700 | 0 | 0.86x |
| cons-prunes | 4 | 0.81 | 0.71 | 0.89x | 0.667 | 0 | 0.79x |
| cons-prunes | 5 | 0.80 | 0.76 | 0.96x | 0.643 | 0 | 0.81x |
| cons-prunes | 6 | 1.03 | 0.99 | 0.97x | 0.625 | 0 | 0.67x |
| knowledge-chain | 2 | 0.76 | n/a | n/a | n/a | 0 | n/a |
| knowledge-chain | 3 | 0.71 | n/a | n/a | n/a | 0 | n/a |
| knowledge-chain | 4 | 0.73 | n/a | n/a | n/a | 0 | n/a |
| knowledge-chain | 5 | 0.84 | n/a | n/a | n/a | 0 | n/a |
| knowledge-chain | 6 | 1.03 | n/a | n/a | n/a | 0 | n/a |
| slippery | 2 | 1.51 | 4.43 | 2.57x | 1.000 | 0 | 0.77x |
| slippery | 3 | 3.53 | 23.37 | 6.55x | 1.000 | 0 | 0.71x |
| slippery | 4 | 8.68 | 324.02 | 33.87x | 1.000 | 0 | 0.58x |
| slippery | 5 | 42.20 | 10774.59 | 154.17x | 1.000 | 1 | 0.40x |
| slippery | 6 | 178.79 | 272.10 | 1.60x | 1.000 | 4 | 0.46x |

### On-the-fly solve, with the formula simplifier

| family | N | EK med (ms) | formula med (ms) | in-process speedup | explored-roots ratio | formula unfinished (EK finished) | ltlfsynt internal speedup vs this EK |
|---|---|---|---|---|---|---|---|
| co-generated | 2 | 0.76 | 0.88 | 1.18x | 0.591 | 0 | 0.90x |
| co-generated | 3 | 0.74 | 1.17 | 1.58x | 0.553 | 0 | 1.07x |
| co-generated | 4 | 0.75 | 2.15 | 2.87x | 0.529 | 0 | 2.05x |
| co-generated | 5 | 0.83 | 1895.75 | 2255.64x | 0.514 | 0 | 3861.35x |
| co-generated | 6 | 1.25 | n/a | n/a | n/a | 2 | 0.60x |
| cons-prunes | 2 | 0.71 | 0.68 | 0.96x | 0.750 | 0 | 0.95x |
| cons-prunes | 3 | 0.77 | 0.73 | 0.95x | 0.700 | 0 | 0.87x |
| cons-prunes | 4 | 0.81 | 0.77 | 0.97x | 0.667 | 0 | 0.78x |
| cons-prunes | 5 | 0.79 | 0.80 | 1.02x | 0.643 | 0 | 0.82x |
| cons-prunes | 6 | 0.96 | 1.00 | 1.06x | 0.625 | 0 | 0.71x |
| knowledge-chain | 2 | 0.74 | n/a | n/a | n/a | 0 | n/a |
| knowledge-chain | 3 | 0.73 | n/a | n/a | n/a | 0 | n/a |
| knowledge-chain | 4 | 0.75 | n/a | n/a | n/a | 0 | n/a |
| knowledge-chain | 5 | 0.77 | n/a | n/a | n/a | 0 | n/a |
| knowledge-chain | 6 | 0.98 | n/a | n/a | n/a | 0 | n/a |
| slippery | 2 | 1.51 | 1.38 | 0.92x | 1.000 | 0 | 0.78x |
| slippery | 3 | 3.58 | 3.00 | 0.84x | 1.000 | 0 | 0.71x |
| slippery | 4 | 8.63 | 6.19 | 0.68x | 1.000 | 0 | 0.58x |
| slippery | 5 | 42.20 | 28.80 | 0.70x | 1.000 | 0 | 0.41x |
| slippery | 6 | 174.26 | 135.12 | 0.77x | 1.000 | 0 | 0.47x |

- in-process speedup = formula median / EK median, both `OtfMtdfaProduct --otf-solve` in the same process
- explored-roots ratio = EK `product_mtdfa_roots` / formula `product_mtdfa_roots`
- "formula unfinished": cells where EK finished and the formula side timed out (20 s), which are left out of that row's speedup
- ltlfsynt internal speedup = ltlfsynt's summed stage times (full synthesis) / that table's EK median

## 3. Full reports

### 3a. EK = `otf-mtdfa-product-otfsolve-simp`

The in-process columns in these renderings stay pinned to the build-then-solve pair; section 2 has the on-the-fly-solve pairs.

#### Sweep report

- build `Release`, Spot 2.15.1, commit `cb1eb35`, repeat=5, timeout=20 s
- EK subject: `otf-mtdfa-product-otfsolve-simp` (wall-clock, median of the repeats); ltlfsynt on the monolithic `psi_in -> phi` encoding, run as full synthesis (it builds a strategy, not `--realizability`)
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
| 2 | 2 | 2 | 1.125 | 2.208 | 1.688 | 0.76 | 0.70 | 6.59 | 8.77x | 9.16x | 1.08x | 1.193 | 0.90x |
| 3 | 2 | 2 | 1.188 | 4.325 | 2.708 | 0.74 | 0.69 | 7.39 | 9.99x | 9.97x | 1.26x | 1.332 | 1.07x |
| 4 | 2 | 2 | 1.219 | 8.403 | 4.725 | 0.75 | 0.71 | 8.51 | 11.34x | 11.54x | 1.70x | 1.412 | 2.05x |
| 5 | 2 | 2 | 1.234 | 16.449 | 8.736 | 0.83 | 0.74 | 3262.38 | 3877.54x | 4473.05x | 3.25x | 1.455 | 3861.35x |
| 6 | 2 | 1 | 1.242 | 32.473 | 16.743 | 1.25 | 1.18 | 12.39 | 9.19x | 9.61x | 9.37x | 1.477 | 0.60x |

**Speedup over ltlfsynt at the largest N finished (N=5, 2 of 2 cells): 3877.54x (median); stops at N=5 because N=6 has 1 ltlfsynt timeout.**

ltlfsynt timed out (20 s) at N=6 in 1 of 2 cells where EK finished: speedup at least 17280.19x (median lower bound).

#### cons-prunes

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 2 | 0.678 | 1.000 | 0.900 | 0.71 | 0.65 | 6.79 | 9.51x | 9.79x | 0.88x | 0.875 | 0.95x |
| 3 | 2 | 2 | 0.509 | 1.000 | 0.917 | 0.77 | 0.72 | 6.55 | 8.55x | 8.92x | 0.86x | 0.900 | 0.87x |
| 4 | 2 | 2 | 0.343 | 1.000 | 0.929 | 0.81 | 0.72 | 6.49 | 8.11x | 8.28x | 0.94x | 0.917 | 0.78x |
| 5 | 2 | 2 | 0.214 | 1.000 | 0.938 | 0.79 | 0.74 | 6.60 | 8.45x | 8.41x | 0.92x | 0.929 | 0.82x |
| 6 | 2 | 2 | 0.127 | 1.000 | 0.944 | 0.96 | 0.94 | 6.63 | 7.60x | 7.71x | 0.97x | 0.938 | 0.71x |

**Speedup over ltlfsynt at the largest N finished (N=6, 2 of 2 cells): 7.60x (median); every cell finished.**

#### knowledge-chain

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 0 | 0.833 | n/a | n/a | 0.74 | 0.69 | n/a | n/a | n/a | n/a | n/a | n/a |
| 3 | 2 | 0 | 0.697 | n/a | n/a | 0.73 | 0.67 | n/a | n/a | n/a | n/a | n/a | n/a |
| 4 | 2 | 0 | 0.527 | n/a | n/a | 0.75 | 0.72 | n/a | n/a | n/a | n/a | n/a | n/a |
| 5 | 2 | 0 | 0.356 | n/a | n/a | 0.77 | 0.75 | n/a | n/a | n/a | n/a | n/a | n/a |
| 6 | 2 | 0 | 0.223 | n/a | n/a | 0.98 | 0.93 | n/a | n/a | n/a | n/a | n/a | n/a |

**No speedup over ltlfsynt: no cell finished on both sides (the EK subject has no finished cell, or ltlfsynt has no value: it is n/a for a family without a declared psi_in).**

#### slippery

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 6 | 6 | 16.000 | 1.882 | 1.882 | 1.51 | 1.42 | 7.88 | 5.33x | 5.47x | 2.58x | 1.000 | 0.78x |
| 3 | 6 | 6 | 64.000 | 1.969 | 1.969 | 3.58 | 3.50 | 10.57 | 2.95x | 2.76x | 7.07x | 1.000 | 0.71x |
| 4 | 6 | 6 | 256.000 | 1.992 | 1.992 | 8.63 | 8.49 | 14.73 | 1.59x | 1.59x | 37.61x | 1.000 | 0.58x |
| 5 | 6 | 6 | 1024.000 | 1.998 | 1.998 | 42.20 | 41.32 | 34.85 | 0.93x | 0.90x | 180.71x | 1.000 | 0.41x |
| 6 | 6 | 6 | 4096.000 | 2.000 | 2.000 | 174.26 | 170.84 | 138.55 | 0.79x | 0.79x | 1.70x | 1.000 | 0.47x |

**Speedup over ltlfsynt at the largest N finished (N=6, 6 of 6 cells): 0.79x (median); every cell finished.**

otf-mtdfa-product-nk timed out (20 s) at N=6 in 4 of 6 cells where otf-mtdfa-product finished: in-process speedup at least 77.11x (median lower bound; the figure in the table leaves those cells out).


### 3b. EK = `otf-mtdfa-product-otfsolve`

#### Sweep report

- build `Release`, Spot 2.15.1, commit `cb1eb35`, repeat=5, timeout=20 s
- EK subject: `otf-mtdfa-product-otfsolve` (wall-clock, median of the repeats); ltlfsynt on the monolithic `psi_in -> phi` encoding, run as full synthesis (it builds a strategy, not `--realizability`)
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
| 2 | 2 | 2 | 1.125 | 2.208 | 1.688 | 0.75 | 0.72 | 6.59 | 8.84x | 8.80x | 1.08x | 1.193 | 0.91x |
| 3 | 2 | 2 | 1.188 | 4.325 | 2.708 | 0.78 | 0.75 | 7.39 | 9.48x | 9.27x | 1.26x | 1.332 | 1.01x |
| 4 | 2 | 2 | 1.219 | 8.403 | 4.725 | 0.81 | 0.73 | 8.51 | 10.52x | 11.29x | 1.70x | 1.412 | 1.90x |
| 5 | 2 | 2 | 1.234 | 16.449 | 8.736 | 0.81 | 0.75 | 3262.38 | 4137.51x | 4499.86x | 3.25x | 1.455 | 4120.63x |
| 6 | 2 | 1 | 1.242 | 32.473 | 16.743 | 1.25 | 1.17 | 12.39 | 9.50x | 10.18x | 9.37x | 1.477 | 0.62x |

**Speedup over ltlfsynt at the largest N finished (N=5, 2 of 2 cells): 4137.51x (median); stops at N=5 because N=6 has 1 ltlfsynt timeout.**

ltlfsynt timed out (20 s) at N=6 in 1 of 2 cells where EK finished: speedup at least 16699.76x (median lower bound).

#### cons-prunes

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 2 | 0.678 | 1.000 | 0.900 | 0.71 | 0.68 | 6.79 | 9.58x | 9.46x | 0.88x | 0.875 | 0.96x |
| 3 | 2 | 2 | 0.509 | 1.000 | 0.917 | 0.78 | 0.70 | 6.55 | 8.43x | 9.17x | 0.86x | 0.900 | 0.86x |
| 4 | 2 | 2 | 0.343 | 1.000 | 0.929 | 0.81 | 0.77 | 6.49 | 8.17x | 7.94x | 0.94x | 0.917 | 0.79x |
| 5 | 2 | 2 | 0.214 | 1.000 | 0.938 | 0.80 | 0.75 | 6.60 | 8.32x | 8.35x | 0.92x | 0.929 | 0.81x |
| 6 | 2 | 2 | 0.127 | 1.000 | 0.944 | 1.03 | 0.96 | 6.63 | 7.09x | 7.37x | 0.97x | 0.938 | 0.67x |

**Speedup over ltlfsynt at the largest N finished (N=6, 2 of 2 cells): 7.09x (median); every cell finished.**

#### knowledge-chain

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 0 | 0.833 | n/a | n/a | 0.76 | 0.72 | n/a | n/a | n/a | n/a | n/a | n/a |
| 3 | 2 | 0 | 0.697 | n/a | n/a | 0.71 | 0.68 | n/a | n/a | n/a | n/a | n/a | n/a |
| 4 | 2 | 0 | 0.527 | n/a | n/a | 0.73 | 0.70 | n/a | n/a | n/a | n/a | n/a | n/a |
| 5 | 2 | 0 | 0.356 | n/a | n/a | 0.84 | 0.78 | n/a | n/a | n/a | n/a | n/a | n/a |
| 6 | 2 | 0 | 0.223 | n/a | n/a | 1.03 | 0.98 | n/a | n/a | n/a | n/a | n/a | n/a |

**No speedup over ltlfsynt: no cell finished on both sides (the EK subject has no finished cell, or ltlfsynt has no value: it is n/a for a family without a declared psi_in).**

#### slippery

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 6 | 6 | 16.000 | 1.882 | 1.882 | 1.51 | 1.46 | 7.88 | 5.33x | 5.31x | 2.58x | 1.000 | 0.77x |
| 3 | 6 | 6 | 64.000 | 1.969 | 1.969 | 3.53 | 3.42 | 10.57 | 2.97x | 2.78x | 7.07x | 1.000 | 0.71x |
| 4 | 6 | 6 | 256.000 | 1.992 | 1.992 | 8.68 | 8.55 | 14.73 | 1.57x | 1.58x | 37.61x | 1.000 | 0.58x |
| 5 | 6 | 6 | 1024.000 | 1.998 | 1.998 | 42.20 | 41.36 | 34.85 | 0.92x | 0.90x | 180.71x | 1.000 | 0.40x |
| 6 | 6 | 6 | 4096.000 | 2.000 | 2.000 | 178.79 | 174.92 | 138.55 | 0.77x | 0.77x | 1.70x | 1.000 | 0.46x |

**Speedup over ltlfsynt at the largest N finished (N=6, 6 of 6 cells): 0.77x (median); every cell finished.**

otf-mtdfa-product-nk timed out (20 s) at N=6 in 4 of 6 cells where otf-mtdfa-product finished: in-process speedup at least 77.11x (median lower bound; the figure in the table leaves those cells out).


### 3c. EK = `otf-mtdfa-product` (build then solve, the default)

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

**No speedup over ltlfsynt: no cell finished on both sides (the EK subject has no finished cell, or ltlfsynt has no value: it is n/a for a family without a declared psi_in).**

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

