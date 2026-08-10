# Run 2026-08-02 — `ltlfsynt` known-output ($\Tout$) oracle, Phase 1

Unattended day-run, wave 1, slot 1. One phase, as instructed.

**PRD:** `docs/prd/ltlfsynt-oracle-known-output.md` (grilled 2026-07-31)
**Feature branch:** `worktree-prd-tout-oracle`
**Phase branch:** `tout-oracle-phase1` → merged into the feature branch
**Phase:** 1 of 2 — "the corpus". Phase 2 (the guard) is **not** started.

## Why this PRD

`docs/BACKLOG.md` under *Now / next* ranks this **#1** and records it as
"GRILLED 2026-07-31 → draft, ready for `/test-writer`", with all 47 corpus rows
verified live against both binaries during the grill.

The freshest unmerged PRD branch was actually the *other* one,
`worktree-input-dependencies-prd` (16 seconds newer). It was **not** run, and
that is the main thing the evening should look at — see *Blocked* below.

## What landed

One commit, one file, test-only and purely additive. No production C++, no CMake
change.

- `9512239` — `tests/ltlfsynt_oracle_test.cpp`, +561 lines:
  - `KnownOutputRow` / `KnownOutputOracleTest` — Tables F–J, **35 rows**, part
    file `a` / `o` / `x`, five $\Tout$ HOA fixtures (`kToutConstFalse`,
    `kToutConstTrue`, `kToutCopyFromOfree`, `kToutCopyFromI`, `kToutDelay`),
    reduction `(phi) & (psi_out)` vs `--ins=a --outs=o,x`.
  - `KnownOutputDivergenceRow` / `KnownOutputDivergenceTest` — Table J-bad,
    **4 rows**, encoded as a *deliberate divergence*: the two verdicts are
    asserted separately, never for equality, so the table can never pass by
    agreeing.
  - `MixedRow` / `MixedOracleTest` — Tables M1–M2, **12 rows**, part file
    `a,k,o,x`, reusing `kTinCopy` / `G(k <-> a)` verbatim from Table C,
    composed reduction `psi_in -> (phi & psi_out)`.
  - One empty-$\Ofree$ smoke fixture, correct by construction
    (`(!x) & G(x)` is patently UNSAT) rather than table-derived.
  - Two AP-naming guards: `KnownOutputCorpusApsMatchPartFile`,
    `MixedCorpusApsMatchPartFile`.

Plus, in the landing commit: this report and the PRD's gate annotations and
*Developer comments* section.

## Gates

Nothing ticked. Each gate is annotated with what Phase 1 discharged, because the
PRD's definition of done spans both phases.

| Gate | Phase 1 evidence |
| --- | --- |
| glossary | not needed — the PRD states there are no new terms and no gaps. |
| tests | 51 new rows **live-executed** (no `GTEST_SKIP` fired), `ctest` **534/534 passed, 0 failed**; Tables A–E and every other suite unchanged and green. |
| code-review | `/code-reviewer` ran **clean, no must-fix**. `/code-review` **could not be invoked** — the harness rejects it with `disable-model-invocation` — so a manual generic-correctness pass stood in. The generic half is **not** discharged. |
| theory-review | theory-reviewer ran (self-spawned by the domain pass): **clean, no `code-bug`**, no wrong test expectation. Two `underspecified` findings, below. |

Verification detail worth keeping: the corpus does not pass vacuously. The
verdict parsers `ADD_FAILURE` on an unexpected exit code or a missing verdict
word, so a broken binary fails loudly instead of reading as UNREALIZABLE; and
row `F_F_x` would read REALIZABLE if `--known-output-transducer` were ignored.
Every $\delta$ is total and every $\lambda$ is defined and functional — the
failure mode *Behaviour* #4 warns about.

Zero divergence: all 51 rows reproduced the PRD's tabulated verdicts exactly, in
both the `verdict` and `bare`/`LB?` columns.

## Findings deferred

Full text in the PRD's *Developer comments / PRD disagreements*. The three that
matter:

1. **Contract deviation (flagged, not absorbed).** *Interfaces & types* specifies
   a mixed row carrying `{t_in_src, psi_in}`; the landed `MixedRow` omits both
   and hardcodes the single shared $\Tin$. Harmless until a second mixed $\Tin$
   appears. Amend the contract or the code — user's call.
2. **J-bad's over-strong $\psiout$ is unsatisfiable outright**, not just at trace
   end (independently verified: UNREALIZABLE even with every AP
   system-controlled). The reduction is therefore U for *every* $\varphi$, the
   two "agrees (both U)" rows agree coincidentally, and **Phase 2's meta-oracle
   would prove the weak claim** — "the guard fires on an unsatisfiable formula" —
   where the $\Tin$ analogue proves "fires on a satisfiable-but-wrong language".
   The rows are PRD-pinned and reproduce; they were **not** edited.
3. Seven "consider" items (bare-command coverage for non-flip rows, AP guards
   that don't parse the part file, no *Corpus guarantees* meta-assertion, a
   drifting line-number citation, duplicate row types, comment hygiene, and two
   coverage gaps). None acted on.

`main.tex` was **not** touched and no submodule moved — in a worktree `latex/` is
an uninitialized submodule, so the drafted `\cl` note was deliberately not
written. No citation drift from this run.

## Questions for the evening grill

1. **Adopt the stronger Phase 2 negative control?** Recommendation from theory
   review: additionally guard $(\lnot x) \land G(X(x \leftrightarrow a))$
   (copy-from-step-1) — satisfiable but wrong, the exact mirror of the retired
   $\Tin$ witness. This is a PRD change and it is the difference between a
   meaningful and a near-vacuous meta-oracle. **Answer this before Phase 2 runs.**
2. **`MixedRow`'s missing `{t_in_src, psi_in}`** — amend the frozen interface, or
   make the code match it?
3. **`main.tex:144`, the live conjecture** — the fixing clause that names who owns
   $\Iknown$/$\Oknown$ in the reduced problem already exists **commented out** at
   `main.tex:145-146`. Activating it is yours. It is now load-bearing for this
   oracle's correctness argument.
4. **`main.tex:101`, the trace-termination `\na`** — Table J is the first corpus
   evidence that this note decides real verdicts (system-controlled termination
   gives R; the literal all-prefixes reading gives U). Worth promoting from `\na`
   to settled text.
5. **`/code-review` is not invocable by an agent** (`disable-model-invocation`).
   Every future launcher run will hit this. Either accept the manual pass as the
   generic half, or drop the generic half from the gate wording.

## Blocked — the other grilled PRD

`docs/prd/input-dependencies-tool.md` on `worktree-input-dependencies-prd` is
grilled and otherwise launchable, but **fails launch gate 3**: it introduces
three new domain concepts (*Dependent input set* $\Xdep$, *Violation automaton*
$\Aneg$, *Projected live-letter region* $\liveproj{s}$) plus three amendments to
existing entries, none of which are in `docs/GLOSSARY.md`. The PRD says so itself:
"**Glossary gaps — run `/glossary` before `/developer`**". `/glossary` interviews
the user, so naming is not something an unattended run may decide.

**One `/glossary` session unblocks a full PRD.** That is the highest-value thing
the evening can do.

## Budget

- Phases run: 1 (as instructed).
- Repair rounds: 0 — green on the first build/test.
- Review rounds: 0 — no must-fix findings.
- Agents: `test-writer` (implementation), one review agent (both passes).
