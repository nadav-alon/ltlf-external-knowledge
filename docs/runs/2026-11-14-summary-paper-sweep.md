# Sweep report

- build `Release`, Spot 2.15.1, commit `4e3beec`, repeat=5, timeout=20 s
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

## co-generated

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 2 | 1.125 | 2.208 | 1.688 | 2.18 | 2.15 | 13.64 | 6.28x | 6.09x | 1.06x | 1.193 | 0.85x |
| 3 | 2 | 2 | 1.188 | 4.325 | 2.708 | 2.35 | 2.29 | 15.44 | 6.60x | 6.69x | 1.28x | 1.332 | 0.92x |
| 4 | 2 | 2 | 1.219 | 8.403 | 4.725 | 2.71 | 2.57 | 21.46 | 7.98x | 8.46x | 1.80x | 1.412 | 1.53x |
| 5 | 2 | 1 | 1.234 | 16.449 | 8.736 | 3.23 | 3.16 | 24.68 | 7.37x | 7.62x | 4.09x | 1.455 | 0.59x |
| 6 | 2 | 1 | 1.242 | 32.473 | 16.743 | 4.30 | 4.18 | 44.48 | 10.09x | 10.13x | 15.32x | 1.477 | 0.49x |

**Speedup over ltlfsynt at the largest N finished (N=4, 2 of 2 cells): 7.98x (median); stops at N=4 because N=5 has 1 ltlfsynt timeout.**

ltlfsynt timed out (20 s) at N=5 in 1 of 2 cells where EK finished: speedup at least 6412.86x (median lower bound).

ltlfsynt timed out (20 s) at N=6 in 1 of 2 cells where EK finished: speedup at least 4762.97x (median lower bound).

## cons-inert

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 2 | 1.000 | 1.000 | 1.000 | 1.68 | 1.60 | 14.67 | 8.82x | 9.03x | 0.94x | 1.000 | 0.96x |
| 3 | 2 | 2 | 1.000 | 1.000 | 1.000 | 1.68 | 1.63 | 14.76 | 8.81x | 8.95x | 0.97x | 1.000 | 0.96x |
| 4 | 2 | 2 | 1.000 | 1.000 | 1.000 | 1.77 | 1.73 | 14.68 | 8.34x | 8.42x | 0.99x | 1.000 | 0.91x |
| 5 | 2 | 2 | 1.000 | 1.000 | 1.000 | 1.95 | 1.87 | 14.98 | 7.68x | 7.96x | 0.97x | 1.000 | 0.84x |
| 6 | 2 | 2 | 1.000 | 1.000 | 1.000 | 2.28 | 2.23 | 14.95 | 6.56x | 6.64x | 0.95x | 1.000 | 0.73x |

**Speedup over ltlfsynt at the largest N finished (N=6, 2 of 2 cells): 6.56x (median); every cell finished.**

## cons-prunes

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 2 | 0.678 | 1.000 | 0.900 | 2.05 | 1.82 | 17.06 | 8.37x | 9.16x | 0.84x | 0.875 | 0.98x |
| 3 | 2 | 2 | 0.509 | 1.000 | 0.917 | 1.92 | 1.86 | 17.66 | 9.30x | 9.35x | 0.94x | 0.900 | 1.09x |
| 4 | 2 | 2 | 0.343 | 1.000 | 0.929 | 1.95 | 1.83 | 17.54 | 9.06x | 9.30x | 0.96x | 0.917 | 1.08x |
| 5 | 2 | 2 | 0.214 | 1.000 | 0.938 | 1.89 | 1.84 | 18.24 | 9.65x | 9.74x | 1.07x | 0.929 | 1.14x |
| 6 | 2 | 2 | 0.127 | 1.000 | 0.944 | 2.72 | 2.42 | 18.08 | 6.68x | 7.46x | 0.86x | 0.938 | 0.81x |

**Speedup over ltlfsynt at the largest N finished (N=6, 2 of 2 cells): 6.68x (median); every cell finished.**

## hidden-counter

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 2 | 2 | 1.896 | 1.264 | 0.722 | 1.79 | 1.73 | 17.01 | 9.49x | 9.60x | 1.15x | 1.234 | 1.30x |
| 2 | 2 | 2 | 2.979 | 1.266 | 0.641 | 2.10 | 1.98 | 17.49 | 8.32x | 8.61x | 1.05x | 1.209 | 1.15x |
| 3 | 2 | 2 | 4.533 | 1.413 | 0.596 | 2.25 | 2.17 | 18.92 | 8.42x | 8.30x | 1.25x | 1.429 | 1.26x |
| 4 | 2 | 2 | 6.939 | 1.550 | 0.621 | 3.04 | 2.96 | 20.35 | 6.73x | 6.77x | 1.38x | 1.602 | 1.26x |
| 5 | 2 | 2 | 11.194 | 1.683 | 0.695 | 5.79 | 5.71 | 23.32 | 4.14x | 4.13x | 1.27x | 1.743 | 1.20x |
| 6 | 2 | 2 | 19.340 | 1.798 | 0.787 | 13.89 | 13.84 | 34.94 | 2.63x | 2.60x | 1.20x | 1.848 | 1.14x |

**Speedup over ltlfsynt at the largest N finished (N=6, 2 of 2 cells): 2.63x (median); every cell finished.**

