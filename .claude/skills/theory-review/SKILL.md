---
name: theory-review
description: Review the theory of this LTLf-external-knowledge project — either code↔math faithfulness, LaTeX internal soundness, or both, auto-selected by what changed. main.tex is the primary but fallible reference; classify each mismatch as code-bug / doc-bug / underspecified and may edit main.tex only under \cl notes. Use to check a method matches the math, or that a main.tex edit is sound.
---

# Theory review

`main.tex` is the **primary reference but not word of god**. Every mismatch is
classified, and a *doc* problem is fixed in the doc (never by silently changing
code behaviour). This skill is the single source of truth for theory review; the
`theory-reviewer` agent is just a wrapper that runs it.

## Mode — auto-select by the diff/scope

- Diff touches **only `main.tex`** → **soundness mode**.
- Diff touches **only semantic code** → **faithfulness mode**.
- **Both** → run both, then reconcile them against each other.

If invoked without a diff, ask the user which target (code, `main.tex`, or a
specific method), then pick the mode.

## Always load first

- `docs/GLOSSARY.md` (the math ↔ prose ↔ C++ mapping) and its **Open theory
  questions** section — you are *seeded* with the known `\na`/stubs (the `FP`
  stub, the aggregated-`F_P`-overwrite doubt, on-the-fly game solving, the
  line-84 parameter gap). Engage with these; do **not** re-flag them as novel
  discoveries.
- The relevant `latex/main.tex` algorithm/definition blocks (the paper lives in
  the `latex/` submodule mirroring Overleaf).

## Faithfulness mode (reference = math, subject = code)

Check the implementation realizes the math exactly:
- `consistent` implements `cons` (including the visible-slice projection).
- Product construction, the ⊥ sink (Method 2) vs skip (OTF), final-state
  classification via the progression bit, aggregation keyed on `[psi]`.
- Complexity claims (secondary): does the built product actually stay linear in
  `|Q_in|·|Q_out|` (the reachability invariant `R × {q_in} × {q_out}`)? Flag,
  don't block.

## Soundness mode (reference = internal logic, subject = the LaTeX)

Check the math is internally sound, independent of code:
- New/edited definitions are well-formed and typecheck against the signatures.
- Proofs actually establish their claim (e.g. the exponential-only-in-φ bound).
- Notation is used consistently with its definition (`cons`, `[psi]`, the four
  variable sets); an edit didn't break a downstream `\cref`ed claim.
- Algorithms are correct (e.g. is the aggregated `F_P` insert really safe?).
- Touched blocks conform to the **LaTeX writing conventions** (below) — report
  any violation as a non-blocking **style** nit alongside the correctness verdicts.

## Verdicts

Report each mismatch with one of:
- **`code-bug`** — code diverges from correct math → describe the fix (do not
  necessarily apply it here; that's `/developer`).
- **`doc-bug`** — the math/LaTeX is wrong → draft a concrete `main.tex` edit.
- **`underspecified`** — the doc is silent/ambiguous → draft a `\cl` note or a
  proposed definition.

## Editing main.tex — only under \cl

You may edit `main.tex`, but **only** using the `\cl{...}` note command
(green, "CL:") already defined in the preamble — never `\na`, `\sz`, `\spc`,
`\df`, and never impersonate the author. Prefer a `\cl` note flagging the issue;
make substantive prose/definition edits only when the user asked you to, and
still annotate them with an adjacent `\cl`.

Format every `\cl` per the **`latex-style`** skill: put it **on its own source
line** (never appended to a prose sentence, a display `\]`, or an
`align`/`algorithm` block), and use **`\cl[inline]{...}`** for anything beyond a
short one-line flag (multi-clause notes, adjacent math, `\cref`s) so it does not
overflow the margin.

When **spawned by `/code-reviewer`**: report verdicts and any proposed `\cl`
patch back to the caller; do **not** commit LaTeX edits mid-review.

## LaTeX writing conventions

The house LaTeX style lives in the **`latex-style`** skill (single source: no
`\paragraph`, macro-only notation, one sentence per source line). In review
context, apply it as follows:

- Report a violation as a non-blocking **style** nit — never a
  `code-bug`/`doc-bug`/`underspecified` correctness verdict.
- Flag violations **only on the blocks the diff touches**; never launch a
  repo-wide reformat.
- Follow the conventions in your own `\cl` edits.
- Fixes obey the same `\cl`-only editing rule: flag + draft the edit, apply
  reformatting only when the user asked.

## Definition of done

- Every mismatch has a verdict (`code-bug` / `doc-bug` / `underspecified`).
- Doc problems come with a drafted `\cl` note / edit; code problems with a
  described fix.
- Known open questions are engaged with, not rediscovered.
- In **faithfulness mode**, if a `docs/prd/` PRD backs the reviewed code, tick
  its **`theory-review`** gate with the ref **only when no `code-bug` remains**
  (leave unchecked while any is open). When **spawned by `/code-reviewer`**,
  don't tick it yourself — report the clean/not-clean verdict and let the caller
  tick. Gate vocabulary is defined in `/grill-prd`.
