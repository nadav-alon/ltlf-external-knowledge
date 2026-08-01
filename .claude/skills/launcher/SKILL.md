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

**You run exactly one phase.** `day-run.sh` gives each phase its own session, so
chaining is the *script's* job, not yours — do the phase, write the status line,
and end. This is why there is no phase cap to respect any more: the day is bounded
by the token allowance and by running out of launchable PRDs, not by a count. Your
own context therefore never has to carry a previous phase, which is the whole
saving; a fresh session pays re-orientation once instead of re-sending a bloated
context on every turn.

**Hard caps per phase** (defaults; the kickoff script may lower them):

| Cap | Default | On exceeding |
| --- | --- | --- |
| Build/test repair rounds | 2 | Stop, report the failure verbatim |
| Review fix rounds | 2 | Stop, leave findings open in the report |
| Wall-clock | `LTLF_EK_RUN_DEADLINE` (epoch secs) | Do not start the phase at all |

Check the deadline **before** starting the phase, never mid-phase — a phase
abandoned halfway is worse than one not started. If your context has been
compacted twice, that is a budget signal: finish this phase and write
`MORE_WORK`.

## Step 0 — orient (cheap)

```sh
scripts/wt-status.sh                 # NOT `git status` — see CLAUDE.md
git rev-parse --abbrev-ref HEAD
```

The sandbox reports phantom files that do not exist. `scripts/wt-status.sh`
filters them. **Do not disable the sandbox to get a truthful status** — that
costs a permission prompt, and there is nobody there to answer it.

Pick the target PRD, in this order — stop at the first that yields one:

1. **Your prompt**, if it named a PRD.
2. **The backlog**: the first item under **Now / next** in `docs/BACKLOG.md` that
   has a `docs/prd/` file on `master`.
3. **An unmerged branch**: a PRD that exists on a branch but *not* on `master`.

Rule 3 exists because the backlog is not the only place a decision gets recorded.
A PRD grilled straight onto a branch — the usual shape of an evening session that
ran out of time before touching `docs/BACKLOG.md` — is finished, launchable work,
and it must not be invisible merely because nobody wrote a backlog line for it.

```sh
git fetch -q origin || true      # an origin-only branch counts too
git branch -a --no-merged master --format='%(refname:short)' |
  grep -v '^origin/HEAD$' | sed 's|^origin/||' | sort -u |
  while read -r b; do
    # Prefer the local branch; fall back to origin/ only when there is no local
    # one.  Never select the remote-tracking ref when a local branch exists ---
    # you cannot commit to origin/<x>, and Step 6 has to merge into something.
    git rev-parse --verify -q "$b" >/dev/null || b="origin/$b"
    # --diff-filter=A: only PRDs the branch ADDS.  A branch that merely edits an
    # existing PRD is not a new piece of work, and matching those would make
    # almost every branch a candidate.
    added=$(git diff --name-only --diff-filter=A "master...$b" -- docs/prd/)
    [ -n "$added" ] && echo "$(git log -1 --format=%ct "$b") $b $added"
  done | sort -rn | head -1          # freshest intent wins
```

If the winner is an `origin/<x>` with no local branch, create one
(`git branch <x> origin/<x>`) before Step 2 — the feature branch must be local.

If that PRD fails the Step 1 launch gate, do *not* fall through to the next
candidate: report the gate failure. A grilled PRD that cannot launch is a
decision the user owes, not a reason to go find different work.

**A rule-3 PRD changes what "the feature branch" means.** That branch *is* the
feature branch: Step 6 merges into it, not into a fresh one, and your phase
worktree must be based on it rather than on `master`, or the PRD will not even be
present in the tree you are working in:

```sh
git worktree add .claude/worktrees/<phase> -b <phase-branch> <feature-branch>
```

then enter it with **EnterWorktree**'s `path` argument.

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
work in the user's checkout. If Step 0 picked the PRD by rule 3, base the
worktree on that PRD's branch instead — see the `git worktree add` form there.

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
DONE <reason>       # no launchable PRD remains, in the backlog or on a branch
MORE_WORK <reason>  # a phase remains and a fresh session could run it
BLOCKED <reason>    # stopped on a decision only the user can make
```

This line is the *only* thing that decides whether another session starts, so it
is a real contract, not bookkeeping:

- **`MORE_WORK`** — a phase remains. `day-run.sh` immediately starts a fresh
  session to run it, so say precisely where you stopped or it will redo landed
  work. Note that a `MORE_WORK` which lands **no commit** is read as *stuck* and
  ends the day — an identical next session would achieve the same nothing.
- **`BLOCKED`** — a later session would hit the *same wall* and burn the window
  for nothing. Use this whenever the blocker is a decision: a missing glossary
  name, an open theory question, a failed launch gate, a substantive interface
  change.
- **`DONE`** — there is no launchable work left: every PRD reachable by Step 0's
  three rules is closed, or its remaining phases are blocked. **A day that ends
  in `DONE` having done nothing is a correct day**, not a failed one — the run
  simply had nothing to pick up.

Never write `MORE_WORK` for something a fresh session cannot fix, and never
write `BLOCKED` merely because you ran out of budget. Getting this backwards
either wastes half the day or ends it early.

If you are resuming (the prompt says so), read the newest `docs/runs/` report and
this file *first*, and continue from there rather than restarting the phase.

Then **stop** — do not begin another phase. The kickoff script starts the next
one in a clean session; that is the point, and chaining here would defeat it.

**Never start work on a PRD that was never grilled.** The next session may move
to a different PRD, but only one that already passes the launch gate under Step
0's rules. An ungrilled idea is not work, it is a guess.

## Definition of done

- Every phase attempted is either fully landed (green, reviewed, gates ticked,
  merged, pushed) or cleanly stopped with a stated blocker. Nothing half-landed.
- `master` untouched; no force-push; no weakened tests.
- A run report exists for the run, and the PR body carries it.
- The report names every decision the user still owes.
