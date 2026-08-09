# Day-run 2026-08-09 — acceptance mark on edgeless states

**Verdict: landed, green, both reviews clean.** One phase (the PRD has no phase
split — the whole PRD is one phase). Branch `acceptance-mark-edgeless`.

PRD: [`docs/prd/acceptance-mark-on-edgeless-states.md`](../prd/acceptance-mark-on-edgeless-states.md),
grilled 2026-08-08. Backlog item **#1**.

## The headline

**O5 fired exactly as predicted.** `ltlf-ek-synth` → **UNREALIZABLE**,
`ltlfsynt` on $\psi_{in}\rightarrow\varphi$ → **REALIZABLE**, on
$\varphi = X[!]\mathtt{tt}$ with a $\delta$-dead $\Tin$ — with
`run_faithfulness_guard` **passing** on the $(\Tin,\psi_{in})$ pair, so this is
not the 2026-07-05 mis-encoding trap again.

This is the project's **first known divergence witness** for the
equirealizability conjecture. `docs/BACKLOG.md`'s "Prove the monolithic
reduction" entry previously recorded that none existed; it now records this one,
and the sentence it asked to be updated has been updated. `/theory-review`
independently assessed the witness as **genuine, not an artifact** (see below).

It ships as an `IMPORTANT`-headed test that **pins a known boundary, not correct
behaviour**. A later reader must not "fix" it.

## What landed

| Commit | What |
|---|---|
| `d813834` | `detail::ensure_acceptance_readable` + all four call sites |
| `7bf0fe8` | O1–O5 oracles; the three PRD-pinned test flips |
| `2bbf4fe` | tests gate ref |
| `0e2f7e2` | glossary + theory-review gates, retired exception, backlog |

New: `include/ltlf_ek/detail/acceptance.hpp`, `src/detail/acceptance.cpp`,
`tests/acceptance_test.cpp` (O4, 5 tests), `tests/acceptance_partiality_matrix_test.cpp`
(O1/O2, the {δ-dead, λ-undefined} × {$\Tin$, $\Tout$} matrix across all five
`Synthesis` implementations).
Changed: `src/product.cpp`, `src/emits_dfa.cpp`, `src/nfa_to_dfa.cpp`,
`src/reverse_dfa_to_nfa.cpp`, `CMakeLists.txt`, and the three knowingly-changed
tests. No `Synthesis` signature moved.

`docs/prd/mtdfa-product.md`'s `emits_dfa` empty-word exception is **retired**:
it was never a language fact, only the mark being lost in transit.

## Gates

- **glossary** ✅ — no new term, as the PRD settled. One pre-existing `doc-bug`
  fixed instead: `docs/GLOSSARY.md`'s *Output-agreement automaton* entry still
  claimed a rejecting sink and completeness, contradicted by its own next
  sentence since the 2026-07-15 sink drop.
- **tests** ✅ — `ctest` **582/582**, 0 failed. O1–O5 all present.
- **theory-review** ✅ — faithfulness mode, **no `code-bug`**. All four sites
  faithful to `alg:dfa_product:final` (`acc` is read from
  `goal->state_is_accepting`, never from $\delta_{Dprod}$) and to
  `\cref{def:consistency}`'s partiality clause (δ-dead and λ-undefined reach an
  identical zero-out-edge state at every site, so the code does not distinguish
  them — as the grill required).
- **code-review** — domain half (`/code-reviewer`) ran and is clean. Generic
  half via `/review <PR#>`; see the PR.

### Stop-list: nothing fired

1. O5 matched the prediction. 2. The faithfulness guard **passed**. 3. No test
outside the PRD's named three changed verdict. 4. The `nfa_to_dfa` /
`reverse_dfa_to_nfa` adoptions changed no verdict.

## Findings deferred — not acted on

