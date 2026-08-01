#!/bin/sh
# Truthful `git status` from inside the bubblewrap sandbox.
#
# The sandbox enforces its deny list by bind-mounting /dev/null over paths --
# including paths that do not exist on disk.  Those masks surface as untracked
# character devices (`.bashrc`, `.mcp.json`, `.claude/hooks`, ...), so a plain
# `git status` inside the sandbox reports a dirty tree that is not dirty.
#
# Agents that hit this tend to re-run `git status` with the sandbox disabled,
# which costs a permission prompt -- the one thing an unattended run cannot pay.
# This script gives them the truth without leaving the sandbox: it drops every
# entry whose path is a character device.
#
# Usage:  scripts/wt-status.sh [--porcelain]
#   default     human-readable summary
#   --porcelain machine-readable, same format as `git status --porcelain`

set -eu

porcelain=0
[ "${1:-}" = "--porcelain" ] && porcelain=1

# -z gives NUL-terminated records, so paths with spaces/newlines survive.
real=$(git status --porcelain -z 2>/dev/null | tr '\0' '\n' | while IFS= read -r line; do
	[ -n "$line" ] || continue
	# Porcelain v1: two status chars, a space, then the path.
	path=${line#???}
	# A character device here is a sandbox mask, not a real file.
	[ -c "$path" ] && continue
	printf '%s\n' "$line"
done)

if [ "$porcelain" -eq 1 ]; then
	printf '%s' "$real" | sed '/^$/d'
	exit 0
fi

branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo '(detached)')
printf 'On branch %s\n' "$branch"

if [ -z "$(printf '%s' "$real" | sed '/^$/d')" ]; then
	printf 'Working tree clean (sandbox phantoms filtered).\n'
else
	printf '\nChanges (sandbox phantoms filtered):\n'
	printf '%s\n' "$real" | sed '/^$/d;s/^/  /'
fi
