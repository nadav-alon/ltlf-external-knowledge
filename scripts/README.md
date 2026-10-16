# scripts

## Notation macros: `gen-md-macros.py`

The paper (`latex/main.tex`) defines its notation once, as zero-arg
`\newcommand` macros (`\Tin`, `\Ifree`, `\cons`, …). The Markdown docs
(`docs/GLOSSARY.md`, `docs/prd/*.md`) use those **same** macros in `$…$` math
rather than hand-expanding them — hand-expansion duplicates a macro body that
then silently drifts when the definition changes.

`gen-md-macros.py` mirrors the macros into `.vscode/settings.json`
(`markdown.math.macros`), which the built-in VS Code Markdown preview feeds to
KaTeX so the macros render.

```sh
python3 scripts/gen-md-macros.py          # regenerate .vscode/settings.json
python3 scripts/gen-md-macros.py --check   # exit 1 if stale (used by the hook/CI)
```

Change a macro in `main.tex` → run the script → commit `.vscode/settings.json`.

**Caveat:** only renderers that support KaTeX macros (the VS Code preview) resolve
these; GitHub's web view shows the raw `\Tin`. VS Code is the reading surface.

## Citation drift: `check-main-tex-refs.py`

`main.tex` is an Overleaf submodule edited out of band, so every pull shifts line
numbers and silently invalidates the `main.tex:NNN` citations spread across
`docs/`, `include/`, `src/` and `tests/`. Nothing used to detect that — drift was
found by tripping over a wrong line months later. And the drift is **not** a
uniform offset: one 2026-07-30 pull moved the align block −3, the consistency
block +3 and the commented-out input-dependency block +50, so "add N to
everything" produces garbage.

The script stops trusting the number and remembers the **content** it named. For
every cited line it records a fingerprint — that line's text, extended over
following lines only as far as needed to be unique — in
`docs/main-tex-anchors.json` (generated; do not edit).

```sh
python3 scripts/check-main-tex-refs.py --check      # exit 1 if a citation drifted
python3 scripts/check-main-tex-refs.py --fix        # relocate, rewrite, re-snapshot
python3 scripts/check-main-tex-refs.py --snapshot   # re-record for the CURRENT main.tex
```

**Run `--fix` in the same commit that bumps the `latex` pointer** — that is the
moment the drift is introduced, and repairing it then keeps every citation
correct for free. Range ends shift with their start, so `main.tex:548–553`
becomes `main.tex:552–557`.

Two things it reports but will not repair, because both are content questions
rather than renumbering: a fingerprint that has **vanished** from `main.tex`
(fails), and a `\cref{…}` with no matching `\label` (warns only, so it cannot
block an unrelated commit).

If `latex/main.tex` is absent the script exits 0 in silence — agent worktrees get
an uninitialised submodule, and a commit there must not be blocked by a file that
was never checked out.

## Git hook (one-time setup per clone)

A fast `pre-commit` hook runs both checks. It is committed under `.githooks/`;
point git at it once after cloning:

```sh
git config core.hooksPath .githooks
```

Neither check launches Python unless the staged changes could invalidate it: the
macro check needs `latex` or `.vscode/settings.json` staged, and the citation
check needs `latex` staged or a staged line that actually adds a `main.tex:NNN`.
Every other commit pays just one `git diff` + `grep` (~10ms).

## The sweep: `run-sweep.sh`, `sweep_report.py`

```sh
scripts/run-sweep.sh [OUT_DIR]     # Release build, one ltlf-ek-bench run, then the report
BUDGET=60 scripts/run-sweep.sh     # a smoke run
```

`run-sweep.sh` writes `sweep.json` and `report.md` (default `build/benchout/sweep/`).
Defaults: repeat=5, 20 s per-case timeout, `ulimit -v` 8 GiB, a one-day budget so the
grid is never truncated; `N_MAX`, `BUDGET`, `MEM_KIB`, `SPOT_ROOT` override. The report
groups by family (all `slippery-*` arms are one), prints the prune ratio and the
speedup over ltlfsynt at each N, and headlines the largest N that finished on both
sides. `test_sweep_report.py` is its unit test (ctest `sweep_report_py`).

## The sweep on the HPC cluster: `scripts/hpc/`

The cluster (CentOS 7, gcc 10, no Spot) runs the sweep from an Apptainer image
instead of a native build.

- `ltlf-ek.def` — the Release image: Spot 2.15.1, MONA, `ltlf-ek-bench`.
- `sweep.sbatch` — a SLURM array, one family group per task (the three `slippery-*`
  arms together), two CPUs per task (one physical core; the cluster refuses `--exclusive`) on one node pinned with `sbatch -w NODE`, constrained to
  `--constraint=gold6130` with `--mem=16G`; each log opens with `hostname` and `lscpu`.
  Same settings as `run-sweep.sh`. `EK_DRY_RUN=1` prints the command.
- `merge_sweep.py` (in `scripts/`) — joins the parts into one sweep JSON; it refuses
  parts from different machines, builds, commits, repeat or timeout.
- `build-image.sh` — clones the repo and builds the image on the cluster's login
  node (the developer's machine has no apptainer); the log is `~/ltlf-ek-build.log` there.
- `wizard.sh` — the developer's half: build the image on the login node over ssh,
  `scp` `sweep.sbatch`, `sbatch`, fetch the parts, merge and write
  `docs/runs/<date>-four-family-sweep.{json,md}`. HEAD must be pushed, and the login node
  must be able to clone the repo URL (the wizard checks with `git ls-remote`; if it
  cannot, give it a URL it can read).

`test_build_image.sh` (ctest `build_image_sh`) runs `build-image.sh` against a stub
apptainer. `test_sweep_sbatch.sh` (ctest `sweep_sbatch_sh`) keeps the array's families, subjects,
repeat and timeout equal to `run-sweep.sh`'s, and checks the gold6130 constraint, the
memory reservation, the hostname/lscpu log and the wizard's gold6130 recon. The image compiles Spot and the bench
with one baseline, `-O3 -march=x86-64-v3`.
