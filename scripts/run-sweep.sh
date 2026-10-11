#!/usr/bin/env bash
# One command from a clean tree to the sweep JSON and the markdown report:
#
#   scripts/run-sweep.sh [OUT_DIR]          # default build/benchout/sweep
#
# Release build, repeat=5, 20 s per-case timeout, one ltlf-ek-bench invocation
# over the whole grid, then scripts/sweep_report.py over its JSON. parity-t3 is
# not in the family list (a capability separation, not a timing).
#
# Environment:
#   SPOT_ROOT      Spot install to build against (default ~/opt/spot-2.15.1 if
#                  it exists, else whatever CMake finds)
#   N_MAX          largest N of the grid (default 6)
#   BUDGET         wall-clock ceiling in seconds. The default is a day, so the
#                  grid is never truncated; a smoke run passes e.g. BUDGET=60.
#   MEM_KIB        per-process address-space cap, a plain shell `ulimit -v`
#                  (default 8 GiB), never RLIMIT_AS from a preexec hook: that
#                  turns bad_alloc into SIGSEGV.
set -euo pipefail

repo="$(cd "$(dirname "$0")/.." && pwd)"
out_dir="${1:-$repo/build/benchout/sweep}"
build_dir="$repo/build/release"
spot_root="${SPOT_ROOT:-}"
if [[ -z "$spot_root" && -d "$HOME/opt/spot-2.15.1" ]]; then
  spot_root="$HOME/opt/spot-2.15.1"
fi

# Families of the run: the three slippery arms (one family in the report),
# knowledge-chain, cons-prunes.
families="slippery-binary,slippery-onehot,slippery-binary-compact,knowledge-chain,cons-prunes"
# The EK champion, the explicit product with and without the knowledge (the
# prune ratio), and the folded no-knowledge product (its second baseline).
subjects="otf-mtdfa-product,dfa-product,dfa-product-unpruned,dfa-product-nk"

cmake_args=(-S "$repo" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release)
[[ -n "$spot_root" ]] && cmake_args+=("-DSPOT_ROOT=$spot_root")
cmake "${cmake_args[@]}" > "$build_dir.cmake.log" 2>&1 || {
  tail -20 "$build_dir.cmake.log"; exit 1; }
cmake --build "$build_dir" -j --target ltlf-ek-bench > "$build_dir.build.log" 2>&1 || {
  tail -40 "$build_dir.build.log"; exit 1; }

# ltlfsynt by absolute path: the bare name resolves through PATH to whichever
# install shadows the others.
if [[ -n "$spot_root" ]]; then
  ltlfsynt="$spot_root/bin/ltlfsynt"
else
  ltlfsynt="$(command -v ltlfsynt)"
fi
ltlfsynt="$(readlink -f "$ltlfsynt")"

mkdir -p "$out_dir"
json="$out_dir/sweep.json"
(
  ulimit -v "${MEM_KIB:-8388608}"
  "$build_dir/ltlf-ek-bench" --families="$families" --subjects="$subjects" \
    --n-min=2 --n-max="${N_MAX:-6}" --repeat=5 --timeout=20 \
    --budget="${BUDGET:-86400}" --ltlfsynt="$ltlfsynt" --out="$json"
)
python3 "$repo/scripts/sweep_report.py" "$json" > "$out_dir/report.md"
echo "sweep: $json"
echo "report: $out_dir/report.md"
