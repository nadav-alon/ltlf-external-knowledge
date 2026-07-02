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
   - **Interface fit**: does it implement `Synthesis`? new base types? reused
     black-boxes (`LtlfToDfa`, `SolveDfa`, `progress`) — implement now or stub?
   - **Semantics to preserve**: the exact invariants from `main.tex` (e.g. the
     `cons` filter, the ⊥ sink for Method 2, aggregation-loses-knowledge).
   - **Edge cases**: partial/undefined transducers, empty alphabet, unrealizable.
   - **Oracles**: how will `/test-writer` know it's correct? (unit fixtures,
     cross-method equivalence, controller verifier, monolithic baseline).
   - **Open questions**: any `\na`/stub in `main.tex` this touches — flag, don't
     resolve here (that's `/theory-review`).
   - **Definition of done** for the feature.

3. **Write the PRD** to `docs/prd/<kebab-feature-name>.md` using the template
   below. Use only glossary terms; reference `main.tex` by `\cref` label /
   algorithm name and the glossary C++ identifiers.

## PRD template

```markdown
# PRD: <feature>

**Status:** draft · **main.tex ref:** <section / \cref / algorithm>
**Interface:** <e.g. implements Synthesis as DfaProduct>

## Goal
<one paragraph: what capability, why>

## Ubiquitous-language terms used
<bullet list of glossary terms this feature touches; flag any missing from GLOSSARY.md>

## Behaviour / semantics (from main.tex)
<the invariants and steps that MUST hold, quoting the algorithm>

## Interfaces & types
<signatures to add/implement; black-boxes to stub vs implement>

## Edge cases
<partial transducers, unrealizable, empty sets, sink, aggregation asymmetry, ...>

## Test oracles (for /test-writer)
<unit fixtures; metamorphic cross-method; verifier post-condition; baseline>

## Open theory questions touched
<related \na / stubs; leave for /theory-review>

## Definition of done
<compiles; tests; glossary updated; ...>
```

## Definition of done

- `docs/prd/<feature>.md` written from the template, in ubiquitous language.
- Any glossary gaps and touched theory questions are called out explicitly.
- End by telling the user the PRD is ready for `/developer` (and `/glossary`
  first if terms are missing).
