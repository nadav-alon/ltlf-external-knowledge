#!/usr/bin/env bash
# The HPC sweep's stages as non-interactive subcommands. wizard.sh calls these for
# its first-run walkthrough; `ek-hpc.sh run` is the routine path. Run from anywhere
# inside the repo; state lives in the wizard's env file, keys unchanged.
#
#   ek-hpc.sh build [COMMIT]       build ~/ltlf-ek.sif on the login node (HEAD by default)
#   ek-hpc.sh sync                 copy sweep.sbatch to the login node's ~/
#   ek-hpc.sh submit [--node N] [--force]
#                                  submit the array; pinned with -w only when --node is
#                                  given, else SLURM places the tasks
#   ek-hpc.sh status               exit 0 when every task COMPLETED, 2 while pending or
#                                  running, 1 on any FAILED/TIMEOUT/CANCELLED
#   ek-hpc.sh fetch                scp the parts into build/hpc/$EK_JOB/
#   ek-hpc.sh merge [--date D] [--name SLUG]
#                                  merge the parts into docs/runs/D-SLUG.{json,md}; D
#                                  defaults to the job's end date, SLUG to four-family-sweep
#   ek-hpc.sh run [--no-wait] [--node N] [COMMIT]
#                                  build, sync, submit, then wait, fetch and merge
#
# Every subcommand is idempotent: once it has succeeded, calling it again does no
# remote work and exits 0. None reads stdin; a missing env key is an error naming it.
#
#   ENV_FILE   the state file (default build/hpc-sweep-wizard.env, the wizard's)
#   EK_POLL    seconds between status polls in `run` (default 60)
set -euo pipefail

cd "$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"
ENV_FILE="${ENV_FILE:-build/hpc-sweep-wizard.env}"
SIF='~/ltlf-ek.sif'
BUILD_LOG="ltlf-ek-build.log"
# CPU-only nodes outside these, as nadav-alon/ltlf-po-benchmarks's sbatch scripts exclude.
EXCLUDE='cn[31-44],gpu[1-4],gpu[6-8]'

say()  { printf 'ek-hpc: %s\n' "$1"; }
warn() { printf 'ek-hpc: %s\n' "$1" >&2; }
die()  { warn "$1"; exit 1; }

# get KEY prints KEY's value from ENV_FILE; fails when absent or empty.
get() {
  [[ -f "$ENV_FILE" ]] || return 1
  local line; line=$(grep -E "^${1}=" "$ENV_FILE" | tail -n1) || return 1
  [[ -n "${line#*=}" ]] || return 1
  printf '%s' "${line#*=}"
}
# need KEY prints KEY's value, or exits naming the key and who writes it.
need() {
  get "$1" || die "$1 is not set in $ENV_FILE; ${2:-run scripts/hpc/wizard.sh once, or add $1=... to it}"
}
# put KEY VALUE upserts KEY=VALUE into ENV_FILE.
put() {
  local tmp
  mkdir -p "$(dirname "$ENV_FILE")"; touch "$ENV_FILE"
  tmp=$(mktemp)
  grep -vE "^${1}=" "$ENV_FILE" > "$tmp" || true
  printf '%s=%s\n' "$1" "$2" >> "$tmp"
  mv "$tmp" "$ENV_FILE"
}

target() { need EK_SSH_TARGET; }
remote() { ssh "$(target)" "$@" < /dev/null; }
# tasks: the array size sweep.sbatch declares.
tasks() { echo $(( $(sed -n 's/^#SBATCH --array=0-\([0-9]*\)$/\1/p' scripts/hpc/sweep.sbatch) + 1 )); }

