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
# Within a wave, each PHASE gets its OWN `claude -p` session.  The launcher used
# to chain phases inside one session, which meant phase 4 paid for phase 1's
# context on every single turn -- and context is re-sent per turn, so that cost
# compounds rather than accumulating once.  A fresh session per phase pays a
# small fixed re-orientation cost (PRD + backlog + skill) and nothing else, which
# is why the phase cap can now be effectively unlimited: what ends a wave is the
# allowance or running out of launchable PRDs, not an arbitrary count.
#
# A wave only fires if the previous one ran out of *budget*.  If it stopped
# because it needs a decision from the user, a second wave would walk into the
# same wall, so the run ends instead -- see the status contract below.
#
# Usage:
#   scripts/day-run.sh                    # next phase of the top backlog PRD
#   scripts/day-run.sh output-dependencies    # a named PRD (docs/prd/<name>.md)
#   WAVES=1 MAX_PHASES=1 scripts/day-run.sh   # single wave, a single phase
#   WAVE_HOURS=3 WAVES=3 scripts/day-run.sh   # three shorter windows
#   DRY_RUN=1 scripts/day-run.sh           # print the plan, run nothing

set -euo pipefail

REPO="${LTLF_EK_REPO:-$HOME/ltlf-external-knowledge}"
cd "$REPO"

WAVE_HOURS="${WAVE_HOURS:-${HOURS:-5}}"  # ~ the usage-allowance reset window
WAVES="${WAVES:-2}"                 # allowance windows per day
# 0 = no cap: a wave ends on the deadline, the allowance, or a lack of launchable
# PRDs -- never on an arbitrary phase count.  Set it to pin a short test run.
MAX_PHASES="${MAX_PHASES:-0}"
# Pure runaway guard.  If the launcher ever reported MORE_WORK forever without
# landing anything, the stuck-detector below would catch it first; this is the
# backstop for the case where it *does* land something each time but should not.
PHASE_BACKSTOP="${PHASE_BACKSTOP:-20}"
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

build_prompt() {   # $1 = wave number, $2 = phase slot, $3 = 1 if resuming
	local wave=$1 slot=$2 resume=$3 p="/launcher"
	[ -n "$PRD" ] && p="/launcher docs/prd/${PRD%.md}.md"
	p="$p

Unattended day-run, wave $wave of $WAVES, phase slot $slot. Nobody is at the
keyboard until this evening.

Do EXACTLY ONE phase, then stop. Do not chain to a second phase yourself: a
fresh session runs the next one, and that is the point -- it starts with a clean
context instead of carrying this phase's. Stop without starting the phase at all
if the epoch has passed \$LTLF_EK_RUN_DEADLINE ($(human_time "$LTLF_EK_RUN_DEADLINE") today).
Stop rather than guess at any decision the user owns, and say so in the report.

Before you finish, write build/runs/last-status with a single line:
  DONE <reason>       -- no launchable PRD remains, in the backlog or on a branch
  MORE_WORK <reason>  -- a phase remains and a fresh session could run it
  BLOCKED <reason>    -- stopped on a decision only the user can make
That line decides whether another session runs, so BLOCKED must mean genuinely
blocked: a later session would hit the same wall and waste the window. Write
DONE when there is simply no work left to pick up -- that is a clean end to the
day, not a failure."

	if [ "$resume" = "1" ]; then
		p="$p

This is a RESUME. Earlier sessions today already landed work. Read the newest
report under docs/runs/ and build/runs/last-status first, pick up exactly where
they stopped, and do not redo landed work."
	fi
	printf '%s' "$p"
}

if [ "${DRY_RUN:-0}" = "1" ]; then
	echo "--- plan ---"
	if (( MAX_PHASES > 0 )); then
		cap="<=$MAX_PHASES phases"
	else
		cap="phases until the allowance, the deadline, or no launchable PRD"
	fi
	for (( w = 1; w <= WAVES; w++ )); do
		s=$(( START + (w - 1) * WAVE_HOURS * 3600 ))
		e=$(( START + w * WAVE_HOURS * 3600 ))
		echo "wave $w: $(human_time "$s") -> $(human_time "$e")  ($cap, one session each)"
	done
	LTLF_EK_RUN_DEADLINE=$(( START + WAVE_HOURS * 3600 ))
	echo "--- wave 1, phase slot 1 prompt ---"
	build_prompt 1 1 0
	echo
	exit 0
fi

overall=0
day_over=0        # set by DONE / BLOCKED / stuck -- ends the day, not just the wave
slot=0            # phase slots used across the whole day; slot > 1 means resume

