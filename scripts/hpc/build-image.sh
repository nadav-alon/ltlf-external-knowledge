#!/usr/bin/env bash
# Builds ltlf-ek.sif on the machine this runs on (a compute node of the cluster, inside
# build-image.sbatch's job): clones or updates the repo, checks out COMMIT, and builds the image from
# it. ek-hpc.sh build submits build-image.sbatch, which runs: build-image.sh URL COMMIT.
#
#   EK_SRC        the clone           (default ~/ltlf-ek-src)
#   EK_SIF        the image           (default ~/ltlf-ek.sif)
#   EK_BUILD_LOG  the whole build log (default ~/ltlf-ek-build.log)
#   EK_JOBS       the compile's job count (default the SLURM job's CPUs, else 8)
set -euo pipefail
url="${1:?usage: build-image.sh URL COMMIT}"
commit="${2:?usage: build-image.sh URL COMMIT}"
src="${EK_SRC:-$HOME/ltlf-ek-src}"
sif="${EK_SIF:-$HOME/ltlf-ek.sif}"
log="${EK_BUILD_LOG:-$HOME/ltlf-ek-build.log}"
jobs="${EK_JOBS:-${SLURM_CPUS_PER_TASK:-8}}"

echo "build log: $log"
exec > "$log" 2>&1
if [[ -d "$src/.git" ]]; then
  git -C "$src" remote set-url origin "$url"
  git -C "$src" fetch --tags origin
else
  git clone "$url" "$src"
fi
git -C "$src" checkout --force --detach "$commit"
git -C "$src" clean -ffdx
cd "$src"
apptainer build --force --build-arg "jobs=$jobs" "$sif" scripts/hpc/ltlf-ek.def
echo "built $sif from $(git rev-parse HEAD)"
