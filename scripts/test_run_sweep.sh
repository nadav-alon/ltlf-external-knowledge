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

# An OUT_DIR under /tmp is refused too. It can only pass the inside-the-repo
# check when the clone itself is under /tmp, so run a copy of the script from a
# fake clone there.
fake="$(mktemp -d /tmp/ek_run_sweep_clone.XXXXXX)"
trap 'rm -rf "$work" "$fake"' EXIT
mkdir "$fake/scripts"
cp "$here/run-sweep.sh" "$fake/scripts/"
rc=0
PATH="$work/bin:$PATH" bash "$fake/scripts/run-sweep.sh" "$fake/sweep" > "$work/tmp.log" 2>&1 || rc=$?
[[ "$rc" -ne 0 ]] || { echo "an OUT_DIR under /tmp was accepted"; exit 1; }
grep -q "must not resolve under /tmp" "$work/tmp.log" || { echo "error does not name the /tmp constraint"; cat "$work/tmp.log"; exit 1; }
grep -qF "$(realpath -m "$fake/sweep")" "$work/tmp.log" || { echo "error does not name the path"; cat "$work/tmp.log"; exit 1; }
[[ ! -e "$fake/sweep" ]] || { echo "the refused OUT_DIR was created"; exit 1; }
[[ ! -e "$work/cmake-ran" ]] || { echo "refusal came after configuring"; exit 1; }

# An OUT_DIR inside the clone but under $TMPDIR is refused as well.
repo="$(cd "$here/.." && pwd -P)"
tmpd="$repo/build/ek-run-sweep-tmpdir-$$"
rc=0
TMPDIR="$tmpd" PATH="$work/bin:$PATH" bash "$here/run-sweep.sh" "$tmpd/sweep" > "$work/tmpdir.log" 2>&1 || rc=$?
[[ "$rc" -ne 0 ]] || { echo "an OUT_DIR under TMPDIR was accepted"; exit 1; }
grep -qF 'must not resolve under $TMPDIR' "$work/tmpdir.log" || { echo "error does not name the TMPDIR constraint"; cat "$work/tmpdir.log"; exit 1; }
grep -qF "$tmpd/sweep" "$work/tmpdir.log" || { echo "error does not name the path"; cat "$work/tmpdir.log"; exit 1; }
[[ ! -e "$tmpd" ]] || { echo "the refused OUT_DIR was created"; exit 1; }
[[ ! -e "$work/cmake-ran" ]] || { echo "refusal came after configuring"; exit 1; }
