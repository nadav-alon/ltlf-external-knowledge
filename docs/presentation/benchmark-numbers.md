# Benchmark numbers for the 2026-08-12 presentation

Workbook: [`docs/runs/2026-08-11-benchmarks.xlsx`](../runs/2026-08-11-benchmarks.xlsx)
— 5 sheets (`summary`, `timings`, `structural`, `ltlfsynt`, `provenance`),
~1400 timing rows.

Produced by `ltlf-ek-bench` (benchmark-suite PRD Phase 2), which lives **only** on
the unmerged branch `worktree-bench-phase2`. The workbook is committed here so the
presentation does not depend on that branch landing.

## Provenance (from the workbook's own sheet)

| key | value |
| --- | --- |
| date | 2026-08-11T09:41:33Z |
| machine | Nadav-Desktop |
| repo_commit | 4b54fd0 |
| spot_version | 2.15.1 (`~/opt/spot-2.15.1`) |
| **cmake_build_type** | **Debug** |
| families / subjects | all 5 / all 5 |
| n_min .. n_max | 2 .. 8 (all 25 pairs reached n=8) |
| repeat | 3, per-case timeout 20 s |
| budget | 2400 s; actual sweep 22.6 s, `stopped_early = 0` |
| verdict_mismatch_count | 0 (T1 race agrees with `ltlfsynt` on every row) |

## Three caveats before quoting a number

1. **The sweep is a Debug build.** Every timing and every `speedup_vs_mtdfa_product`
   ratio is Debug-vs-Debug. The comparison is internally consistent, but absolute
   times are not representative and the ratios are not guaranteed to survive `-O2`
   (the methods do not pay equally for `-DNDEBUG`). Re-run in Release before
   presenting any figure as *the* cost of a method.
2. **Do not cite `parity-t3`.** It is tier `t3`, which Stop-list 1 already bars from
   the comparison tables; separately, its declared `expected_realizable = true` is
   wrong (all five methods say unrealizable, and no system strategy exists for any
   `n` — see the PRD's *Developer comments*, 2026-08-11). Its timing rows are real,
   its realizability story is not.
3. **The `summary` sheet is the `realizable = true` polarity only.** The
   realizable/unrealizable breakdown lives in `timings` / `structural`.
   `best_time_ns` is the full stage-sum; `construction_ns` is
   `automaton_construction + product_construction` (0 when a method never opens the
   span, as `OtfMtdfaProduct` does not).

## What the four citable families show

`speedup_vs_mtdfa_product` is relative to `MtdfaProduct` (= 1); above 1 is faster.
At n=8:

| family | dfa | nfa | mtdfa | mtnfa | otf-mtdfa |
| --- | --- | --- | --- | --- | --- |
| cons-prunes | 0.63 | 0.048 | 1 | 0.022 | **3.09** |
| cons-inert | 0.24 | 0.012 | 1 | 0.0015 | 0.39 |
| mirror-small | 0.16 | 0.060 | 1 | 0.065 | 0.42 |
| mirror-degenerate | 0.24 | 0.013 | 1 | 0.0016 | 0.40 |

The one clean qualitative story: **`OtfMtdfaProduct` wins only where `cons` actually
prunes** (`cons-prunes`, 3.1x), and loses to `MtdfaProduct` everywhere the pruning
is inert. The `structural` sheet backs this with sizes rather than times —
`cons-prunes` goal_dfa_states grows exponentially while product_states stays linear;
`cons-inert` has product_states == goal_dfa_states at every `n`.

The MONA-backed methods (`nfa-product`, `mtnfa-product`) are 2–3 orders of magnitude
slower throughout; that is the shell-out, not the algorithm, and it is a Debug build.
