#!/usr/bin/env bash
# The family sweep as a SLURM job array, one family group per task, on CPU
# nodes of the `work` partition:
#
#   sbatch scripts/hpc/sweep.sbatch
#
# Submit unpinned: --constraint=gold6130 and the --exclude list below (the GPU and
# non-gold6130 nodes, as nadav-alon/ltlf-po-benchmarks's sbatch scripts exclude
# them) let SLURM place the tasks on different nodes, so they run side by side.
# merge_sweep.py merges parts from different hostnames of one CPU model, and each
# task times EK and ltlfsynt itself, so a cell's speedup never crosses nodes. The
# constraint is a feature tag, not proof: see the lscpu in each log, and the
# cpu_model in each part's provenance. `sbatch -w NODE` pins every task to one
# node, where they run one after another. The cluster refuses every form of
# --exclusive, so each task asks for two CPUs instead: the bench is
# single-threaded, and two CPUs are one physical core with its hyperthread
# sibling, so no other job shares that core. Other jobs do share the node, so
# absolute timings carry some noise; each cell's speedup is timed within one
# task. --mem=16G is the task's reservation; the per-process
# cap (MEM_KIB, 8 GiB) sits below it.
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
#SBATCH --array=0-4
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=2
#SBATCH --constraint=gold6130
#SBATCH --exclude=cn[31-44],gpu[1-4],gpu[6-8]
#SBATCH --mem=16G
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
  4) families="hidden-counter" ;;
  *) echo "sweep.sbatch: no family group for task $SLURM_ARRAY_TASK_ID" >&2; exit 1 ;;
esac
subjects="otf-mtdfa-product,otf-mtdfa-product-otfsolve,otf-mtdfa-product-otfsolve-simp,otf-mtdfa-product-nk,otf-mtdfa-product-nk-otfsolve,otf-mtdfa-product-nk-otfsolve-simp,dfa-product,dfa-product-unpruned,dfa-product-nk,knowledge-sizes"

image="${EK_IMAGE:-$HOME/ltlf-ek.sif}"
out_dir="${EK_OUT:-$HOME/ek-sweep/${SLURM_ARRAY_JOB_ID:-local}}"
part="part-${SLURM_ARRAY_TASK_ID}.json"

# ulimit and TMPDIR are set inside the container: a cap on apptainer itself
# would starve its Go runtime. --budget is set far above the bench's 2 h default
# so it never truncates the grid. The 8 GiB default cap is run-sweep.sh's.
# The bench refuses an --out outside the repository (/src in the image), so the
# part directory is bound under it.
inner="ulimit -v ${MEM_KIB:-8388608}; export TMPDIR=\$(mktemp -d);
exec /src/build/ltlf-ek-bench --families=$families --subjects=$subjects \
  --n-min=2 --n-max=${N_MAX:-6} --repeat=5 --timeout=20 --budget=4000000000 \
  --ltlfsynt=\$(readlink -f /usr/local/bin/ltlfsynt) --out=/src/sweep-out/$part"

cmd=(apptainer exec --cleanenv --bind "$out_dir:/src/sweep-out" "$image" bash -c "$inner")
if [[ -n "${EK_DRY_RUN:-}" ]]; then
  echo "${cmd[*]}"
  exit 0
fi

# The sweep is CPU-only: refuse a GPU node of the shared partition.
if scontrol show node "${SLURMD_NODENAME:?run under sbatch}" | grep -qi 'Gres=.*gpu'; then
  echo "sweep.sbatch: $SLURMD_NODENAME has GPUs; keep the --exclude list, or pass a CPU-only gold6130 node to sbatch -w" >&2
  exit 1
fi
mkdir -p "$out_dir"
# The gold6130 tag is a label, not hardware proof: record what actually ran.
hostname
lscpu
"${cmd[@]}"
echo "part: $out_dir/$part"