cmd_build() {
  local t url commit
  t=$(target); url=$(need EK_REPO_URL)
  commit=$(git rev-parse --verify "${1:-HEAD}^{commit}") || die "no such commit: ${1:-HEAD}"
  if [[ "$(get EK_IMAGE_SHA || true)" == "$commit" ]] && remote "test -f $SIF"; then
    say "image for $commit already built on $t"; return 0
  fi
  git status --short | grep -v '^??' >&2 && warn "tracked changes are uncommitted; the image records only $commit" || true
  git fetch -q origin 2>/dev/null || true
  git branch -r --contains "$commit" | grep -q . || die "$commit is not on any remote branch; push it first"
  remote true || die "cannot ssh to $t (host, VPN or key); nothing was built"
  remote git ls-remote "$url" HEAD > /dev/null ||
    die "the login node cannot read $url (no network to GitHub, or a private repo); give a URL it can clone, e.g. an ssh one with a deploy key"
  say "building $commit on $t (about 20 min); log: ~/$BUILD_LOG there"
  ssh "$t" "EK_BUILD_LOG=\$HOME/$BUILD_LOG" bash -s -- "$url" "$commit" < scripts/hpc/build-image.sh ||
    die "the build failed; the log is ~/$BUILD_LOG on the cluster: ssh $t tail -50 $BUILD_LOG (a missing or old log means it failed before the build started)"
  put EK_IMAGE_SHA "$commit"
  say "built $commit"
}

cmd_sync() {
  local t want got
  t=$(target)
  want=$(sha256sum scripts/hpc/sweep.sbatch | cut -d' ' -f1)
  got=$(remote "sha256sum sweep.sbatch 2>/dev/null" | cut -d' ' -f1 || true)
  if [[ "$got" == "$want" ]]; then say "sweep.sbatch on $t is current"; return 0; fi
  scp -q scripts/hpc/sweep.sbatch "$t:" || die "scp of sweep.sbatch to $t failed"
  say "copied sweep.sbatch to $t"
}

cmd_submit() {
  local node="" force="" t image job place
  while (( $# )); do
    case "$1" in
      --node) node="${2:?--node needs a node name}"; shift 2 ;;
      --force) force=1; shift ;;
      *) die "submit: unknown argument $1" ;;
    esac
  done
  t=$(target)
  image=$(get EK_IMAGE_SHA || true)
  if [[ -z "$force" ]] && job=$(get EK_JOB) && [[ "$(get EK_JOB_IMAGE || true)" == "$image" ]]; then
    say "job $job is already submitted for image ${image:-unknown}; --force submits another"; return 0
  fi
  [[ -n "$image" ]] || warn "no image build is recorded; ~/ltlf-ek.sif on $t is whatever is there"
  if [[ -n "$node" ]]; then
    place="-w $node"
  else
    place="--exclude=$EXCLUDE"
    warn "unpinned: tasks may land on different nodes, and merge_sweep.py refuses parts from different machines until #136; pin with --node"
  fi
  job=$(remote "sbatch --parsable $place sweep.sbatch") || die "sbatch on $t refused the job (its error is above)"
  job=${job%%;*}
  [[ "$job" =~ ^[0-9]+$ ]] || die "sbatch on $t did not return a job id: $job"
  put EK_JOB "$job"; put EK_JOB_IMAGE "$image"
  [[ -z "$node" ]] || put EK_NODE "$node"
  say "submitted array job $job${node:+ on $node}"
}

cmd_status() {
  local job states n done_ failed
  job=$(need EK_JOB)
  if [[ "$(get EK_DONE_JOB || true)" == "$job" ]]; then say "job $job: all tasks COMPLETED"; return 0; fi
  states=$(remote "sacct -n -X -P -j $job -o JobID,State,Elapsed") || die "sacct on $(target) failed"
  printf '%s\n' "$states"
  n=$(tasks)
  failed=$(cut -d'|' -f2 <<<"$states" | grep -cE '^(FAILED|TIMEOUT|CANCELLED|OUT_OF_MEMORY|NODE_FAIL|BOOT_FAIL|DEADLINE|PREEMPTED)' || true)
  done_=$(cut -d'|' -f2 <<<"$states" | grep -cx COMPLETED || true)
  if (( failed )); then warn "job $job: $failed task(s) did not complete"; return 1; fi
  if (( done_ == n )); then put EK_DONE_JOB "$job"; say "job $job: all $n tasks COMPLETED"; return 0; fi
  say "job $job: $done_ of $n tasks COMPLETED"
  return 2
}

