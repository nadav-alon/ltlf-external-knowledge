# Run report — 2026-08-03, input-dependency extraction, Phase 1

Unattended day-run, wave 1 slot 1 (`/launcher`). One phase, as instructed.

**PRD:** `docs/prd/input-dependencies-tool.md` — picked by Step 0 rule 2 (first
**Now / next** backlog item with a PRD on `master`; no unmerged branch added a
PRD, so rule 3 found nothing).
**Phase:** 1 — the shared `detail::` dependency core and `dependent_inputs`.
**Workflow:** concurrent, as the PRD prescribes (freeze confidence *high*).
**Feature branch:** `input-dependencies-tool`. **Phase branch:**
`worktree-indeps-phase1`.

## Launch gate — clean

All five checks passed before any code was written: `Status: draft` with two
phases outstanding; *Interfaces & types → Phase 1* frozen with no placeholder;
all four new glossary terms (*Dependent input set*, *Violation automaton*,
*Projected live-letter region*, *Input-dependency extraction*) already present in
`docs/GLOSSARY.md`, so `/developer` never had to stop and interview anyone; an
evaluable green checkpoint; and no stop-list.

## What landed

`developer` and `test-writer` ran **in parallel**, each in its own worktree, and
the launcher merged both branches and integrated.

| file | change |
|---|---|
| `include/ltlf_ek/dependency_types.hpp` | new — `UnsatisfiableFormula`, `CandidateObserver`, lifted verbatim out of `dependent_outputs.hpp` |
| `include/ltlf_ek/detail/dependency_core.hpp`, `src/detail/dependency_core.cpp` | new — `detail::run_dependency_analysis`, the direction-neutral extraction (liveness BFS, live-letter regions, greedy loop, totalisation, unsatisfiable check), role-parameterised per the PRD's four-axis table |
| `include/ltlf_ek/dependent_inputs.hpp`, `src/dependent_inputs.cpp` | new — `DependentInputs` / `dependent_inputs`, a thin delegation with `Role::t_in` |
| `src/dependent_outputs.cpp`, `include/ltlf_ek/dependent_outputs.hpp` | reduced to a delegation; **public surface unchanged** |
| `tests/dependent_inputs_test.cpp` | new, 401 lines — U1-in–U6-in, O5-in, and six library-level edge cases |
| `CMakeLists.txt` | registered the two new sources and the new test |

## Green checkpoint

- Build: **OK** (Debug, Spot 2.15.1).
- `ctest`: **556 / 556 passed, 0 failed** (543 before the merge; +13 new). One
  test correctly skipped as `Disabled` (the soak).
- **The extraction's regression bar is met**: `tests/dependent_outputs_test.cpp`
  and `tests/ltlf_ek_deps_test.cpp` pass **unedited** — the diff does not touch
  either file. Under the PRD that is the proof the public surface did not move.
- Repair rounds used: **0**.

## Gates

| gate | state | evidence |
|---|---|---|
| glossary | already `[x]` | closed 2026-08-03, user-attended, before this run |
| tests | **left open** | Phase 1's fixtures are written and green, but O1-in / O3-in / O4-in are Phase 2 work; an unrun pass is never a ticked gate |
| code-review | **left open** | see below — the generic half could not run |
| theory-review | **ticked** | `/theory-review` faithfulness mode, no `code-bug` |

### Domain review (`/code-reviewer`) — clean

No must-fix. Glossary spellings match the C++ column exactly
(`DependentInputs::dependent`, `dependent_inputs`, `t_in`,
`ltlf_to_dfa(spot::formula::Not(phi), dict)`); no banned synonym appears in a
public identifier; BDDs are built from the shared `spot::bdd_dict` throughout,
with every universe AP pre-registered once and deterministically; no Spot
machinery reinvented. Three "consider" items were recorded in the PRD's new
*Developer comments / PRD disagreements* section and **not acted on**.

### Theory review (`/theory-review`) — clean, no `code-bug`

I1–I11 each confirmed against code with `file:line` anchors: the analysed
automaton is `Not(phi)` translated (never an acceptance flip); the projection is
`bdd_exist` over the **full** output cube, so `O=∅` is a no-op automatically; the
liveness BFS is reflexive, skips `bddfalse` guards, and does not use
`purge_dead_states`; emission is over the complete automaton with the fixed
all-negative default cube; the greedy loop uses the **accumulated** `Xdep`.
`dependent_outputs` is behaviourally unchanged.

