# Smoke run of `scripts/run-sweep.sh` (60 s budget)

Command: `SPOT_ROOT=/usr/local BUDGET=60 scripts/run-sweep.sh` on 2026-10-11 at
commit `6ed61c3` (the script as pushed), Release build, Spot 2.15.1 (system
install; `~/opt/spot-2.15.1` is absent on this machine), repeat=5, timeout 20 s,
`ulimit -v` 8 GiB. The sweep stopped on its budget (`stopped_early`), so the
largest N rows are partial; the report below is verbatim.

## Sweep report

**STOPPED EARLY: the sweep hit its budget, so the grid is truncated and the largest N rows are partial.**

- build `Release`, Spot 2.15.1, commit `6ed61c3`, repeat=5, timeout=20 s
- EK subject: `otf-mtdfa-product` (wall-clock, median of the repeats); ltlfsynt on the monolithic `psi_in -> phi` encoding
- prune ratio: reachable product states with knowledge over without (`dfa-product` / `dfa-product-unpruned`)
- nk-product ratio: `dfa-product` over `dfa-product-nk` (psi_in folded into the Goal; the same decision problem ltlfsynt is given)
- a family's figure at N is the median over its cells (arm x realizable); all slippery-* arms are one family
- a cell with any timed-out or failed repeat, on either side, is left out (its median would cover the surviving repeats only)
- ltlfsynt startup floor (~4 ms) is noted, not subtracted
- verdict mismatches: 0

### cons-prunes

| N | cells | prune ratio | nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) |
|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 0.678 | 1.000 | 0.69 | 0.67 | 5.71 | 8.28x | 8.14x |
| 3 | 2 | 0.509 | 1.000 | 0.78 | 0.71 | 6.84 | 8.76x | 9.05x |
| 4 | 2 | 0.343 | 1.000 | 0.77 | 0.74 | 5.50 | 7.16x | 7.19x |
| 5 | 1 | 0.216 | 1.000 | 0.71 | 0.68 | 5.23 | 7.39x | 7.60x |

**Speedup over ltlfsynt at the largest N finished (N=5, 1 of 1 cells): 7.39x (median).**

### knowledge-chain

| N | cells | prune ratio | nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) |
|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 0.833 | n/a | 0.71 | 0.68 | n/a | n/a | n/a |
| 3 | 2 | 0.697 | n/a | 0.78 | 0.73 | n/a | n/a | n/a |
| 4 | 2 | 0.527 | n/a | 0.75 | 0.72 | n/a | n/a | n/a |
| 5 | 1 | 0.378 | n/a | 0.69 | 0.66 | n/a | n/a | n/a |

**No speedup over ltlfsynt: no cell finished on both sides (ltlfsynt is n/a for a family without a declared psi_in).**

### slippery

| N | cells | prune ratio | nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) |
|---|---|---|---|---|---|---|---|---|
| 2 | 6 | 16.000 | 1.882 | 1.44 | 1.40 | 6.63 | 4.52x | 4.50x |
| 3 | 6 | 64.000 | 1.969 | 3.20 | 3.12 | 8.96 | 2.55x | 2.57x |
| 4 | 6 | 256.000 | 1.992 | 7.30 | 7.15 | 11.58 | 1.31x | 1.31x |
| 5 | 1 | n/a | 1.998 | 24.71 | 24.50 | n/a | n/a | n/a |

**Speedup over ltlfsynt at the largest N finished (N=4, 6 of 6 cells): 1.31x (median).**

