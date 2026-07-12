---
name: grill-debug
description: Interactive human-in-the-loop root-cause debugging for this LTLf-external-knowledge project. Point it at a failing test or a natural-language symptom (optionally a suspected location); it reproduces, bisects the flow with invariant assertions, triangulates with the repo's oracles, and pins the first broken stage + line + WHY — then hands the fix off. Use when a test regressed, a realizability verdict / output looks wrong, or something crashes or hangs and you want the cause localized.
---

# grill-debug (interactive root-cause hunt)

Localize a bug to its **first broken stage and line, with the mechanism-level
WHY**, working *with* the user in a tight empirical loop. This is the debugging
counterpart to `/grill-me`, but it is **practical, not pedagogical** — be
**direct and efficient**; do not withhold hypotheses to "teach". Runs **inline on
the main (Opus) session** because it is interactive — never delegate it to an
agent.

Invocation: `grill-debug <symptom or failing cases> [suspected locations]`.

## Scope — what this skill owns, and where it stops

Owns **reproduce → localize → root-cause**. Then it **hands off**:

- the **fix** → the user, or `/developer`;
- **verification** → `ctest`, the oracles, `/code-reviewer`.

Do **not** edit product code to *fix* the bug. The only source edits you make are
*temporary instrumentation*, and they are always reverted (see Cleanup).

## The two entry shapes

1. **A failing test / known case** — run it, confirm the symptom, then localize.
2. **A natural-language symptom with no test yet** ("`G o` looks unrealizable",
   "the CLI hangs on this transducer", "this output smells wrong"). **Build a
   minimal reproduction and confirm it exhibits the symptom *before* localizing**
   — never chase a phantom. Prefer a repro that needs **no source edit**: the CLI
   (`./build/ltlf-ek-synth …`) or a scratchpad script. Only add a throwaway gtest
   if nothing else reproduces it, and keep repros **off the repo tree**
   (scratchpad) so teardown stays clean.

## The method (backbone — always these, in order)

