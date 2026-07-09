# Backlog

Personal "what I intend to do next" — a lightweight capture of intentions, **not**
the developer task tracker and **not** a grilling session. Jot the *what* and
*why* now; the decisions get made later (often via `/grill-prd` or `/grill-me`).

Move items between sections as they progress. Each item: a title, the intent,
and optional **seeds** — half-formed questions/ideas to feed the eventual grill.

---

## Now / next

_Priority order within this section: #1. Rationale (grilled 2026-07-05, updated
2026-07-08): the internal controller verifier is now **banked** (shipped in
`81a4cf4`, migrated onto the shared product core, all four PRD gates clean), so
the known-**output** $\Tout$ oracle rounds out the external `ltlfsynt`
cross-check next._

### `ltlfsynt` oracle — known-**output** ($\Tout$) reduction — **#1** (promoted from #2)
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

### Co-generated $(\Tin,\psi_{in})$ family → known-knowledge differential (generated corpus v2)
- **PRD:** extends `docs/prd/generated-corpus-oracle.md` (v1 draft, grilled
  2026-07-06). v1 grades a fixed-seed generated corpus with two **self-labeling**
  oracles — the empty-knowledge `ltlfsynt` differential and the
  `synthesize`$\to$`verify_controller` metamorphic round-trip (incl. a **free-form
  random $\Tin$**). This item closes v1's one acknowledged gap.
- **The gap (from the v1 PRD "Open theory questions"):** on generated
  **known-knowledge** cases the metamorphic oracle is **one-directional** — it
  catches a wrongly-*realizable* controller (it verifies the $T_C$ `synthesize`
  returned) but **not** a wrongly-*unrealizable* verdict (no controller to verify,
  and no $\psi_{in}$ to feed `ltlfsynt`). The empty-knowledge differential covers
  the wrongly-unrealizable direction only for $\mathcal V=\emptyset$.
- **Why the obvious fixes don't work (settled in the grill, don't re-litigate):**
  the differential needs **both** a $\Tin$ file (for `ltlf-ek-synth`) **and** a
  $\psi_{in}$ string (for `ltlfsynt`), denoting the **same** language. Neither
  free-form direction gives that: **transducer→$\psi_{in}$** fails because a
  free-form table $\delta$ can be a modular counter (regular but **not** star-free
  ⇒ no LTLf $\psi_{in}$ exists; LTLf ⊊ regular); **random-$\psi_{in}$→transducer**
  fails because a random LTLf formula is usually **not a strategy** (non-functional:
  two $\Iknown$ for one history; or non-total: it constrains the env's *free*
  inputs).
- **The approach that works — co-generation from a bounded-memory source:** draw
  from the `const / copy / delay / window-boolean` family, where $\Iknown_t$ is a
  **total boolean function of a fixed window** $\ifree_t\ldots\ifree_{t-d}$. Such a
  function is *simultaneously* (a) a finite **deterministic total** transducer
  (state = last $d$ $\Ifree$-values — build the table directly) and (b) a **direct
  LTLf** formula (`G`-guarded safety over the window, star-free by construction).
  Emit **both** from one drawn parameter set `(which iknown, window depth d,
  boolean fn)` ⇒ provably equal, so no mis-encoding. Feeds a **third** test body: a
  known-knowledge differential (`ek-synth` vs `ltlfsynt` on
  $\psi_{in}\!\rightarrow\!\varphi$, load-bearing guard), giving **bidirectional**
  known-knowledge coverage. Complementary to v1's free-form table (which keeps its
  broader counter/parity-capable one-directional metamorphic coverage), not a
  replacement.
