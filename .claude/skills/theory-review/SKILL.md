---
name: theory-review
description: Review the theory of this LTLf-external-knowledge project — either code↔math faithfulness, LaTeX internal soundness, or both, auto-selected by what changed. main.tex is the primary but fallible reference; classify each mismatch as code-bug / doc-bug / underspecified and may edit main.tex only under \cl notes. Use to check a method matches the math, or that a main.tex edit is sound.
---

# Theory review

`main.tex` is the **primary reference but not word of god**. Every mismatch is
classified, and a *doc* problem is fixed in the doc (never by silently changing
code behaviour). This skill is the single source of truth for theory review; the
`theory-reviewer` agent is just a wrapper that runs it.

## Scope & spawn discipline (read first)

- **Stay on the diff; don't re-derive what's settled.** Review the blocks the
  diff touches, not the whole paper. The known open questions (below) are
  *seeded* — engage with them, do **not** re-flag them as novel discoveries or
  re-prove them from scratch. A conclusion the PRD or a prior review already
  reached is an input, not something to re-establish before you can use it.
- **When spawning this skill** (main session or `/code-reviewer`): keep the
  prompt tight — name the diff/method and defer to this skill, which the agent
  reads. Do **not** restate these steps back at the agent; echoing "check X,
  verify Y" reopens settled questions and burns tokens on re-derivation. State
  only what's authoritative and what the target is.

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
`\df`, and never impersonate the author. Make substantive prose/definition
edits only when the user asked you to.

**Write the notes — do not merely propose them.** A `\cl` note you decided to
write is an edit you make, in the file, by default. Reporting a note as a
patch-to-be-applied is the *exception*, and it needs a reason beyond caution
(the surrounding block is mid-rewrite, the placement genuinely depends on an
answer only the user has). "It's the author's paper" is not that reason —
`\cl` exists precisely so Claude-authored text is visible and trivially
revertable, which is what makes writing it the safe default rather than the
bold one. A note that lives only in a chat message or a PRD appendix is lost
the moment the session ends; a note in `main.tex` is one `git checkout` from
being undone. Say plainly in your report what you wrote and where.

**Writing is not landing.** `latex/` is the Overleaf submodule, so do **not**
`git commit` or `git push` it — that is outward-facing and the user's call.
Leave the edit in the working tree, tell the user the submodule is dirty, and
flag that added lines shift the `main.tex:NNN` refs other docs cite (the
`\cref`/§-number resync lesson in `docs/BACKLOG.md`).

Every such edit must stay clearly visible, per the **`latex-style`** skill: wrap
new/changed **prose** directly inside `\cl[inline]{...}` (the note *is* the
text); for a **live equation, algorithm line, or theorem/lemma/proof
environment** that can't be nested inside `\cl` without risking the build or
its `\label`/`\cref`, make the correction in place and add an **adjacent**
`\cl[inline]{...}` note describing exactly what changed. Never leave a
substantive edit as silent, unflagged prose.

Format every `\cl` per the **`latex-style`** skill: put it **on its own source
line** (never appended to a prose sentence, a display `\]`, or an
`align`/`algorithm` block), and use **`\cl[inline]{...}`** for anything beyond a
short one-line flag (multi-clause notes, adjacent math, `\cref`s) so it does not
overflow the margin.

When **spawned by `/code-reviewer`**: write the `\cl` notes as above, then
report the verdicts *and* what you wrote (file + placement) back to the caller
so it can fold them into the review summary. Still no `git commit`/`git push`
of the submodule mid-review.

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
