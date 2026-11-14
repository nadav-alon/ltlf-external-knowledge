#!/usr/bin/env bash
# A bare `-j` gives make no job limit, and ~70 parallel cc1plus processes fill
# the sandbox's 8 GiB. Build commands in CLAUDE.md and scripts/ must bound it.
set -u
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root" || exit 1

# `-j` at end of line, or followed by whitespace and then something that is
# not a number, on a line that drives a build.
hits=$(grep -rnE '(cmake --build|make)\b.*[[:space:]]-j([[:space:]]+[^0-9[:space:]]|[[:space:]]*$)' \
         CLAUDE.md scripts --include='*' 2>/dev/null \
       | grep -v 'test_no_bare_jobs.sh')
if [ -n "$hits" ]; then
  echo "bare -j in build commands (give it a bound, e.g. -j8):" >&2
  echo "$hits" >&2
  exit 1
fi
echo "no bare -j"
