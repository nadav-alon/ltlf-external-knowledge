#!/usr/bin/env bash
# A bare `-j` gives make no job limit, and ~70 parallel cc1plus processes fill
# the sandbox's 8 GiB. Build commands in CLAUDE.md, README.md and scripts/ must
# bound it. Run by ctest.
set -u
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root" || exit 1

# `-j` or `--parallel` not followed by a digit (bare, or `$(nproc)`, which
# follows the core count rather than memory) on a line that drives a build.
# grep exits 1 for no match and 2 for an error; only the latter is a failure.
hits=$(grep -rnE --exclude=test_no_bare_jobs.sh \
         '\b(make|cmake|ninja)\b.*[[:space:]](-j|--parallel)([^0-9]|$)' \
         CLAUDE.md README.md scripts)
[ $? -le 1 ] || { echo "grep failed" >&2; exit 1; }
if [ -n "$hits" ]; then
  echo "bare -j in build commands (give it a bound, e.g. -j8):" >&2
  echo "$hits" >&2
  exit 1
fi
echo "no bare -j"
