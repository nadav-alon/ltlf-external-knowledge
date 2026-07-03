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

## Git hook (one-time setup per clone)

A fast `pre-commit` hook keeps `.vscode/settings.json` in sync. It is committed
under `.githooks/`; point git at it once after cloning:

```sh
git config core.hooksPath .githooks
```

The hook only runs the (Python) check when a commit stages the `latex`
submodule or `.vscode/settings.json`; every other commit pays just one
`git diff` + `grep` (~10ms).
