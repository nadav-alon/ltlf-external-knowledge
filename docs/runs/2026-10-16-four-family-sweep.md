# Sweep report

- build `Release`, Spot 2.15.1, commit `76b9e5a`, repeat=5, timeout=20 s
- EK subject: `otf-mtdfa-product` (wall-clock, median of the repeats); ltlfsynt on the monolithic `psi_in -> phi` encoding
- prune ratio: reachable product states with knowledge over without (`dfa-product` / `dfa-product-unpruned`)
- nk-product ratio: `dfa-product` over `dfa-product-nk` (psi_in folded into the Goal; the same decision problem ltlfsynt is given)
- a family's figure at N is the median over its cells (arm x realizable); all slippery-* arms are one family
- a cell with any timed-out or failed repeat, on either side, is left out (its median would cover the surviving repeats only)
- ltlfsynt startup floor (~4 ms) is noted, not subtracted
- verdict mismatches: 0

## co-generated

| N | cells | raced | prune ratio | nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) |
|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 2 | 1.125 | 2.208 | 2.08 | 2.03 | 12.92 | 6.23x | 6.25x |
| 3 | 2 | 2 | 1.188 | 4.325 | 2.23 | 2.20 | 13.38 | 6.02x | 6.06x |
| 4 | 2 | 2 | 1.219 | 8.403 | 2.52 | 2.50 | 16.47 | 6.57x | 6.59x |
| 5 | 2 | 1 | 1.234 | 16.449 | 3.20 | 3.16 | 16.02 | 4.86x | 4.91x |
| 6 | 2 | 1 | 1.242 | 32.473 | 4.23 | 4.19 | 21.39 | 4.90x | 4.94x |

**Speedup over ltlfsynt at the largest N finished (N=4, 2 of 2 cells): 6.57x (median); stops at N=4 because N=5 has 1 ltlfsynt timeout.**

ltlfsynt timed out (20 s) at N=5 in 1 of 2 cells where EK finished: speedup at least 6441.39x (median lower bound).

ltlfsynt timed out (20 s) at N=6 in 1 of 2 cells where EK finished: speedup at least 4874.16x (median lower bound).

## cons-prunes

| N | cells | raced | prune ratio | nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) |
|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 2 | 0.678 | 1.000 | 1.93 | 1.89 | 12.52 | 6.50x | 6.56x |
| 3 | 2 | 2 | 0.509 | 1.000 | 1.97 | 1.94 | 12.86 | 6.55x | 6.53x |
| 4 | 2 | 2 | 0.343 | 1.000 | 2.24 | 2.20 | 14.02 | 6.26x | 5.94x |
| 5 | 2 | 2 | 0.214 | 1.000 | 2.10 | 2.02 | 12.40 | 5.90x | 6.13x |
| 6 | 2 | 2 | 0.127 | 1.000 | 2.05 | 2.03 | 12.31 | 6.01x | 6.02x |

**Speedup over ltlfsynt at the largest N finished (N=6, 2 of 2 cells): 6.01x (median); every cell finished.**

## knowledge-chain

| N | cells | raced | prune ratio | nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) |
|---|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 0 | 0.833 | n/a | 1.90 | 1.86 | n/a | n/a | n/a |
| 3 | 2 | 0 | 0.697 | n/a | 1.98 | 1.94 | n/a | n/a | n/a |
| 4 | 2 | 0 | 0.527 | n/a | 2.02 | 2.00 | n/a | n/a | n/a |
| 5 | 2 | 0 | 0.356 | n/a | 2.11 | 2.07 | n/a | n/a | n/a |
| 6 | 2 | 0 | 0.223 | n/a | 2.21 | 2.18 | n/a | n/a | n/a |

**No speedup over ltlfsynt: no cell finished on both sides (ltlfsynt is n/a for a family without a declared psi_in).**

## slippery

| N | cells | raced | prune ratio | nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) |
|---|---|---|---|---|---|---|---|---|---|
| 2 | 6 | 6 | 16.000 | 1.882 | 3.99 | 3.92 | 14.95 | 3.60x | 3.64x |
| 3 | 6 | 6 | 64.000 | 1.969 | 9.83 | 9.64 | 20.58 | 1.89x | 1.86x |
| 4 | 6 | 6 | 256.000 | 1.992 | 29.88 | 29.52 | 33.19 | 0.97x | 0.97x |
| 5 | 6 | 6 | 1024.000 | 1.998 | 138.35 | 137.63 | 80.82 | 0.63x | 0.63x |
| 6 | 6 | 6 | 4096.000 | 2.000 | 575.87 | 573.48 | 272.13 | 0.48x | 0.47x |

**Speedup over ltlfsynt at the largest N finished (N=6, 6 of 6 cells): 0.48x (median); every cell finished.**

