#!/usr/bin/env bash
# The array tasks of sweep.sbatch together cover exactly the families of
# run-sweep.sh, each once, every task has a part file of its own, and the subjects,
# repeat and timeout are run-sweep.sh's, the header carries --constraint=gold6130, the
# --exclude list and --mem=16G and pins no node, hostname and lscpu are logged before
# the bench, and the wizard's recon lists idle gold6130 nodes with their features. Run by ctest.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
want="$(sed -n 's/^families="\(.*\)"$/\1/p' "$here/../run-sweep.sh" | tr ',' '\n' | sort)"
want_subjects="$(sed -n 's/^subjects="\(.*\)"$/\1/p' "$here/../run-sweep.sh")"
last="$(sed -n 's/^#SBATCH --array=0-\([0-9]*\)$/\1/p' "$here/sweep.sbatch")"
got=""
for i in $(seq 0 "$last"); do
  line="$(SLURM_ARRAY_TASK_ID=$i EK_DRY_RUN=1 EK_OUT=/x EK_IMAGE=/i.sif "$here/sweep.sbatch")"
  grep -q -- "--out=/out/part-$i.json" <<<"$line" || { echo "task $i: wrong part file"; exit 1; }
  [[ "$(sed -n 's/.*--subjects=\([^ ]*\) .*/\1/p' <<<"$line" | head -1)" == "$want_subjects" ]] \
    || { echo "task $i: subjects differ from run-sweep.sh"; exit 1; }
  for flag in repeat timeout; do
    want_flag="$(sed -n "s/.*--$flag=\([0-9]*\).*/\1/p" "$here/../run-sweep.sh" | head -1)"
    got_flag="$(sed -n "s/.*--$flag=\([0-9]*\).*/\1/p" <<<"$line" | head -1)"
    [[ "$want_flag" == "$got_flag" ]] || { echo "task $i: --$flag is '$got_flag', run-sweep.sh has '$want_flag'"; exit 1; }
  done
  got+="$(sed -n 's/.*--families=\([^ ]*\) .*/\1/p' <<<"$line" | tr ',' '\n')"$'\n'
done
got="$(sort <<<"$got" | sed '/^$/d')"
[[ "$got" == "$want" ]] || { echo "families differ:"; diff <(echo "$want") <(echo "$got"); exit 1; }
if SLURM_ARRAY_TASK_ID=$((last + 1)) EK_DRY_RUN=1 "$here/sweep.sbatch" 2>/dev/null; then
  echo "task past the array was accepted"; exit 1
fi
for d in --constraint=gold6130 '--exclude=cn[31-44],gpu[1-4],gpu[6-8]' --mem=16G; do
  grep -qxF -- "#SBATCH $d" "$here/sweep.sbatch" || { echo "sweep.sbatch lacks $d"; exit 1; }
done
run_at="$(grep -n '^"${cmd\[@\]}"$' "$here/sweep.sbatch" | cut -d: -f1)"
for tool in hostname lscpu; do
  at="$(grep -n "^$tool$" "$here/sweep.sbatch" | cut -d: -f1)"
  [[ -n "$at" && "$at" -lt "$run_at" ]] || { echo "sweep.sbatch does not log $tool before the bench"; exit 1; }
done
recon="$(grep '^step "Pick an idle' "$here/wizard.sh")"
for part in '%f' '-t idle' gold6130; do
  grep -qF -- "$part" <<<"$recon" || { echo "wizard.sh recon lacks $part"; exit 1; }
done
[[ -n "$recon" ]] \
  || { echo "wizard.sh recon does not list idle gold6130 nodes"; exit 1; }
if grep -q '^#SBATCH .*-w\b\|^#SBATCH --nodelist' "$here/sweep.sbatch"; then
  echo "sweep.sbatch pins a node"; exit 1
fi
echo ok