1. **`/code-reviewer` "consider": `src/reverse_dfa_to_nfa.cpp:38-44`.** The
   adoption there is not the same construction as the code it replaced, and its
   comment overstates the equivalence ("same final graph either way"). The old
   code added the `bddfalse` self-loop **unconditionally**; the new one adds it
   only when $s_0$ is edgeless. Language and mark-read are equivalent — Stop-list
   4's bar — but `purge_dead_states()` is a **Büchi liveness** purge and
   $F_N=\{s_0\}$, so the old unconditional `kFinal` self-loop put $s_0$ on an
   accepting cycle unconditionally. The refactor therefore narrows the safe input
   domain to *DFAs all of whose states are reachable from the initial state*
   (given which the risky case is vacuous, which is why nothing went red).
   Recorded in the PRD's *Developer comments* section.
2. **`doc-bug` (`main.tex` §`Transducers`, `~:117`) — the substantive one.**
   "Totalized in a way that does not impact its behaviour in allowed traces" is
   true of the *transducer* and **false of the synthesis verdict**: O5 is the
   counterexample. `def:consistency`'s partiality clause turns an undefined
   δ/λ into a **deleted letter**, which removes moves from the system as much as
   from the environment. `main.tex:544` (`lem:outdep-transducer`) already needed
   the same hypothesis locally; the global sentence is unqualified.
3. **`doc-bug` — a `\cl` note belongs on the equirealizability conjecture**
   recording the O5 witness.
4. **PRD open question #3** — `alg:dfa_product` is silent (not ambiguous) on
   edgeless states; a `\cl` note would stop the next reader rediscovering this.

**`latex/` was NOT touched.** It is an uninitialized submodule in a worktree, so
per `CLAUDE.md` all three `\cl` notes were **drafted** into `docs/BACKLOG.md`
*Later* → "Pending `\cl` notes on partiality", ready to paste, each with its
target location. Applying them from the main checkout will shift `main.tex:NNN`
citations — run `scripts/check-main-tex-refs.py --fix` in the **same commit**.

## Questions for the evening

1. **The big one: what does O5 do to the conjecture?** The reduction now needs
   either a **totality hypothesis** on $\Tin/\Tout$, or a ruling on runs that
   leave the transducer's domain. `/theory-review` was explicit that this bottoms
   out on the **open termination `\na` after `def:probDef`** — it is not
   independently decidable, and no unattended run may settle it. Both PRD open
   questions #1 and #2 were left open on purpose.
2. **Does the `ltlfsynt` oracle need a guard now?** It cross-checks the
   known-input half against exactly the reduction O5 just falsified on the
   partial fragment. Today it is safe only because the corpus has no partial
   $\Tin$ (PRD open question #4 keeps Case B out of scope for this reason). That
   is now a *load-bearing* restriction rather than an incidental one, and
   probably wants to be asserted somewhere rather than remembered.
3. **`reverse_dfa_to_nfa`** — state the reachability precondition in the header
   doc-comment, or restore the unconditional self-loop at that one site and let
   the helper cover the other three? Either way `:41-43` should stop asserting
   "same final graph".
4. **Apply the three drafted `\cl` notes?** They need the main checkout and an
   Overleaf round-trip; the run deliberately did not start one.

## Budget

- Phases run: **1** (the whole PRD).
- Repair rounds: **0**. Review rounds: **0** (both reviews clean first pass).
- Agents: `test-writer` ×1, `theory-reviewer` ×1. The `developer` agent was
  **not** spawned — see below.

### Salvage note

This run began by recovering, not redoing. The 08:50 wave-1 session was
terminated at its 600 s background-wait ceiling with both agents still running
and **nothing committed**; it left a complete developer diff uncommitted in
`.claude/worktrees/agent-a5281449fd00a9fb8`. That diff was applied to the phase
worktree and committed as `d813834` rather than regenerated.

It also left the phase worktree's `build/` configured **Release**. That is worth
knowing: under `-DNDEBUG` the `assert()` in `BenchScopeDeathTest.NestedBenchScope
InstallAsserts` is compiled out and the death test fails with "failed to die" —
a config artifact that looks exactly like a Stop-list 3 regression. Reconfiguring
to **Debug** (matching the main checkout) cleared it. The canonical configure is
`-DCMAKE_BUILD_TYPE=Debug -DSPOT_ROOT=~/opt/spot-2.15.1`.
