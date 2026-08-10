# Day-run 2026-08-10 — parametric benchmark suite, Phase 1

**Verdict: landed, green, domain review clean.** Phase 1 of three (metric sink +
charge-table instrumentation). Branch `bench-phase1`.

PRD: [`docs/prd/benchmark-suite.md`](../prd/benchmark-suite.md), grilled
2026-08-09. Backlog item **#2**.

## The headline

**The no-op promise was not reachable from a call site, and the repair round
caught it.** B2 and `bench.hpp` both promised `record_size_metric` is a
"near-zero cost no-op if no `BenchScope` is active" — but a normal function
argument is evaluated *before* the callee's `g_active_collector == nullptr`
check runs. At the three `product_bdd_nodes` sites the argument is
`product->get_stats(/*nodes=*/true, …).nodes`, a full BDD-node traversal linear
in the largest structure in the run. Unguarded, **every production
`synthesize()` call of all three mtdfa-family methods would have paid it**,
benchmarking on or off.

The fix is additive, not a re-shaping: a new `bool bench_scope_active()` lets a
call site guard the expensive argument itself. `record_size_metric`'s signatures
and behaviour are unchanged. Recorded in the PRD's *Developer comments / PRD
disagreements*, which is where a deviation from a frozen interface block
belongs.

This is worth carrying forward as a **pattern, not an incident**: any future
"cheap when disabled" sink has the same hole, because C++ argument evaluation
does not care about the callee's early return.

## What landed

| Commit | What |
|---|---|
| `3698049` | `SizeMetric` sink + the B2 charge table wired into all five methods |
| `2238875` | Phase 1 metric-sink tests, bound to the frozen interface |
| `c317beb` | merge of the test-writer worktree into `bench-phase1` |
| `b2c936f` | repair round: `bench_scope_active()` + the four guarded call sites |
| this commit | tests + code-review gate refs, review notes, this report |

New: `tests/bench_size_metric_test.cpp` (17 cases).
Changed: `include/ltlf_ek/bench.hpp`, `src/bench.cpp`, the five method `.cpp`
files, `tests/bench_test.cpp`, `CMakeLists.txt`. **No `Synthesis` signature
moved** — every method edit is a `record_size_metric` call plus a mechanical
`return f(x)` → `auto c = f(x); …; return c;` refactor.

Phase 1 is the only phase that edits the five method `.cpp` files. No families,
no registry, no binary yet — those are Phase 2.

## Green checkpoint

The PRD's Phase 1 checkpoint asks for three things; all three are present and
were verified rather than assumed:

- the `bench_test.cpp` schema case **knowingly updated** for the new `metrics`
  array (plus a case pinning that the array is empty when nothing is recorded);
- a case per method asserting it emits **exactly** its charge-table row set —
  all five present, including the two absent-never-zero shapes
  (`OtfMtdfaProduct` emits no `goal_*` at all; an unrealizable run emits no
  `controller_states`);
- a **zero-perturbation** case per method — verdict and controller identical
  with and without an active `BenchScope`. This is the case that matters most
  here, since the guarded `get_stats()` call is the only code path whose
  execution now depends on whether a scope is installed.

`cmake --build` clean; `ctest` **603/603 passed**, `TMPDIR` inside the worktree
per `CLAUDE.md`.

## Gates

- **glossary** ✅ — closed 2026-08-09, pre-`/developer`. Re-checked here:
  `SizeMetric`, its six values, `size_metric_name` and `record_size_metric`
  appear in `docs/GLOSSARY.md` spelled exactly. `bench_scope_active()` is new
  since that entry was written and gets **no** row — it falls under the entry's
  own explicit exemption for "recording / emission plumbing", the same rule that
  exempts `BenchScope`/`BenchTimer`.
- **tests** ✅ — see the checkpoint above.
- **code-review** — **domain half done and clean** (`/code-reviewer`, no
  must-fix). The **generic half is open**: `/code-review` is not agent-invocable
  (`disable-model-invocation`), so a manual generic pass was substituted and the
  box is deliberately **left unticked**. Run `/review` on the PR to close it.
- **theory-review** — **not spawned, deliberately.** Phase 1 has no theory
  surface: `cons`, progression, sink placement, final-state classification and
  the `Synthesis`/`Transducer` contracts are all untouched, and the PRD's own
  header says *main.tex ref: none — this is infrastructure*. The gate stays open
  for Phases 2–3, where the *comparability tier* lands and with it the unstated
  LTLf $\equiv$ star-free claim (Stop-list 1).

Verified cell-for-cell that each method charges exactly its B2 row: `DfaProduct`
(`goal_dfa_states`, `product_states`), `NfaProduct` (`goal_nfa_states`,
`nfa_product_states`, `product_states`), `MtdfaProduct` / `MtnfaProduct` /
`OtfMtdfaProduct` (`product_states`, `product_bdd_nodes`, plus their respective
`goal_*` or its absence), all five plus `controller_states` when realizable.
`product_states` is taken by **role** — post-`minimize_mtdfa` in `MtdfaProduct`,
post-determinization in `NfaProduct` — matching consequence (1) of the glossary
entry.

Three `consider`-level notes, none acted on, all recorded in the PRD's
*Developer comments*: the `std::optional<BenchTimer>` guard in
`src/nfa_product.cpp` is the one guard of the four that buys nothing (SSO);
the three `get_stats` guards are deliberate verbatim triplicates (deduping would
drag Spot into a pure-`std` infra header); and `spot::complete_here(nfa)` mutates
in place, making the `goal_nfa_states` record's position order-critical.

## Why the run stopped, and what picked it up

The unattended run stopped after `b2c936f` — the repair round landed, but the
wrap-up (verify, review, gates, report, PR) never ran. Picked up manually the
same day; nothing was re-implemented, only verified and recorded.

## Next

Phase 2 — registry, families, timing sweep, workbook, `ltlfsynt` T1 race. It is
the Tuesday 2026-08-11 day-run and the **only phase on the critical path** for
the Wednesday 2026-08-12 progress presentation; Phase 3 (committed structural
baseline) is regression protection with no presentation value and was
deliberately swapped behind it.
