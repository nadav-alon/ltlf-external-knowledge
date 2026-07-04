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

### Symbolic DFA-product construction (skip the minterm loop)
- **Intent:** the Method-2 `DfaProduct` (spec'd in `docs/prd/dfa-product.md`)
  builds the product by enumerating full letters $v\in2^{\mathcal{I}\cup\mathcal{O}}$
  and grouping them into guarded edges — faithful to `alg:dfa_product` but
  **exponential in $|\mathcal{I}\cup\mathcal{O}|$** by design (the deliberate
  baseline cost). Later, replace the minterm loop with **symbolic BDD-guard
  algebra**: compute successors and the $\cons$ filter directly on edge-guard
  BDDs, never materialising individual letters.
- **Seeds for grilling:**
  - Needs a **symbolic `cons`** — the current `consistent(...)` is per-full-letter
    only; a whole-region version must be reconciled with the math.
  - This is essentially the Method-3 (on-the-fly) construction style — decide
    whether it lives as a `DfaProduct` optimisation or belongs only to
    `OtfDfaProduct`.
  - Measure first (see benchmarking below): only worth it if the letter loop is
    the actual bottleneck vs `SolveDfa`.

### Trace-level controller verifier oracle
- **Intent:** build the linchpin correctness oracle from `docs/prd/dfa-product.md`
  (oracle #2): for a synthesized `Controller` $T_C$, check that **every trace
  agreeing with $\Tin,\Tout,T_C$ satisfies $\varphi$**. Reusable by every method.
  Deferred during Method-2 `/test-writer` — realizability is currently
  cross-checked only via Spot's monolithic baseline.
- **Also blocks the CLI `--model-check`:** `docs/prd/cli-wrapper.md` wires the
  `--model-check` flag but leaves it erroring "not yet implemented" pending this
  verifier. Give it its own `/grill-prd` (`Verifier`), then re-run `/developer`
  on the CLI to un-defer the flag. Heed the seed below — the naive
  $T_C\cap\Tin\cap\Tout\cap\neg\varphi$-empty check is *not* the right property.
- **Seeds for grilling:**
  - A naive language-inclusion intersection ($T_C\cap\Tin\cap\Tout\cap\neg\varphi$
    empty?) is **wrong**: LTLf lets the system *stop* at any accepting state, so
    the real property is **reachability under adversarial env**, not inclusion of
    all prefixes.
  - Risk: a correct verifier essentially re-derives the game — how to keep it
    **independent** of `solve_dfa` so it's a genuine oracle (e.g. plain graph
    reachability on the $T_C\times A_\varphi$ composition vs re-solving)?
  - Mealy turn order + non-empty traces + weak-`X` all bite here (see
    `docs/prd/dfa-product.md` developer comments).

### Benchmarking / evaluation
- **Intent:** address the eventual benchmarking needed to assess the methods —
  automaton construction times, synthesis times, controller size, etc.
- **Seeds for grilling:** _(tbd)_

## Done

### Git integration (Overleaf sync)
- **Intent:** sync `latex/main.tex` with Overleaf via its git bridge.
- **Outcome:** `latex/` submodule tracks Overleaf's `main` (`branch = main`,
  `update = rebase`); submodule pointer is committed in the parent (already was,
  via `784c296`); two-way sync workflow documented in `docs/overleaf-sync.md` and
  linked from the README.
