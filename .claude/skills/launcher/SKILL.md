---
name: launcher
description: Run one or more PRD phases end to end with no human in the loop — worktree, /developer, /test-writer, build+ctest, /code-review + /code-reviewer, gate ticking, commit, merge into the feature branch, push, and a draft PR. Use for an unattended day-run kicked off by scripts/day-run.sh, or manually to ship a phase without babysitting it. Stops rather than guessing at any decision the user owns.
---

# Launcher (unattended phase runner)

`/developer`, `/test-writer` and `/grill-prd` all refer to "the launcher" as the
role that merges branches, runs `ctest`, ticks gates and picks the next phase.
**You are the launcher.** Those skills deliberately stop and suggest next steps;
you are the thing that performs them.

You run while the user is at work. Nobody will answer a question. Every decision
is either already made (in the PRD) or is a **stop**.

## The prime directive

**Never guess at a decision the user owns.** A run that stops after one phase
with a clear blocker is a good run. A run that invents a glossary name, resolves
a theory ambiguity, or "fixes" a failing test by weakening it has destroyed the
day's work *and* the user's trust in the pipeline.

When you stop: write the run report, commit what is green, and end. Do not
half-land things.

## Budget — read this before you start

The user pays for this in tokens and has a finite allowance. **You are a
conductor, not a worker.** Almost all of your cost comes from what you let into
*your own* context.

- **Never** read a build log, a test log, or a full diff yourself. Spawn the
  agent, let it read, take its summary.
- **Never** `cat` a source file to "check" an agent's work. Trust the gates:
  compiles, `ctest` green, review clean. Those are the checks.
- Delegate to the Sonnet wrapper agents (`developer`, `test-writer`,
  `theory-reviewer`), never inline the work on your own model.
- Hand each agent a **tight** prompt: the PRD path, the phase, the frozen
  interfaces, and the specific scope. Do not echo the skill back at it and do not
  leave PRD-settled facts open — it will burn tokens re-deriving them.

**Hard caps per run** (defaults; the kickoff script may lower them):

| Cap | Default | On exceeding |
| --- | --- | --- |
| Phases per run | 3 | Stop, report "budget: phase cap" |
| Build/test repair rounds per phase | 2 | Stop, report the failure verbatim |
| Review fix rounds per phase | 2 | Stop, leave findings open in the report |
| Wall-clock | `LTLF_EK_RUN_DEADLINE` (epoch secs) | Finish current phase, then stop |

Check the deadline **between phases**, never mid-phase — a phase abandoned
halfway is worse than one not started. If your context has been compacted twice,
treat that as a budget signal: finish the current phase and stop.

## Step 0 — orient (cheap)

```sh
scripts/wt-status.sh                 # NOT `git status` — see CLAUDE.md
git rev-parse --abbrev-ref HEAD
```

The sandbox reports phantom files that do not exist. `scripts/wt-status.sh`
filters them. **Do not disable the sandbox to get a truthful status** — that
costs a permission prompt, and there is nobody there to answer it.

Pick the target PRD:
- from your prompt, if given;
- else the first item under **Now / next** in `docs/BACKLOG.md` that has a
  `docs/prd/` file.

Pick the target phase: the first phase in the PRD's **Implementation phases**
section whose work is not yet landed. If the PRD has no phases, the whole PRD is
one phase.

## Step 1 — the launch gate

Before any code, verify the phase is actually launchable. **Stop if any fails:**

1. `Status:` is `draft` or a phase remains; the PRD is not already fully closed.
2. **Interfaces & types** is frozen for this phase (not `<placeholder>`, not an
   open question).
3. Every domain identifier the phase introduces **already exists** in
   `docs/GLOSSARY.md`. This is the big one: `/developer` will otherwise stop and
   demand `/glossary`, which *interviews the user*. Naming is the user's call —
   it belongs in the evening grill, not in your run.
4. The phase has a green checkpoint you can actually evaluate (compiles + named
   tests/oracles pass).
5. The PRD's **stop-list**, if present, does not name a condition that already
   holds.

A failed gate is not a failure of the run — it is the PRD not being
unattended-ready. Report exactly which gate failed and what the evening session
must decide.

## Step 2 — isolate

Use **EnterWorktree** unless you are already under `.claude/worktrees/`. Never
work in the user's checkout.

Never `git add -A` — other worktrees are usually live and `-A` will swallow them.
Stage explicit paths.

## Step 3 — implement

Read the PRD's `Recommended workflow:` field and obey it.

**sequential** — spawn the `developer` agent; when it returns, spawn the
`test-writer` agent against the landed code.

**concurrent** — spawn `developer` and `test-writer` **in the same message** so
they run in parallel, each with `isolation: "worktree"`. `test-writer` binds to
the frozen *Interfaces & types* block, not to the implementation. **You own
integration**: merge both branches, then build.

If the interfaces change mid-flight, that is a PRD-change event: update the PRD
and propagate to the in-flight test branch. If the change is *substantive*
(a different contract, not a typo), that is the user's call — **stop**.

## Step 4 — green checkpoint

