# Project standards

The rules of this repo's C++, on top of [`docs/agents/coding-standards.md`](agents/coding-standards.md).
An implementation run follows them; the review's standards axis checks against them.

## Linked documents

- [`docs/GLOSSARY.md`](GLOSSARY.md) — the ubiquitous language: math ↔ prose ↔ C++. Binding. A run
  may propose an entry in its pull request (see **Glossary names**).
- `latex/main.tex` — the paper, a submodule mirroring Overleaf. The reference for the math, and
  fallible. Read-only: no agent commits anything under `latex/` or moves the submodule pointer.
- [`docs/grilling-contract.md`](grilling-contract.md) — what a grilling here produces. Binds a
  grilling, not a run.

## Thin wrappers over Spot

The architecture is thin domain wrappers over Spot, with custom types only for what Spot lacks: the
lambda-split `Transducer`, the NFA as a domain type, product bookkeeping.

- Automata, BDD and formula machinery come from Spot: `spot::twa_graph_ptr`, `bdd` cubes for
  letters, `spot::formula`, one shared `spot::bdd_dict`.
- A letter is built from the shared `spot::bdd_dict`; a `bdd` outlives no dict it was built from.
- Acceptance conditions are the finite-semantics ones the surrounding code uses.
- The black boxes (`LtlfToDfa`, `SolveDfa`, `progress`) stay behind their named wrappers, so the
  glossary maps onto them even while one is a stub.
- A stub throws `std::logic_error("… not yet implemented")`. It never returns an answer.
- New code fits the existing `Synthesis` and `Transducer` shapes. Every synthesis method implements
  `Synthesis::synthesize(phi, vars, t_in, t_out)` and returns a controller the verifier accepts.
- Style follows the surrounding code: namespace `ltlf_ek`, `.hpp` headers with `#pragma once`,
  doc comments that cite the `main.tex` label or algorithm they implement.
- For an unknown Spot signature, read its header once, write the call, and let the compiler answer.

## Glossary names

Every public identifier naming a domain concept is in `docs/GLOSSARY.md`'s C++ column, spelled
exactly, and is none of that entry's "Do not call it" synonyms. Tests use the same names.

A run that needs a concept the glossary lacks proposes the entry in its own pull request and keeps
working; merging the pull request is the approval. The entry's `main.tex` field is:

- the label, where the concept exists in the paper;
- "none", for an implementation-only concept;
- where the concept belongs in the math and the paper lacks it, that is `underspecified` (see
  **Theory fidelity**), and the entry says so.

## Theory fidelity

Semantic code matches `latex/main.tex`. Semantic code is the methods, `consistent`, progression,
product construction, final-state logic, and the `Synthesis` and `Transducer` contracts. The
invariants that have been got wrong before:

- `consistent` (`cons`) is the paper's, including the projection onto the transducer's visible slice.
- Method 2 sends `¬cons` to the self-looping ⊥ sink. The on-the-fly methods skip an inconsistent or
  undefined letter; they route it nowhere.
- A state is final by the progression bit.
- Aggregation is keyed on `[psi]` alone, and loses knowledge: it may report unrealizable where a
  non-aggregating method reports realizable, never the reverse.

The review checks every semantic diff against the paper and gives each mismatch one verdict:

- `code-bug` — the code is wrong against the paper. A review finding.
- `doc-bug` — the paper is wrong.
- `underspecified` — the paper does not say.

The review files every `doc-bug` and `underspecified` verdict of one pull request together, as a
single suggestion discovery listing them all. They are the developer's to carry into Overleaf.

A citation of the paper names a `\cref` label. Where it names a line (`main.tex:NNN`), the checks
below keep it true.

## Tests

One run writes a behavior's test and its code together. Tests are GoogleTest, under `tests/`, each
new file added to the `unit_tests` target in `CMakeLists.txt`.

Use the cheapest oracle layer that establishes the truth, and the review checks that a changed
method is covered by each layer that applies:

1. **Unit fixtures** — the primary layer. A tiny hand-built input with a hand-computed expected
   output, one test per function with a contract. A test name reads as a sentence about one behavior.
2. **Cross-method equivalence** — for one `(phi, vars, t_in, t_out)`, the methods agree on
   realizability, and the non-aggregating ones yield strategy-equivalent controllers. For an
   aggregating method assert the implication of **Theory fidelity**, not equality.
3. **Controller verifier** — every synthesized controller, in whichever test produced it, passes
   `verify_controller`.
4. **Monolithic baseline** — a coarse cross-check against `ltlfsynt` on small instances. A mismatch
   means investigate: the encoding itself can be wrong.

Feed the oracles past trivial inputs: nesting depth of three and more, mixed operators, several
free inputs and outputs, formulas whose verdict turns on the current input or on weak next at the
last position, more than one `(phi, T_in)` pair where knowledge flips the verdict, and partitions
with several known inputs or an empty free-output set. A fixed-seed generated corpus, printing the
offending case on failure, is preferred to hand-picked cases: the oracle is its label.

A partial transducer's non-enabled letters are skipped. A test asserts nothing that contradicts
`main.tex`; an expected value the paper leaves open is the **stop rule**'s.

## Before committing

An implementation run runs both, and repairs with `--fix`:

```sh
python3 scripts/gen-md-macros.py --check
python3 scripts/check-main-tex-refs.py --check
```

The review's standards axis reruns both. CI has no `latex/` and runs neither.

`cmake --build` and `ctest` output goes to a log, and only the failing lines are read back: see
`CLAUDE.md`.

## Stop rule

A decision the developer owns is never guessed. A run that meets one — an ambiguity in `main.tex`,
an expected verdict nobody declared, a frozen interface that proves wrong, a condition its ticket
lists as a stop — files a correction discovery and stops. It is never a clarification.

## This file

A run edits this file only when its ticket asks for that.