## knowledge-chain

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 0 | 0.833 | n/a | n/a | 2.06 | 1.94 | n/a | n/a | n/a | n/a | n/a | n/a |
| 3 | 2 | 0 | 0.697 | n/a | n/a | 2.11 | 2.05 | n/a | n/a | n/a | n/a | n/a | n/a |
| 4 | 2 | 0 | 0.527 | n/a | n/a | 2.15 | 2.06 | n/a | n/a | n/a | n/a | n/a | n/a |
| 5 | 2 | 0 | 0.356 | n/a | n/a | 2.21 | 2.13 | n/a | n/a | n/a | n/a | n/a | n/a |
| 6 | 2 | 0 | 0.223 | n/a | n/a | 2.28 | 2.16 | n/a | n/a | n/a | n/a | n/a | n/a |

**Not raced against ltlfsynt: the family declares no psi_in, so ltlfsynt has no encoding of it (n/a by expressibility). EK finished 10 of 10 cells.**

## knowledge-chain-inert

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 0 | 1.944 | n/a | n/a | 1.80 | 1.74 | n/a | n/a | n/a | n/a | n/a | n/a |
| 3 | 2 | 0 | 2.846 | n/a | n/a | 2.02 | 1.95 | n/a | n/a | n/a | n/a | n/a | n/a |
| 4 | 2 | 0 | 3.833 | n/a | n/a | 2.46 | 2.42 | n/a | n/a | n/a | n/a | n/a | n/a |
| 5 | 2 | 0 | 4.865 | n/a | n/a | 3.40 | 3.37 | n/a | n/a | n/a | n/a | n/a | n/a |
| 6 | 2 | 0 | 5.906 | n/a | n/a | 5.45 | 5.44 | n/a | n/a | n/a | n/a | n/a | n/a |

**Not raced against ltlfsynt: the family declares no psi_in, so ltlfsynt has no encoding of it (n/a by expressibility). EK finished 10 of 10 cells.**

## mirror-degenerate

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 2 | 1.000 | 1.000 | 1.000 | 1.71 | 1.61 | 14.66 | 8.61x | 9.00x | 0.93x | 1.000 | 0.94x |
| 3 | 2 | 2 | 1.000 | 1.000 | 1.000 | 1.74 | 1.67 | 14.76 | 8.53x | 8.75x | 0.95x | 1.000 | 0.93x |
| 4 | 2 | 2 | 1.000 | 1.000 | 1.000 | 1.79 | 1.74 | 14.99 | 8.37x | 8.48x | 0.96x | 1.000 | 0.91x |
| 5 | 2 | 2 | 1.000 | 1.000 | 1.000 | 1.94 | 1.89 | 14.59 | 7.52x | 7.64x | 0.95x | 1.000 | 0.83x |
| 6 | 2 | 2 | 1.000 | 1.000 | 1.000 | 2.25 | 2.22 | 14.78 | 6.59x | 6.59x | 0.96x | 1.000 | 0.73x |

**Speedup over ltlfsynt at the largest N finished (N=6, 2 of 2 cells): 6.59x (median); every cell finished.**

## mirror-small

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 2 | 1.000 | 1.000 | 1.000 | 1.65 | 1.60 | 14.73 | 8.92x | 9.10x | 0.95x | 1.000 | 0.98x |
| 3 | 2 | 2 | 1.000 | 1.000 | 1.000 | 1.74 | 1.68 | 14.81 | 8.53x | 8.74x | 0.97x | 1.000 | 0.93x |
| 4 | 2 | 2 | 1.000 | 1.000 | 1.000 | 1.78 | 1.75 | 14.63 | 8.22x | 8.35x | 0.96x | 1.000 | 0.91x |
| 5 | 2 | 2 | 1.000 | 1.000 | 1.000 | 2.04 | 1.95 | 14.80 | 7.32x | 7.52x | 0.97x | 1.000 | 0.81x |
| 6 | 2 | 2 | 1.000 | 1.000 | 1.000 | 2.38 | 2.31 | 14.76 | 6.21x | 6.34x | 0.99x | 1.000 | 0.70x |

**Speedup over ltlfsynt at the largest N finished (N=6, 2 of 2 cells): 6.21x (median); every cell finished.**

## slippery

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) | in-process speedup | explored-roots ratio | ltlfsynt internal speedup |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 6 | 6 | 16.000 | 1.882 | 1.882 | 4.13 | 4.05 | 17.49 | 4.13x | 4.14x | 3.14x | 1.000 | 0.73x |
| 3 | 6 | 6 | 64.000 | 1.969 | 1.969 | 9.77 | 9.60 | 27.57 | 2.48x | 2.46x | 9.21x | 1.000 | 0.64x |
| 4 | 6 | 6 | 256.000 | 1.992 | 1.992 | 29.26 | 29.03 | 49.12 | 1.50x | 1.50x | 34.55x | 1.000 | 0.54x |
| 5 | 6 | 6 | 1024.000 | 1.998 | 1.998 | 140.22 | 139.85 | 145.77 | 1.04x | 1.05x | 1.19x | 1.000 | 0.46x |
| 6 | 6 | 6 | 4096.000 | 2.000 | 2.000 | 549.31 | 541.30 | 569.64 | 1.04x | 1.05x | 1.36x | 1.000 | 0.52x |

**Speedup over ltlfsynt at the largest N finished (N=6, 6 of 6 cells): 1.04x (median); every cell finished.**

otf-mtdfa-product-nk timed out (20 s) at N=5 in 4 of 6 cells where otf-mtdfa-product finished: in-process speedup at least 91.03x (median lower bound; the figure in the table leaves those cells out).

otf-mtdfa-product-nk timed out (20 s) at N=6 in 4 of 6 cells where otf-mtdfa-product finished: in-process speedup at least 20.66x (median lower bound; the figure in the table leaves those cells out).

