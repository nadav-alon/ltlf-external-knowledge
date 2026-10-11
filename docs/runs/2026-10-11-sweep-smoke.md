# Smoke run of `scripts/run-sweep.sh` (60 s budget)

Command: `BUDGET=60 scripts/run-sweep.sh` on 2026-10-11, Release build, Spot 2.15.1
(system install, `/usr/local`; `~/opt/spot-2.15.1` is absent on this machine),
repeat=5, timeout 20 s, `ulimit -v` 8 GiB. Wall-clock for the command including
the Release build: 1 m 20 s. The sweep stopped on its budget (`stopped_early`),
so the largest N rows are partial; the report below is verbatim.

## Sweep report

- build `Release`, Spot 2.15.1, commit `5b8314b`, repeat=5, timeout=20 s
- EK subject: `otf-mtdfa-product` (wall-clock, median of the repeats); ltlfsynt on the monolithic `psi_in -> phi` encoding
- prune ratio: reachable product states with knowledge over without (`dfa-product` / `dfa-product-unpruned`)
- prune ratio (vs -nk): the same over `dfa-product-nk` (psi_in folded into the Goal; the same decision problem ltlfsynt is given)
- a family's figure at N is the median over its cells (arm x realizable); all slippery-* arms are one family
- ltlfsynt startup floor (~4 ms) is noted, not subtracted
- stopped early: True; verdict mismatches: 0

### cons-prunes

| N | cells | prune ratio | prune ratio (vs -nk) | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) |
|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 0.678 | 1.000 | 0.72 | 0.70 | 6.18 | 8.63x | 8.52x |
| 3 | 2 | 0.509 | 1.000 | 0.77 | 0.74 | 6.00 | 7.85x | 7.84x |
| 4 | 2 | 0.343 | 1.000 | 0.85 | 0.82 | 6.25 | 7.38x | 7.49x |
| 5 | 1 | 0.216 | 1.000 | 0.75 | 0.72 | 8.16 | 10.82x | 7.87x |

**Speedup over ltlfsynt at the largest N finished (N=5): 10.82x (median).**

### knowledge-chain

| N | cells | prune ratio | prune ratio (vs -nk) | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) |
|---|---|---|---|---|---|---|---|---|
| 2 | 2 | 0.833 | n/a | 0.72 | 0.70 | n/a | n/a | n/a |
| 3 | 2 | 0.697 | n/a | 0.80 | 0.76 | n/a | n/a | n/a |
| 4 | 2 | 0.527 | n/a | 0.84 | 0.82 | n/a | n/a | n/a |
| 5 | 1 | 0.378 | n/a | 0.73 | 0.69 | n/a | n/a | n/a |

**No speedup over ltlfsynt: no cell finished on both sides (ltlfsynt is n/a for a family without a declared psi_in).**

### slippery

| N | cells | prune ratio | prune ratio (vs -nk) | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) |
|---|---|---|---|---|---|---|---|---|
| 2 | 6 | 16.000 | 1.882 | 1.59 | 1.53 | 7.01 | 4.39x | 4.46x |
| 3 | 6 | 64.000 | 1.969 | 3.51 | 3.43 | 8.74 | 2.46x | 2.42x |
| 4 | 6 | 256.000 | 1.992 | 8.07 | 7.86 | 12.23 | 1.39x | 1.32x |
| 5 | 1 | n/a | 1.998 | 26.17 | 25.61 | n/a | n/a | n/a |

**Speedup over ltlfsynt at the largest N finished (N=4): 1.39x (median).**

