---
name: developer
description: Implement a synthesis method or feature for this LTLf-external-knowledge C++ project, against a PRD (docs/prd/) and the ubiquitous-language glossary, using thin wrappers over Spot. Use when writing or extending C++ in include/ltlf_ek and src/. Enforces glossary naming; ends by suggesting tests + review.
---

# Developer

Implement C++ for this project. Architecture: **thin domain wrappers over Spot**
(`spot::twa_graph`, `spot::formula`, `bdd` letters) with **custom types for the
non-standard pieces** (lambda-split `Transducer`, NFAs, product states). All five
methods share `Synthesis::synthesize(phi, vars, t_in, t_out)`.

## Before writing code

1. **Read the PRD** for this feature in `docs/prd/` if one exists. If none
   exists and the task is non-trivial, recommend `/grill-prd` first.
2. **Read `docs/GLOSSARY.md`.** Names in code MUST match the C++ column. If a
   concept has no entry, you may not invent a name — stop and run `/glossary`
   (or tell the user to), then continue.
3. **Read the relevant `main.tex` algorithm** and the existing interfaces in
   `include/ltlf_ek/`. Fit the existing `Synthesis` / `Transducer` shapes; do
   not fork parallel abstractions.

## While writing

- **Spot, not reinvention.** Use Spot for automata/BDD/formula machinery; only
  hand-roll what Spot lacks (the lambda-split output, NFA-as-domain-type, product
  bookkeeping). Prefer `spot::twa_graph_ptr`, `bdd` cubes for letters, the shared
  `spot::bdd_dict`.
- **Glossary discipline (enforced).** Every new public identifier naming a domain
  concept must already be in `docs/GLOSSARY.md` under the C++ column, spelled
  exactly. Never introduce a synonym on the "Do not call it" list. New concept →
  glossary first.
- **Match `main.tex` semantics exactly**: the `cons` filter, the ⊥ sink
  self-loop (Method 2), consistency-skip for partial transducers (OTF),
  final-state classification via the progression bit, aggregation keyed on
  `[psi]` alone. If the code must diverge from `main.tex`, do **not** silently
  paper over it — surface it for `/theory-review`.
- **Style:** match surrounding code (namespace `ltlf_ek`, `.hpp` headers with
  `#pragma once`, doc-comments that cite the `main.tex` symbol/algorithm). Keep
  black-boxes (`LtlfToDfa`, `SolveDfa`, `progress`) behind named wrappers so the
  glossary maps onto them even while stubbed.
- Keep functions small and unit-testable — `/test-writer` will want one test per
  function that sensibly has one.

## Build & self-check

- Build with `cmake --build build -j` (configure `cmake -S . -B build` if needed).
- The tree must **compile** before you're done. Do not leave it red.
- Stubs are fine, but a stub throws `std::logic_error("… not yet implemented")`
  or is clearly marked `TODO(developer)` — never a silent wrong answer.

## Definition of done

- Code compiles (`cmake --build build`).
- Every new public domain identifier is in `docs/GLOSSARY.md`.
- Any divergence from `main.tex` is flagged for `/theory-review`.
- **Suggest next steps** (do not auto-run): `/test-writer` on the new functions,
  then `/code-review` (generic) and `/code-reviewer` (domain) before committing.
