# LTLf external knowledge — agent instructions

C++ research code: LTLf synthesis under external knowledge, as thin wrappers over
Spot. `latex/main.tex` is the primary (but fallible) reference for the math.

## The sandbox lies about some files — do not escape it

Sessions run under a bubblewrap sandbox. The deny list is enforced by
bind-mounting `/dev/null` over paths, **including paths that do not exist**. So
`ls` and `git status` report entries that are not really there:

- in the repo root: `.bashrc`, `.zshrc`, `.bash_profile`, `.profile`, `.zprofile`,
  `.gitconfig`, `.mcp.json`, `.idea`, `.ripgreprc`
- under `.claude/`: `hooks`, `commands`, `workflows`, `routines`, `launch.json`,
  `scheduled_tasks.json`, `loop.md`

They show as character devices (`crw-rw-rw- 1 nobody nogroup 1, 3`). **None of
them exist on disk.** They are not untracked files, not something to clean up,
and not evidence that the repo is dirty.

**Never re-run a command with `dangerouslyDisableSandbox: true` just to get a
truthful `ls`/`git status`.** That is the single largest source of permission
prompts in this repo, and it is exactly the interruption unattended runs cannot
afford. Instead:

- `scripts/wt-status.sh` — truthful `git status` with the phantoms filtered out.
- Stage explicit paths (`git add src/foo.cpp`), never `git add -A`. There are
  usually live worktrees under `.claude/worktrees/`, and `-A` will swallow them.

Disable the sandbox only when a command genuinely needs the network or a write
outside the allowed roots — and in an unattended run, prefer to fail and report.

## Build and test

Never let raw build output into your context — it is re-read every turn and
dominates the token bill.

```sh
cmake --build build -j > "${TMPDIR:-/tmp}/ek_build.log" 2>&1 && echo OK || tail -40 "${TMPDIR:-/tmp}/ek_build.log"
ctest --test-dir build --output-on-failure > "${TMPDIR:-/tmp}/ek_test.log" 2>&1; grep -nE "FAILED|Failed" "${TMPDIR:-/tmp}/ek_test.log" | head -40 || echo "all passed"
```

`TMPDIR` matters: the sandbox makes `/tmp` read-only, so the CLI suites fail on
`mkstemp` unless `TMPDIR` points inside the repo (`build/testtmp`). Set it rather
than widening the sandbox's write roots.

Spot **>= 2.15** is required (`-DSPOT_ROOT=~/opt/spot-2.15.1`). Several Spot
installs shadow each other via `LD_LIBRARY_PATH`, so `pkg-config`'s version is
not necessarily what runs — `ldd` the binary before diagnosing any Spot crash.

## LaTeX

`latex/main.tex` compiles on Overleaf only. Do not run `pdflatex`/`latexmk`
locally; review the `.tex` by reading it. Edits to `main.tex` go in under
`\cl{...}` notes and are left uncommitted unless the user asks to push.

In a worktree, `latex/` is an uninitialized submodule — draft notes into
`docs/BACKLOG.md` instead of editing through it.

After any submodule bump, `main.tex:NNN` citations drift per-region (never by a
uniform offset). Run `scripts/check-main-tex-refs.py --fix` in the *same commit*
as the bump. Prefer `\cref` labels over line numbers in new citations.

## Workflow

`docs/prd/` holds the specs; each PRD carries a `Status:` and four gates
(`glossary`, `tests`, `code-review`, `theory-review`) ticked by the skill that
performs each pass. `docs/GLOSSARY.md` is the ubiquitous language — every public
domain identifier must appear there, spelled exactly.

For unattended day-runs, see `docs/unattended-workflow.md` and the `/launcher`
skill. The rule that matters there: **an unattended run never guesses at a
decision the user owns.** It records the blocker and stops.
