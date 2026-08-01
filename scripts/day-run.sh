#!/usr/bin/env bash
# Kick off an unattended day-run: hand the /launcher skill the next PRD phase and
# let it go dev -> test -> review -> merge -> PR without a human in the loop.
#
# Designed to be fired on logon (see docs/unattended-workflow.md).  Firing on
# logon rather than at a fixed clock time matters here: the PC is off overnight,
# so a 08:30 cron entry silently loses the whole day if the machine boots at
# 08:45.
#
# Usage:
#   scripts/day-run.sh                    # next phase of the top backlog PRD
#   scripts/day-run.sh output-dependencies    # a named PRD (docs/prd/<name>.md)
#   HOURS=4 MAX_PHASES=1 scripts/day-run.sh   # tighter budget
#   DRY_RUN=1 scripts/day-run.sh           # print the invocation, run nothing

set -euo pipefail

REPO="${LTLF_EK_REPO:-$HOME/ltlf-external-knowledge}"
cd "$REPO"

HOURS="${HOURS:-9}"                 # wall-clock budget
MAX_PHASES="${MAX_PHASES:-3}"       # phase cap, enforced by the skill
PRD="${1:-}"

LOG_DIR="$REPO/build/runs"
LOCK="$LOG_DIR/day-run.lock"
mkdir -p "$LOG_DIR"

# --- idempotence -------------------------------------------------------------
# Fired on logon, this can be invoked more than once a day (re-login, WSL
# restart).  A stale lock from a killed run must not wedge the pipeline
# permanently, so the lock carries a PID and is reclaimed if that PID is gone.
if [ -e "$LOCK" ]; then
	old_pid=$(cat "$LOCK" 2>/dev/null || echo "")
	if [ -n "$old_pid" ] && kill -0 "$old_pid" 2>/dev/null; then
		echo "day-run: already running (pid $old_pid); nothing to do."
		exit 0
	fi
	echo "day-run: clearing stale lock (pid ${old_pid:-unknown} is gone)."
	rm -f "$LOCK"
fi
echo $$ > "$LOCK"
trap 'rm -f "$LOCK"' EXIT INT TERM

# --- preflight ---------------------------------------------------------------
# Report problems; only a missing toolchain is fatal.  A missing `gh` login is
# not: the launcher lands the work locally and says the PR is owed.
command -v claude >/dev/null || { echo "day-run: claude CLI not on PATH." >&2; exit 1; }

if ! gh auth status >/dev/null 2>&1; then
	echo "day-run: WARNING - gh not authenticated; no PR will be opened."
	echo "day-run:   fix with: gh auth login"
fi

STAMP=$(date +%Y%m%dT%H%M%S)
LOG="$LOG_DIR/day-run-$STAMP.log"
export LTLF_EK_RUN_DEADLINE=$(( $(date +%s) + HOURS * 3600 ))
export TMPDIR="$REPO/build/testtmp"      # sandbox makes /tmp read-only
mkdir -p "$TMPDIR"

PROMPT="/launcher"
[ -n "$PRD" ] && PROMPT="/launcher docs/prd/${PRD%.md}.md"
PROMPT="$PROMPT

Unattended day-run. Nobody is at the keyboard until this evening.
Budget: at most $MAX_PHASES phases; stop after the phase in flight once the epoch
passes $LTLF_EK_RUN_DEADLINE (${HOURS}h from now; also in \$LTLF_EK_RUN_DEADLINE).
Stop rather than guess at any decision the user owns, and say so in the report."

if [ "${DRY_RUN:-0}" = "1" ]; then
	echo "--- would run ---"
	echo "deadline : $(date -d "@$LTLF_EK_RUN_DEADLINE" 2>/dev/null || echo "$LTLF_EK_RUN_DEADLINE")"
	echo "log      : $LOG"
	echo "prompt   : $PROMPT"
	exit 0
fi

echo "day-run: starting $STAMP, ${HOURS}h budget, <=$MAX_PHASES phases"
echo "day-run: log -> $LOG"

# bypassPermissions is the point: the bubblewrap sandbox provides containment
# (writes confined to the repo, egress limited to github.com), so prompting adds
# nothing an absent user could act on.  Containment beats a longer allowlist.
claude -p "$PROMPT" \
	--permission-mode bypassPermissions \
	> "$LOG" 2>&1 &
CLAUDE_PID=$!
echo "day-run: pid $CLAUDE_PID"
wait $CLAUDE_PID
status=$?

echo "day-run: finished with status $status"
if [ -d "$REPO/docs/runs" ]; then
	latest=$(ls -t "$REPO/docs/runs" 2>/dev/null | head -1 || true)
	[ -n "$latest" ] && echo "day-run: report -> docs/runs/$latest"
fi
exit $status
