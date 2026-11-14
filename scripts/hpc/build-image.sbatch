#!/usr/bin/env bash
# The image build as a SLURM job, so the login node runs nothing heavier than git and
# sbatch. ek-hpc.sh build copies this and build-image.sh to ~/ on the cluster and runs:
#
#   sbatch --wait --exclude=... build-image.sbatch URL COMMIT
#
# The compile's job count is this job's CPUs (build-image.sh reads SLURM_CPUS_PER_TASK).
# No --exclusive: the cluster refuses it, as sweep.sbatch notes. The build log is
# ~/ltlf-ek-build.log, written by build-image.sh.
#SBATCH --job-name=ek-image
#SBATCH --partition=work
#SBATCH --account=rg-fried
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem=16G
#SBATCH --time=02:00:00
#SBATCH --output=%x-%j.log
set -euo pipefail
exec bash "$HOME/build-image.sh" "$@"