```sh
export TMPDIR="$PWD/build/testtmp"; mkdir -p "$TMPDIR"
cmake --build build -j > "$TMPDIR/ek_build.log" 2>&1 && echo BUILD_OK || tail -40 "$TMPDIR/ek_build.log"
ctest --test-dir build --output-on-failure > "$TMPDIR/ek_test.log" 2>&1; grep -cE "Failed|FAILED" "$TMPDIR/ek_test.log"
```

`TMPDIR` must point inside the repo — the sandbox makes `/tmp` read-only and the
CLI suites fail on `mkstemp` without it.

On red: spawn `developer` with **only** the failing test names and their log
excerpt (never the whole log). Up to the repair cap. Then stop.

**A test may never be weakened, skipped, or deleted to make the suite green.**
If the honest fix is that the test encodes a wrong expectation, that is a
theory question — stop and say so.

## Step 5 — review

Run both, in this order:

1. `/code-review` — generic correctness.
2. `/code-reviewer` — domain. It self-spawns `theory-reviewer` on semantic
   diffs; do not spawn that yourself.

Triage what comes back:

- **must-fix** → spawn `developer` to fix, re-run Step 4, re-review *only the
  fix*. Up to the review cap.
- **consider** → do **not** act. Append to the PRD's "Developer comments / PRD
  disagreements" section and list in the run report.
- **`code-bug`** (from theory review) → treat as must-fix.
- **`doc-bug` / `underspecified`** → the theory-reviewer writes `\cl` notes into
  `latex/main.tex` itself. Leave the submodule dirty, do **not** commit or push
  it, and note in the report that `main.tex:NNN` refs elsewhere may have drifted.
  In a worktree `latex/` is an uninitialized submodule — if so, the notes go to
  `docs/BACKLOG.md` instead.

## Step 6 — land it

Only when the phase is green **and** review is clean:

1. Tick the PRD gates the passes actually closed, each with its ref. Never tick a
   gate whose pass you did not run.
2. Commit with explicit paths. If `latex/` or `docs/main-tex-anchors.json` moved,
   run `scripts/check-main-tex-refs.py --fix` **in the same commit** — the
   pre-commit hook enforces this and citation drift is per-region, never uniform.
3. Merge the worktree branch into the **feature branch**. Never into `master`.
4. `git push -u origin <feature-branch>`.
5. Open or update a **draft** PR against `master`:
   ```sh
   gh pr create --draft --base master --head <feature-branch> \
     --title "<feature>: <phase>" --body-file <report>
   ```
   If a PR for the branch already exists, `gh pr edit` it instead. The PR is how
   the user scans the day's work from their phone — the body is the run report,
   so make it readable by someone who has not seen the code.

If `gh` is not authenticated, **do not** try to work around it. Commit and merge
locally, and record in the report that the PR needs `gh auth login`.

## Step 7 — report, then decide whether to continue

Write `docs/runs/<YYYY-MM-DD>-<feature>-<phase>.md`:

- what landed (files, commit refs);
- gates ticked and their evidence (`ctest` counts, review verdicts);
- **findings deferred** — every "consider", every `doc-bug`, every stop-list hit;
- **questions for the evening grill** — what you would have asked if anyone were
  there. This is the most valuable section; it is the input to the next
  `/grill-prd`.
- budget used: phases run, repair rounds, review rounds.

Then **always** write `build/runs/last-status`, one line, verdict first:

```
DONE <reason>       # nothing further to do without the user
MORE_WORK <reason>  # work remains and a fresh session could continue it
BLOCKED <reason>    # stopped on a decision only the user can make
```

The day is split into **waves**, one per token-allowance window (`day-run.sh`
starts wave 2 about five hours after wave 1). This line is what decides whether
a later wave fires, so it is a real contract, not bookkeeping:

- **`MORE_WORK`** — you hit a cap, the deadline, or the allowance. A later wave
  resumes you. Say precisely where you stopped so it does not redo landed work.
- **`BLOCKED`** — a later wave would hit the *same wall* and burn the window for
  nothing. Use this whenever the blocker is a decision: a missing glossary name,
  an open theory question, a failed launch gate, a substantive interface change.
- **`DONE`** — the PRD is closed, or the remaining phases are all blocked.

Never write `MORE_WORK` for something a fresh session cannot fix, and never
write `BLOCKED` merely because you ran out of budget. Getting this backwards
either wastes half the day or ends it early.

If you are resuming (the prompt says so), read the newest `docs/runs/` report and
this file *first*, and continue from there rather than restarting the phase.

Then: if the next phase passes Step 1's launch gate, and no cap is exceeded, and
the deadline has not passed — loop to Step 2. Otherwise stop and say why.

**Never start work on a PRD that was never grilled.** Chaining is within a PRD,
or to the next backlog item that already *has* a launch-gate-passing PRD. An
ungrilled idea is not work, it is a guess.

## Definition of done

- Every phase attempted is either fully landed (green, reviewed, gates ticked,
  merged, pushed) or cleanly stopped with a stated blocker. Nothing half-landed.
- `master` untouched; no force-push; no weakened tests.
- A run report exists for the run, and the PR body carries it.
- The report names every decision the user still owes.