The steps are fixed; the flow is **discovered per bug** (read the actual path
fresh, don't assume the synthesis spine).

0. **Reproduce** (or build a repro). Confirm with the user that what you observe
   matches their symptom.
1. **Get the ground truth.** Ask what the output *should* be. The failing test is
   **not** the oracle of correctness — the test's *expectation* may be the bug.
   Classify early: **code-wrong** vs **expectation-wrong** vs **underspecified**
   (cf. `/theory-review`'s trichotomy).
2. **Reason from the symptom direction.** A boolean verdict flip is the strongest
   clue: **realizable → unrealizable (true→false) = something was *removed* /
   over-constrained**; **unrealizable → realizable (false→true) = something was
   *added* / under-constrained**. For non-boolean symptoms (wrong output, crash,
   hang) ask the analogous "what changed, in which direction". This tells you
   whether to hunt code that *narrows* the data or code that *widens* it.
3. **Narrow to the stages that TRANSFORM the suspect data.** Skip code that only
   passes it through. (E.g. a lost-winning-path flip can only come from the hops
   that *build / regroup / project* edges.)
4. **Map & bisect the flow.** Lay the actual pipeline out as a chain of stage
   boundaries, then binary-search it: assert the expected invariant at a boundary
   and find the **first** boundary where it fails — that stage is the culprit.
5. **Design the assertion to catch THIS bug.** Pick an invariant the bug *cannot*
   satisfy — **coverage, not count**. (Canonical trap: an overwrite-vs-OR bug
   preserves edge **count** but shrinks guard **coverage** — assert
   `⋃ out-guards == ⋃ enabled letters`, not `#edges`.) `bdd` defaulting to
   `bddfalse` is the paired idiom to check around.
6. **Triangulate with the repo's independent oracles** — the killer advantage no
   generic debugger has:
   - **`ltlfsynt` differential** (external, `tests/ltlfsynt_oracle_test.cpp`): if
     `ltlfsynt` disagrees with `ek-synth`, the bug is in **our construction**, not
     the user's expectation — settles code-wrong vs expectation-wrong.
   - **`verify_controller`** (independent of `solve_dfa`): a controller it accepts
     that `synthesize` couldn't find localizes the bug to
     `synthesize`/`solve_dfa`/materialize, not the semantics.
   - **metamorphic round-trip** (`synthesize` → `verify_controller`).
   A *cluster* of these failing together is itself the diagnosis: one shared-
   construction bug, not many independent test bugs.
7. **If it's a regression, let git localize it.** `git bisect run ctest …`
   binary-searches commits to the one that introduced it — the diff is the suspect
   list.
8. **Code-smell pass** at the pinned stage: `=` where `|=`/accumulate was meant,
   off-by-one, BDD value-cube vs variable-cube mix-ups, `std::optional` partiality
   holes, `bdd_dict` / bdd lifetime issues.

## The loop (self-instrument, checkpoint at judgment)

You do the mechanical grind: write the assertion/print, build, run
`ctest`/oracles, read the output. **Pause only for the user's judgment**:

- "does this repro match your symptom?"
- "what *should* this be?" (ground truth)
- at a **fork** in the flow: "which branch do we bisect first?"

Speak the **ubiquitous language** (`docs/GLOSSARY.md`) and anchor every claim to a
`file:line`.

## Cleanup — leave zero instrumentation (hard protocol)

Instrumentation lives on a **throwaway debug branch**; the user's tree is restored
*exactly*, even if they started dirty.

**Setup (before any edit):**
1. `git worktree list` — if other worktrees exist, **never `git add -A`** (it can
   corrupt a sibling worktree's index); stage explicit paths instead.
2. Snapshot `git status --porcelain`; record branch `B` and whether the changes
   were **staged** vs unstaged/untracked.
3. If dirty, **WIP-commit** the current state (worktree-safe staging) so the
   user's in-progress work is captured.
4. `git checkout -b debug/grill-<slug>-<ts>`.

**While debugging:** mark every probe `// DEBUG:grill-debug`. Commit debug state
freely (it's throwaway). Keep repros off the repo tree (CLI / scratchpad).

**Teardown (always, even on abort/error):**
5. Commit or discard the debug edits, then `git checkout B`; `git branch -D
   debug/grill-<slug>-<ts>`.
6. If a WIP commit was made: `git reset --mixed HEAD~1` (or `--soft` if the
   snapshot was staged) → working tree back to the **exact** pre-session state.
7. **Show the user `git status` / `git diff`**; confirm no `DEBUG:grill-debug`
   remains and the state matches the opening snapshot.

**Never** leave the user on the debug branch. **Never** `git add`/commit the fix.

## The deliverable + routing

When the root cause is pinned, print a tight card:

```
── ROOT CAUSE ──
symptom : <the flip / wrong output / crash>
stage   : <first broken boundary>
line    : <file:line>
why     : <the mechanism, in glossary terms>
fix     : <proposed change, in words — handed off>
```

Ephemeral by default (no file). Then **offer routes**: `→ /developer` (implement
the fix), `+ /backlog` (if it exposes a deeper/systemic issue), `+ memory` (if
it's a recurring gotcha, e.g. the `=`/`|=` accumulation idiom), or `none`.

**Ordering hazard (hard rule):** any *durable* artifact — a memory file, a
`/backlog` edit, a handoff doc — must be written **after teardown, on the real
working tree**, never on the doomed debug branch (it would die with the branch).
The card is just in-context text, so it crosses the branch boundary for free; if
some file genuinely must be produced pre-teardown, stash it and pop after the
`checkout B`.

## Definition of done

- Root cause pinned to a stage + `file:line` with a mechanism-level WHY — or an
  honest "narrowed to `<stage>`, couldn't pin further" plus what's left to check.
- Tree restored: `git status` shown clean of `DEBUG:grill-debug`, matching the
  opening snapshot; the user is back on their original branch.
- Fix **not** applied by this skill — handed off, with the chosen route taken (or
  declined) *after* teardown.
