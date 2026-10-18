#!/usr/bin/env bash
# run-sweep.sh refuses an OUT_DIR outside the clone, under /tmp, or under
# $TMPDIR, naming the constraint and the path, before it configures or builds
# anything. The /tmp case needs a writable /tmp and is skipped otherwise.
# Run by ctest.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
work="$(mktemp -d "${TMPDIR:-/tmp}/ek_run_sweep.XXXXXX")"
fake=""
trap 'rm -rf "$work" ${fake:+"$fake"}' EXIT

# A cmake stub that records being run: every refusal must come before it.
mkdir "$work/bin"
printf '#!/bin/sh\ntouch "%s/cmake-ran"\n' "$work" > "$work/bin/cmake"
chmod +x "$work/bin/cmake"

# expect_refused LOG CONSTRAINT PATH: the run that wrote LOG was refused with
# a message naming CONSTRAINT and PATH, PATH was not created, cmake never ran.
expect_refused() {
  local log="$1" constraint="$2" path="$3"
  [[ "$rc" -ne 0 ]] || { echo "an OUT_DIR was accepted: $constraint"; exit 1; }
  grep -qF "$constraint" "$log" || { echo "error does not name the constraint: $constraint"; cat "$log"; exit 1; }
  grep -qF "$path" "$log" || { echo "error does not name the path"; cat "$log"; exit 1; }
  [[ ! -e "$path" ]] || { echo "the refused OUT_DIR was created"; exit 1; }
  [[ ! -e "$work/cmake-ran" ]] || { echo "refusal came after configuring"; exit 1; }
}

# Never created, and independent of TMPDIR (ctest points it inside the clone).
outside="/nonexistent/ek-run-sweep-$$/sweep-dry"
rc=0
PATH="$work/bin:$PATH" bash "$here/run-sweep.sh" "$outside" > "$work/out.log" 2>&1 || rc=$?
expect_refused "$work/out.log" "inside the repository" "$outside"

# An OUT_DIR under /tmp passes the inside-the-repo check only when the clone
# itself is under /tmp, so run a copy of the script from a fake clone there.
# The copy relies on every check before the /tmp refusal needing nothing else
# from a clone; a check that reads the tree must not move above it.
if fake="$(mktemp -d /tmp/ek_run_sweep_clone.XXXXXX 2>/dev/null)"; then
  mkdir "$fake/scripts"
  cp "$here/run-sweep.sh" "$fake/scripts/"
  rc=0
  PATH="$work/bin:$PATH" bash "$fake/scripts/run-sweep.sh" "$fake/sweep" > "$work/tmp.log" 2>&1 || rc=$?
  expect_refused "$work/tmp.log" "must not resolve under /tmp" "$(realpath -m "$fake/sweep")"
else
  fake=""
  echo "skip: /tmp not writable"
fi

# An OUT_DIR inside the clone but under $TMPDIR is refused as well.
repo="$(cd "$here/.." && pwd -P)"
tmpd="$repo/build/ek-run-sweep-tmpdir-$$"
rc=0
TMPDIR="$tmpd" PATH="$work/bin:$PATH" bash "$here/run-sweep.sh" "$tmpd/sweep" > "$work/tmpdir.log" 2>&1 || rc=$?
expect_refused "$work/tmpdir.log" 'must not resolve under $TMPDIR' "$tmpd/sweep"

# A clone with no build/ gets past configuring: the cmake log is written. The
# fake clone sits under the repo's build/ (not /tmp, not $TMPDIR) so its OUT_DIR
# passes the checks; the stub cmake builds nothing, so the run ends at the
# missing bench, after the point where the log is written.
fakeroot="$repo/build/ek-run-sweep-fresh-$$"
trap 'rm -rf "$work" ${fake:+"$fake"} "$fakeroot"' EXIT
mkdir -p "$fakeroot/scripts" "$fakeroot/spot/bin"
cp "$here/run-sweep.sh" "$fakeroot/scripts/"
printf '#!/bin/sh\n' > "$fakeroot/spot/bin/ltlfsynt"
chmod +x "$fakeroot/spot/bin/ltlfsynt"
rc=0
env -u TMPDIR SPOT_ROOT="$fakeroot/spot" PATH="$work/bin:$PATH" \
  bash "$fakeroot/scripts/run-sweep.sh" > "$work/fresh.log" 2>&1 || rc=$?
[[ -f "$fakeroot/build/release.cmake.log" ]] || { echo "no cmake log in a clone without build/"; cat "$work/fresh.log"; exit 1; }