for (( wave = 1; wave <= WAVES; wave++ )); do
	if [ "$day_over" = 1 ]; then break; fi

	wave_at=$(( START + (wave - 1) * WAVE_HOURS * 3600 ))
	now=$(date +%s)
	if (( now < wave_at )); then
		echo "day-run: wave $wave waits until $(human_time "$wave_at") (allowance window)"
		sleep $(( wave_at - now ))
	fi

	# Each wave owns exactly one window, so it stops before the next begins.
	export LTLF_EK_RUN_DEADLINE=$(( START + wave * WAVE_HOURS * 3600 ))
	echo "day-run: wave $wave/$WAVES until $(human_time "$LTLF_EK_RUN_DEADLINE")"

	wave_slot=0
	while : ; do
		now=$(date +%s)
		if (( now >= LTLF_EK_RUN_DEADLINE )); then
			echo "day-run: wave $wave deadline reached; starting no further phase."
			break
		fi
		if (( MAX_PHASES > 0 )) && (( wave_slot >= MAX_PHASES )); then
			echo "day-run: wave $wave reached MAX_PHASES=$MAX_PHASES."
			break
		fi
		if (( wave_slot >= PHASE_BACKSTOP )); then
			echo "day-run: wave $wave hit PHASE_BACKSTOP=$PHASE_BACKSTOP; stopping."
			break
		fi

		wave_slot=$(( wave_slot + 1 ))
		slot=$(( slot + 1 ))
		resume=0
		if (( slot > 1 )); then resume=1; fi

		# Progress is measured in commits, not in the launcher's own say-so: a
		# session that reports MORE_WORK without landing anything would be
		# repeated verbatim by the next one, burning the window.
		commits_before=$(git rev-list --all --count)

		STAMP=$(date +%Y%m%dT%H%M%S)
		LOG="$LOG_DIR/day-run-$STAMP-w$wave-p$wave_slot.log"
		rm -f "$STATUS"      # absence afterwards == died without reporting

		echo "day-run: wave $wave phase slot $wave_slot starting $STAMP"
		echo "day-run: log -> $LOG"

		# bypassPermissions is the point: the bubblewrap sandbox provides
		# containment (writes confined to the repo, egress limited to
		# github.com), so prompting adds nothing an absent user could act on.
		# Containment beats an allowlist.
		claude -p "$(build_prompt "$wave" "$wave_slot" "$resume")" \
			--permission-mode bypassPermissions \
			> "$LOG" 2>&1 &
		CLAUDE_PID=$!
		echo "day-run: pid $CLAUDE_PID"
		set +e
		wait $CLAUDE_PID
		overall=$?
		set -e
		commits_after=$(git rev-list --all --count)
		echo "day-run: slot $wave_slot finished with status $overall ($(( commits_after - commits_before )) commit(s) landed)"

		verdict=$(awk 'NR==1{print $1}' "$STATUS" 2>/dev/null || true)
		reason=$(awk 'NR==1{$1=""; sub(/^ /,""); print}' "$STATUS" 2>/dev/null || true)
		case "${verdict:-MISSING}" in
		DONE)
			echo "day-run: DONE - ${reason:-no launchable PRD remains}. Ending the day."
			day_over=1
			break
			;;
		BLOCKED)
			# A later session would hit the same wall; spending the window is
			# worse than stopping, because the report names what the user owes.
			echo "day-run: BLOCKED - ${reason:-needs a decision}. Ending the day; see the run report."
			day_over=1
			break
			;;
		MORE_WORK)
			if (( commits_after == commits_before )); then
				echo "day-run: MORE_WORK but nothing landed - treating as stuck. Ending the day."
				day_over=1
				break
			fi
			echo "day-run: MORE_WORK - ${reason:-a phase remains}. Next phase gets a fresh session."
			;;
		MISSING)
			# Almost always the allowance dying mid-turn.  The next wave opens a
			# new window, so end this one rather than immediately retrying.
			echo "day-run: slot $wave_slot reported no status (likely allowance exhausted mid-turn)."
			echo "day-run: ending wave $wave; the next wave brings a fresh allowance."
			break
			;;
		esac
	done

	if (( wave == WAVES )) && [ "$day_over" != 1 ]; then
		echo "day-run: wave cap reached; work may remain. Raise WAVES or resume this evening."
	fi
done

if [ -d "$REPO/docs/runs" ]; then
	latest=$(ls -t "$REPO/docs/runs" 2>/dev/null | head -1 || true)
	[ -n "$latest" ] && echo "day-run: report -> docs/runs/$latest"
fi
exit "$overall"
