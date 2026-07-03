---
name: latex-style
description: LaTeX writing conventions for this project (main.tex and any file under latex/). Use WHENEVER creating or editing a .tex file — writing prose, definitions, theorems, algorithms, or \cl notes — so the output conforms. Three rules: no \paragraph (bold key ideas inline, \emph only for a term's first definition); macro-only notation; one sentence per source line. The theory-review skill defers to this skill for style.
---

# LaTeX style

The house style for this project's LaTeX. The paper is **`latex/main.tex`**, a
git submodule mirroring Overleaf (there is **no** root-level `main.tex`; when any
skill says `main.tex`, it means `latex/main.tex`). **Read and apply this before
editing any `.tex` file** — these hold
whether the edit comes from a general conversation, `/developer`, or
`/theory-review`. Do **not** launch a repo-wide reformat: apply the rules to the
prose you are **writing or touching**, and leave untouched blocks alone.

## The conventions

1. **No `\paragraph{}` — keep prose flowing.** `\paragraph` headers fragment the
   text and break the reader's flow. Let the prose run continuously and surface
   key ideas inline with `\textbf{}`. Reserve `\emph{}` for **introducing /
   defining a term at its first occurrence**; use `\textbf{}` for **in-flow
   emphasis** of an important concept or takeaway (the role a heading would have
   played). A normal blank-line paragraph break is fine; the banned thing is the
   `\paragraph{...}` run-in heading command.

2. **Macro-only notation.** Every recurring symbol goes through its macro
   (`\cons`, `\Tin`, `\Tout`, `\Ifree`, `\Iknown`, `\Sigma_0`, …) — never
   re-spell it raw (`\mathrm{cons}`, `T_{in}`, `\mathcal{I}_f`). A recurring
   notion with no macro yet → add one to the preamble. This mirrors the
   glossary's single-source discipline and pre-empts notation drift.

3. **One sentence per source line.** Start each sentence on its own line in the
   `.tex` source. Overleaf preserves the breaks and the compiled PDF is
   identical, but git diffs and `\cl`-note placement stay precise for the
   diff-driven `/theory-review`.

## Editing discipline

- Only the `\cl{...}` note command marks Claude-authored content (green, "CL:").
  Never use `\na`, `\sz`, `\spc`, `\df`, and never impersonate the author. See
  `/theory-review` for the full editing-under-`\cl` rules.
- **`\cl` placement — always on its own source line.** Never append a `\cl{...}`
  to the end of a prose sentence, a display `\]`, or an `align`/`algorithm`
  block. Put it on the next line, so the one-sentence-per-line diff stays clean
  and the note is easy to move or delete. (A `\todo` may be typeset anywhere in
  the source, so this is a source-hygiene rule, not a compile requirement.)
- **`\cl` style — reach for `[inline]` unless the note is a short flag.** The
  bare `\cl{...}` is a margin note; anything longer than a few words (multi-clause
  reasoning, adjacent display math, `\cref`s) overflows the margin, so use
  `\cl[inline]{...}` (renders as an in-text box; cf. the `\na[inline]` and
  `\cl[inline]` notes already in `main.tex`). The optional first arg passes
  straight through to `\todo`, so any other `todonotes` option (`[color=…]`,
  `[size=…]`) is available when appropriate. Keep bare margin `\cl{}` only for a
  terse one-line flag.
- Local build verification is unreliable — see the project memory note; the paper
  compiles on Overleaf, so review `.tex` edits by reading, not by running
  `pdflatex`/`latexmk`.
