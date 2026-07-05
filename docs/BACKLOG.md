# Backlog

Personal "what I intend to do next" — a lightweight capture of intentions, **not**
the developer task tracker and **not** a grilling session. Jot the *what* and
*why* now; the decisions get made later (often via `/grill-prd` or `/grill-me`).

Move items between sections as they progress. Each item: a title, the intent,
and optional **seeds** — half-formed questions/ideas to feed the eventual grill.

---

## Now / next

_Priority order within this section: #1 → #3. Rationale (grilled 2026-07-05):
get an **external, independent** check in first (anything we write ourselves can
be wrong the same way the code is), then round it out with the internal verifier;
the verifier outranks the $\Tout$ extension because it's reusable by every method
and unblocks the live `--model-check` flag._

### Implement the `ltlfsynt` external oracle (known-**input** $\Tin$) — **#1**
- **PRD:** `docs/prd/ltlfsynt-oracle.md` (draft, ready for `/developer` +
  `/test-writer`). **Not yet implemented** — verified 2026-07-05: no
  `tests/ltlfsynt_oracle_test.cpp`, no CMake `find_program(ltlfsynt)` /
  `LTLFSYNT_BINARY` wiring, and **no test invokes the external `ltlfsynt`
  binary**. (`tests/ltlf_ek_synth_test.cpp` drives the known-input *CLI* path —
  e.g. `KnownInputTransducerTurnsUnrealizableIntoRealizable` — but that shares
  our code and is *not* the independent cross-check.)
- **Intent:** highest priority. An external, independent realizability oracle:
  cross-check the built `ltlf-ek-synth` against Spot's `ltlfsynt` on the
  equirealizable `psi_in -> phi` reduction. All deps are present (CLI
  `--known-input-transducer`, the `%%LAMBDA` format in `src/transducer_io.cpp`,
  and `ltlfsynt` on PATH), and the corpus is pre-verified in the PRD — so this is
  a pure test-only addition with zero production risk.
- **Seeds for grilling:** mostly mechanical (encode Tables A–E, wire CMake
  `find_program` + `GTEST_SKIP`, env override `LTLFSYNT_BIN`). Respect the
  **excluded-class divergence witness** — do *not* encode it as a passing
  agreement.

### Trace-level controller verifier oracle — **#2** (promoted from Later)
- **Intent:** the internal linchpin correctness oracle from
  `docs/prd/dfa-product.md` (oracle #2): for a synthesized `Controller` $T_C$,
  check that **every trace agreeing with $\Tin,\Tout,T_C$ satisfies $\varphi$**.
  Reusable by every method. Rounds out the external oracle (#1) with a self-test
  once an independent check is banked. Needs its own `/grill-prd` (`Verifier`)
  first.
- **Also blocks the CLI `--model-check`:** `docs/prd/cli-wrapper.md` wires the
  `--model-check` flag but leaves it erroring "not yet implemented" pending this
  verifier. After the PRD, re-run `/developer` on the CLI to un-defer the flag.
  Heed the seed below — the naive
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

### `ltlfsynt` oracle — known-**output** ($\Tout$) reduction — **#3**
- **PRD:** the known-**input** ($\Tin$) half is spec'd in
  `docs/prd/ltlfsynt-oracle.md` (ready for `/developer` + `/test-writer`). This
  item is the $\Tout$ follow-up it explicitly deferred.
- **Intent:** extend the external `ltlfsynt` cross-check to a known **output**
  strategy. Unlike $\Tin$ (an *assumption* $\psi_{in} \rightarrow \varphi$),
  $\Oknown$ is a **system-side** helper ($\Sigma_0=\mathcal{I}\cup\Ofree$), so it
  reduces as a **guarantee/conjunction**: `--outs=Ofree,Oknown`, formula
  $\varphi \land \psi_{out}$ (and, mixed with a known input,
  $\psi_{in} \rightarrow (\varphi \land \psi_{out})$).
- **Seeds for grilling:**
  - Is $\varphi \land \psi_{out}$ with $\Oknown$ as a system output actually
    equirealizable with the $\Tout$ problem? The controller must *drive* $\Oknown$
    per the strategy, not choose it freely — verify the conjunction pins it.
  - Same discriminating-fixture discipline: load-bearing, verdict-mixed, guard by
    dropping $\psi_{out}$.
  - Turn order: $\Tout$ observes $\Ofree$ of the *same* step — confirm `ltlfsynt`
    Mealy semantics still line up when $\Oknown$ is a synthesis output.

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

### Benchmarking / evaluation — do **last**, before moving on to other methods
- **Intent:** address the eventual benchmarking needed to assess the methods —
  automaton construction times, synthesis times, controller size, etc.
- **Seeds for grilling:** _(tbd)_

## Done

### Sharpen the Transducer definition, signature & input API
- **Intent:** firm up the `Transducer` abstraction — its definition, C++
  signature, and how the CLI hands a transducer to the library as input.
- **Outcome:** both halves shipped. In-library C++ path — `concrete-transducer.md`
  (**implemented**, `2b45755`): `OutputLabeledTransducer` concretises the
  `Transducer` base, consumed by `Synthesis::synthesize` as `t_in`/`t_out`.
  External file format / CLI parser — `transducer-file-format.md`
  (**implemented**): the `%%LAMBDA` format + `parse_transducer`
  (`src/transducer_io.cpp`), wired into the CLI via `--known-input-transducer`.

### Git integration (Overleaf sync)
- **Intent:** sync `latex/main.tex` with Overleaf via its git bridge.
- **Outcome:** `latex/` submodule tracks Overleaf's `main` (`branch = main`,
  `update = rebase`); submodule pointer is committed in the parent (already was,
  via `784c296`); two-way sync workflow documented in `docs/overleaf-sync.md` and
  linked from the README.
