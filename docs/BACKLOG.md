# Backlog

Personal "what I intend to do next" — a lightweight capture of intentions, **not**
the developer task tracker and **not** a grilling session. Jot the *what* and
*why* now; the decisions get made later (often via `/grill-prd` or `/grill-me`).

Move items between sections as they progress. Each item: a title, the intent,
and optional **seeds** — half-formed questions/ideas to feed the eventual grill.

---

## Now / next

### Sharpen the Transducer definition, signature & input API
- **PRD:** in-library C++ path spec'd in `docs/prd/concrete-transducer.md` (ready
  for `/developer`). The **external file format / CLI parser** is still open — a
  future PRD.
- **Intent:** firm up the `Transducer` abstraction (`include/ltlf_ek/transducer.hpp`)
  — its definition, its C++ signature, and especially **how the eventual CLI
  tool hands a transducer to the library as input**. Right now `Transducer` is
  an abstract base (`delta`, `lambda`, `initial_state`) with no concrete
  construction path and no external representation.
- **Seeds for grilling:**
  - **External format:** how does a user *supply* $\Tin/\Tout$ on the CLI?
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

### Git integration
- **Intent:** add git integration to the LaTeX **Overleaf** project specifically
  (sync `main.tex` / the `latex/` submodule with Overleaf via its git bridge).
- **Progress:** `latex/` submodule added (Overleaf git bridge); root `main.tex`
  removed and its content ported in, so **`latex/main.tex` is now the single
  source**; skills repointed. Left to do: commit the submodule pointer in the
  parent repo, and settle the push-to-Overleaf sync workflow.
- **Seeds for grilling:** _(tbd)_

## Later

### Infer lambda from transducer edge labels
- **Intent:** stop storing $\lambda$ as independent state and instead read it off
  $\delta$'s (surviving) edge labels — the $\Sigma_1$-projection of the enabled
  edge. Only sound under the **Case-A partial-transducer** representation
  (undefined = only the inconsistent completions dropped). Keep the explicit
  `lambda` for now: it lets us **verify a transducer obeys its own
  well-formedness invariant** (output is a function of the observation alone) for
  debugging, and it decouples the interface from the edge encoding.
- **Seeds for grilling:**
  - $\lambda$ may later return a **set** of possible outputs (non-deterministic
    knowledge) rather than a deterministic answer — inferring that from edges
    could be *less efficient*, so weigh before committing.
  - Where does the (WF) check live if `lambda` becomes derived — an assertion in
    the concrete class?

### Benchmarking / evaluation
- **Intent:** address the eventual benchmarking needed to assess the methods —
  automaton construction times, synthesis times, controller size, etc.
- **Seeds for grilling:** _(tbd)_

## Done

_(nothing yet)_