- **Seeds for grilling:**
  - **The weak-X trap still bites.** The original delay-fixture bug *was* a
    $\psi_{in}$ mis-encoding in exactly this family (weak-`X`-at-final-position vs
    `X[!]`). Co-generation must emit the corrected guarded-weak-`X` safety shape
    (like `kPsiInDelayCorrected` = `(!k) & G(a -> X k) & G(!a -> X !k)`), **not**
    `X[!]`.
  - **It feeds the faithfulness guard, doesn't retire it.** Run every co-generated
    pair through `run_faithfulness_guard` as a cheap library-only self-check — a
    co-generation bug is exactly what it catches.
  - **Middle path, if the family feels too narrow:** generate free-form random
    $\psi_{in}$, then **filter** — `ltlf_to_dfa`, accept only if it encodes a total
    deterministic $\Iknown$-function, extract the transducer from that DFA. Covers
    more $\psi_{in}$ shapes but adds a functionality check + rejection sampling +
    DFA→transducer extraction. Heavier; weigh vs the bounded-memory family.
  - Subsumes/relates to the known-**output** $\Tout$ oracle (Now/next #2): a
    co-generated $\Tout$ family would extend this to the guarantee half.

### Generated $\Tout$ / $\Oknown$ in the generated corpus (generated corpus v2)
- **PRD:** extends `docs/prd/generated-corpus-oracle.md` (v1 fully implemented,
  all 3 phases). v1 fixes $\Oknown=\emptyset$ and $\Tout$ = `trivial_transducer`
  always — the generator never exercises a non-trivial known-output strategy.
- **Intent:** extend the corpus generator to draw a random $\Oknown$ split and a
  random known-output transducer $\Tout$ (the guarantee/system-side half), so the
  metamorphic round-trip and differential cover known-output cases too.
- **Seeds for grilling:**
  - The co-generation constraint from the $(\Tin,\psi_{in})$ item applies here for
    a known-output *differential* ($\psi_{out}$ as a guarantee conjunct); pairs
    with the known-output $\Tout$ oracle (Now/next #2).
  - $\Tout$ observes $\mathcal I\cup\Ofree$ of the same step — the random-$\Tout$
    builder needs $\Sigma_0=\mathcal I\cup\Ofree,\Sigma_1=\Oknown$, not the
    $\Tin$ shape.

### Intense "soak" mode for the generated corpus (run sparingly)
- **PRD:** operational extension of `docs/prd/generated-corpus-oracle.md` (v1 fully
  implemented). The default corpus (256 cases, one `kCorpusSeed`, tree-size ≤10,
  differential width ≤3) runs in well under a second — deliberately tuned for the
  fast `ctest` gate. This item adds an opt-in heavy configuration to run
  occasionally (nightly / pre-release), not on every build.
- **Intent:** make the tunable knobs env-overridable (no recompile) so a soak run
  can crank them, and add a **seed sweep** so breadth no longer rests on a single
  256-case draw. Knobs: `kCorpusCaseCount`, `kCorpusTreeSizeMax`, the partition
  width ranges, the differential width cap (currently the hardcoded `3` literal),
  and `kCorpusSubprocessTimeoutSecs`. Default path stays byte-for-byte unchanged
  (defaults substituted when env unset), so the fast gate and the landed tree's
  reproducibility are untouched.
- **Seeds for grilling:**
  - The three bodies scale differently: the two **library** bodies (structural +
    metamorphic) are in-process/self-labeling → crank case count freely; the
    **differential** is the one that genuinely needs "sparingly" (raising its width
    cap past 3 is where `ltlfsynt` blows up and the skip counter moves).
  - **Seed sweep is the highest-value axis** — 50 seeds × 256 gives new
    formula/partition/Tin shapes, catching the "unlucky single seed" blind spot
    that bumping case count on one seed does not.
  - Reproducibility: once the seed is swept, `SCOPED_TRACE` must also print the
    **seed** (not just `(phi, partition, index)`) so a soak failure stays
    re-runnable.
  - Metamorphic's `random_tin` pays `2^|Ifree|` (enumerates every Ifree cube), so
    widening the partition ranges has a steeper in-process cost than tree size —
    weigh which axis buys more coverage per second.

### Formula shrinking on generated-corpus failure (generated corpus v2)
- **PRD:** extends `docs/prd/generated-corpus-oracle.md`. v1 has **no shrinking**:
  a failing case prints its `(phi, partition, index)` for manual reproduction, but
  the offending $\varphi$ is whatever size the generator emitted (≤~10 nodes).
- **Intent:** on a differential/metamorphic failure, shrink $\varphi$ (and maybe
  the partition / $\Tin$) to a minimal still-failing witness before reporting, so a
  surfaced `DfaProduct`/semantics bug lands as a small reproducer.
- **Seeds for grilling:** _(tbd)_

### Prove the monolithic reduction $\psi_{in}\!\rightarrow\!(\varphi \land \psi_{out})$
- **Intent:** prove that synthesis with external information
  (`main.tex` `def:probDefTransducer`) is **equirealizable** with plain
  $\text{LTL}_f$ synthesis of $\psi_{in} \rightarrow (\varphi \land \psi_{out})$
  over the same partition — $\Iknown$ exposed as environment inputs, $\Oknown$
  kept as system outputs — where $\psi_{in},\psi_{out}$ are the $\text{LTL}_f$
  languages of the traces produced by $\Tin,\Tout$. Currently only a **conjecture**
  (added as a `\cl` note right after `def:probDefTransducer` in `main.tex`,
  2026-07-05). The known-**input** half ($\Tout$ absent, $\psi_{out}=\top$) is
  already cross-checked externally against Spot's `ltlfsynt`
  (`docs/prd/ltlfsynt-oracle.md`); this item is the *theory* generalising and
  proving the whole thing, incl. the known-output guarantee half.
- **Why:** it is the correctness backbone of the external `ltlfsynt` oracle and
  would justify a monolithic baseline for *every* method — but it is **not yet a
  theorem**. The oracle's one-time divergence witness (below) turned out to be a
  fixture bug, not a counterexample, so the proof is not blocked on carving out
  an exception; it is a clean conjecture to attack directly.
- **Seeds for grilling:**
  - The two knowledge halves are **asymmetric**: $\Tin$ is an **assumption**
    (implication antecedent, constrains environment-chosen $\Iknown$), $\Tout$ is
    a **guarantee** (conjunct, constrains system-chosen $\Oknown$). The proof must
    respect Mealy turn order (`main.tex` §86) for both.
  - **Divergence witness retired (2026-07-05, `docs/prd/oracle-faithfulness-
    guard.md`).** A delay-$\Tin$ witness, $\text{X[!]}(a \rightarrow
    \text{X[!]}\,k)$, once looked like a soundness-boundary counterexample (EK
    REALIZABLE, reduction UNREALIZABLE). It was a $\psi_{in}\leftrightarrow$
    transducer **mis-encoding** (the hand-authored $\psi_{in}$ was
    copy-from-step-1, not delay); with the corrected delay $\psi_{in}$
    (`(!k) & G(a -> X k) & G(!a -> X !k)`) the same pair **agrees** (both
    REALIZABLE). There is currently **no known divergence witness** for the
    conjecture, and no known sound-fragment carve-out is needed. A mechanical
    **faithfulness guard** now cross-checks every corpus $(\Tin,\psi_{in})$ pair
    against itself so this class of drift cannot recur silently
    (`tests/ltlfsynt_oracle_test.cpp`).
  - Interacts with the **non-empty-trace / empty-word** convention (`1` rejects
    the empty word) and **system-controlled termination** — both bite exactly at
    trace-continuation boundaries, so any future divergence candidate should be
    checked against these first.
  - Relates to the deferred known-**output** $\Tout$ oracle already logged for
    `docs/prd/ltlfsynt-oracle.md`; proving this subsumes it.

### Harden `verify_controller` Witness bdd lifetime (non-blocking, from code-review 2026-07-06)
- **Intent:** the throwaway letter `registrar` `twa_graph` inside
  `verify_controller` is the sole owner of the AP registrations backing the
  `bdd` letters that escape into the returned `Witness`. It is destroyed on
  return, so those vars stay valid only because every *current* caller
  independently keeps $\mathcal I\cup\mathcal O$ registered (the CLI's
  `ap_registrar` lives the whole run; the tests hold the transducers). A library
  caller with a partition AP registered by neither $\varphi$ nor $\Tin/\Tout/T_C$
  could observe a corrupted witness letter after a later `register_ap`.
- **Why:** latent, not a live bug (no shipped call site triggers it) — but a
  fragile coupling worth removing.
- **Seeds for grilling:** either document a caller precondition on
  `verify_controller` ("keep $\mathcal I\cup\mathcal O$ registered for the
  `Witness`'s lifetime"), or build the letters without a throwaway graph so the
  registration lifetime $\ge$ the returned `Witness`.

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

### Trace-level controller verifier oracle
- **Intent:** the internal linchpin correctness oracle — for a synthesized
  `Controller` $T_C$, check **every trace agreeing with $\Tin,\Tout,T_C$ satisfies
  $\varphi$** (reachability of $F_\varphi$ under adversarial env, built directly on
  `def:probDefTransducer`). Reusable by every method; unblocks `--model-check`.
- **Outcome:** shipped. `verify_controller` + `Role::t_c` +
  `controller_as_transducer` + `VerifyResult`/`Witness` landed (`81a4cf4`), then
  migrated onto the shared product core (`f5f53e7`) and cleaned post-merge
  (`6c2950b`). All four PRD gates clean — glossary, tests (`verify_controller_test.cpp`,
  12 cases across oracles #1–#6; suite green 186/186), `/code-reviewer` + generic
  `/code-review` both clean, `/theory-review` code↔math faithful. PRD
  `docs/prd/controller-verifier.md` `Status: implemented`. One non-blocking
  residue (Witness bdd lifetime) tracked separately under **Later**.

### Implement the `ltlfsynt` external oracle (known-**input** $\Tin$)
- **Intent:** an external, independent realizability oracle — cross-check the
  built `ltlf-ek-synth` against Spot's `ltlfsynt` on the equirealizable
  `psi_in -> phi` reduction.
- **Outcome:** shipped (`745b3d7`). `tests/ltlfsynt_oracle_test.cpp` drives the
  external `ltlfsynt` binary as a subprocess; CMake wires
  `find_program(LTLFSYNT_EXECUTABLE)` + `LTLFSYNT_BINARY` with `GTEST_SKIP` on a
  box without Spot's CLI (env override `LTLFSYNT_BIN`). PRD:
  `docs/prd/ltlfsynt-oracle.md`. A faithfulness guard (`6fc2b34`) also
  cross-checks each corpus $(\Tin,\psi_{in})$ pair against itself. The
  known-**output** $\Tout$ half remains open (now Now/next #2).

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