The reviewer also generated independent evidence: a brute-force `def:indep`
oracle (a direct LTLf trace evaluator with no automaton in the loop) confirmed
subset-maximality on 12 hand-picked φ, and an in-library O1-in equivalent over
812 formulas found **0 realizability mismatches** between plain synthesis and
synthesis with the derived `T_in`.

## Findings deferred — none acted on

1. **`underspecified` (`lem:indep-transducer`)** — the sketch's step "any
   environment deviation enters a state from which no continuation violates φ"
   *silently requires* the ∃-projection; under ∀ the step is false. So I3's
   quantifier is forced by the proof obligation, not by modelling taste, and the
   paper never says so.
2. **`underspecified` (`lem:indep-diagonal`)** — the diagonal collapse is now
   checked directly against `def:indep` rather than only asserted to be
   "structurally the same statement as `lem:outdep-diagonal`"; the paper should
   record that.
3. **`doc-bug`** — I2's and the glossary's "an acceptance flip is an untested
   equivalence" rationale is stale: the reviewer tested the flip (17 formulas ×
   all traces to length 4, including the empty/length-0 cases) and found it
   holds. The conclusion stands, the justification should be reworded as a design
   choice. **Glossary wording is user-owned; no edit made.**
4. **CLI stderr text changed** in the output direction — `run_dependency_analysis:`
   where it read `dependent_outputs:`. Not covered by any test; a user-facing
   wording decision.
5. **An open theory question got answered.** I10's "does λ_in stay valid when
   `T_out` is later replaced?" is **yes**, and for a stronger reason than the PRD
   claims: a deviation from λ_in lands where *every* O-completion is dead; dead ⇒
   (by reflexivity) non-accepting ⇒ the trace already satisfies φ, so no `T_out`
   can block the win. Since λ_in is derived from `A¬` alone it is valid for every
   `T_out` simultaneously. Worth folding into the PRD/`main.tex` deliberately
   rather than by an unattended edit.

**Where the `\cl` notes went.** `latex/` is an uninitialized submodule in a
worktree, so per `CLAUDE.md` items 1 and 2 were drafted into `docs/BACKLOG.md`
under a dated heading, as ready-to-paste `\cl[inline]{…}` blocks. **`main.tex`
was not edited and the submodule was not touched**, so no `main.tex:NNN`
citation drift was introduced by this run.

## Questions for the evening grill

1. **Should the two `\cl` notes now go into `main.tex`?** They are drafted and
   ready in `docs/BACKLOG.md` but need a non-worktree session (or a submodule
   init) to land, plus `scripts/check-main-tex-refs.py --fix` in the same commit.
2. **Reword I2 and the *Violation automaton* glossary entry?** (Deferred finding
   3.) The acceptance-flip equivalence is now empirically established, so the
   current text overstates the hazard. Naming and glossary prose are yours.
3. **Restore the `dependent_outputs:` prefix on CLI errors?** (Deferred finding
   4.) Trivial to do; it is a question about user-facing text, not about code.
4. **Record finding 5 in the PRD's *Open theory questions*, or close that bullet
   outright?** The argument looks complete, but promoting an agent's argument to
   a settled claim is your call.
5. **The measured non-empty-`Xdep` rate.** The PRD's DoD asks for it from O1-in
   in Phase 2. The theory reviewer's in-library probe measured **20/400 and
   13/400 (~4–5%)** across two partitions, which brackets the output tool's
   12/150 floor. Phase 2 should confirm this through the real CLI harness rather
   than inheriting the number.

## Budget

Phases run: **1** (as instructed). Agents spawned: 3 (`developer`,
`test-writer`, `theory-reviewer`). Repair rounds: **0**. Review fix rounds:
**0**. No context compaction. Well inside the 13:02 deadline.

## What remains

**Phase 2 — the `--direction in|out` CLI flag**, plus its oracles O1-in, O3-in
and O4-in. Its launch gate is clean for the same reasons Phase 1's was: the flag
table is frozen, no new glossary term is introduced, and the green checkpoint is
stated. A fresh session can pick it up directly from this branch.
