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

## PRD template

```markdown
# PRD: <feature>

**Status:** draft
**Interface:** <e.g. implements Synthesis as DfaProduct>
**Recommended workflow:** <concurrent | sequential> — <one-line reason, from the Interface-contract freeze confidence>
**main.tex ref:** <section / \cref / algorithm>

**Gates:**
- [ ] glossary        — new terms in docs/GLOSSARY.md C++ column
- [ ] tests           — unit + oracle coverage
- [ ] code-review     — domain (/code-reviewer) + generic (/code-review)
- [ ] theory-review   — code ↔ math faithfulness vs main.tex

## Goal
<one paragraph: what capability, why>

## Ubiquitous-language terms used
<bullet list of glossary terms this feature touches; flag any missing from GLOSSARY.md>

## Behaviour / semantics (from main.tex)
<the invariants and steps that MUST hold, quoting the algorithm>

## Interface contract
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
- **Interface contract** carries a freeze confidence, names each type by its
  glossary term, and states the re-freeze path; the header **Recommended
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
- The **Interface contract** is frozen with a freeze confidence, and the header
  **Recommended workflow** field is set and consistent with it.
- Any PRD this supersedes is marked `superseded by …` and cross-linked (not
  deleted).
- For a large feature, an **Implementation phases** section splits it into
  independently landable, separately-sessioned phases (omitted for a small one).
- Self-review pass done; gaps reported.
- Any glossary gaps and touched theory questions are called out explicitly.
- End by telling the user the PRD is ready for `/developer` (and `/glossary`
  first if terms are missing).
