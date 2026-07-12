# Backlog

Personal "what I intend to do next" — a lightweight capture of intentions, **not**
the developer task tracker and **not** a grilling session. Jot the *what* and
*why* now; the decisions get made later (often via `/grill-prd` or `/grill-me`).

Move items between sections as they progress. Each item: a title, the intent,
and optional **seeds** — half-formed questions/ideas to feed the eventual grill.

---

## Now / next

_Priority order within this section: #1 symbolic DFA-product, then #2 $\Tout$
oracle. #2's rationale (grilled 2026-07-05, updated 2026-07-08): the internal
controller verifier is now **banked** (shipped in `81a4cf4`, migrated onto the
shared product core, all four PRD gates clean), so the known-**output** $\Tout$
oracle rounds out the external `ltlfsynt` cross-check. #1 promoted 2026-07-10
(from "Later"): the tool's eventual purpose is **benchmarking** the methods, so
the minterm loop's $2^{|\mathcal I\cup\mathcal O|}$ cost is no longer an
acceptable baseline — the symbolic rewrite moves ahead of further oracle work,
overriding its former "measure first" deferral._

### Symbolic DFA-product construction (skip the minterm loop) — **#1** (promoted 2026-07-10)
- **Intent:** the Method-2 `DfaProduct` (spec'd in `docs/prd/dfa-product.md`)
  builds the product by enumerating full letters $v\in2^{\mathcal{I}\cup\mathcal{O}}$
  (`all_letters`, `src/product.cpp`) and grouping them into guarded edges —
  faithful to `alg:dfa_product` but **exponential in $|\mathcal{I}\cup\mathcal{O}|$**
  by design (the deliberate baseline cost). Replace the minterm loop with
  **symbolic BDD-guard algebra**: compute successors and the $\cons$ filter
  directly on edge-guard BDDs, never materialising individual letters. Note the
  pipeline already round-trips today — `build_product` explodes $\Sigma$ into
  minterms, then materialisation (`src/dfa_product.cpp`,
  `guards[dst] |= letters[idx]`) re-compresses them into per-destination BDD
  guards; the symbolic build computes those guards directly and drops the
  round-trip.
- **Why high priority (2026-07-10):** the tool's eventual use is **benchmarking**
  the methods (automaton-construction / synthesis times, controller size), so
  efficiency is *not* negligible and the $2^{|\mathcal I\cup\mathcal O|}$ letter
  loop is an unacceptable baseline for wide partitions — promoted ahead of further
  `ltlfsynt` oracle work, overriding the former "measure first / do last" seed.
- **Seeds for grilling:**
  - Needs a **symbolic `cons`** — the current `consistent(...)` is per-full-letter
    only; a whole-region version must be reconciled with the math.
  - The `Transducer` interface exposes `delta(q, v)` / `emits` **per full letter**;
    a symbolic build needs each transducer's $\delta$/$\lambda$ as a **BDD relation**
    (current-state, letter, next-state), not per-minterm functions — a base-class
    contract change, not a local edit. The goal DFA is already symbolic.
  - This is essentially the Method-3 (on-the-fly) construction style — decide
    whether it lives as a `DfaProduct` optimisation or belongs only to
    `OtfDfaProduct`.
  - **Keep the explicit build as a differential oracle:** the symbolic version's
    likeliest bug class is guard construction (lost-transition failures — cf. the
    `|=`→`=` seeded bug); assert metamorphically that both builds yield the same
    game so the per-letter reference isn't discarded.
  - A **quick measurement still de-risks scope** (letter loop vs `SolveDfa` as the
    dominant cost) even though the rewrite is now prioritised — cheap to run first,
    and it sets the benchmark baseline the rewrite is judged against.

### `ltlfsynt` oracle — known-**output** ($\Tout$) reduction — **#2** (was #1; symbolic build promoted above 2026-07-10)
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

### Symbolic `verify_controller` ν-fixpoint (spun off from symbolic DFA-product, 2026-07-12)
- **Intent:** the *Controller verifier*'s ν-fixpoint (`src/verify_controller.cpp`)
  is inherently **per-$\Ifree$-combo** — `StateInfo::edges` is an array indexed by
  `ifree_index`, and `compute_bad`/`extract_witness` enumerate $\Ifree$ choices as
  the adversary's moves. So it still pays the minterm loop via `build_product` +
  `LetterAlphabet`, which the symbolic DFA-product rewrite
  (`docs/prd/symbolic-dfa-product.md`) deliberately **left in place** (scoped to
  `DfaProduct` only). This item is the symbolic rewrite of the verifier's fixpoint.
- **Why deferred:** the verifier is an **audit path**, not the benchmarked
  synthesis path, and it is the project's **linchpin correctness oracle** — a
  symbolic ν-fixpoint over BDDs is a real re-architecture with its own theory/test
  burden, not worth folding into a perf change. Pursue **only** if the verifier's
  own construction cost ever shows up as a bottleneck.
- **Seeds for grilling:**
  - A symbolic one-player reachability/safety fixpoint replaces the
    $\Ifree$-combo enumeration — the $\exists\Ifree$ adversary move becomes a
    `bdd_exist` over the $\Ifree$ cube, the greatest fixpoint an iteration over
    BDD state sets. Reconcile with the current `Bad` nu-fixpoint spelling
    (`docs/prd/controller-verifier.md`).
  - Witness (lasso) extraction must survive the symbolic rewrite — currently it
    walks concrete $\Ifree$ choices; a symbolic version needs to pick a concrete
    witness letter out of a BDD region.
  - Reuse `emits_region`/`delta_edges` (added by `symbolic-dfa-product.md`) so the
    two symbolic builds share the contract.

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

### Soak metamorphic body has no per-case time bound (from Phase 2, 2026-07-13)
- **Context:** the soak mode's escalating driver (`run_corpus`,
  `docs/prd/generated-corpus-soak-mode.md`, implemented) uses a **soft** deadline —
  the case in flight when the budget passes always finishes. For the `differential`
  body that in-flight case is bounded by the per-subprocess `ltlfsynt` timeout, but
  the **`MetamorphicRoundTrip`** body's `DfaProduct::synthesize` /
  `verify_controller` are **in-process Spot calls with no time bound**, so one large
  case at a high level can dominate the wall clock (observed: 105 s for a 20 s
  budget). The OOM/crash class is fixed (per-case `bad_alloc`→skip, lowered
  ceilings); this is the residual *time* overrun.
