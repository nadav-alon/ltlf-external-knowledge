# Run report — `ltlfsynt` known-output ($\Tout$) oracle, **Phase 2 (the guard)**

**Date:** 2026-08-02 · **Mode:** unattended day-run, wave 2 slot 1 (resume)
**PRD:** `docs/prd/ltlfsynt-oracle-known-output.md`
**Feature branch:** `worktree-prd-tout-oracle` · **Phase branch:** `tout-oracle-phase2`
**Worktree:** `.claude/worktrees/tout-phase1` (pre-existing, already configured;
the directory name is a leftover from the aborted wave-1 slot 2 — the branch it
holds is `tout-oracle-phase2`)

This is the **last implementation phase** of this PRD. What remains is one
`/glossary` pass and a `docs/BACKLOG.md` move, both of which need the user.

## What landed

`7ee38ae` — **test-writer: Phase 2 — the Role-generic faithfulness guard**,
`tests/ltlfsynt_oracle_test.cpp` only, +206/−70. Test-only: **no production
C++, no CMake change**.

1. `run_faithfulness_guard` gained a trailing **`Role role`** parameter,
   deliberately **not defaulted**, and now derives its observed/produced slices
   from `sigma_slices(partition, role)` instead of hard-coding
   $(\Ifree, \Iknown)$. `GuardResult` is unchanged.
2. Internals renamed for role-genericity, behaviour unchanged:
   `ifree_sequences` → `sigma0_sequences`, `ifree_sequences_of_length` →
   `sigma0_sequences_of_length`, `single_bit_iknown_mutations` →
   `single_bit_sigma1_mutations`.
3. Both existing $\Tin$ call sites now pass `Role::t_in` **explicitly**. Tables
   A–E unchanged and green.
4. New `ToutCorpus/KnownOutputFaithfulnessGuardTest` — **7 fixtures**, one per
   distinct $(\Tout, \psiout)$ pair across Tables F, G, H, I, J and mixed
   M1, M2 (the guard never looks at $\varphi$, so it is one fixture per table,
   not per row). All pass.
5. New `FaithfulnessGuardMetaOracle.FiresOnOverStrongPsiOutDelayPairingTableJBad`
   — the Table J-bad negative control. Asserts the guard **fires** *and* that
   `result.detail` says **"too STRONG"**.

## Gates

| Gate | State | Evidence |
| --- | --- | --- |
| `tests` | **ticked** | `ctest` **542/542, 0 failed** (up from 534 at Phase 1). Both new suites verified individually: `ToutCorpus` ×7, `FaithfulnessGuardMetaOracle` ×2, `TinCorpus` ×4 — 13/13. |
| `theory-review` | **ticked** | theory-reviewer, faithfulness mode, clean after one fix. |
| `code-review` | **left open** | `/code-reviewer` (domain) clean after one fix round. `/code-review` (generic) **could not be invoked** — the harness blocks it with `disable-model-invocation`. A manual generic pass stood in; the generic half is **not** discharged. |
| `glossary` | **left open** | Needs `/glossary`, which interviews the user. See below. |

**Budget:** 1 phase, **0** build/test repair rounds, **1** review fix round.

## Review findings

**Theory review — semantics clean.** Explicitly confirmed:

- **Mutation soundness carries over to `Role::t_out`.** $\Sigma_0 \cap \Sigma_1 =
  \emptyset$, the prefix is untouched and $\delta_{out}$ is deterministic, so the
  flipped $\Oknown$ bit contradicts $\lambda_{out}(q_p, \cdot)$ and the mutant is
  genuinely outside $L(\tau_{out})$. That $\Sigma_0$ now contains
  *system-controlled* $\Ofree$ is irrelevant — agreement (`main.tex:93`) is a
  per-trace predicate, not a strategy composition.
- The $\Sigma_0/\Sigma_1$ split matches `main.tex:83/93/127`; `src/role.cpp:16`
  correctly puts **all** of $\mathcal{I}$ (not just $\Ifree$) in $\Sigma_0$,
  which is what makes the mixed fixtures legal.
- Under `t_out` the letter `sigma0 & lambda(q, sigma0)` spans all of
  $\mathcal{I}\cup\mathcal{O}$, so $\psiout$ mentioning an $\Ofree$ AP (Tables G,
  M1: `G(x <-> o)`) is handled **structurally** rather than by corpus accident —
  the $\Tin$ side's "no fixture mentions an $\Ofree$ AP" side condition is
  discharged, not inherited.
