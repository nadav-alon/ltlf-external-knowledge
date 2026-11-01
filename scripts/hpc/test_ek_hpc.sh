#!/usr/bin/env bash
# ek-hpc.sh's subcommands against fake ssh/scp/sbatch/sacct/apptainer: each does its
# stage, a second call after success does no remote work, a missing env key is an exit
# naming it, and `run` chains them. The "cluster" is a directory standing in for the
# login node's home. Run by ctest.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
work="$(mktemp -d "${TMPDIR:-/tmp}/ek_hpc.XXXXXX")"
trap 'rm -rf "$work"' EXIT
export GIT_AUTHOR_NAME=t GIT_AUTHOR_EMAIL=t@t GIT_COMMITTER_NAME=t GIT_COMMITTER_EMAIL=t@t
fail() { echo "$1"; exit 1; }

# A repo with ek-hpc.sh and stand-ins for what it calls, pushed to a bare origin.
repo="$work/repo"; mkdir -p "$repo/scripts/hpc" "$repo/docs/runs" "$work/bin" "$work/home"
cp "$here/ek-hpc.sh" "$here/build-image.sh" "$here/sweep.sbatch" "$repo/scripts/hpc/"
# The array's task ids, as sweep.sbatch declares them.
tasks=$(seq 0 "$(sed -n 's/^#SBATCH --array=0-\([0-9]*\)$/\1/p' "$here/sweep.sbatch")")
n=$(wc -w <<<"$tasks")
echo 'families="a,b"' > "$repo/scripts/run-sweep.sh"
echo 'import sys; print("{\"parts\": %d}" % (len(sys.argv) - 2))' > "$repo/scripts/merge_sweep.py"
echo 'import sys; print("# report of " + sys.argv[1])' > "$repo/scripts/sweep_report.py"
echo stub-def > "$repo/scripts/hpc/ltlf-ek.def"
git init -q -b master "$repo"; git -C "$repo" add . ; git -C "$repo" commit -qm one
git clone -q --bare "$repo" "$work/origin.git"
git -C "$repo" remote add origin "$work/origin.git"; git -C "$repo" fetch -q origin

