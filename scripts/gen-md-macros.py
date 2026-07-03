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

Only zero-argument, math-safe macros are mirrored.  Argument-taking macros
(\\cl, \\na, \\algname, ...) are skipped automatically: they are \\todo / \\text
based and cannot render in KaTeX anyway.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAIN_TEX = ROOT / "latex" / "main.tex"
SETTINGS = ROOT / ".vscode" / "settings.json"

# \newcommand{\Name}{body}  with NO argument spec after the name (so [n] macros
# are skipped) and a single-line body.
_NEWCMD = re.compile(r"^\s*\\newcommand\{\\([A-Za-z]+)\}\{(.*)\}\s*$")
# Bodies that cannot render in KaTeX (belt-and-braces; the arg check already
# excludes the \todo-based note macros).
_UNSAFE = re.compile(r"\\(todo|textbf|textsc|text|algname)\b")


def parse_macros(tex: str) -> dict:
    macros = {}
    for line in tex.splitlines():
        line = re.sub(r"(?<!\\)%.*$", "", line)  # drop trailing LaTeX comment
        m = _NEWCMD.match(line)
        if not m:
            continue
        name, body = m.group(1), m.group(2).strip()
        if "#" in body or _UNSAFE.search(body):
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
