# The unattended day-run

The working model: **grill in the evening, build during the workday.** You spend
an evening session turning an intention into a PRD with every decision closed;
the next day, while you are at your job, a run takes that PRD from code through
tests, review, merge and a draft PR without asking you anything.

The whole design follows from one constraint: **nobody can answer a question
between 09:00 and 18:00.** So every question must be answered the night before,
and anything that cannot be is a *stop*, not a guess.

## The two halves

**Evening — `/grill-prd`.** Produces `docs/prd/<feature>.md` with frozen
interfaces, phases that each have a machine-checkable green checkpoint, an
**Unattended-ready** verdict and a **Stop-list**. The gate that matters most:
every domain identifier must already be in `docs/GLOSSARY.md`. If it is not,
`/developer` will stop mid-run and demand `/glossary`, which interviews you —
and you are not there. Naming is your call; make it at night.

**Day — `/launcher`.** Picks the PRD — the one it was handed, else the top
**Now / next** backlog item that has one, else a PRD that exists on an unmerged
branch but not on `master` (freshest branch wins). That last rule is what keeps a
feature grilled straight onto a branch from going unnoticed just because the
evening ran out before the backlog line got written; such a branch *is* the
feature branch, so the phase worktree is based on it. Then it checks the launch
gate, and per phase:
worktree → `/developer` → `/test-writer` → build + `ctest` → `/code-reviewer`
(which self-spawns `theory-reviewer`) → tick gates → commit → merge into the
feature branch → push → draft PR → `/review <PR#>`. Then it moves to the next
phase if the caps allow, and writes `docs/runs/<date>-<feature>-<phase>.md`.

**Why the generic review comes last.** `/code-review` carries
`disable-model-invocation`, so no unattended session can reach it — that is what
left the `code-review` gate open on every run before 2026-08-03. `/review` is the
substitute the CLI itself offers, it *is* agent-invocable, and it reviews the
PR's diff — the same code, one step later in the pipeline, which is the whole
reason the review moved after the PR is opened. `/code-review ultra` stays out of
the loop entirely: it is billed and user-triggered.

Read the run report — and its **questions for the evening grill** section — to
start the next evening session.

## One-time setup

### 1. Authenticate `gh` (required for the PR)

Interactive, so you must do it yourself. From a Claude Code prompt, the `!`
prefix runs it in-session:

```
! gh auth login
```

Choose **HTTPS** and let it set up git credentials. Without this the run still
lands the work locally and records that the PR is owed — it does not fail.

### 2. Wire the kickoff

`scripts/day-run.sh` is the entry point. It is idempotent (a PID lock, reclaimed
if stale), so firing it twice is harmless.

```sh
scripts/day-run.sh                          # next phase of the top backlog PRD
scripts/day-run.sh output-dependencies-tool # a named PRD
WAVES=1 MAX_PHASES=1 scripts/day-run.sh     # one wave, exactly one phase
DRY_RUN=1 scripts/day-run.sh                # show the wave plan, run nothing
```

**A week plan can take over the day, without a PRD.** The launcher's Step-0 rule
can only select a backlog item that *has a PRD on `master`*, so a day of work
that is deliberately not a PRD phase — a measurement sweep, a corpus recon, a
feasibility probe — is unselectable by construction. To schedule one, put a
literal `<!-- day-run: <Weekday> <YYYY-MM-DD> -->` in a `docs/plans/*.md` file
next to that day's entry. The no-argument `day-run.sh` matches today against
those markers and, on a hit, runs that day's entry instead of picking a PRD
phase; everything else — waves, deadline, the `DONE`/`MORE_WORK`/`BLOCKED`
contract, resume, logging, the pause switch — is unchanged. Dating the marker
makes it self-expiring, and a day left unmarked falls back to normal PRD
selection, which is how a plan reserves a day for the user. `PLAN_FILE` /
`PLAN_DAY` force it by hand; an explicit PRD argument still wins over both.

**Skip a day with `touch build/runs/PAUSED`.** The trigger fires on logon and you
cannot un-log-on, so the off switch lives in the script: while that file exists,
`day-run.sh` prints the reason and exits before starting a single session. Write
the reason into it (`echo 'nothing ranked' > build/runs/PAUSED`) and it is echoed
back on the skipped day. Resume with `rm build/runs/PAUSED`.

Pause the evening you realise **no PRD is ranked and launchable** — the backlog's
`Now / next` slot is unranked, or its `#1` is already implemented on a branch that
is not merged. The second case is the expensive one: `master`'s copy of the PRD
still reads `Status: draft`, so the launcher picks it up under Step 0 rule 2 and
redoes landed work. Merging the branch first is the real fix; pausing is what you
do when you are too tired to decide tonight.

**Fire it on logon, not at a fixed time.** Your PC is off overnight, so a cron
entry at 08:30 silently loses the entire day if the machine boots at 08:45.
Logon triggers whenever you actually turn it on.

There is also nothing to schedule *with* right now: this box has no systemd
(`systemctl` reports `offline`) and `cron` is installed but not running. Both
options below work around that.

**Option A — Windows Task Scheduler (recommended).** Survives WSL restarts and
does not need a Linux service.

