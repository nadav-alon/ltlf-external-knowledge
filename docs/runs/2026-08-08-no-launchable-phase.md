# Run report — 2026-08-08, wave 1 slot 1: no launchable phase

**Verdict: `DONE`.** No code was written, no branch created, no worktree opened.
Step 0 found no PRD that clears the Step 1 launch gate, and the one PRD with a
remaining phase fails it in a way only an evening grill can fix.

This is the day the backlog predicted: *"The queue is currently empty, which is
why day one is a grill."*

## What landed

Nothing but this report (`docs/runs/2026-08-08-no-launchable-phase.md`, committed
directly on `master` — docs only; no feature branch existed to carry it).

## Step 0 — candidate search, all three rules

| Rule | Result |
|---|---|
| 1. PRD named in the prompt | none named |
| 2. First **Now / next** item with a `docs/prd/` file on `master` | see below |
| 3. A PRD added on an unmerged branch | none |

**Rule 2, item by item:**

- **#1 acceptance-mark bug** — no PRD file. *Grill A is scheduled for tonight*
  (backlog week table, Sat 08-08), so this is expected, not a gap.
- **#2 parametric benchmark suite** — no PRD file. `docs/prd/benchmarking.md`
  exists but is a **different, already-implemented** PRD (the `bench.hpp`
  span-timing observability, merged 2026-07-13). The new suite's state lives in
  `docs/handoffs/2026-08-08-benchmark-suite.md`; Grill B is Mon 08-10.
- **#3 input-dependency gallery** — no PRD file.
  `docs/prd/input-dependencies-tool.md` is the shipped tool (implemented, all
  four gates closed 2026-08-03), not the gallery.
- **Method 3.2 (`otfagg`)** — no PRD, and a proven blocker (state-keyed $F_P$
  over-accepts).
- **Method 3.1 Phase 2** (`docs/prd/otf-mtdfa-product.md`) — the only Now/next
  item that *does* have a PRD with an unlanded phase. **Selected, then failed the
  launch gate.** Per the skill, no fall-through to another candidate.

**Rule 3:** the only unmerged branches are `docs/bootcamp` (2026-07-30) and
`practice/onboarding` (2026-07-09); neither adds a file under `docs/prd/`.

Also checked: `git worktree list` shows only the main checkout — the ten stale
worktrees noted in the backlog are already pruned.

## Step 1 — the gate failure, Method 3.1 Phase 2

Two gates fail, and the PRD says so itself:

- **Gate 2 (interfaces frozen).** The Phase 2 block is titled *"least settled;
  expect revision"* (`docs/prd/otf-mtdfa-product.md:426`). The backprop-node
  mapping — one node per MTBDD node vs per product state, terminal/leaf handling,
  `freeze_state` timing, hand-rolled feed vs public
  `mtdfa_to_backprop(dfa, early_stop=true)` — is explicitly *not* pinned, with
  the words **"`/developer` must not guess it"** (`:545`). The PRD closes:
  **"Phase 2 therefore starts with its own grill, not with code"** (`:554`).
- **Gate 3 (glossary).** `otf_solve_fused` is flagged *"Phase 2's name is not
  canonical yet"* (`:49`). Naming is the user's call; it belongs in a grill, not
  in a day-run.

Two further decisions are pre-flagged as Phase 2's to make (`:556`): whether to
fill unexpanded states with `bddfalse` under early abort, and whether interning
moves to Spot's two-map scheme (a PRD-change event if taken).

Both are decisions the user owns. A later session hits the identical wall, which
is why this ends the day rather than deferring.

## Findings deferred

None — no code was reviewed and no test was run.

One observation worth a moment tonight: **`docs/prd/ltlfsynt-oracle.md` still
reads `Status: draft`** with `glossary`, `code-review` and `theory-review`
unticked, even though its `tests` gate is ticked, the suite is landed, and its
successor `ltlfsynt-oracle-known-output.md` closed all four gates on 2026-08-03.
That looks like stale bookkeeping rather than open work — but it is not
launchable either way (it is not in **Now / next**, and it is gate-closing, not a
phase), so nothing was touched.

## Questions for the evening grill

1. **Grill A (acceptance-mark), the load-bearing one:** is a $\cons$-dead
   transducer state reached *after* acceptance a **win**? The whole fix shape
   waits on this, and the fix is not a drive-by — it re-opens `emits_dfa`'s
   contract and breaks `EmitsDfa.UndefinedAtStateHasNoOutgoingEdgesForAnyLetter`
   and `EmitsDfa.AcceptsTheEmptyWordAcrossEveryFixture`.
2. Fix locus for the same item: defensive `bddfalse`-guarded self-loop in
   `materialize_product` (mirroring the two correct precedents), or carry
   acceptance out-of-band from `ProductGuards` so no caller can lose it again?
3. Does the generated corpus want a **partial-transducer regime (Case B)**? The
   bug is evidence the total-by-construction corpus has a systematic blind spot.
4. Should `docs/prd/ltlfsynt-oracle.md`'s `Status:` be reconciled (see above)?

## Budget used

Phases run: 0. Repair rounds: 0. Review rounds: 0. Agents spawned: 0. Deadline
(`LTLF_EK_RUN_DEADLINE` = 16:37) was ~5 h away and was not a factor.
