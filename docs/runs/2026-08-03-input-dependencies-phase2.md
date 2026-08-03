# Run report — 2026-08-03, input-dependency extraction, Phase 2

Unattended day-run, wave 2 slot 1 (`/launcher`), resuming after wave 1 slot 1
hit the session limit. One phase, as instructed.

**PRD:** `docs/prd/input-dependencies-tool.md` — backlog **#1**, picked by Step 0
rule 2.
**Phase:** 2 — the `--direction in|out` CLI flag and the semantic oracles O1-in,
O3-in, O4-in. **This was the PRD's last implementation phase.**
**Workflow:** concurrent, as the PRD prescribes.
**Feature branch:** `input-dependencies-tool`. **Phase branch:**
`worktree-indeps-phase2`, based on the feature branch (not `master`).

## Orientation note — Phase 1 *did* land

`build/runs/last-status` was absent and today's wave-1 log contains only
`You've hit your session limit`. That is misleading: wave 1 landed Phase 1 in
full (5 commits, 556/556 green, draft PR #6) at 08:32 and died before writing
the status line. Phase 1 was **not** redone.

## Launch gate — clean

All five checks passed before any code was written: `Status: in progress` with
Phase 2 outstanding; *Interfaces & types → Phase 2 — the CLI flag* frozen, no
placeholder; **no new domain identifier** introduced by Phase 2 (`--direction`
is CLI plumbing; the glossary gate closed user-attended on 2026-08-03), so
`/developer` never had to stop and interview anyone; a stated, evaluable green
checkpoint; no stop-list.

## What landed

`developer` and `test-writer` ran **in parallel**, each in its own worktree; the
launcher merged both and integrated.

| file | change |
|---|---|
| `src/ltlf_ek_deps.cpp` | `--direction in\|out` (**default `out`**), validated in `ParseArgs`; `main()` dispatches to `dependent_inputs`/`dependent_outputs` behind direction-neutral locals so the guards, `CommitArtifacts` and the keystone ordering are untouched logic; `RemoveStaleTransducer` gained a `noun`; stdout noun follows the direction; exit-3 unchanged and still keyed on the `UnsatisfiableFormula` **type**, never a message substring |
| `tests/ltlf_ek_deps_test.cpp` | +52 — a `LtlfEkDepsDirectionFlag` group: default-is-`out` byte-identical to no flag, invalid value ⇒ exit 2, the `in` noun line, empty-$\Xdep$ ⇒ no transducer file |
| `tests/ltlf_ek_deps_input_test.cpp` | new, 656 lines — **O1-in** (corpus equirealizability vs baseline **and** `ltlfsynt`, plus the I6 totality witness) and **O3-in** (part-file pass-through, the two refusals, I12 commutation) |
| `tests/dependent_inputs_controller_test.cpp` | new, 332 lines — **O4-in**: four hand fixtures, an I6 companion, and a fixed-seed 6000-case generated-corpus sweep |
| `CMakeLists.txt` | the two new test files registered; **no new target** — the flag needed no build change |
| `docs/prd/input-dependencies-tool.md` | status, `tests` gate closed with evidence, the measured rate recorded, three new "consider" entries |

Production C++ changed in exactly one file, and only in its CLI orchestration
layer — `detail::dependency_core` and both library entry points are untouched by
this phase.

## Green checkpoint

- Build: **OK** (Debug, Spot 2.15.1).
- `ctest`: **572 / 572 passed, 0 failed** (556 after Phase 1).
- The extraction's regression bar still holds: `tests/dependent_outputs_test.cpp`
  and `tests/dependent_inputs_test.cpp` are untouched, and
  `tests/ltlf_ek_deps_test.cpp` only gained a new group — no pre-existing case
  was edited.
- **Repair rounds used: 1 of 2.** Integration surfaced 2 failures, both in
  oracle fixtures that had been blocked before the flag existed and were running
  for the first time. Both were **test bugs, not code bugs**, and both were fixed
  without weakening an assertion:
  - `I12DirectionsCommute` threw `parse_transducer: missing HOA --END--
    terminator` because it parsed a $\Tin$ file the contract says is never
    written when $\Xdep=\emptyset$. Fixed by guarding each direction's parse on
    its own known-set and asserting both orders agree on presence/absence.
  - `I6TotalityWitnessAgreesAcrossAllThreeAndIsRealizable`: all three oracles
    **agreed** — the oracle held — and all three said *unrealizable*; the witness
    $F(a \oplus b)$ is simply not realizable. Swapped to U3-in's shape. The
    realizability half of the assertion was kept; relaxing it to "all three
    agree" would have deleted the entire I6 content.

## Gates

| gate | state | evidence |
|---|---|---|
| glossary | already `[x]` | closed 2026-08-03, user-attended |
| tests | **ticked** | O1-in/O3-in/O4-in written and executing; 572/572; regression bar met |
| code-review | **left open** | domain half clean (below); the generic half is `/review` on the PR — see *Deviation* |
| theory-review | already `[x]` | closed in Phase 1; this phase changed no semantic code, so no re-run was owed |

### Domain review (`/code-reviewer`) — clean, no must-fix

No new public identifier names a domain concept (`direction`, `is_in`, `noun`
are anonymous-namespace CLI locals). Both transducer parses in the commutation
test share **one** `spot::bdd_dict`, so the BDD equality being asserted is
meaningful. No Spot machinery reinvented. `--direction out` is byte-identical to
the pre-flag invocation by construction, not by assertion. One "consider",
recorded in the PRD and below.

**`theory-reviewer` was deliberately not spawned.** The diff is CLI
orchestration plus tests; `detail::dependency_core` and both library entries are
unchanged, and Phase 1's faithfulness pass already covered them. `latex/` was
never touched, so **no `main.tex:NNN` citation drift** was introduced.

## Findings deferred — none acted on

1. **I12's commutation oracle covers $\Tout$ but not $\Tin$.** Its fixture has
   $\Xdep^{out}=\{x\}$ but $\Xdep^{in}=\emptyset$, so the $\Tin$ comparison is
   guarded away. Probing the built CLI showed the obstruction is **structural**:
   $F(\lnot b \lor (c \oplus y))$ alone gives $\Xdep^{in}=\{b\}$, and conjoining
   any out-dependent constraint drives it to $\emptyset$ — the input side
   analyses $L(\lnot\varphi)$, and $\lnot(A \land B)=\lnot A \lor \lnot B$ opens
   live escape routes so no input stays forced. Exactly what I2 specifies, but it
   means one $\varphi$ non-empty in **both** directions may not exist by
   conjunction at all. (PRD disagreement 5.)
2. **The measured $\Xdep\neq\emptyset$ rate clears its floor by one case:**
   **7/150 = 4.7%**, versus the output tool's 12/150. The floor is integer
   `150/20 = 7`, so O1-in passes exactly at the boundary. Recorded honestly
   rather than tuned. (PRD disagreement 6, and the DoD's required number.)
3. **The realizable-**and**-input-dependent trap cost two fixtures.**
   $F(a \oplus b)$ was picked twice as an I6 witness and is unrealizable (no
   output occurs in it). The PRD's oracle section should say that an I6 witness
   must satisfy both conditions. (PRD disagreement 4.)
4. Phase 1's three "consider" entries and its five deferred findings remain
   open — including the two `\cl` notes still parked in `docs/BACKLOG.md`
   awaiting a non-worktree session.

## Deviation from the skill, stated plainly

Step 6a prescribes `/review <PR#>` for the generic half of the `code-review`
gate. See the *Budget* section for whether it ran; if it did not, the gate is
left **open** and unticked — an unrun pass is never a ticked gate.

## Questions for the evening grill

1. **I12: hunt for a both-directions-non-empty witness, or accept split
   coverage and write that into the oracle section?** Finding 1 argues the
   conjunctive route is closed; whether another route exists is a theory
   question, and the fixture is only worth the hunt if the answer is yes.
2. **Is 4.7% an acceptable floor, or is O1-in's corpus filter too narrow?** The
   input side is measurably thinner than the output side's 8%. Retuning the floor
   to make the margin comfortable would be exactly the kind of test-weakening
   this run is forbidden from doing unattended.
3. **Phase 1's still-open questions carry forward unchanged:** land the two `\cl`
   notes into `main.tex`; reword I2 and the *Violation automaton* glossary entry
   now that the acceptance flip is empirically established; restore the
   `dependent_outputs:` stderr prefix or accept `run_dependency_analysis:`;
   promote (or not) the theory reviewer's I10 argument to a settled claim.
4. **This PRD is now feature-complete.** Its DoD is met apart from the
   `code-review` gate. What is next in the backlog is an open question the
   backlog itself does not answer: the two runners-up (Method 3.2, Method 3.1
   Phase 2) are both marked as needing their own grill before they can start.

## Budget

Phases run: **1** (as instructed). Agents spawned: 3 (`developer`,
`test-writer`, a repair `developer`). Repair rounds: **1** of 2. Review fix
rounds: **0**. No context compaction. Inside the 18:02 deadline.