calls="$work/calls"; : > "$calls"
cat > "$work/bin/ssh" <<'STUB'
#!/usr/bin/env bash
shift
echo "ssh $*" >> "$CALLS"
cd "$FAKE_HOME" && HOME="$FAKE_HOME" exec bash -c "$*"
STUB
cat > "$work/bin/scp" <<'STUB'
#!/usr/bin/env bash
args=()
for a; do case "$a" in -q) ;; *:*) args+=("$FAKE_HOME/${a#*:}") ;; *) args+=("$a") ;; esac; done
echo "scp ${args[*]}" >> "$CALLS"
cp ${args[@]:0:${#args[@]}-1} "${args[-1]}"
STUB
cat > "$work/bin/sbatch" <<'STUB'
#!/usr/bin/env bash
echo "sbatch $*" >> "$CALLS"
echo "4242;cluster"
STUB
cat > "$work/bin/sacct" <<'STUB'
#!/usr/bin/env bash
echo "sacct $*" >> "$CALLS"
if [[ "$*" == *"-o End"* ]]; then printf '2026-10-17T09:00:00\n2026-10-17T11:30:00\nUnknown\n'; else cat "$FAKE_HOME/sacct.txt"; fi
STUB
cat > "$work/bin/apptainer" <<'STUB'
#!/usr/bin/env bash
cp "$4" "$3"
STUB
chmod +x "$work/bin/"*
export PATH="$work/bin:/usr/bin:/bin" CALLS="$calls" FAKE_HOME="$work/home" EK_POLL=0
export ENV_FILE="$work/env"
# stdin is closed: a subcommand that tried to read it would fail.
ek() { "$repo/scripts/hpc/ek-hpc.sh" "$@" <&-; }
# remote_since MARK: the ssh/scp calls logged after line MARK.
remote_since() { tail -n +"$(( $1 + 1 ))" "$calls"; }
mark() { wc -l < "$calls"; }

grep -vE '^\s*#|"[^"]*read [^"]*"' "$repo/scripts/hpc/ek-hpc.sh" | grep -qE '(^|[;&|({[:space:]])read ' && fail "ek-hpc.sh reads stdin"

# A missing key is an error naming it, never a prompt.
for sub in build sync submit status fetch merge; do
  key=EK_SSH_TARGET; [[ $sub == status || $sub == fetch || $sub == merge ]] && key=EK_JOB
  if err=$(ek "$sub" 2>&1); then fail "$sub ran without $key"; fi
  grep -q "$key" <<<"$err" || fail "$sub's error does not name $key: $err"
done
printf 'EK_SSH_TARGET=fake\n' > "$ENV_FILE"
err=$(ek build 2>&1) && fail "build ran without EK_REPO_URL"
grep -q EK_REPO_URL <<<"$err" || fail "build's error does not name EK_REPO_URL"
echo "EK_REPO_URL=$work/origin.git" >> "$ENV_FILE"

# build: refuses an unpushed commit, builds HEAD, then is a no-op.
echo two > "$repo/x"; git -C "$repo" add x; git -C "$repo" commit -qm unpushed
err=$(ek build 2>&1) && fail "build accepted an unpushed commit"
grep -q "push it first" <<<"$err" || fail "unpushed refusal lacks the wizard's text"
git -C "$repo" reset -q --hard HEAD~1
head=$(git -C "$repo" rev-parse HEAD)
ek build > /dev/null
[[ "$(cat "$work/home/ltlf-ek.sif")" == stub-def ]] || fail "build did not build the image"
grep -qx "EK_IMAGE_SHA=$head" "$ENV_FILE" || fail "build did not record EK_IMAGE_SHA"
m=$(mark); ek build > /dev/null
remote_since "$m" | grep -q 'bash -s' && fail "a second build rebuilt"
rm "$work/home/ltlf-ek.sif"; m=$(mark); ek build > /dev/null
remote_since "$m" | grep -q 'bash -s' || fail "build skipped although the image is gone"

# sync: copies, then is a no-op while the copy matches.
ek sync > /dev/null
cmp -s "$repo/scripts/hpc/sweep.sbatch" "$work/home/sweep.sbatch" || fail "sync did not copy sweep.sbatch"
m=$(mark); ek sync > /dev/null
remote_since "$m" | grep -q '^scp' && fail "a second sync copied again"

# submit: pins only with --node, records EK_JOB, then is a no-op.
ek submit --node cn07 > /dev/null
grep -q 'sbatch --parsable -w cn07 sweep.sbatch' "$calls" || fail "submit --node did not pin"
grep -qx EK_JOB=4242 "$ENV_FILE" || fail "submit did not record EK_JOB"
m=$(mark); ek submit --node cn07 > /dev/null
remote_since "$m" | grep -q sbatch && fail "a second submit submitted again"
ek submit --force > /dev/null 2>&1
remote_since "$m" | grep -q 'sbatch --parsable --exclude=' || fail "unpinned submit lacks the exclude list"

# A refused sbatch is an error that says so, and records no job.
cp "$ENV_FILE" "$work/env.bak"; sed -i '/^EK_JOB/d' "$ENV_FILE"
printf '#!/usr/bin/env bash\necho "sbatch: error: --exclusive is disabled" >&2; exit 1\n' > "$work/bin/sbatch"
err=$(ek submit --node cn07 2>&1) && fail "a refused sbatch was reported as submitted"
grep -q "refused the job" <<<"$err" || fail "a refused sbatch gave no explanation: $err"
grep -q '^EK_JOB=' "$ENV_FILE" && fail "a refused sbatch recorded a job"
printf '#!/usr/bin/env bash\necho "sbatch $*" >> "$CALLS"\necho "4242;cluster"\n' > "$work/bin/sbatch"
cp "$work/env.bak" "$ENV_FILE"

# status: 2 while running, 1 on a failed task, 0 when all complete, then no remote work.
printf '4242_0|COMPLETED|1:00\n4242_1|RUNNING|0:30\n' > "$work/home/sacct.txt"
rc=0; ek status > /dev/null || rc=$?; [[ $rc == 2 ]] || fail "status of a running job exited $rc, not 2"
printf '4242_0|COMPLETED|1:00\n4242_1|TIMEOUT|12:00:00\n' > "$work/home/sacct.txt"
rc=0; ek status > /dev/null 2>&1 || rc=$?; [[ $rc == 1 ]] || fail "status of a timed-out job exited $rc, not 1"
printf '4242_%s|COMPLETED|1:00\n' $tasks > "$work/home/sacct.txt"
ek status > /dev/null || fail "status of a completed job did not exit 0"
m=$(mark); ek status > /dev/null
remote_since "$m" | grep -q . && fail "a second status went remote"

# fetch: copies every part, then is a no-op.
mkdir -p "$work/home/ek-sweep/4242"
for i in $tasks; do echo "{}" > "$work/home/ek-sweep/4242/part-$i.json"; done
ek fetch > /dev/null
[[ $(ls "$repo/build/hpc/4242" | wc -l) == "$n" ]] || fail "fetch did not bring all $n parts"
m=$(mark); ek fetch > /dev/null
remote_since "$m" | grep -q . && fail "a second fetch went remote"

# merge: dated by the job's end in sacct, then a no-op; --date and --name name the files.
ek merge > /dev/null
grep -q "\"parts\": $n" "$repo/docs/runs/2026-10-17-four-family-sweep.json" || fail "merge did not write the dated report from all parts"
[[ -f "$repo/docs/runs/2026-10-17-four-family-sweep.md" ]] || fail "merge did not render the report"
m=$(mark); ek merge > /dev/null
remote_since "$m" | grep -q . && fail "a second merge went remote"
ek merge --date 2026-10-20 --name five-family-sweep > /dev/null
[[ -f "$repo/docs/runs/2026-10-20-five-family-sweep.md" ]] || fail "--date/--name did not name the report"

# run: --no-wait stops after submit; a second run waits, fetches and merges.
rm -rf "$repo/build/hpc" "$repo/docs/runs/"*
printf 'EK_SSH_TARGET=fake\nEK_REPO_URL=%s\n' "$work/origin.git" > "$ENV_FILE"
m=$(mark); ek run --no-wait --node cn07 > /dev/null
remote_since "$m" | grep -q sacct && fail "run --no-wait polled"
grep -qx EK_JOB=4242 "$ENV_FILE" || fail "run --no-wait did not submit"
m=$(mark); ek run > /dev/null
remote_since "$m" | grep -qE 'bash -s|^sbatch ' && fail "the second run rebuilt or resubmitted"
[[ -f "$repo/docs/runs/2026-10-17-four-family-sweep.md" ]] || fail "run did not merge"
echo ok
