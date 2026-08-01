#!/usr/bin/env bash
# Kick off an unattended day-run: hand the /launcher skill the next PRD phase and
# let it go dev -> test -> review -> merge -> PR without a human in the loop.
#
# Designed to be fired on logon (see docs/unattended-workflow.md).  Firing on
# logon rather than at a fixed clock time matters here: the PC is off overnight,
# so a 08:30 cron entry silently loses the whole day if the machine boots at
# 08:45.
#
# The run is split into WAVES, one per token-allowance window.  A single session
# can exhaust the allowance long before the workday ends, so wave 2 starts
# WAVE_HOURS after startup and resumes whatever wave 1 left unfinished.
#
# A wave only fires if the previous one ran out of *budget*.  If it stopped
# because it needs a decision from the user, a second wave would walk into the
# same wall, so the run ends instead -- see the status contract below.
#
# Usage:
#   scripts/day-run.sh                    # next phase of the top backlog PRD
#   scripts/day-run.sh output-dependencies    # a named PRD (docs/prd/<name>.md)
#   WAVES=1 MAX_PHASES=1 scripts/day-run.sh   # single short wave
#   WAVE_HOURS=3 WAVES=3 scripts/day-run.sh   # three shorter windows
#   DRY_RUN=1 scripts/day-run.sh           # print the plan, run nothing

set -euo pipefail

REPO="${LTLF_EK_REPO:-$HOME/ltlf-external-knowledge}"
cd "$REPO"

WAVE_HOURS="${WAVE_HOURS:-${HOURS:-5}}"  # ~ the usage-allowance reset window
WAVES="${WAVES:-2}"                 # sessions per day
MAX_PHASES="${MAX_PHASES:-3}"       # phase cap per wave, enforced by the skill
PRD="${1:-}"

LOG_DIR="$REPO/build/runs"
LOCK="$LOG_DIR/day-run.lock"
# The launcher writes one of DONE / MORE_WORK / BLOCKED here, plus a reason.
# Missing after a wave means the session died without reporting (usually the
# allowance running out mid-turn), which is precisely a resumable case.
STATUS="$LOG_DIR/last-status"
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

export TMPDIR="$REPO/build/testtmp"      # sandbox makes /tmp read-only
mkdir -p "$TMPDIR"

START=$(date +%s)
# A status left over from yesterday must not decide today's waves.
rm -f "$STATUS"

human_time() { date -d "@$1" '+%H:%M' 2>/dev/null || echo "epoch $1"; }

build_prompt() {   # $1 = wave number
	local wave=$1 p="/launcher"
	[ -n "$PRD" ] && p="/launcher docs/prd/${PRD%.md}.md"
	p="$p

Unattended day-run, wave $wave of $WAVES. Nobody is at the keyboard until this
evening. Budget: at most $MAX_PHASES phases; stop after the phase in flight once
the epoch passes \$LTLF_EK_RUN_DEADLINE ($(human_time "$LTLF_EK_RUN_DEADLINE") today).
Stop rather than guess at any decision the user owns, and say so in the report.

Before you finish, write build/runs/last-status with a single line:
  DONE <reason>       -- nothing further to do without the user
  MORE_WORK <reason>  -- work remains and a fresh session could continue it
  BLOCKED <reason>    -- stopped on a decision only the user can make
That line decides whether a later wave runs, so BLOCKED must mean genuinely
blocked: a later wave would hit the same wall and waste the window."

	if [ "$wave" -gt 1 ]; then
		p="$p

This is a RESUME. An earlier wave today ran out of its token allowance before
finishing. Read the newest report under docs/runs/ and build/runs/last-status
first, pick up exactly where it stopped, and do not redo landed work."
	fi
	printf '%s' "$p"
}

if [ "${DRY_RUN:-0}" = "1" ]; then
	echo "--- plan ---"
	for (( w = 1; w <= WAVES; w++ )); do
		s=$(( START + (w - 1) * WAVE_HOURS * 3600 ))
		e=$(( START + w * WAVE_HOURS * 3600 ))
		echo "wave $w: $(human_time "$s") -> $(human_time "$e")  (<=$MAX_PHASES phases)"
	done
	LTLF_EK_RUN_DEADLINE=$(( START + WAVE_HOURS * 3600 ))
	echo "--- wave 1 prompt ---"
	build_prompt 1
	echo
	exit 0
fi

overall=0
for (( wave = 1; wave <= WAVES; wave++ )); do
	wave_at=$(( START + (wave - 1) * WAVE_HOURS * 3600 ))
	now=$(date +%s)
	if (( now < wave_at )); then
		echo "day-run: wave $wave waits until $(human_time "$wave_at") (allowance window)"
		sleep $(( wave_at - now ))
	fi

	# Each wave owns exactly one window, so it stops before the next begins.
	export LTLF_EK_RUN_DEADLINE=$(( START + wave * WAVE_HOURS * 3600 ))
	STAMP=$(date +%Y%m%dT%H%M%S)
	LOG="$LOG_DIR/day-run-$STAMP-w$wave.log"
	rm -f "$STATUS"          # absence after the wave == died without reporting

	echo "day-run: wave $wave/$WAVES starting $STAMP, until $(human_time "$LTLF_EK_RUN_DEADLINE")"
	echo "day-run: log -> $LOG"

	# bypassPermissions is the point: the bubblewrap sandbox provides containment
	# (writes confined to the repo, egress limited to github.com), so prompting
	# adds nothing an absent user could act on.  Containment beats an allowlist.
	claude -p "$(build_prompt "$wave")" \
		--permission-mode bypassPermissions \
		> "$LOG" 2>&1 &
	CLAUDE_PID=$!
	echo "day-run: pid $CLAUDE_PID"
	set +e
	wait $CLAUDE_PID
	overall=$?
	set -e
	echo "day-run: wave $wave finished with status $overall"

	verdict=$(awk 'NR==1{print $1}' "$STATUS" 2>/dev/null || true)
	reason=$(awk 'NR==1{$1=""; sub(/^ /,""); print}' "$STATUS" 2>/dev/null || true)
	case "${verdict:-MISSING}" in
	DONE)
		echo "day-run: DONE - ${reason:-nothing further to do}. No later waves."
		break
		;;
	BLOCKED)
		# A later wave would hit the same wall; spending the window is worse
		# than stopping, because the report already names what the user owes.
		echo "day-run: BLOCKED - ${reason:-needs a decision}. Stopping; see the run report."
		break
		;;
	MORE_WORK)
		echo "day-run: MORE_WORK - ${reason:-work remains}."
		;;
	MISSING)
		echo "day-run: wave $wave reported no status (likely allowance exhausted mid-turn)."
		;;
	esac

	if (( wave == WAVES )); then
		echo "day-run: wave cap reached; work may remain. Raise WAVES or resume this evening."
	fi
done

if [ -d "$REPO/docs/runs" ]; then
	latest=$(ls -t "$REPO/docs/runs" 2>/dev/null | head -1 || true)
	[ -n "$latest" ] && echo "day-run: report -> docs/runs/$latest"
fi
exit "$overall"
