#!/usr/bin/env bash
# The Oct 17-18 sweep as a SLURM job array, one family group per task, on one
# exclusive CPU node of the `work` partition:
#
#   sbatch -w NODE scripts/hpc/sweep.sbatch
#
# Pin every task to one node with -w: the parts are merged into a single report
# and merge_sweep.py refuses parts from different machines. Tasks then run one
# after another on that node (--exclusive), so none perturbs another's timings.
#
# Same settings as scripts/run-sweep.sh (Release, repeat=5, 20 s per-case
# timeout, parity-t3 not in the grid, no budget truncating it), but run out of
# the Apptainer image built from scripts/hpc/ltlf-ek.def.
#
# Environment:
#   EK_IMAGE   the .sif (default ~/ltlf-ek.sif)
#   EK_OUT     where the parts land (default ~/ek-sweep/<array job id>)
#   N_MAX      largest N of the grid (default 6)
#   MEM_KIB    per-process address-space cap, a plain `ulimit -v` inside the
#              container (default 8 GiB, as run-sweep.sh)
#   EK_DRY_RUN print the bench command instead of running it
#
#SBATCH --job-name=ek-sweep
#SBATCH --partition=work
#SBATCH --account=rg-fried
#SBATCH --array=0-3
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --exclusive
#SBATCH --time=12:00:00
#SBATCH --output=%x-%A_%a.log
set -euo pipefail

# Task -> families. Together they are the families of scripts/run-sweep.sh; the
# three slippery arms stay in one task because the report treats them as one.
case "${SLURM_ARRAY_TASK_ID:?run under sbatch --array}" in
  0) families="slippery-binary,slippery-onehot,slippery-binary-compact" ;;
  1) families="knowledge-chain" ;;
  2) families="cons-prunes" ;;
  3) families="co-generated" ;;
  *) echo "sweep.sbatch: no family group for task $SLURM_ARRAY_TASK_ID" >&2; exit 1 ;;
esac
subjects="otf-mtdfa-product,dfa-product,dfa-product-unpruned,dfa-product-nk"

image="${EK_IMAGE:-$HOME/ltlf-ek.sif}"
out_dir="${EK_OUT:-$HOME/ek-sweep/${SLURM_ARRAY_JOB_ID:-local}}"
part="part-${SLURM_ARRAY_TASK_ID}.json"

# ulimit and TMPDIR are set inside the container: a cap on apptainer itself
# would starve its Go runtime. BUDGET is the bench's own 2 h default lifted.
inner="ulimit -v ${MEM_KIB:-8388608}; export TMPDIR=\$(mktemp -d);
exec /src/build/ltlf-ek-bench --families=$families --subjects=$subjects \
  --n-min=2 --n-max=${N_MAX:-6} --repeat=5 --timeout=20 --budget=4000000000 \
  --ltlfsynt=\$(readlink -f /usr/local/bin/ltlfsynt) --out=/out/$part"

if [[ -n "${EK_DRY_RUN:-}" ]]; then
  echo "apptainer exec --bind $out_dir:/out $image bash -c '$inner'"
  exit 0
fi
mkdir -p "$out_dir"
apptainer exec --cleanenv --bind "$out_dir:/out" "$image" bash -c "$inner"
echo "part: $out_dir/$part"
