#!/usr/bin/env bash
# The array tasks of sweep.sbatch together cover exactly the families of
# run-sweep.sh, each once, and every task has a part file of its own. Run by ctest.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
want="$(sed -n 's/^families="\(.*\)"$/\1/p' "$here/../run-sweep.sh" | tr ',' '\n' | sort)"
last="$(sed -n 's/^#SBATCH --array=0-\([0-9]*\)$/\1/p' "$here/sweep.sbatch")"
got=""
for i in $(seq 0 "$last"); do
  line="$(SLURM_ARRAY_TASK_ID=$i EK_DRY_RUN=1 EK_OUT=/x EK_IMAGE=/i.sif "$here/sweep.sbatch")"
  grep -q -- "--out=/out/part-$i.json" <<<"$line" || { echo "task $i: wrong part file"; exit 1; }
  got+="$(sed -n 's/.*--families=\([^ ]*\) .*/\1/p' <<<"$line" | tr ',' '\n')"$'\n'
done
got="$(sort <<<"$got" | sed '/^$/d')"
[[ "$got" == "$want" ]] || { echo "families differ:"; diff <(echo "$want") <(echo "$got"); exit 1; }
if SLURM_ARRAY_TASK_ID=$((last + 1)) EK_DRY_RUN=1 "$here/sweep.sbatch" 2>/dev/null; then
  echo "task past the array was accepted"; exit 1
fi
echo ok
