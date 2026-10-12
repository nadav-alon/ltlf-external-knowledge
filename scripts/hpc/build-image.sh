#!/usr/bin/env bash
# Builds ltlf-ek.sif on the machine this runs on (the cluster's login node, which has
# apptainer): clones or updates the repo, checks out COMMIT, and builds the image from
# it. wizard.sh pipes this over ssh: bash -s -- URL COMMIT < build-image.sh.
#
#   EK_SRC        the clone           (default ~/ltlf-ek-src)
#   EK_SIF        the image           (default ~/ltlf-ek.sif)
#   EK_BUILD_LOG  the whole build log (default ~/ltlf-ek-build.log)
set -euo pipefail
url="${1:?usage: build-image.sh URL COMMIT}"
commit="${2:?usage: build-image.sh URL COMMIT}"
src="${EK_SRC:-$HOME/ltlf-ek-src}"
sif="${EK_SIF:-$HOME/ltlf-ek.sif}"
log="${EK_BUILD_LOG:-$HOME/ltlf-ek-build.log}"

exec > "$log" 2>&1
if [[ -d "$src/.git" ]]; then
  git -C "$src" fetch --tags origin
else
  git clone "$url" "$src"
fi
git -C "$src" checkout --force --detach "$commit"
cd "$src"
apptainer build --force "$sif" scripts/hpc/ltlf-ek.def
echo "built $sif from $(git rev-parse HEAD)"
