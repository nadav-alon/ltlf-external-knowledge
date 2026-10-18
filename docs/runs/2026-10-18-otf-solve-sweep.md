# On-the-fly-solve sweep, floor-free timing (2026-10-18)

The four Oct 16 families (the three slippery arms as one, knowledge-chain, cons-prunes, co-generated) at N=2..6 on the desktop. Every subject is included: `otf-mtdfa-product`, `otf-mtdfa-product-otfsolve`, `otf-mtdfa-product-nk`, `otf-mtdfa-product-nk-otfsolve`, `dfa-product`, `dfa-product-unpruned` and `dfa-product-nk`, plus ltlfsynt with `--verbose`.

- commit `488a075` (master after #25, #148 and #149), Release build, Spot 2.15.1 (`~/opt/spot-2.15.1`), repeat=5, timeout=20 s
- machine: desktop (WSL2, 24 cores, 19 GiB), run 2026-10-18 07:04–07:42 IDT, via `SPOT_ROOT=~/opt/spot-2.15.1 scripts/run-sweep.sh build/benchout/sweep159`
- verdict mismatches across all subjects: **0**
- raw data: `2026-10-18-otf-solve-sweep.json`

## 1. On-the-fly-solve EK vs on-the-fly-solve formula, in process

Both sides use the same `OtfMtdfaProduct` solver with `--otf-solve`, in the same process, with no startup floor. EK is `otf-mtdfa-product-otfsolve` (φ × T_in); the formula baseline is `otf-mtdfa-product-nk-otfsolve` (ψ_in → φ, trivial transducers).
- in-process speedup = formula median / EK median
- explored-roots ratio = EK `product_mtdfa_roots` / formula `product_mtdfa_roots`, so below 1 means EK explored less
- each figure is the median over the family's cells

Largest N at which both finished:

| family | N | cells | in-process speedup | explored-roots ratio |
|---|---|---|---|---|
| co-generated | 6 | 2 of 2 | 9.59x | 0.508 |
| cons-prunes | 6 | 2 of 2 | 0.99x | 0.625 |
| slippery | 5 | 6 of 6 | 158.68x | 1.000 |
| slippery | 6 | 2 of 6: the formula side timed out (20 s) in the other 4, where EK took ~150 ms | 1.54x (survivors only) | 1.000 |
| knowledge-chain | n/a: no ψ_in | | | |

Per N:

| family | N | EK otf-solve med (ms) | formula otf-solve med (ms) | in-process speedup (otf-solve) | explored-roots ratio (otf-solve) | formula timeouts (EK finished) | ltlfsynt explored roots (median) |
|---|---|---|---|---|---|---|---|
| co-generated | 2 | 0.69 | 0.77 | 1.11x | 0.591 | 0 | 1 |
| co-generated | 3 | 0.70 | 0.97 | 1.38x | 0.553 | 0 | 1 |
| co-generated | 4 | 0.76 | 1.33 | 1.76x | 0.529 | 0 | 1 |
| co-generated | 5 | 0.73 | 3.09 | 4.30x | 0.515 | 0 | 1 |
| co-generated | 6 | 0.97 | 9.29 | 9.59x | 0.508 | 0 | 1 |
| cons-prunes | 2 | 0.67 | 0.61 | 0.92x | 0.750 | 0 | 1 |
| cons-prunes | 3 | 0.72 | 0.70 | 0.98x | 0.700 | 0 | 1 |
| cons-prunes | 4 | 0.71 | 0.73 | 1.03x | 0.667 | 0 | 1 |
| cons-prunes | 5 | 0.74 | 0.71 | 0.96x | 0.643 | 0 | 1 |
| cons-prunes | 6 | 0.96 | 0.93 | 0.99x | 0.625 | 0 | 1 |
| knowledge-chain | 2 | 0.70 | n/a | n/a | n/a | 0 | n/a |
| knowledge-chain | 3 | 0.71 | n/a | n/a | n/a | 0 | n/a |
| knowledge-chain | 4 | 0.69 | n/a | n/a | n/a | 0 | n/a |
| knowledge-chain | 5 | 0.75 | n/a | n/a | n/a | 0 | n/a |
| knowledge-chain | 6 | 0.96 | n/a | n/a | n/a | 0 | n/a |
| slippery | 2 | 1.43 | 4.26 | 2.57x | 1.000 | 0 | 1 |
| slippery | 3 | 3.24 | 21.90 | 6.59x | 1.000 | 0 | 1 |
| slippery | 4 | 8.24 | 305.92 | 32.73x | 1.000 | 0 | 1 |
| slippery | 5 | 38.23 | 10608.15 | 158.68x | 1.000 | 0 | 1 |
| slippery | 6 | 149.93 | 219.40 | 1.54x | 1.000 | 4 | 1 |

## 2. On-the-fly-solve EK vs ltlfsynt

This is the report rendered with `--subject otf-mtdfa-product-otfsolve`, given in full in section 4. The column to read is **ltlfsynt internal speedup**: the sum of the stage times ltlfsynt reports, over the EK median, with no process startup on either side. N=2→6:

| family | ltlfsynt internal speedup | wall-clock speedup (floor included) |
|---|---|---|
| co-generated | 0.93x, 1.05x, 1.91x, 3982.84x, 0.70x (the N=5 cell is ltlfsynt's erratic 2.8 s cell, as on Oct 12 and Oct 16) | 15.05x → 11.25x |
| cons-prunes | 0.91x → 0.69x | 9.42x → 7.11x |
| slippery | 0.74x → 0.42x | 4.99x → 0.62x |

Floor-free, ltlfsynt is faster than on-the-fly-solve EK in almost every cell, and the gap widens with N. The wall-clock "speedups" come from ltlfsynt's process startup.

**Caveat, unverified:** ltlfsynt's explored-roots count, parsed from its `translation to MTDFA (k roots, …)` line by #149, is **1 in every cell**, including slippery N=6 at 85 ms. Either ltlfsynt's preprocessing decides at the root, or the parser reads only the first of several such lines. The "ltlfsynt explored-roots ratio" columns below should not be used until that is checked.

## 3. Reading

- **Against our own formula solver**, the transducer wins once both solve on the fly:
  - EK explores about half the roots on co-generated (0.51–0.59, falling) and cons-prunes (0.63–0.75, falling);
  - EK is up to 159x faster on slippery and 9.6x faster on co-generated;
  - cons-prunes is at parity on time.
- **Against ltlfsynt** it does not: floor-free, ltlfsynt is 1.1–2.4x faster on cons-prunes and slippery, and the gap grows with N. The formula baseline's weakness relative to ltlfsynt (slippery N=5: 10.6 s vs 26 ms) points at ltlfsynt's preprocessing (#158) or solver, not at the transducer.

## 4. Full reports

### 4a. EK = `otf-mtdfa-product-otfsolve`

The in-process columns in this rendering stay pinned to the build-then-solve pair; section 1 has the on-the-fly-solve pair.

#### Sweep report

- build `Release`, Spot 2.15.1, commit `488a075`, repeat=5, timeout=20 s
- EK subject: `otf-mtdfa-product-otfsolve` (wall-clock, median of the repeats); ltlfsynt on the monolithic `psi_in -> phi` encoding
- prune ratio: reachable product states with knowledge over without (`dfa-product` / `dfa-product-unpruned`)
- nk-product ratio: `dfa-product` over `dfa-product-nk` (psi_in folded into the Goal; the same decision problem ltlfsynt is given)
- minimized nk-product ratio: the nk-product ratio with both products Moore-minimized before their states are counted (`product_states_min`); the completion sink is counted on neither side (a state with no path to acceptance adds nothing)
- in-process speedup: `otf-mtdfa-product-nk` median over `otf-mtdfa-product` median, both timed in process, whatever --subject says; n/a for a family without a psi_in
- explored-roots ratio: `product_mtdfa_roots` of `otf-mtdfa-product` over that of `otf-mtdfa-product-nk`, whatever --subject says; n/a for a family without a psi_in
- a family's figure at N is the median over its cells (arm x realizable); all slippery-* arms are one family
- a cell with any timed-out or failed repeat, on either side, is left out (its median would cover the surviving repeats only)
- ltlfsynt internal speedup: the sum of the stage times `ltlfsynt --verbose` prints, over the EK median; no process startup floor on the ltlfsynt side. A cell whose stage lines are missing or unparseable is left out and counted in a note; a cell that timed out is left out and counted in the ltlfsynt-timeout note
- ltlfsynt explored-roots ratio: EK `product_mtdfa_roots` over the root count of ltlfsynt's `translation to MTDFA` line; a cell without that count is left out and counted in a note
- ltlfsynt startup floor (~4 ms) is noted, not subtracted, in the wall-clock columns
- verdict mismatches: 0

#### co-generated

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup | ltlfsynt explored-roots ratio |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 2 | 1.125 | 2.208 | 1.688 | 0.69 | 0.65 | 10.67 | 15.05x | 10.45x | 1.12x | 1.193 | 0.93x | 1.500 |
| 3 | 2 | 2 | 1.188 | 4.325 | 2.708 | 0.70 | 0.67 | 6.84 | 9.76x | 9.88x | 1.20x | 1.332 | 1.05x | 1.500 |
| 4 | 2 | 2 | 1.219 | 8.403 | 4.725 | 0.76 | 0.71 | 8.10 | 10.67x | 10.79x | 1.69x | 1.412 | 1.91x | 1.500 |
| 5 | 2 | 2 | 1.234 | 16.449 | 8.736 | 0.73 | 0.71 | 2821.92 | 3997.68x | 4085.47x | 3.41x | 1.455 | 3982.84x | 1.500 |
| 6 | 2 | 1 | 1.242 | 32.473 | 16.743 | 0.97 | 0.93 | 11.03 | 11.25x | 11.43x | 9.65x | 1.477 | 0.70x | 1.000 |

**Speedup over ltlfsynt at the largest N finished (N=5, 2 of 2 cells): 3997.68x (median); stops at N=5 because N=6 has 1 ltlfsynt timeout.**

ltlfsynt timed out (20 s) at N=6 in 1 of 2 cells where EK finished: speedup at least 20676.19x (median lower bound).

#### cons-prunes

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup | ltlfsynt explored-roots ratio |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 2 | 0.678 | 1.000 | 0.900 | 0.67 | 0.63 | 6.22 | 9.42x | 9.66x | 0.91x | 0.875 | 0.91x | 2.500 |
| 3 | 2 | 2 | 0.509 | 1.000 | 0.917 | 0.72 | 0.68 | 7.48 | 10.56x | 9.43x | 0.98x | 0.900 | 0.84x | 3.000 |
| 4 | 2 | 2 | 0.343 | 1.000 | 0.929 | 0.71 | 0.68 | 6.15 | 8.76x | 8.42x | 0.98x | 0.917 | 0.84x | 3.500 |
| 5 | 2 | 2 | 0.214 | 1.000 | 0.938 | 0.74 | 0.72 | 6.13 | 8.36x | 8.28x | 0.93x | 0.929 | 0.76x | 4.000 |
| 6 | 2 | 2 | 0.127 | 1.000 | 0.944 | 0.96 | 0.92 | 6.19 | 7.11x | 7.10x | 0.93x | 0.938 | 0.69x | 4.500 |

**Speedup over ltlfsynt at the largest N finished (N=6, 2 of 2 cells): 7.11x (median); every cell finished.**

#### knowledge-chain

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup | ltlfsynt explored-roots ratio |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 0 | 0.833 | n/a | n/a | 0.70 | 0.66 | n/a | n/a | n/a | n/a | n/a | n/a | n/a |
| 3 | 2 | 0 | 0.697 | n/a | n/a | 0.71 | 0.68 | n/a | n/a | n/a | n/a | n/a | n/a | n/a |
| 4 | 2 | 0 | 0.527 | n/a | n/a | 0.69 | 0.68 | n/a | n/a | n/a | n/a | n/a | n/a | n/a |
| 5 | 2 | 0 | 0.356 | n/a | n/a | 0.75 | 0.73 | n/a | n/a | n/a | n/a | n/a | n/a | n/a |
| 6 | 2 | 0 | 0.223 | n/a | n/a | 0.96 | 0.92 | n/a | n/a | n/a | n/a | n/a | n/a | n/a |

**No speedup over ltlfsynt: no cell finished on both sides (the EK subject has no finished cell, or ltlfsynt has no value: it is n/a for a family without a declared psi_in).**

#### slippery

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup | ltlfsynt explored-roots ratio |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 6 | 6 | 16.000 | 1.882 | 1.882 | 1.43 | 1.37 | 7.34 | 4.99x | 5.11x | 2.60x | 1.000 | 0.74x | 16.000 |
| 3 | 6 | 6 | 64.000 | 1.969 | 1.969 | 3.24 | 3.20 | 9.20 | 2.74x | 2.69x | 7.06x | 1.000 | 0.66x | 64.000 |
| 4 | 6 | 6 | 256.000 | 1.992 | 1.992 | 8.24 | 8.07 | 13.22 | 1.45x | 1.42x | 35.14x | 1.000 | 0.51x | 256.000 |
| 5 | 6 | 6 | 1024.000 | 1.998 | 1.998 | 38.23 | 37.80 | 25.82 | 0.79x | 0.80x | 176.30x | 1.000 | 0.36x | 1024.000 |
| 6 | 6 | 6 | 4096.000 | 2.000 | 2.000 | 149.93 | 147.26 | 85.12 | 0.62x | 0.59x | 1.60x | 1.000 | 0.42x | 4096.000 |

**Speedup over ltlfsynt at the largest N finished (N=6, 6 of 6 cells): 0.62x (median); every cell finished.**

otf-mtdfa-product-nk timed out (20 s) at N=6 in 4 of 6 cells where otf-mtdfa-product finished: in-process speedup at least 87.92x (median lower bound; the figure in the table leaves those cells out).


### 4b. EK = `otf-mtdfa-product` (build then solve, the default)

#### Sweep report

- build `Release`, Spot 2.15.1, commit `488a075`, repeat=5, timeout=20 s
- EK subject: `otf-mtdfa-product` (wall-clock, median of the repeats); ltlfsynt on the monolithic `psi_in -> phi` encoding
- prune ratio: reachable product states with knowledge over without (`dfa-product` / `dfa-product-unpruned`)
- nk-product ratio: `dfa-product` over `dfa-product-nk` (psi_in folded into the Goal; the same decision problem ltlfsynt is given)
- minimized nk-product ratio: the nk-product ratio with both products Moore-minimized before their states are counted (`product_states_min`); the completion sink is counted on neither side (a state with no path to acceptance adds nothing)
- in-process speedup: `otf-mtdfa-product-nk` median over `otf-mtdfa-product` median, both timed in process, whatever --subject says; n/a for a family without a psi_in
- explored-roots ratio: `product_mtdfa_roots` of `otf-mtdfa-product` over that of `otf-mtdfa-product-nk`, whatever --subject says; n/a for a family without a psi_in
- a family's figure at N is the median over its cells (arm x realizable); all slippery-* arms are one family
- a cell with any timed-out or failed repeat, on either side, is left out (its median would cover the surviving repeats only)
- ltlfsynt internal speedup: the sum of the stage times `ltlfsynt --verbose` prints, over the EK median; no process startup floor on the ltlfsynt side. A cell whose stage lines are missing or unparseable is left out and counted in a note; a cell that timed out is left out and counted in the ltlfsynt-timeout note
- ltlfsynt explored-roots ratio: EK `product_mtdfa_roots` over the root count of ltlfsynt's `translation to MTDFA` line; a cell without that count is left out and counted in a note
- ltlfsynt startup floor (~4 ms) is noted, not subtracted, in the wall-clock columns
- verdict mismatches: 0

#### co-generated

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup | ltlfsynt explored-roots ratio |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 2 | 1.125 | 2.208 | 1.688 | 0.73 | 0.69 | 10.67 | 14.26x | 9.79x | 1.12x | 1.193 | 0.89x | 7.000 |
| 3 | 2 | 2 | 1.188 | 4.325 | 2.708 | 0.83 | 0.79 | 6.84 | 8.31x | 8.37x | 1.20x | 1.332 | 0.90x | 15.000 |
| 4 | 2 | 2 | 1.219 | 8.403 | 4.725 | 0.96 | 0.92 | 8.10 | 8.46x | 8.35x | 1.69x | 1.412 | 1.52x | 31.000 |
| 5 | 2 | 2 | 1.234 | 16.449 | 8.736 | 1.29 | 1.24 | 2821.92 | 2185.62x | 2239.80x | 3.41x | 1.455 | 2177.32x | 63.000 |
| 6 | 2 | 1 | 1.242 | 32.473 | 16.743 | 1.93 | 1.89 | 11.03 | 5.38x | 5.42x | 9.65x | 1.477 | 0.34x | 127.000 |

**Speedup over ltlfsynt at the largest N finished (N=5, 2 of 2 cells): 2185.62x (median); stops at N=5 because N=6 has 1 ltlfsynt timeout.**

ltlfsynt timed out (20 s) at N=6 in 1 of 2 cells where EK finished: speedup at least 11064.76x (median lower bound).

#### cons-prunes

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup | ltlfsynt explored-roots ratio |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 2 | 0.678 | 1.000 | 0.900 | 0.67 | 0.62 | 6.22 | 9.37x | 9.73x | 0.91x | 0.875 | 0.91x | 3.000 |
| 3 | 2 | 2 | 0.509 | 1.000 | 0.917 | 0.70 | 0.68 | 7.48 | 10.72x | 9.49x | 0.98x | 0.900 | 0.85x | 4.000 |
| 4 | 2 | 2 | 0.343 | 1.000 | 0.929 | 0.74 | 0.71 | 6.15 | 8.41x | 8.01x | 0.98x | 0.917 | 0.80x | 5.000 |
| 5 | 2 | 2 | 0.214 | 1.000 | 0.938 | 0.80 | 0.75 | 6.13 | 7.65x | 7.91x | 0.93x | 0.929 | 0.70x | 6.000 |
| 6 | 2 | 2 | 0.127 | 1.000 | 0.944 | 1.00 | 0.99 | 6.19 | 6.63x | 6.36x | 0.93x | 0.938 | 0.64x | 7.000 |

**Speedup over ltlfsynt at the largest N finished (N=6, 2 of 2 cells): 6.63x (median); every cell finished.**

#### knowledge-chain

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup | ltlfsynt explored-roots ratio |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 0 | 0.833 | n/a | n/a | 0.67 | 0.64 | n/a | n/a | n/a | n/a | n/a | n/a | n/a |
| 3 | 2 | 0 | 0.697 | n/a | n/a | 0.74 | 0.68 | n/a | n/a | n/a | n/a | n/a | n/a | n/a |
| 4 | 2 | 0 | 0.527 | n/a | n/a | 0.75 | 0.71 | n/a | n/a | n/a | n/a | n/a | n/a | n/a |
| 5 | 2 | 0 | 0.356 | n/a | n/a | 0.80 | 0.74 | n/a | n/a | n/a | n/a | n/a | n/a | n/a |
| 6 | 2 | 0 | 0.223 | n/a | n/a | 1.01 | 0.98 | n/a | n/a | n/a | n/a | n/a | n/a | n/a |

**No speedup over ltlfsynt: no cell finished on both sides (the EK subject has no finished cell, or ltlfsynt has no value: it is n/a for a family without a declared psi_in).**

#### slippery

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup | ltlfsynt explored-roots ratio |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 6 | 6 | 16.000 | 1.882 | 1.882 | 1.40 | 1.37 | 7.34 | 5.07x | 5.11x | 2.60x | 1.000 | 0.76x | 16.000 |
| 3 | 6 | 6 | 64.000 | 1.969 | 1.969 | 3.09 | 3.01 | 9.20 | 2.82x | 2.79x | 7.06x | 1.000 | 0.68x | 64.000 |
| 4 | 6 | 6 | 256.000 | 1.992 | 1.992 | 7.48 | 7.26 | 13.22 | 1.52x | 1.47x | 35.14x | 1.000 | 0.55x | 256.000 |
| 5 | 6 | 6 | 1024.000 | 1.998 | 1.998 | 34.27 | 33.71 | 25.82 | 0.87x | 0.87x | 176.30x | 1.000 | 0.39x | 1024.000 |
| 6 | 6 | 6 | 4096.000 | 2.000 | 2.000 | 128.01 | 125.61 | 85.12 | 0.71x | 0.67x | 1.60x | 1.000 | 0.47x | 4096.000 |

**Speedup over ltlfsynt at the largest N finished (N=6, 6 of 6 cells): 0.71x (median); every cell finished.**

otf-mtdfa-product-nk timed out (20 s) at N=6 in 4 of 6 cells where otf-mtdfa-product finished: in-process speedup at least 87.92x (median lower bound; the figure in the table leaves those cells out).

