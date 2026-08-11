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

| family | tier | dfa | nfa | mtdfa | mtnfa | otf-mtdfa |
| --- | --- | --- | --- | --- | --- | --- |
| cons-prunes | t1 | 0.70 | 0.061 | 1 | 0.049 | **3.60** |
| cons-inert | t1 | 0.53 | 0.044 | 1 | 0.0094 | 0.94 |
| mirror-small | t1 | 0.36 | 0.135 | 1 | 0.155 | 1.01 |
| mirror-degenerate | t1 | 0.54 | 0.045 | 1 | 0.0082 | 0.88 |
| knowledge-chain | t2 | 0.75 | 0.073 | 1 | 0.059 | **3.03** |
| knowledge-chain-inert | t2 | 0.42 | 0.012 | 1 | 0.0062 | **0.55** |

> ### Run-to-run jitter: quote one significant figure
>
> `cons-prunes`' otf ratio measured **4.37** on the first Release sweep and
> **3.60** on the second — same binary, same build, same machine, `repeat=3`
> min-of-3. That is ~20% spread on the headline number, so **do not quote it to
> two decimals**. "3–4x" is the defensible claim; the *sign* and rough magnitude
> are stable, the precision is not. Raising `--repeat` is the fix if a tighter
> number is ever needed.

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

## The knowledge-size axis (added 2026-08-11)

The four original families all carry **one-state** knowledge — `trivial_transducer`
(1 state) or `one_state_const_transducer` (1 state) — so `|product| ≤ |goal|` holds
*by construction* and nothing measured what large knowledge costs. That was the
degenerate axis. Two `t2` families fix it, both with `|Ŧin| = n`:

| family | Ŧin | φ mentions the known var? | product at n=8 |
| --- | --- | --- | --- |
| `knowledge-chain` | n-state positional chain | yes | `2n+1` — `cons` collapses the goal's 2ⁿ |
| `knowledge-chain-inert` | n-state saturating run-length counter over free input `a` | no | **n × goal** exactly |

`knowledge-chain-inert` is the demonstration: `product_states / goal_dfa_states`
is exactly **2, 3, 4, 5, 6, 7, 8** at n=2..8 — the plain `|Ŧin| × |goal|`, asserted
cell-exact by `BenchSuiteDiscrimination.KnowledgeChainInertProductIsExactlyNTimesTheGoal`.

The run-length counter (not the positional chain) is what makes it multiply: a
chain that advances every step is synchronised with the goal's own step counter,
so it shares structure instead of multiplying — it measured only ~1.5x. Knowledge
whose state depends on the *input history* multiplies properly.

**Both are tier `t2`, deliberately.** The transducers are aperiodic (a saturating
counter, unlike a mod-n one — that is the parity witness generalised), so a ψ_in
exists and declaring `t3` would be a false claim that Stop-list 1 forbids guessing
at. No ψ_in is supplied, so they skip the `ltlfsynt` race rather than racing an
encoding — exactly what `t2` denotes. They remain fully legitimate for
**cross-method** comparison, which involves no expressibility claim.

**New result:** `OtfMtdfaProduct` is at its *worst* here — **0.55x** on
`knowledge-chain-inert`, well below the 0.88–1.01x of the small-knowledge inert
families. Large non-pruning knowledge is the regime where building the product
on the fly costs the most, which is worth knowing given that is exactly the case
the method is pitched at.

## The `ltlfsynt` race

**56 comparable (`t1`) cases, 0 verdict mismatches.** A further 14 `t3` cases
are recorded as `"n/a -- by expressibility"` — skipped by construction, with no
`ltlfsynt` contact attempted, because the monolithic encoding is not legitimate
for that tier. This is a cross-check on verdicts only; it does **not** settle
the monolithic reduction, which remains a conjecture with a known divergence
witness on partial transducers (O5).

### `ltlfsynt_ns` is startup-dominated — do not quote it as a speedup

The `ltlfsynt` sheet carries a wall time per case, and it is **flat**:

| | value |
| --- | --- |
| range over *every* family, every n, both polarities | **4.98 – 5.74 ms** |
| growth from n=2 to n=8 | **1.06 – 1.09x** |
| `MtdfaProduct` growth over the same range | **5.7x** |
| goal DFA growth over the same range | **5 → 257 states** |

The problem grows exponentially and the measured `ltlfsynt` time does not move,
so most of that number is a fixed floor. Measured directly
(`docs/presentation/slides/startup-floor.py`, minimum over 15 spawns):

| | min ms |
| --- | --- |
| `/bin/true` — bare `fork`+`exec` | 0.37 |
| `ltlfsynt --version` — no synthesis at all | 1.02 |
| `ltlfsynt`, trivial instance | 3.97 |
| **`ltlf-ek-synth`, trivial instance (ours)** | **4.07** |

**It is not spawn cost, and it is not specific to `ltlfsynt`.** Spawning is
0.4 ms and dynamic linking gets to 1 ms; the remaining ~3 ms is one-time
initialisation of the synthesis stack (Spot/BDD), and **our own CLI pays the
identical floor**. The asymmetry is purely *where the stopwatch starts*:
`ltlf-ek-bench` pays the init once at its own startup and then times
`method.synthesize(...)` in-process (`src/bench_suite.cpp`), while `ltlfsynt`
is timed around a whole `RunSubprocessCaptured` (`src/ltlf_ek_bench.cpp:846`).

So the naive ratio (10–77x "faster", *falling* with n — itself the tell) must
not be presented as a speedup. Subtracting the floor, `ltlfsynt` does roughly
`5.4 - 4.0 = 1.4 ms` of actual work at n=8 against `MtdfaProduct`'s 0.39 ms —
about **3.5x**, not 14x, and even that is an estimate by subtraction on a
different (monolithic) encoding.

To compare synthesis cost properly the suite needs instances large enough that
the work dominates the ~4 ms floor. **None of the four citable families get
there by n=8** — a gap for a future phase, not a result. (The cheap fix for a
like-for-like number would be to time `ltlf-ek-synth` as a subprocess too, so
both sides pay the same init.)

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
