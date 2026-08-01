---
name: grill-prd
description: Grill-me wrapper that interviews the user about a feature/method to implement, speaking the project's ubiquitous language (docs/GLOSSARY.md), and emits a PRD under docs/prd/ that the /developer skill consumes. Use when starting a new method, algorithm, or feature and you want a spec before coding.
---

# Grill → PRD (in ubiquitous language)

Produce a **Product Requirements Document** for one feature/method, by
interviewing the user grill-style, using the project's canonical vocabulary so
the PRD hands cleanly to `/developer`. The paper `main.tex` lives at
**`latex/main.tex`** (git submodule → Overleaf).

## Procedure

1. **Load context first (do not skip):**
   - Read `docs/GLOSSARY.md` in full — every term you use in questions and in
     the PRD must be the canonical one. If the feature needs a concept not yet
     in the glossary, note it and, at the end, tell the user to run `/glossary`.
   - Read the relevant part of `latex/main.tex` (the paper, a submodule
     mirroring Overleaf) for the method/feature (e.g. the algorithm block and
     its surrounding definitions).
   - Skim `include/ltlf_ek/` for the interfaces the feature must fit
     (`Synthesis`, `Transducer`, `VariablePartition`, existing methods).

2. **Interview relentlessly, one question at a time, with a recommendation each**
   (this is the grill-me contract). Walk the decision tree; resolve dependencies
   before moving on. Cover at least:
   - **Scope**: which `main.tex` algorithm/section, and which parts are in vs
     out of this PRD.
   - **Phasing (for large features).** `/developer` runs on a cold Sonnet agent
     whose token cost scales with turns × context; a PRD big enough to push one
     spawn past ~40 turns / a large accumulated context is cheaper implemented as
     **several phases done in separate sessions**. When the feature is that big,
     grill out a phase breakdown: each phase must be **independently landable** —
     it compiles green, has its own testable checkpoint, and leaves the tree in a
     valid state (later phases may stub what they don't yet need). Order phases so
     each builds on the last (types/interfaces → core algorithm → edge cases /
     aggregation → CLI wiring, as fits). A small, self-contained feature needs no
     phasing — don't invent phases where one session suffices.
   - **Interface fit & freeze**: does it implement `Synthesis`? new base types?
     reused black-boxes (`LtlfToDfa`, `SolveDfa`, `progress`) — implement now or
     stub? Then **pin the exact public signature** — parameter order, return
     type, const-ness, which header — naming each type by its **glossary term**
     rather than eliciting raw C++ (the user need not author C++; propose the
     shape from the glossary types and confirm it). This frozen signature is the
     contract both `/developer` and `/test-writer` bind to, so both can be
     written against it without one reading the other's code. Judge a **freeze
     confidence**: *high* when the signature falls straight out of the glossary
     types (a thin wrapper over an existing Spot construct), *tentative* when the
     interface is genuinely being invented here and implementation is likely to
     revise it. Record how the contract re-freezes if coding proves it wrong: it
     is a **PRD-change event** that updates the contract and propagates to any
     in-flight test branch — never a unilateral re-shape on the dev branch.
   - **Workflow recommendation (advisory, derived from freeze confidence)**: how
     should `/developer` and `/test-writer` run? *high* → **concurrent** — they
     run on separate branches from the frozen contract (mostly-disjoint
     territories: `src/`+`include/` vs `test/`, so file merges stay clean), and
     if the tests land first the developer finishes red→green TDD-style against
     them. *tentative* → **sequential** — developer first, so the test-writer
     binds to the real signature and no branch is locked to a contract that
     churned. This governs the **per-function unit tests and the developer-TDD
     path only**; the **domain oracles parallelize regardless**, since they bind
     to the public interface and the math, never internals. It is a
     recommendation, not an order — the launcher may override.
   - **Semantics to preserve**: the exact invariants from `main.tex` (e.g. the
     `cons` filter, the ⊥ sink for Method 2, aggregation-loses-knowledge).
   - **Novel mechanisms — grill to the code.** When the feature introduces an
     algorithm/mechanism **not lifted verbatim from `main.tex`** (a bespoke
     guard, check, driver, encoding), a sketch-level pseudocode block is **not
     enough** — it leaves load-bearing decisions for `/developer` to *discover*
     at implementation time (the exact iteration bounds and their boundary
     behaviour, the return/result type, what counts as pass vs fail, how
     don't-cares/empty/degenerate inputs are handled, determinism/seed). Flag any
     such mechanism during the interview and drill each of these until the PRD
     pins them down, or explicitly records the decision as deferred-to-developer
     with a rationale. A rule of thumb: if the developer would have to *run it to
     find out* how it should behave, the PRD is underspecified.
   - **Edge cases**: partial/undefined transducers, empty alphabet, unrealizable.
   - **Oracles**: how will `/test-writer` know it's correct? (unit fixtures,
     cross-method equivalence, controller verifier, monolithic baseline).
   - **Open questions**: any `\na`/stub in `main.tex` this touches — flag, don't
     resolve here (that's `/theory-review`).
   - **Definition of done** for the feature.

3. **Check for a PRD this one supersedes.** Skim `docs/prd/` for an existing PRD
   covering the same feature/method. If this PRD replaces or substantially
   overlaps one, mark the **old** one `**Status:** superseded by <this-prd-name>`
   and add a one-line pointer to it; reference the old one from this PRD's Goal.
   Never delete the old file — supersession is a link, not a deletion.

4. **Write the PRD** to `docs/prd/<kebab-feature-name>.md` using the template
   below. Use only glossary terms; reference `main.tex` by `\cref` label /
   algorithm name and the glossary C++ identifiers.
   - **Use the `main.tex` macros directly in math — never hand-expand** (same
     rule as `/glossary`). Write `$\Tin$`, `$\Ifree$`, `$\cons$`, … exactly as
     the paper; do **not** expand them (`T_{\mathit{Inp}}`, `\mathcal{I}_{f}`,
     `\mathrm{cons}`). The VS Code Markdown preview resolves them from the
     generated `.vscode/settings.json` (`scripts/gen-md-macros.py`, kept in sync
     by a pre-commit hook + CI). Only spell out notation that has no macro; name
     a `\cref` label in backticks as prose.

## Status & gates

A PRD carries two orthogonal axes, both in its header block:

- **`Status:`** — the *lifecycle* of the document (one value):
  `draft` → `implemented — <commit/PR>` → `superseded by <prd-name>` /
  `abandoned`. `draft` while being written or awaiting `/developer`;
  `implemented` once code lands (set by `/developer`); `superseded`/`abandoned`
  retire it. **Never delete an implemented or superseded PRD** — its dated
  "Developer comments / PRD disagreements" are an archival decision record.

- **`Gates:`** — a *checklist* of independent quality passes, each ticked by the
  skill that performs it, each carrying a ref (commit/PR) so a stale pass is
  visible (gate ref older than the code it covers → re-run). Exactly these four,
  no more (don't add `verify`/`simplify` — those are actions, not durable
  states): `glossary`, `tests`, `code-review`, `theory-review`.

Emit every new PRD with `Status: draft` and all four gates unchecked.

## Workflow recommendation — how the field is consumed

The header's **`Recommended workflow:`** field tells whoever implements the PRD
how `/developer` and `/test-writer` run. It is derived from the *Interfaces &
types* **freeze confidence** and is **advisory** — the launcher may override. It
governs only the per-function unit tests and the developer-TDD path; the **domain
oracles parallelize regardless**, since they bind to the public interface and the
math, never internals. The two values:

- **`sequential`** (from a *tentative* freeze): `/developer` first, then
  `/test-writer` against the real signatures — the current linear order. Use when
  implementation is likely to revise the interface, so the test-writer binds to
  the settled shape and no branch is locked to a contract that churned.
- **`concurrent`** (from a *high* freeze): spawn `/developer` and `/test-writer`
  at the same time, each on its **own worktree**, both bound to the frozen
  *Interfaces & types* contract. `src/`+`include/` and `test/` are disjoint
  territories, so the branches merge clean — but **never `git add -A` while both
  worktrees are live**. The **launcher owns integration**: merge both, build, and
  run `ctest` once (foreground). If the tests land before the implementation,
  `/developer` finishes **red→green** against them. If implementation proves the
  contract wrong, that is a **PRD-change event** — update *Interfaces & types* and
  propagate to the in-flight test branch; neither agent reshapes the interface
  unilaterally on its own branch.

The spawning mechanics live in the `/developer` and `/test-writer` delegation
guards, which defer here for what the modes mean.

## PRD template

```markdown
# PRD: <feature>

**Status:** draft
**Interface:** <e.g. implements Synthesis as DfaProduct>
**Recommended workflow:** <concurrent | sequential> — <one-line reason, from the *Interfaces & types* freeze confidence>
**main.tex ref:** <section / \cref / algorithm>

**Gates:**
- [ ] glossary        — new terms in docs/GLOSSARY.md C++ column
- [ ] tests           — unit + oracle coverage
- [ ] code-review     — domain (/code-reviewer) + generic (/code-review)
- [ ] theory-review   — code ↔ math faithfulness vs main.tex

**Unattended-ready:** <yes | no — what the user must still decide>

## Stop-list
<Conditions under which an unattended run must STOP rather than guess. One line
each. Always includes anything the grill left genuinely open. If there is
nothing, say "none — every decision in this PRD is closed.">

## Goal
<one paragraph: what capability, why>

## Ubiquitous-language terms used
<bullet list of glossary terms this feature touches; flag any missing from GLOSSARY.md>

## Behaviour / semantics (from main.tex)
<the invariants and steps that MUST hold, quoting the algorithm>

## Interfaces & types
**Freeze confidence: <high | tentative>.** high = the signature falls straight
out of the glossary types (thin wrapper over an existing Spot construct);
tentative = the interface is being invented here and implementation may revise it.

<exact signatures to add/implement — parameter order, return type, const-ness,
which header — naming each type by its glossary term rather than restating it.
Black-boxes: implement now or stub.>

**If implementation proves this contract wrong:** that is a PRD-change event —
update this section and propagate to any in-flight test branch; the developer
does not silently re-shape the interface on its own branch.

## Implementation phases
<Omit this section entirely for a small feature that fits one /developer session.
For a large PRD, list ordered phases, each a separate implementation session:
- **Phase N — <name>**: what lands, and its green checkpoint (compiles; which
  tests/oracles pass). Note what it may stub for a later phase.
Each phase must leave the tree compiling and independently testable.>

## Edge cases
<partial transducers, unrealizable, empty sets, sink, aggregation asymmetry, ...>

## Test oracles (for /test-writer)
<unit fixtures; metamorphic cross-method; verifier post-condition; baseline>

## Open theory questions touched
<related \na / stubs; leave for /theory-review>

## Definition of done
<compiles; tests; glossary updated; ...>
```

## Self-review before handing off

After writing, re-read the drafted PRD once against its own template as a
standalone artifact — the reader will be `/developer`, who wasn't in the
interview. Flag (don't silently fix) anything that would make it
under-implementable:

- Every template section present and non-empty; no `<placeholder>` left in.
- Each stated invariant traces to a `main.tex` `\cref`/algorithm — no floating
  "should" with no source.
- Edge cases and test oracles are concrete enough for `/test-writer` to act on.
- Every domain term used is in `docs/GLOSSARY.md` (or listed as a gap).
- Interfaces name real types/signatures, not vague "some function".
- The **Interfaces & types** section carries a freeze confidence, names each type
  by its glossary term, and states the re-freeze path; the header **Recommended
  workflow** is consistent with it (high→concurrent, tentative→sequential).
- **Any bespoke algorithm/mechanism** (not lifted from `main.tex`) is specified
  past sketch level: iteration bounds and their boundary behaviour, result type,
  pass/fail condition, don't-care/empty/degenerate handling, and determinism are
  all pinned (or explicitly deferred with rationale). If the developer would have
  to run it to learn how it should behave, that's an under-implementable gap —
  flag it and grill it out before handoff, don't ship it to `/developer`.
- **If phased:** each phase is independently landable (green checkpoint, tree
  compiles) and the phases together cover the whole PRD with no orphaned work.
  If the whole feature comfortably fits one session, there should be **no**
  phases section — don't over-split.

Report the gaps to the user; fix the cheap ones, leave genuine open questions
for `/theory-review`. This is a read-through, not a second interview.

## Definition of done

- `docs/prd/<feature>.md` written from the template, in ubiquitous language,
  with `Status: draft` and the four unchecked gates in the header.
- The **Interfaces & types** section is frozen with a freeze confidence, and the
  header **Recommended workflow** field is set and consistent with it.
- Any PRD this supersedes is marked `superseded by …` and cross-linked (not
  deleted).
- For a large feature, an **Implementation phases** section splits it into
  independently landable, separately-sessioned phases (omitted for a small one).
- Self-review pass done; gaps reported.
- Any glossary gaps and touched theory questions are called out explicitly.
- The **Unattended-ready** field and the **Stop-list** are filled in (see below).
- End by telling the user the PRD is ready for `/developer` (and `/glossary`
  first if terms are missing) — and whether `/launcher` can run it unattended.

## Unattended-readiness — close the decisions tonight

This PRD may be handed to `/launcher` and run while the user is at work, with
nobody available to answer anything. **Every question left open here becomes a
stalled workday.** The single most common stall: `/developer` hits a domain
concept with no `docs/GLOSSARY.md` entry, and stops to run `/glossary` — which
interviews the user. Naming is the user's call, so it must be settled *now*.

Before setting **Unattended-ready: yes**, all of these must hold:

1. **Interfaces & types** frozen — no `<placeholder>`, no "decide during
   implementation".
2. **Every** domain identifier the PRD introduces is already in
   `docs/GLOSSARY.md`, spelled exactly. If any are missing, run `/glossary`
   *this evening* — do not defer it into the run.
3. Each phase has a **machine-checkable** green checkpoint: it compiles, and
   named tests/oracles pass. "Looks right" is not a checkpoint.
4. Open theory questions are either resolved or explicitly listed in the
   **Stop-list**. An unattended run must never resolve a `main.tex` ambiguity.
5. The **Recommended workflow** field is set — the launcher needs it to decide
   whether to parallelize `/developer` and `/test-writer`.

If any fail, set **Unattended-ready: no** and say precisely what the user must
decide. That is a useful answer, not a failure — the alternative is an agent
guessing at the user's mathematics for eight hours.