1. Task Scheduler → *Create Task*.
2. Trigger: **At log on**, delay 1 minute (let WSL settle).
3. Action: *Start a program*
   - Program: `wsl.exe`
   - Arguments: `-u cowclaw -- bash -lc "~/ltlf-external-knowledge/scripts/day-run.sh"`
4. Check *Run whether user is logged on or not* only if you want it headless.

**Option B — WSL boot hook.** Fires when WSL itself starts. Needs `sudo` and a
`wsl --shutdown` to take effect. Add to `/etc/wsl.conf`:

```ini
[boot]
command = "su - cowclaw -c '~/ltlf-external-knowledge/scripts/day-run.sh &'"
```

If you would rather have a real clock schedule, start cron the same way
(`command = "service cron start"`) and add a crontab entry — but re-read the
warning above about a boot time you do not control.

## Budget

One session can exhaust the token allowance long before the workday ends, so the
day is split into **waves**, one per allowance window. Wave 2 starts
`WAVE_HOURS` after startup and resumes whatever wave 1 left unfinished, reading
the newest `docs/runs/` report to avoid redoing landed work.

**Inside a wave, every phase gets its own `claude -p` session.** The launcher
does exactly one phase and stops; `day-run.sh` starts the next. That is what
makes chaining across many PRDs affordable: context is re-sent on *every* turn,
so a session that has already run three phases pays for all three on each turn of
the fourth. A fresh session pays re-orientation (PRD + backlog + skill) once.

Consequently there is **no phase cap** by default. What ends a day is the token
allowance, the wave deadline, or simply running out of launchable PRDs — never an
arbitrary count.

`/launcher` ends every phase by writing `build/runs/last-status`:

| Verdict | Meaning | Another session? |
| --- | --- | --- |
| `DONE` | no launchable PRD remains, in backlog or on a branch | no — day ends |
| `MORE_WORK` | a phase remains | **yes, immediately, clean context** |
| `MORE_WORK` *with no commit landed* | read as **stuck** | no — day ends |
| `BLOCKED` | needs a decision only the user can make | no — day ends |
| *(missing)* | session died mid-turn, usually the allowance | wave ends; next wave resumes |

Two of those are worth understanding. `BLOCKED` ends the day because a second
session would hit the same wall and burn the window, leaving the question in the
run report either way. And a `MORE_WORK` that landed **no commit** is treated as
stuck: an identical next session would achieve the same nothing, so the loop
stops rather than spin. A day that ends immediately in `DONE` is a *correct*
day — there was nothing to pick up.

| Cap | Default | Override |
| --- | --- | --- |
| Waves per day | 2 | `WAVES` |
| Hours per wave | 5 | `WAVE_HOURS` |
| Phases per wave | unlimited | `MAX_PHASES` (0 = no cap) |
| Runaway guard | 20 phases/wave | `PHASE_BACKSTOP` |
| Build/test repair rounds per phase | 2 | — |
| Review fix rounds per phase | 2 | — |

The structural saving is bigger than the caps: `/launcher` is a **conductor**. It
never reads a build log, a test log or a full diff — it spawns the Sonnet-pinned
`developer` / `test-writer` agents, which absorb that bulk in their own context,
and keeps only their summaries. The deadline is checked *between* phases, never
mid-phase; a half-finished phase is worse than one not started.

## Permission prompts

Prompts are the thing that kills an unattended run, and the run cannot answer
one. Two mechanisms remove them:

- **Containment, not allowlisting.** A prompt is what a *sandbox boundary hit*
  looks like: a blocked command triggers an offer to re-run it unsandboxed, and
  that offer is the prompt. Growing `permissions.allow` therefore does not reduce
  prompts. `allowUnsandboxedCommands: false` removes the escalation path
  altogether, so a boundary hit fails cleanly instead of asking an absent user;
  `day-run.sh` additionally passes `--permission-mode bypassPermissions`.
  `failIfUnavailable: true` means a broken bwrap stops the run rather than
  silently running unconfined — loud failure is the right trade when nobody is
  watching, but it does mean a broken sandbox costs the day.
- **Never escape the sandbox for a truthful `ls`.** The sandbox masks its deny
  list by bind-mounting `/dev/null` over paths, *including paths that do not
  exist*, so `git status` reports phantom entries (`.bashrc`, `.mcp.json`,
  `.claude/hooks`, …). Re-running with `dangerouslyDisableSandbox` to see through
  them was the single largest source of prompts in this repo. Use
  `scripts/wt-status.sh`, which filters them. This is spelled out in `CLAUDE.md`
  so spawned agents inherit it.

`~/.ssh` stays unreadable inside the sandbox, and `master` is protected by
`permissions.deny` rules (no force-push, no push to `master`, no checkout of it).

## What a run will not do

- Guess a glossary name, or resolve a `main.tex` ambiguity.
- Weaken, skip or delete a test to make `ctest` green.
- Touch `master`, force-push, or commit the `latex/` submodule.
- Start work on a PRD that was never grilled.

Each of those is a **stop** with a line in the run report. A run that stops after
one phase with a clear blocker is a good run.