- **Why deferred:** you cannot safely interrupt a running in-process Spot synthesis
  mid-case, so a firm bound needs either a per-case complexity cap (a tighter
  width/tree ceiling for the metamorphic body specifically — trades escalation
  depth for a firmer budget) or running each case under a watchdog process. Not
  worth it until soak is run routinely enough that the overrun bites.
- **Also minor:** `run_corpus`'s `levels_reached` can overcount a zero-case level by
  1 (set at level entry, before the inner deadline check) — diagnostic-only
  cosmetic, noted so it isn't rediscovered as a bug.
- **Seeds for grilling:** _(tbd)_

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

### Benchmarking / evaluation — do **last**, before moving on to other methods
- **Intent:** address the eventual benchmarking needed to assess the methods —
  automaton construction times, synthesis times, controller size, etc.
- **Note (2026-07-10):** this is the **driver** for promoting the symbolic
  DFA-product rewrite (Now/next #1) — benchmarking is the tool's eventual purpose,
  so the minterm loop's cost stops being an acceptable baseline. A first
  measurement pass here also sets the baseline the symbolic rewrite is judged
  against.
- **Seeds for grilling:** _(tbd)_

## Done

### Intense "soak" mode for the generated corpus
- **Intent:** an opt-in, wall-clock-budgeted escalating soak over the generated
  corpus (nightly / pre-release), leaving the fast `ctest` gate byte-for-byte
  unchanged. Reframed in the grill from "env-overridable knobs + seed sweep" to a
  `LTLF_EK_SOAK=<secs>` runner that escalates complexity (wider $\Ifree$, deeper
  formulas, more $\Tin$ states) until the deadline.
- **Outcome:** shipped in two phases (PRD `docs/prd/generated-corpus-soak-mode.md`,
  `implemented`, all gates clean). Phase 1 (`c897c82`): `CorpusConfig` +
  `LTLF_EK_CORPUS_*` per-knob env overrides (loud-on-malformed) + the byte-identical
  golden guard. Phase 2 (this commit): the `LTLF_EK_SOAK` escalating `run_corpus`
  driver (fresh-seed levels, `ladder`, width ceilings, joint clamp, per-case
  `bad_alloc`→skip), the metamorphic `t_in` replay dump, and ladder/soak tests.
  Suite green 215/215; `/code-reviewer` + `/code-review` both clean. Two follow-ups
  logged under **Later** (metamorphic per-case time bound; `levels_reached`
  cosmetic). A deeper Spot finding surfaced and is captured in memory:
  `randltlgenerator` in-process rebuilds are **not** seed-reproducible (global RNG +
  AP apid recycling), so per-case replay is via the printed `phi`/partition/`t_in`,
  not corpus regeneration.

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