- "too STRONG" is the theoretically correct J-bad verdict: $L(\psi)=\emptyset
  \subsetneq L(\tau_{out})$.

**One `code-bug`, fixed in-phase.** The J-bad meta-oracle originally asserted
only `EXPECT_FALSE(result.ok)` while its comment promised "too STRONG" — it
would have passed had the guard fired for the *wrong* reason. Fixed by asserting
`result.detail` contains `"too STRONG"`. Rebuilt, re-ran: 542/542.

No `\cl` note was warranted, so `latex/` is **not** dirty and no `main.tex:NNN`
ref drifted.

## Findings deferred (nothing acted on)

1. **The $\Tin$ meta-oracle has the same latent gap** just fixed on the $\Tout$
   side: `FiresOnOldCopyFromStepOnePsiInDelayPairing` never inspects
   `result.detail`; its correct expectation is **"too WEAK"**. One line. Left
   alone as outside Phase 2's scope.
2. **The empty-$\Ofree$ smoke fixture carries no guard.**
   `EmptyOfreeWithNonEmptyOknownForcesXAndStaysUnrealizable` is a `TEST_F`, not a
   corpus row, so it is not among the 7 guarded pairs. Cheapest remaining
   widening of guard coverage.
3. **Phase 1's seven "consider" items are all still open** — Phase 2 touched
   none of them.
4. **The new $\Tout$ block header narrates PRD rationale** rather than what the
   code does; same trade-off, same counterargument, as Phase 1's item 6.
5. **Theory-review observation, not a finding.** For mixed M1/M2 the guard
   demands language equality at $\Iknown$ valuations $\Tin$ would never produce —
   a larger trace set than the reduction exercises. That is the stronger, safe
   reading and is what `main.tex:144` says, so it folds into the existing
   `underspecified` flag on that line.

## Questions for the evening grill

1. **Adopt the satisfiable-but-wrong J-bad analogue?** *(highest value)* Phase 1
   established that Table J-bad's $\psiout$ is **unsatisfiable outright**, so the
   meta-oracle proves only "the guard fires on an unsatisfiable formula" — a
   strictly weaker claim than the $\Tin$ analogue's "fires on a
   satisfiable-but-wrong language". The recommended addition,
   $(\lnot x) \land G(X(x \leftrightarrow a))$, was **deliberately not
   implemented**: it is a PRD change and yours to make. Until you do, the $\Tout$
   half of this oracle is provably weaker than the $\Tin$ half.
2. **`/glossary` — the last thing between this PRD and done.**
   `docs/GLOSSARY.md:1100` still spells the *Faithfulness guard* C++ column as
   `run_faithfulness_guard(transducer_src, psi_in, partition)`; it is now
   `(transducer_src, psi, partition, role)` and the concept spans both roles.
   The PRD pre-authorised this as a non-blocking wording update, which is why the
   phase landed with the gate open.
3. **Correct the PRD's enumeration claim.** *Interfaces & types* says the seed
   "matters only if a future fixture widens the partition" — the **mixed**
   fixtures already do. Alphabet 8, and the split is **per length**, so M1/M2
   exhaust lengths 1–4 ($8^4 = 4096$, hitting the cap exactly) and **sample**
   length 5 (4096 of 32768) under the fixed seed. Tables F–J are fully
   exhaustive as claimed. Deterministic either way, but the mixed guards are a
   sampled check, not a proof. Nothing was re-tuned to "fix" this.
4. **`MixedRow` still omits `{t_in_src, psi_in}`** (Phase 1's open item): amend
   the contract, or amend the code? Unchanged and still harmless.
5. **Two `underspecified` `main.tex` sites from Phase 1 remain unwritten** —
   `main.tex:101`'s termination `\na` (now load-bearing for this oracle) and
   `main.tex:144`'s commented-out ownership clause at `:141-142`. Both need
   `latex/` initialized and both are your call.
6. **Run `/code-review` by hand** on `master...worktree-prd-tout-oracle` — the
   agent-invocable path is blocked, so the generic gate cannot be closed
   unattended, in this PRD or any other.

## Not done, on purpose

- `docs/BACKLOG.md` item "#1 $\Tout$ oracle" **not** moved to Done — the PRD's
  definition of done also requires the `/glossary` pass, so the outcome line
  would be premature. Yours to move.
- The extra J-bad witness — see question 1.
- No PRD-tabulated corpus row was edited; no test weakened, skipped or deleted.
