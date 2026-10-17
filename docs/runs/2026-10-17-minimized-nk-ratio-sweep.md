# Minimized nk-product ratio, structural-only rerun (2026-10-17)

The four Oct 16 families (the three slippery arms as one, knowledge-chain, cons-prunes,
co-generated) at N=2..6, with `dfa-product`, `dfa-product-unpruned` and `dfa-product-nk`
only, repeat=1, on the desktop. Only the structural columns mean anything here: the
EK subject `otf-mtdfa-product` was not run (its columns read n/a, cells 0), and the
ltlfsynt column is an unrepeated side figure. The raw prune and nk-product ratios equal
the Oct 16 report's, so the new column sits beside the same raw figures. Raw data:
`2026-10-17-minimized-nk-ratio-sweep.json`.

# Sweep report

- build `Release`, Spot 2.15.1, commit `cc6090e`, repeat=1, timeout=20 s
- EK subject: `otf-mtdfa-product` (wall-clock, median of the repeats); ltlfsynt on the monolithic `psi_in -> phi` encoding
- prune ratio: reachable product states with knowledge over without (`dfa-product` / `dfa-product-unpruned`)
- nk-product ratio: `dfa-product` over `dfa-product-nk` (psi_in folded into the Goal; the same decision problem ltlfsynt is given)
- minimized nk-product ratio: the nk-product ratio with both products Moore-minimized before their states are counted (`product_states_min`); the completion sink is counted on neither side (a state with no path to acceptance adds nothing)
- a family's figure at N is the median over its cells (arm x realizable); all slippery-* arms are one family
- a cell with any timed-out or failed repeat, on either side, is left out (its median would cover the surviving repeats only)
- ltlfsynt startup floor (~4 ms) is noted, not subtracted
- verdict mismatches: 0

## co-generated

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) |
|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 0 | 0 | 1.125 | 2.208 | 1.688 | n/a | n/a | 5.41 | n/a | n/a |
| 3 | 0 | 0 | 1.188 | 4.325 | 2.708 | n/a | n/a | 5.68 | n/a | n/a |
| 4 | 0 | 0 | 1.219 | 8.403 | 4.725 | n/a | n/a | 6.60 | n/a | n/a |
| 5 | 0 | 0 | 1.234 | 16.449 | 8.736 | n/a | n/a | 2960.50 | n/a | n/a |
| 6 | 0 | 0 | 1.242 | 32.473 | 16.743 | n/a | n/a | 7.93 | n/a | n/a |

**No speedup over ltlfsynt: no cell finished on both sides (ltlfsynt is n/a for a family without a declared psi_in).**

## cons-prunes

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) |
|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 0 | 0 | 0.678 | 1.000 | 0.900 | n/a | n/a | 6.11 | n/a | n/a |
| 3 | 0 | 0 | 0.509 | 1.000 | 0.917 | n/a | n/a | 5.54 | n/a | n/a |
| 4 | 0 | 0 | 0.343 | 1.000 | 0.929 | n/a | n/a | 5.32 | n/a | n/a |
| 5 | 0 | 0 | 0.214 | 1.000 | 0.938 | n/a | n/a | 5.65 | n/a | n/a |
| 6 | 0 | 0 | 0.127 | 1.000 | 0.944 | n/a | n/a | 6.93 | n/a | n/a |

**No speedup over ltlfsynt: no cell finished on both sides (ltlfsynt is n/a for a family without a declared psi_in).**

## knowledge-chain

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) |
|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 0 | 0 | 0.833 | n/a | n/a | n/a | n/a | n/a | n/a | n/a |
| 3 | 0 | 0 | 0.697 | n/a | n/a | n/a | n/a | n/a | n/a | n/a |
| 4 | 0 | 0 | 0.527 | n/a | n/a | n/a | n/a | n/a | n/a | n/a |
| 5 | 0 | 0 | 0.356 | n/a | n/a | n/a | n/a | n/a | n/a | n/a |
| 6 | 0 | 0 | 0.223 | n/a | n/a | n/a | n/a | n/a | n/a | n/a |

**No speedup over ltlfsynt: no cell finished on both sides (ltlfsynt is n/a for a family without a declared psi_in).**

## slippery

| N | cells | raced | prune ratio | nk-product ratio | minimized nk-product ratio | EK med (ms) | EK min (ms) | ltlfsynt med (ms) | speedup (med) | speedup (min) |
|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 0 | 0 | 16.000 | 1.882 | 1.882 | n/a | n/a | 6.39 | n/a | n/a |
| 3 | 0 | 0 | 64.000 | 1.969 | 1.969 | n/a | n/a | 8.07 | n/a | n/a |
| 4 | 0 | 0 | 256.000 | 1.992 | 1.992 | n/a | n/a | 11.53 | n/a | n/a |
| 5 | 0 | 0 | 1024.000 | 1.998 | 1.998 | n/a | n/a | 21.90 | n/a | n/a |
| 6 | 0 | 0 | 4096.000 | 2.000 | 2.000 | n/a | n/a | 71.67 | n/a | n/a |

**No speedup over ltlfsynt: no cell finished on both sides (ltlfsynt is n/a for a family without a declared psi_in).**

