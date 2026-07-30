#!/usr/bin/env python3
"""Sync VS Code's Markdown-preview KaTeX macros with the paper's notation.

Single source of truth = latex/main.tex.  The paper defines its notation as
zero-arg \\newcommand macros (\\Tin, \\Ifree, \\cons, ...).  So that the Markdown
docs (docs/GLOSSARY.md, docs/prd/*.md) can use those *same* macros instead of
hand-expanding them (which silently drifts when a definition changes), this
script mirrors them into `.vscode/settings.json` under `markdown.math.macros`,
which the built-in VS Code Markdown preview feeds to KaTeX.

Usage:
  gen-md-macros.py           # (re)write .vscode/settings.json
  gen-md-macros.py --check   # exit 1 if it is stale (used by the pre-commit hook / CI)

Math-safe macros are mirrored whether or not they take arguments: KaTeX supports
`#1`..`#9` in a macro body, so `\\liveset` -> `R_{#1}` renders in the preview
exactly as it does in the paper.  Two families are skipped automatically:

  * macros with an OPTIONAL argument, i.e. `\\newcommand{\\na}[2][]{...}` --- KaTeX
    has no `[n][default]` equivalent, and these are the \\todo-based note macros
    (\\cl, \\na, \\sz, \\spc, \\df) which cannot render anyway;
  * macros whose body is not math-renderable (\\todo / \\text / \\textbf / \\textsc /
    \\algname bodies), per _UNSAFE below.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAIN_TEX = ROOT / "latex" / "main.tex"
SETTINGS = ROOT / ".vscode" / "settings.json"

# \newcommand{\Name}{body} or \newcommand{\Name}[n]{body}, single-line body.
# The optional-argument form \newcommand{\Name}[n][default]{body} deliberately
# does NOT match: after `[n]` this requires `{`, so the \todo-based note macros
# (\cl, \na, ...) are excluded by the grammar rather than by a special case.
_NEWCMD = re.compile(r"^\s*\\newcommand\{\\([A-Za-z]+)\}(?:\[([0-9])\])?\{(.*)\}\s*$")
# Bodies that cannot render in KaTeX (e.g. \algname, which takes an argument but
# expands through \textbf/\textsc).
_UNSAFE = re.compile(r"\\(todo|textbf|textsc|text|algname)\b")
# A parameter reference in a macro body.
_PARAM = re.compile(r"#(.?)")


def parse_macros(tex: str) -> dict:
    macros = {}
    for line in tex.splitlines():
        line = re.sub(r"(?<!\\)%.*$", "", line)  # drop trailing LaTeX comment
        m = _NEWCMD.match(line)
        if not m:
            continue
        name, nargs, body = m.group(1), int(m.group(2) or 0), m.group(3).strip()
        if _UNSAFE.search(body):
            continue
        # Every #k in the body must be a declared parameter, or KaTeX would
        # render the macro wrong rather than fail loudly.
        if any(not k.isdigit() or not 1 <= int(k) <= nargs
               for k in _PARAM.findall(body)):
            continue
        macros["\\" + name] = body
    return macros


def main() -> int:
    check = "--check" in sys.argv[1:]
    macros = parse_macros(MAIN_TEX.read_text())

    if check:
        if not SETTINGS.exists():
            print("gen-md-macros: .vscode/settings.json missing", file=sys.stderr)
            return 1
        current = json.loads(SETTINGS.read_text()).get("markdown.math.macros", {})
        if current != macros:
            print(
                "gen-md-macros: .vscode/settings.json out of sync with "
                "latex/main.tex — run: python3 scripts/gen-md-macros.py",
                file=sys.stderr,
            )
            return 1
        return 0

    settings = {}
    if SETTINGS.exists():
        settings = json.loads(SETTINGS.read_text())  # preserve other settings
    settings["markdown.math.macros"] = macros
    SETTINGS.parent.mkdir(exist_ok=True)
    SETTINGS.write_text(json.dumps(settings, indent=2, ensure_ascii=False) + "\n")
    print(f"gen-md-macros: wrote {len(macros)} macros to {SETTINGS.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
