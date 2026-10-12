#!/usr/bin/env bash
# run-sweep.sh refuses an OUT_DIR outside the clone, naming the constraint and the
# path, before it configures or builds anything. Run by ctest.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
work="$(mktemp -d "${TMPDIR:-/tmp}/ek_run_sweep.XXXXXX")"
trap 'rm -rf "$work"' EXIT

rc=0
bash "$here/run-sweep.sh" "$work/sweep-dry" > "$work/out.log" 2>&1 || rc=$?
[[ "$rc" -ne 0 ]] || { echo "an OUT_DIR outside the repo was accepted"; exit 1; }
grep -q "inside the repository" "$work/out.log" || { echo "error does not name the constraint"; cat "$work/out.log"; exit 1; }
grep -qF "$work/sweep-dry" "$work/out.log" || { echo "error does not name the path"; cat "$work/out.log"; exit 1; }
[[ ! -e "$work/sweep-dry" ]] || { echo "the refused OUT_DIR was created"; exit 1; }
if grep -q "cmake\|Configuring" "$work/out.log"; then echo "refusal came after configuring"; exit 1; fi
