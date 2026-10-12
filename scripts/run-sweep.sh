#!/usr/bin/env bash
# One command from a clean tree to the sweep JSON and the markdown report:
#
#   scripts/run-sweep.sh [OUT_DIR]          # default build/benchout/sweep
#
# OUT_DIR must resolve inside this clone: ltlf-ek-bench refuses --out anywhere
# else, so the script refuses it up front, before configuring or building. It
# likewise refuses one that resolves under /tmp or under $TMPDIR when set,
# mirroring the bench's own checks.
#
# Release build, repeat=5, 20 s per-case timeout, one ltlf-ek-bench invocation
# over the whole grid, then scripts/sweep_report.py over its JSON. parity-t3 is
# not in the family list (a capability separation, not a timing).
#
# Environment:
#   SPOT_ROOT      Spot >= 2.15 install to build and race against (default
#                  ~/opt/spot-2.15.1). Required: without one the run fails,
#                  since an unpinned Spot can be shadowed through
#                  LD_LIBRARY_PATH and the sweep's numbers go into the paper.
#   N_MAX          largest N of the grid (default 6)
#   BUDGET         wall-clock ceiling in seconds. Unset, the run passes an
#                  unreachable ceiling (the bench's own default is 2 h) and the
#                  grid is never truncated; a smoke
#                  run passes e.g. BUDGET=60.
#   MEM_KIB        per-process address-space cap, a plain shell `ulimit -v`
#                  (default 8 GiB), never RLIMIT_AS from a preexec hook: that
#                  turns bad_alloc into SIGSEGV.
set -euo pipefail

repo="$(cd "$(dirname "$0")/.." && pwd -P)"
out_dir="${1:-$repo/build/benchout/sweep}"
build_dir="$repo/build/release"
resolved_out="$(realpath -m -- "$out_dir")"
# under PATH DIR: PATH is DIR or inside it (a trailing "/" on DIR is ignored,
# so DIR=/ holds every absolute path, as the bench's PathStartsWith does).
under() { local dir="${2%/}"; [[ $1 == "$dir" || $1 == "$dir"/* ]]; }
if ! under "$resolved_out" "$repo"; then
  echo "run-sweep: OUT_DIR must resolve inside the repository ($repo)," \
       "got $resolved_out" >&2
  exit 1
fi
if under "$resolved_out" /tmp; then
  echo "run-sweep: OUT_DIR must not resolve under /tmp, got $resolved_out" >&2
  exit 1
fi
if [[ -n "${TMPDIR:-}" ]]; then
  resolved_tmpdir="$(realpath -m -- "$TMPDIR")"
  if under "$resolved_out" "$resolved_tmpdir"; then
    echo "run-sweep: OUT_DIR must not resolve under \$TMPDIR" \
         "($resolved_tmpdir), got $resolved_out" >&2
    exit 1
  fi
fi
spot_root="${SPOT_ROOT:-}"
if [[ -z "$spot_root" && -d "$HOME/opt/spot-2.15.1" ]]; then
  spot_root="$HOME/opt/spot-2.15.1"
fi
if [[ -z "$spot_root" || ! -x "$spot_root/bin/ltlfsynt" ]]; then
  echo "run-sweep: set SPOT_ROOT to a Spot >= 2.15 install with bin/ltlfsynt" \
       "(none at '${spot_root:-~/opt/spot-2.15.1}')" >&2
  exit 1
fi

# Families of the run: the three slippery arms (one family in the report),
# knowledge-chain, cons-prunes, co-generated.
families="slippery-binary,slippery-onehot,slippery-binary-compact,knowledge-chain,cons-prunes,co-generated"
# The EK champion, the explicit product with and without the knowledge (the
# prune ratio), and the folded no-knowledge product (its second baseline).
subjects="otf-mtdfa-product,dfa-product,dfa-product-unpruned,dfa-product-nk"

cmake_args=(-S "$repo" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release)
cmake_args+=("-DSPOT_ROOT=$spot_root")
cmake "${cmake_args[@]}" > "$build_dir.cmake.log" 2>&1 || {
  tail -20 "$build_dir.cmake.log"; exit 1; }
cmake --build "$build_dir" -j --target ltlf-ek-bench > "$build_dir.build.log" 2>&1 || {
  tail -40 "$build_dir.build.log"; exit 1; }

# ltlfsynt by absolute path out of the pinned install: the bare name resolves
# through PATH to whichever install shadows the others.
ltlfsynt="$(readlink -f "$spot_root/bin/ltlfsynt")"

mkdir -p "$out_dir"
json="$out_dir/sweep.json"
(
  ulimit -v "${MEM_KIB:-8388608}"
  "$build_dir/ltlf-ek-bench" --families="$families" --subjects="$subjects" \
    --n-min=2 --n-max="${N_MAX:-6}" --repeat=5 --timeout=20 \
    --budget="${BUDGET:-4000000000}" --ltlfsynt="$ltlfsynt" --out="$json"
)
python3 "$repo/scripts/sweep_report.py" "$json" > "$out_dir/report.md"
echo "sweep: $json"
echo "report: $out_dir/report.md"