cmd_fetch() {
  local job dir n have
  job=$(need EK_JOB); dir="build/hpc/$job"; n=$(tasks)
  have=$(compgen -G "$dir/part-*.json" | wc -l || true)
  if (( have == n )); then say "all $n parts of job $job are in $dir"; put EK_PARTS "$dir"; return 0; fi
  mkdir -p "$dir"
  scp -q "$(target):ek-sweep/$job/part-*.json" "$dir/" || die "scp of job $job's parts failed; is it finished? ek-hpc.sh status"
  have=$(compgen -G "$dir/part-*.json" | wc -l || true)
  (( have == n )) || die "fetched $have of $n parts of job $job into $dir"
  put EK_PARTS "$dir"
  say "fetched $n parts into $dir"
}

cmd_merge() {
  local date="" name="four-family-sweep" job json md families
  while (( $# )); do
    case "$1" in
      --date) date="${2:?--date needs YYYY-MM-DD}"; shift 2 ;;
      --name) name="${2:?--name needs a slug}"; shift 2 ;;
      *) die "merge: unknown argument $1" ;;
    esac
  done
  job=$(need EK_JOB)
  [[ -n "$date" || "$(get EK_MERGED_JOB || true)" != "$job" ]] || date=$(need EK_DATE)
  if [[ -z "$date" ]]; then
    date=$(remote "sacct -n -X -P -j $job -o End" | grep -E '^[0-9]{4}-' | sort | tail -n1 | cut -c1-10)
    [[ -n "$date" ]] || die "job $job has no end time in sacct; pass --date YYYY-MM-DD"
  fi
  json="docs/runs/$date-$name.json"; md="docs/runs/$date-$name.md"
  if [[ "$(get EK_MERGED_JOB || true)" == "$job" && "$(get EK_REPORT || true)" == "$md" && -f "$json" && -f "$md" ]]; then
    say "job $job is already merged into $md"; return 0
  fi
  compgen -G "build/hpc/$job/part-*.json" > /dev/null || die "no parts in build/hpc/$job; ek-hpc.sh fetch first"
  families="$(sed -n 's/^families="\(.*\)"$/\1/p' scripts/run-sweep.sh)"
  python3 scripts/merge_sweep.py --expect-families="$families" build/hpc/"$job"/part-*.json > "$json.tmp"
  mv "$json.tmp" "$json"
  python3 scripts/sweep_report.py "$json" > "$md.tmp"
  mv "$md.tmp" "$md"
  put EK_DATE "$date"; put EK_REPORT "$md"; put EK_MERGED_JOB "$job"
  say "wrote $json and $md; commit both"
}

# toast MSG raises a Windows notification from WSL, as ~/waitSlurm does; silent elsewhere.
toast() {
  command -v powershell.exe > /dev/null 2>&1 || return 0
  powershell.exe -Command "Import-Module BurntToast; New-BurntToastNotification -Text 'Slurm', '$1'" > /dev/null 2>&1 || true
}

cmd_run() {
  local wait=1 node=() commit="" rc
  while (( $# )); do
    case "$1" in
      --no-wait) wait=""; shift ;;
      --node) node=(--node "${2:?--node needs a node name}"); shift 2 ;;
      -*) die "run: unknown argument $1" ;;
      *) commit="$1"; shift ;;
    esac
  done
  cmd_build ${commit:+"$commit"}
  cmd_sync
  cmd_submit "${node[@]}"
  if [[ -z "$wait" ]]; then say "not waiting; run ek-hpc.sh run again once the job is done"; return 0; fi
  while :; do
    rc=0; cmd_status || rc=$?
    (( rc == 2 )) || break
    sleep "${EK_POLL:-60}"
  done
  if (( rc )); then toast "Job array $(get EK_JOB) failed"; return 1; fi
  cmd_fetch
  cmd_merge
  toast "Job array $(get EK_JOB) merged into $(get EK_REPORT)"
}

sub="${1:-}"; shift || true
case "$sub" in
  build|sync|submit|status|fetch|merge|run) "cmd_$sub" "$@" ;;
  *) sed -n '2,/^set -euo/{/^#/s/^# \{0,1\}//p}' "${BASH_SOURCE[0]}" >&2; exit 64 ;;
esac
