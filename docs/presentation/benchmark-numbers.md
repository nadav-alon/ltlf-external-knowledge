# Benchmark numbers for the 2026-08-12 presentation

**Cite the Release sweep.** Workbook
[`docs/runs/2026-08-11-benchmarks-release.xlsx`](../runs/2026-08-11-benchmarks-release.xlsx),
report [`docs/runs/2026-08-11-benchmarks-release.json`](../runs/2026-08-11-benchmarks-release.json)
— 5 sheets (`summary`, `timings`, `structural`, `ltlfsynt`, `provenance`),
1400 timing rows.

The earlier Debug workbook (`2026-08-11-benchmarks.xlsx`) is kept for the
comparison in the deck's appendix and **should not be quoted** — see *What
changed under `-O2`* below.

Produced by `ltlf-ek-bench` (benchmark-suite PRD Phase 2), merged into this
branch from `worktree-bench-phase2` so the workbook is reproducible from a
checkout of the presentation branch alone.

## Reproducing it

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DSPOT_ROOT=~/opt/spot-2.15.1
cmake --build build-release -j --target ltlf-ek-bench
./build-release/ltlf-ek-bench --families=all --subjects=all --n-min=2 --n-max=8 \
  --repeat=3 --timeout=20 --budget=2400 \
  --ltlfsynt=/home/cowclaw/opt/spot-2.15.1/bin/ltlfsynt \
  --out=docs/runs/2026-08-11-benchmarks-release.json \
  --xlsx=docs/runs/2026-08-11-benchmarks-release.xlsx
```

`ltlfsynt` must be an **absolute path** — the bare name resolves through `PATH`
to a 2.14.4.dev install and would silently race the wrong version.

## Provenance (from the workbook's own sheet)

| key | value |
| --- | --- |
| date | 2026-08-11T18:05:51Z |
| machine | Nadav-Desktop |
| repo_commit | efd88a3 |
| spot_version | 2.15.1 (`~/opt/spot-2.15.1`) |
| **cmake_build_type** | **Release** |
| families / subjects | all 5 / all 5 |
| n_min .. n_max | 2 .. 8 (all 25 pairs reached n=8) |
| repeat | 3, per-case timeout 20 s |
| budget | 2400 s; `stopped_early = 0` |
| verdict_mismatch_count | 0 (T1 race agrees with `ltlfsynt` on every row) |

## Two caveats before quoting a number

1. **Do not cite `parity-t3`.** It is tier `t3`, which Stop-list 1 already bars
   from the comparison tables; separately, its declared
   `expected_realizable = true` is wrong (all five methods say unrealizable, and
   no system strategy exists for any `n` — see the PRD's *Developer comments*,
   2026-08-11). Its timing rows are real, its realizability story is not.
2. **The `summary` sheet is the `realizable = true` polarity only.** The
   realizable/unrealizable breakdown lives in `timings` / `structural`.
   `best_time_ns` is the full stage-sum; `construction_ns` is
   `automaton_construction + product_construction` (0 when a method never opens
   the span, as `OtfMtdfaProduct` does not).

The Debug-build caveat that used to head this list is **discharged** — the
sweep above is Release.

## What the four citable families show

`speedup_vs_mtdfa_product` is relative to `MtdfaProduct` (= 1); above 1 is
faster. At n=8, Release:

| family | dfa | nfa | mtdfa | mtnfa | otf-mtdfa |
| --- | --- | --- | --- | --- | --- |
| cons-prunes | 0.69 | 0.058 | 1 | 0.048 | **4.37** |
| cons-inert | 0.53 | 0.043 | 1 | 0.0087 | 0.91 |
| mirror-small | 0.36 | 0.141 | 1 | 0.149 | 0.92 |
| mirror-degenerate | 0.55 | 0.043 | 1 | 0.0090 | 0.95 |

The one clean qualitative story: **`OtfMtdfaProduct` wins where `cons` actually
prunes (`cons-prunes`, 4.4x) and costs essentially nothing where the pruning is
inert (0.91–0.95x).** The `structural` sheet backs this with sizes rather than
times, and the separation is exact:

| family | `goal_dfa_states`, n=2..8 | `product_states`, n=2..8 |
| --- | --- | --- |
| cons-prunes | 5, 9, 17, 33, 65, 129, 257 | 4, 5, 6, 7, 8, 9, 10 |
| cons-inert | 5, 9, 17, 33, 65, 129, 257 | *identical* |
| mirror-small | 8, 16, 32, 64, 128, 256, 512 | *identical* |
| mirror-degenerate | 5, 9, 17, 33, 65, 129, 257 | *identical* |

i.e. on `cons-prunes` the goal DFA is 2^n+1 while the product is n+2; on the
other three the product never gets smaller than the goal.

The MONA-backed methods (`nfa-product`, `mtnfa-product`) are 1–2 orders of
magnitude slower throughout; that is the shell-out, not the algorithm.

## The `ltlfsynt` race

**56 comparable (`t1`) cases, 0 verdict mismatches.** A further 14 `t3` cases
are recorded as `"n/a -- by expressibility"` — skipped by construction, with no
`ltlfsynt` contact attempted, because the monolithic encoding is not legitimate
for that tier. This is a cross-check on verdicts only; it does **not** settle
the monolithic reduction, which remains a conjecture with a known divergence
witness on partial transducers (O5).

## What changed under `-O2`

The Debug sweep materially understated `OtfMtdfaProduct` — the methods do not
pay equally for assertions, so cross-method ratios are **not build-portable**:

| family | Debug | Release |
| --- | --- | --- |
| cons-prunes | 3.09 | **4.37** |
| cons-inert | 0.39 | **0.91** |
| mirror-small | 0.42 | **0.92** |
| mirror-degenerate | 0.40 | **0.95** |

Debug read as "wins 3x here, costs you 2.5x everywhere else". Release reads as
"free when inert, 4.4x when it prunes" — a different conclusion, not a sharper
version of the same one.

## The deck

`docs/presentation/slides/` builds the 2026-08-12 deck from the Release report
directly (`charts.py` reads the JSON, not the workbook), so no number on a slide
is retyped by hand. `make` in that directory rebuilds figures and PDF.
