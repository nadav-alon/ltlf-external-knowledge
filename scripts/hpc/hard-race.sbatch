#!/usr/bin/env bash
# scripts/hard_race.py over the hard instances as a SLURM job array, one instance per task,
# out of the Apptainer image built from scripts/hpc/ltlf-ek.def (ek-hpc.sh build):
#
#   sbatch --exclude=... hard-race.sbatch
#
# Task i races the i-th instance of hard_race.instances() (tests/data/hard-instances, 71 of
# them) under every arm, with hard_race.py's defaults: 120 s per cell, 8 GiB per process
# (its own ulimit -v). Each task times all arms of its instance itself, so a speedup never
# crosses nodes. Placement, CPUs and memory are sweep.sbatch's: unpinned on gold6130, two
# CPUs (one physical core), --mem=16G above the 8 GiB per-process cap. hard_race.py exits
# 1 on a verdict mismatch or a fixed failure; the task still succeeds when its part is
# written, and the merge recomputes both over all parts.
#
# Environment:
#   EK_IMAGE   the .sif (default ~/ltlf-ek.sif)
#   EK_OUT     where the parts land (default ~/ek-hard-race/<array job id>)
#   EK_DRY_RUN print the race command instead of running it
#
#SBATCH --job-name=ek-hard-race
#SBATCH --partition=work
#SBATCH --account=rg-fried
#SBATCH --array=0-70
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=2
#SBATCH --constraint=gold6130
#SBATCH --exclude=cn[31-44],gpu[1-4],gpu[6-8]
#SBATCH --mem=16G
#SBATCH --time=01:00:00
#SBATCH --output=%x-%A_%a.log
set -euo pipefail

task="${SLURM_ARRAY_TASK_ID:?run under sbatch --array}"
image="${EK_IMAGE:-$HOME/ltlf-ek.sif}"
out_dir="${EK_OUT:-$HOME/ek-hard-race/${SLURM_ARRAY_JOB_ID:-local}}"
part="part-$task.json"

# The instance's name picks --family and, for a sized one, --n-min = --n-max = n.
inner="set -euo pipefail; export TMPDIR=\$(mktemp -d); cd /src/scripts
read -r family n < <(python3 -c 'import hard_race as h, sys
names = h.instances(h.DEFAULT_SPECS)
i = $task
if i >= len(names): sys.exit(\"hard-race.sbatch: no instance for task %d of %d\" % (i, len(names)))
fam, n = h.instance_key(names[i])
print(fam, \"\" if n is None else n)')
echo \"instance \$family\${n:+-\$n}, commit \$(git -C /src rev-parse HEAD)\"
rc=0
python3 hard_race.py --build /src/build --ltlfsynt \$(readlink -f /usr/local/bin/ltlfsynt) \
  --family \"\$family\" \${n:+--n-min \$n --n-max \$n} --out /out/$part || rc=\$?
echo \"hard_race.py exit \$rc\"
test -s /out/$part"

cmd=(apptainer exec --cleanenv --bind "$out_dir:/out" "$image" bash -c "$inner")
if [[ -n "${EK_DRY_RUN:-}" ]]; then
  echo "${cmd[*]}"
  exit 0
fi

# CPU-only: refuse a GPU node of the shared partition.
if scontrol show node "${SLURMD_NODENAME:?run under sbatch}" | grep -qi 'Gres=.*gpu'; then
  echo "hard-race.sbatch: $SLURMD_NODENAME has GPUs; keep the --exclude list" >&2
  exit 1
fi
mkdir -p "$out_dir"
# The gold6130 tag is a label, not hardware proof: record what actually ran.
hostname
lscpu
"${cmd[@]}"
echo "part: $out_dir/$part"
