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
