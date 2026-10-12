#!/usr/bin/env bash
# build-image.sh clones the repo, builds from the requested commit (not the clone's tip),
# reuses an existing clone, and leaves the build log behind when apptainer fails.
# Run by ctest, with a stub apptainer.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
work="$(mktemp -d "${TMPDIR:-/tmp}/ek_build_image.XXXXXX")"
trap 'rm -rf "$work"' EXIT
export GIT_AUTHOR_NAME=t GIT_AUTHOR_EMAIL=t@t GIT_COMMITTER_NAME=t GIT_COMMITTER_EMAIL=t@t
git init -q "$work/origin"
mkdir -p "$work/origin/scripts/hpc" "$work/bin"
echo one > "$work/origin/scripts/hpc/ltlf-ek.def"
git -C "$work/origin" add . && git -C "$work/origin" commit -qm one
first="$(git -C "$work/origin" rev-parse HEAD)"
echo two > "$work/origin/scripts/hpc/ltlf-ek.def"
git -C "$work/origin" commit -qam two
cat > "$work/bin/apptainer" <<'STUB'
#!/usr/bin/env bash
[[ -z "${STUB_FAIL:-}" ]] || { echo "FATAL: stub build failed"; exit 7; }
cp "$4" "$3"
STUB
chmod +x "$work/bin/apptainer"
export PATH="$work/bin:$PATH" EK_SRC="$work/src" EK_SIF="$work/out.sif" EK_BUILD_LOG="$work/build.log"

bash "$here/build-image.sh" "$work/origin" "$first"
[[ "$(cat "$EK_SIF")" == one ]] || { echo "did not build the requested commit"; exit 1; }
grep -q "built .* from $first" "$EK_BUILD_LOG" || { echo "log lacks the built commit"; exit 1; }

second="$(git -C "$work/origin" rev-parse HEAD)"
bash "$here/build-image.sh" "$work/origin" "$second"
[[ "$(cat "$EK_SIF")" == two ]] || { echo "existing clone was not updated"; exit 1; }

# A changed URL is honoured on a reused clone, and stray untracked files stay out of the build.
git clone -q "$work/origin" "$work/origin2"
echo three > "$work/origin2/scripts/hpc/ltlf-ek.def"
git -C "$work/origin2" commit -qam three
third="$(git -C "$work/origin2" rev-parse HEAD)"
echo stray > "$work/src/stray.txt"
bash "$here/build-image.sh" "$work/origin2" "$third"
[[ "$(cat "$EK_SIF")" == three ]] || { echo "a changed URL was ignored by the reused clone"; exit 1; }
[[ ! -e "$work/src/stray.txt" ]] || { echo "an untracked file survived in the reused clone"; exit 1; }

if STUB_FAIL=1 bash "$here/build-image.sh" "$work/origin" "$second"; then
  echo "a failed build was reported as success"; exit 1
fi
grep -q "stub build failed" "$EK_BUILD_LOG" || { echo "failed build left no log"; exit 1; }
echo ok
