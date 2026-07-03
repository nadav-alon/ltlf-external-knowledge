---
name: glossary
description: Maintain the project's ubiquitous-language glossary (docs/GLOSSARY.md) as a 3-column math ↔ prose ↔ C++ mapping. Use when adding/refining a domain term, reconciling a LaTeX symbol with a C++ identifier, or when naming drift is suspected. Interviews grill-style before writing.
---

# Glossary maintenance

`docs/GLOSSARY.md` is the single source of truth for this project's vocabulary.
The paper itself lives at **`latex/main.tex`** (git submodule → Overleaf); every
`main.tex` reference below means that file. Every domain concept has **one**
canonical English term, **one** `main.tex` symbol, and **one** canonical C++
identifier. Your job is to keep those three in
sync and kill synonyms before they spread.

## When invoked

1. **Read `docs/GLOSSARY.md` in full** and skim `latex/main.tex` (the paper, a
   submodule mirroring Overleaf) for the symbol(s) in question. Never propose a
   term without checking what `main.tex` already calls it.
2. If the request is vague ("add a term for X"), **grill the user** one question
   at a time until the entry is unambiguous. Resolve, in order:
   - What does `main.tex` call it, and where is it defined (`\cref` label / section)?
   - One-sentence definition — does it already overlap an existing entry?
   - The canonical C++ identifier (type / function / member). Does code already
     use a *different* name? If so, which one wins, and is the loser now a
     "do not call it"?
   - What rejected synonyms should go on the **Do not call it** line?
   For each question, give your recommended answer.
3. **Write the entry** in the existing format (Term / `main.tex` / Definition /
   C++ / Do not call it), under the right `#` group. Keep entries alphabetical-ish
   within a group only if the file already is; otherwise append logically.

## Rules

- **Use the `main.tex` macros directly in math — never hand-expand.** Write
  `$\Iknown$`, `$\Tin$`, `$\cons$` in glossary math exactly as the paper does,
  *not* the expansion (`\mathcal{I}_{k}`, `T_{\mathit{Inp}}`, `\mathrm{cons}`).
  The VS Code Markdown preview resolves these from `.vscode/settings.json`, which
  `scripts/gen-md-macros.py` generates from `main.tex`'s `\newcommand`s (a
  pre-commit hook + CI keep it in sync). Hand-expanding re-duplicates a macro
  body that then silently drifts when the definition changes — so only spell out
  notation that has *no* macro. (Caveat: GitHub's web view won't load the
  workspace macros; VS Code is the reading surface.)
- **Three columns are mandatory.** If a term has no `main.tex` symbol yet, say so
  explicitly (`— (no symbol; code-only)`) rather than leaving it blank.
- **One canonical C++ name.** If you discover two names in code for one concept,
  that is a finding: record the winner, add the loser to "Do not call it", and
  tell the user which files need renaming (do not rename silently here).
- **Do not invent theory.** If `main.tex` is silent or contradictory on a term,
  flag it for `/theory-review` instead of guessing.
- Keep the **Open theory questions** section current: if a term relates to a
  known `\na`/stub, cross-reference it.

## Definition of done

- The entry (or edit) is in `docs/GLOSSARY.md` with all three columns + the
  "Do not call it" line.
- Any code/`main.tex` rename implied by the change is listed for the user (not
  performed here).
- If new drift or a theory gap was uncovered, it is named explicitly.
