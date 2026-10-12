#!/usr/bin/env bash
# run-sweep.sh refuses an OUT_DIR outside the clone, naming the constraint and the
# path, before it configures or builds anything. Run by ctest.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
work="$(mktemp -d "${TMPDIR:-/tmp}/ek_run_sweep.XXXXXX")"
trap 'rm -rf "$work"' EXIT

# Never created, and independent of TMPDIR (ctest points it inside the clone).
outside="/nonexistent/ek-run-sweep-$$/sweep-dry"
# A cmake stub that records being run: the refusal must come before it.
mkdir "$work/bin"
printf '#!/bin/sh\ntouch "%s/cmake-ran"\n' "$work" > "$work/bin/cmake"
chmod +x "$work/bin/cmake"

rc=0
PATH="$work/bin:$PATH" bash "$here/run-sweep.sh" "$outside" > "$work/out.log" 2>&1 || rc=$?
[[ "$rc" -ne 0 ]] || { echo "an OUT_DIR outside the repo was accepted"; exit 1; }
grep -q "inside the repository" "$work/out.log" || { echo "error does not name the constraint"; cat "$work/out.log"; exit 1; }
grep -qF "$outside" "$work/out.log" || { echo "error does not name the path"; cat "$work/out.log"; exit 1; }
[[ ! -e "$outside" ]] || { echo "the refused OUT_DIR was created"; exit 1; }
[[ ! -e "$work/cmake-ran" ]] || { echo "refusal came after configuring"; exit 1; }
