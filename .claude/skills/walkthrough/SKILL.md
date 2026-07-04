---
name: walkthrough
description: After a PRD is implemented, give a live, code-first guided tour of how it was built — walk the major code components with file:line anchors, summarize the tests written, then save an as-built guide under docs/walkthroughs/. Minimal theory (the user wrote main.tex). Use to get familiar with a finished method's implementation.
---

# Walkthrough (as-built code tour)

Familiarize the user with **how a finished PRD was actually implemented**. This
is a **live, interactive, code-first tour**, not a math lecture — the user wrote
`main.tex` and knows the theory. "Major concepts" here means the **major code
components** (key structs, functions, control flow), not the math behind them.
The tour ends by saving a condensed **as-built guide** for later reference.

Runs **inline on the main (Opus) session** — it is interactive, so it is *not*
delegated to an agent.

## Pick the target PRD

1. If the user named a PRD slug (`/walkthrough dfa-product`), use
   `docs/prd/<slug>.md`.
2. If not, list the PRDs in `docs/prd/` whose **`Status:`** is `implemented — …`
   and ask which one. Do not tour a `draft` PRD without warning — there may be
   little or no code yet; confirm before proceeding.

## Load the map (do not skip)

The PRD is your index into the code — read it first:

- **`Interface:`** and **main.tex ref** lines → the entry points and the header
  the feature plugs into.
- **Ubiquitous-language terms used** / **Glossary** sections → the concept →
  `main.tex` symbol → **C++ identifier** map. Use the C++ column to `grep` the
  real definitions; anchor everything you say to the canonical names.
- **`tests` gate** → the test files to summarize.
- **Developer comments / PRD disagreements** section, if present → call out where
  the code deliberately diverged from the spec.

Then read the actual code (`src/`, `include/ltlf_ek/`) and tests for those
identifiers. Prefer `file:line` anchors over paraphrase.

## The tour (interactive, code-first)

Walk the **major components in dependency order**, one section at a time, pausing
between sections so the user can ask "why", "show me that function", "what
happens if…", or say "next". Keep theory to a one-line orientation per component
(cite the `main.tex` symbol, don't re-derive it).

For each major component:

- **What it is** — the C++ type/function and the one glossary term it realizes,
  with a `file:line` anchor.
- **How it works** — the control flow / data shape that matters (e.g. "λ is a
  per-state BDD relation, so `consistent` intersects in one `bdd` op"), and any
  non-obvious Spot/BDD idiom or invariant (`cons` filter, ⊥ sink self-loop,
  final-state progression bit, aggregation keyed on `[psi]`).
- **Design decisions** — why it's built this way; anything that reads oddly until
  you know the reason.

Do not exhaustively narrate every line — curate the pieces that carry the method.

## Summarize the tests written

From the `tests` gate files, give a digestible summary — **the point of the tour
the user asked for**:

- **Unit fixtures** — one line each: the function under test and what the case
  pins down.
- **Domain oracles — call these out specially**, since they carry the real
  correctness argument:
  - **metamorphic cross-method equivalence** — which methods must agree on
    realizability / strategy-equivalence, and the aggregation **asymmetry**
    (incomplete-but-never-wrong: implication, not equality);
  - **controller verifier** — the universal post-condition (every trace
    agreeing with `t_in, t_out, T_C` satisfies `phi`);
  - **monolithic baseline** — the coarse fallback and its caveats.
- Note any **gap** — a function or edge case (partiality, unrealizable, ⊥ sink,
  empty variable sets, aggregation knowledge-loss) that is *not* yet covered.

## Save the as-built guide

When the tour ends, write **`docs/walkthroughs/<slug>.md`** (create the
directory if needed). This is an **as-built code guide** that *complements* the
PRD — it records **how/where in the code**, not the spec's what/why, so do not
restate the PRD's Goal or requirements. Structure:

- A one-line pointer back to `docs/prd/<slug>.md` and the implementing commit.
- **Major components** — the curated list, each with its glossary term and a
  `file:line` anchor and a one-sentence "how".
- **Tests** — the summary above, oracles flagged, gaps noted.

Do **not** modify the PRD file itself. Keep the doc condensed — it is a map into
the code, not a copy of it.
