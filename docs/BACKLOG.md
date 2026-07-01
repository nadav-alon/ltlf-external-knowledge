# Backlog

Personal "what I intend to do next" — a lightweight capture of intentions, **not**
the developer task tracker and **not** a grilling session. Jot the *what* and
*why* now; the decisions get made later (often via `/grill-prd` or `/grill-me`).

Move items between sections as they progress. Each item: a title, the intent,
and optional **seeds** — half-formed questions/ideas to feed the eventual grill.

---

## Now / next

### Sharpen the Transducer definition, signature & input API
- **Intent:** firm up the `Transducer` abstraction (`include/ltlf_ek/transducer.hpp`)
  — its definition, its C++ signature, and especially **how the eventual CLI
  tool hands a transducer to the library as input**. Right now `Transducer` is
  an abstract base (`delta`, `lambda`, `initial_state`) with no concrete
  construction path and no external representation.
- **Seeds for grilling:**
  - **External format:** how does a user *supply* $T_{in}/T_{out}$ on the CLI?
    Spot HOA file? explicit state/transition table? a small DSL? Reuse Spot's
    automaton parsers where possible (thin-wrapper principle).
  - **The lambda-split ($\lambda: Q\times\Sigma_0\to\Sigma_1$)** has no native
    Spot/HOA equivalent — how is the output function encoded in the input format,
    separately from $\delta$? Per-state output labels? a companion map?
  - **Partial / undefined transducers:** the OTF methods "skip undefined
    letters" — does the input format allow partiality, and how is it signalled?
  - **Construction API:** factory from a parsed automaton + output map vs a
    builder; how it references the `VariablePartition` (which APs are the
    visible slice $\Sigma_0$ vs produced $\Sigma_1$).
  - **Glossary impact:** likely new terms (input format, output-encoding,
    partial transducer) → run `/glossary` before/after.
  - Consider writing a **PRD via `/grill-prd`** since this feeds `/developer`.

## Later

_(nothing yet)_

## Done

_(nothing yet)_
