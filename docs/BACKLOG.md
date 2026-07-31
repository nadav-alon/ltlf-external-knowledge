# Backlog

Personal "what I intend to do next" — a lightweight capture of intentions, **not**
the developer task tracker and **not** a grilling session. Jot the *what* and
*why* now; the decisions get made later (often via `/grill-prd` or `/grill-me`).

Move items between sections as they progress. Each item: a title, the intent,
and optional **seeds** — half-formed questions/ideas to feed the eventual grill.

---

## Now / next

_Top priority (updated 2026-07-29): **Method 3.2 (aggregation)** or the $\Tout$
oracle — **Method 3.1 is DONE** (see Done; it landed as `OtfMtdfaProduct` in
`0ce5fab`, closed every gate, and benchmarked **POSITIVE** — up to 5488x over
`MtdfaProduct` where $\cons$ prunes, the first method to beat the standing
champion). Its Phase 2 (`otf_solve_fused`) is spun out below._

### Method 3.2 — on-the-fly **aggregated** product (`otfagg`, `\cref{alg:otfdfa_agg_product}`)
- **Intent:** the next unbuilt cell after 3.1. Aggregate on $[\psi]$ alone
  (collapsing transducer states), bounding the product by the size of the original
  DFA at the cost of losing knowledge on each aggregation.
- **Blocking, and now PROVEN not merely suspected:** `\cref{alg:otfdfa_product}`'s
  state-keyed $F_P$ **over-accepts** — theory review (2026-07-29) produced a
  one-state witness, $\varphi=(c \wedge G(a \rightarrow Xb)) \vee (\lnot c \wedge
  X[!]G(a \rightarrow Xb))$ with trivial transducers. the `\na` after `\cref{alg:otfdfa_agg_product}` (`main.tex:452`) asked
  whether to drop the $F_P$ insert; the answer is **re-key it on the transition**.
  3.1 dodges this for free (an mtdfa terminal $2d+b$ is transition-keyed); an
  aggregating method must face it. `\cl` note written into `latex/main.tex`,
  unpushed.
- **Hazard, flagged in the header:** `otf_product_to_mtdfa` is deliberately NOT
  language-exact (I5 collapses to the accepting sink once $\varphi$ is irrevocably
  satisfied, dropping $\cons$ on continuations). Sound for `solve_mtdfa` under
  system-controlled termination; **not** sound for a consumer that reads $L(P)$.
  Aggregation is exactly such a consumer — do not reuse the builder blind.

### Method 3.1 Phase 2 — fused construct-and-solve (`otf_solve_fused`, `--otf-solve`)
- **Intent:** feed the Phase 1 BFS into a `spot::backprop_graph` and abort as soon
  as the initial state is decided. Spec'd in `docs/prd/otf-mtdfa-product.md`;
  **needs its own grill** before coding.
- **Concrete lead from the benchmark:** in the no-pruning family, `game_solving` is
  consistently ~2x slower for `OtfMtdfaProduct` than `MtdfaProduct` at an
  *identical* state count (501 ms vs 249 ms at $n=20$) — same solver, same
  substrate, so the fused build's rows must differ in sharing or variable order.
  Worth understanding **before** Phase 2 fuses solving into that same build.
- **Worth weighing first:** 3.1's win is already 5488x where it matters, and it is
  *flat* — Phase 2 optimizes a term that is no longer the bottleneck in family A.
  The honest question is whether it beats 3.2 or the $\Tout$ oracle for the next slot.

_Then: #1 $\Tout$ oracle. (The former #1, the MTDFA
scaffolding replacement, **shipped 2026-07-16** — see Done; it removed the explicit
DFA materialisation from the goal *construction*, the other half of the cost the
symbolic DFA-product rewrite started on the *product*, and a live benchmark
confirmed the win.) #1's rationale (grilled 2026-07-05, updated 2026-07-08): the
internal controller verifier is **banked** (shipped in `81a4cf4`, migrated onto the
shared product core, all four PRD gates clean), so the known-**output** $\Tout$
oracle rounds out the external `ltlfsynt` cross-check._

### `ltlfsynt` oracle — known-**output** ($\Tout$) reduction — **#1** (MTDFA replacement shipped 2026-07-16, see Done)
- **PRD: GRILLED 2026-07-31 → `docs/prd/ltlfsynt-oracle-known-output.md` (draft),
  ready for `/test-writer`.** The known-**input** ($\Tin$) half remains
  `docs/prd/ltlfsynt-oracle.md`, which this extends in place (not superseded).
  Two phases: (1) the corpus, (2) generalize `run_faithfulness_guard` over `Role`.
  **All 47 corpus rows were verified live against both binaries during the grill**
  (Spot 2.15.1) — zero divergence — so `/test-writer` does no fixture design.
  Three seeds settled: the reduction **is** equirealizable ($\psiout$ pins
  $\Oknown$ to a forced move, same argument `solve_mtdfa` uses; $S_C\circ\Tout$
  composes to exactly a Mealy pair-choice, so no turn-order loss); the
  discriminating-fixture discipline **inverts** ($\Tout$ constrains the *system*,
  so flips run bare-R → guarantee-U, the opposite of Tables A–C); turn order lines
  up, and the copy-from-$\Ofree$ fixture ($x \leftrightarrow o$) is the only family
  that can detect a violation — under a wrong $\Sigma_0=\mathcal{I}$ it fails
  `parse_transducer`'s functionality check outright. Mixed
  $\psiin \to (\varphi \land \psiout)$ is in scope and shows **both** flip
  directions in one table.
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

### Acceptance mark lost on an edgeless accepting state — **known live bug, TWO sites** (found 2026-07-17; widened from one site to a class 2026-07-27)
- **The class:** a builder computes acceptance correctly, then attaches the mark
  **only inside a per-edge loop** — so a state with **zero** out-edges emits no edges
  and no marks, and Spot's `state_is_accepting` (which reads a state-based mark off a
  state's *first out-edge*) reads the flag back as `false`. **Lost in transit**, not
  deliberately reinterpreted. Four builders, two broken:
  - `materialize_product` (`src/product.cpp:341`) — **broken.** Affects `DfaProduct`
    and `NfaProduct`.
  - `emits_dfa` (`src/emits_dfa.cpp:49`) — **broken; found 2026-07-27** by the
    `MtnfaProduct` expected-divergence fixture on its first run. A $\delta$-dead
    transducer state still gets a state (via `discover(d)`) but zero edges, so
    `spot::twadfa_to_mtdfa` reads it non-accepting and the product intersection
    rejects. Affects **`MtdfaProduct`** — which this item previously implied was
    immune, because acceptance in the mtdfa representation rides the *incoming*
    terminal. The correction: that is true only when acceptance never passes through
    a `twa_graph`. `MtdfaProduct` launders it through `emits_dfa`
    → `twadfa_to_mtdfa`, so it is exposed; `MtnfaProduct` reads
    `any(goal.accepting[s])` straight off `Mtnfa::accepting` and is genuinely immune.
  - `nfa_to_dfa` (`src/nfa_to_dfa.cpp:105`) and `reverse_dfa_to_nfa` (`:40`) —
    **defend correctly**, with a `bddfalse`-guarded self-loop carrying the mark. That
    is the known-good idiom the two broken sites are missing.
- **Intent:** fix, and decide the semantics first. Fixing `emits_dfa` alone is a
  *partial* fix (the mtdfa route becomes right while the explicit route stays wrong —
  the methods still disagree, just along a different line), so the class wants fixing
  together, which is what makes the semantics call below load-bearing.
- **Confirmed fix shape, verified 2026-07-27:** adding the defensive self-loop to
  `emits_dfa` makes the divergence fixture pass — but breaks two existing tests,
  `EmitsDfa.UndefinedAtStateHasNoOutgoingEdgesForAnyLetter` (which deliberately pins
  the current edgeless shape) and `EmitsDfa.AcceptsTheEmptyWordAcrossEveryFixture`.
  So the fix is **not a drive-by**: it re-opens `emits_dfa`'s contract.
- **Reachability:** needs a **partial transducer** — a $\cons$-passing product state
  whose $\delta$ is undefined on every letter. Legal (`transducer.hpp:24`,
  `main.tex:114-115`) and explicitly handled by `build_product_nondet`. Reproduced end
  to end: $\varphi=b$, $\Ofree=\{b\}$, `t_in` with a delta-dead state 1 →
  **both** `DfaProduct` and `NfaProduct` say UNREALIZABLE where REALIZABLE is
  expected.
- **Pre-existing, NOT introduced by `NfaProduct`** (found by its domain review;
  deliberately deferred out of `docs/prd/nfa-product.md` on 2026-07-17 to keep that
  PRD's scope to Method 1 explicit). It affects `DfaProduct` equally, so fixing it
  **changes shipped `DfaProduct` verdicts on partial transducers** — a semantic
  change worth its own grill, which is why it isn't a drive-by.
- **Why the oracles were blind to it — the load-bearing lesson:**
  - the **cross-method** oracle couldn't see it while only the explicit route existed:
    `DfaProduct` and `NfaProduct` share `materialize_product` + `solve_dfa`, so they
    fail **identically and agree**. This changed on 2026-07-27: `MtnfaProduct` is
    genuinely immune, so the cross-method oracle now *would* catch the class — but only
    on a partial transducer, which is the next bullet;
  - the **generated corpus** can't see it: `random_tin` is deterministic + **total**
    by construction (the committed Case-A regime,
    `tests/ltlfsynt_oracle_test.cpp:1337`) and `t_out` is `trivial_transducer`, so
    the partial-transducer regime is simply unexercised at corpus scale.
  A green suite was fully consistent with this bug, and that is how it survived from
  2026-07-17 to 2026-07-27 with a second site undiscovered. **Now pinned:**
  `MtnfaProductExpectedDivergence.*` (`tests/mtnfa_product_test.cpp`) asserts the
  current wrong verdicts of **both** broken sites, so the class can no longer regress
  silently and the eventual fix has its regression test ready — flip both
  `EXPECT_FALSE`s to `EXPECT_TRUE`. Any fix must still ship coverage for a partial
  `t_out` too (the fixture only exercises a $\delta$-dead `t_in`).
- **Seeds for grilling:**
  - **Semantics first:** is a $\cons$-dead transducer state reached *after*
    acceptance a **win**? LTLf acceptance is at the end of a finite trace, so an
    accepting state where the trace can only stop looks like a win — but that is a
    $\Tin$/$\Tout$ partiality reading (`\cref{def:consistency}`), not something
    `alg:dfa_product` spells out. Settle this before touching code; the bug is real
    either way (the flag is *lost*, not deliberately reinterpreted).
  - Fix locus: the defensive self-loop in `materialize_product` (mirrors the two
    existing precedents), or make `ProductGuards`→graph carry acceptance
    out-of-band so no caller can lose it again?
  - Does the corpus want a **partial-transducer regime** (Case B) generally? This
    bug is evidence the total-by-construction corpus has a systematic blind spot,
    not just this one gap.

### `main.tex` `\algname{NfaToDfa}` empty-subset rule is underspecified (LaTeX-only, from theory-review 2026-07-17)
- **Intent:** a *documentation* fix in `main.tex` (the latex submodule), not a code
  change. The `\algname{NfaToDfa}` black box (~main.tex:268) states no rule for the
  empty subset, and both sources of an empty $\delta_{prod}$ — a **non-$\cons$**
  letter and a **$\cons$-dead** letter ($\delta_N(s,v)=\emptyset$) — collapse to
  $\emptyset$ in the paper (main.tex:225–232). No uniform reading of the black box is
  sound: skip-both → spuriously realizable; sink-both → spuriously unrealizable. The
  explicit `NfaProduct` already corrects this by completing $N$ before the product
  (`complete_here`), exactly as Method 2 completes $A$ — but the paper is silent.
- **Fix:** apply the drafted `\cl[inline]{…}` note (verbatim in
  `docs/prd/nfa-product.md` "Open theory questions touched") after the reachability
  note at ~main.tex:241. **Verified faithful; code needs no change** — this is purely
  a clarity gap in the write-up.
- **Why Later:** main.tex is a submodule that only builds on Overleaf; batch it with
  the next LaTeX pass (re-run `/glossary` + `/theory-review` after the Overleaf pull,
  since line numbers drift).

### Investigate Nondeterministic Decision Diagrams for representing the NFA (Method 1)
- **Intent:** Method 1 — the NFA route (`LtlfToNfa` / `NfaProduct` / `NfaToDfa`,
  glossary *NFA / DFA for the Goal*) — **isn't built yet**. Before building it on
  an explicit `twa_graph`, probe *nondeterministic* decision diagrams (nBDD /
  nFBDD / nOBDD — decision diagrams carrying explicit "or"/nondeterminism nodes)
  as the NFA's native representation, the way MTBDD is the DFA's (Now/next #1).
  The pitch: an NFA is the natural symbolic object for Method 1 (no
  determinization until *after* the product), and nondeterministic DDs are known
  to be exponentially more succinct than their deterministic counterparts.
- **Why this is an *investigate*, not a build:** it may well conclude "no usable
  library, do the explicit thing." Two headwinds to establish up front: (1) the
  succinctness win (nFBDD ⊋ uFBDD ⊋ FBDD, exponential separations) is paid for by
  **losing canonicity** and cheap equivalence/complement — which is exactly what
  BDD-based product and fixpoint code leans on; (2) **Spot ships no nBDD type** —
  `mtdfa` is deterministic-DFA-shaped — so this means BuDDy-level or external
  machinery, i.e. real cost, not a library swap.
- **Seeds for grilling:**
  - **Which operations does `NfaProduct` actually need?** If it's only $\land$ with
    cons-guards plus reachability, nondeterminism nodes may be cheap. If it needs
    equivalence or complement, the canonicity loss probably kills it outright.
    Settle this first — it's the cheapest question that can end the investigation.
  - `NfaToDfa` runs **after** the product (the stage-mapping question deferred
    under benchmarking), so a symbolic NFA has to survive determinization. Does
    subset construction over an nDD land anywhere better than Spot's `powerset`?
  - **Honest baseline — the "semi-symbolic" shape** (explicit states, BDD-symbolic
    transition labels) is what Spot's `twa_graph` *already is*. Establish what a
    fully-symbolic nDD adds over that before assuming there's a gap to close.
  - **Does Method 1 survive Now/next #1?** If Method 2 goes fully MTDFA, ask
    whether the NFA route stays interesting as a distinct method or collapses into
    "the same pipeline without early determinization."
  - Literature starting points (from a quick unvetted search — verify these are the
    right entry point before leaning on them): knowledge-compilation succinctness
    for nFBDD/uFBDD/OBDD (arXiv `1802.04544`, `1811.02944`); "A Circus of Circuits"
    (arXiv `2404.09674`) for the decision-diagram ↔ circuit ↔ automata map.

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
  DFA-product rewrite (shipped, see Done) — benchmarking is the tool's eventual purpose,
  so the minterm loop's cost stops being an acceptable baseline. A first
  measurement pass here also sets the baseline the symbolic rewrite is judged
  against.
- **Grilled 2026-07-13 → PRD `docs/prd/benchmarking.md` (draft).** Decided: a
  two-tier design — a soft closed registry of canonical comparable `Stage`s
  (`automaton_construction` / `product_construction` / `game_solving` /
  `aggregation`) plus free-form nested sub-spans; an ambient thread-local RAII
  collector (`BenchScope`/`BenchTimer`) so the `Synthesis` contract and the
  free-function black-boxes stay frozen and a new phase needs no infra; whole-run
  wall `total`; structured-nested JSON via `--benchmark=FILE` (stdout untouched);
  always-compiled, runtime-gated no-op; time-only, `DfaProduct` wired now.
- **Deferred out of that PRD (track here):**
  - **Size metrics** — |states|/|edges| of the DFA & product, BDD node counts,
    controller size. The `BenchReport` container is designed to hold them; only
    time is populated in the first pass. Do next after the timing infra lands.
  - **Chrome-trace exporter** — a second serializer over the same generic
    `BenchReport` span tree (`ph:"X"`, `ts`/`dur`) for perfetto / `chrome://tracing`
    flame-chart viewing of one run. Pure add-on; wanted only once eyeballing a
    slow run matters.
  - **Wire the other four methods** — each adopts the frozen mechanism by adding
    span guards (no infra change); this is also where the **NFA-method stage
    mapping** convention gets settled (determinization runs *after* the product —
    is it folded into `product_construction` or its own reserved stage?).

### Link `libmona` directly instead of shelling out to the `mona` binary
- **Intent:** replace the `std::system("mona …")` subprocess call in `run_mona`
  (`src/mona_dfa.cpp`) with a direct link against `libmona`, dropping the fixed
  fork/exec + shell + process-startup cost paid on every NFA construction.
- **Note (2026-07-18, from benchmarking the three product methods):** on small
  formulas that ~1.5 ms fixed overhead *is* essentially all of `NfaProduct`'s
  `automaton_construction` stage (mona's own compute stays sub-10 ms, below its
  `-t` timer resolution). Lower priority than it looks: on **bigger** formulas the
  NFA path's real scaling cost turned out to be the in-process subset
  determinization (`nfa_to_dfa`, the `determinize` sub-span under
  `product_construction`, worst-case exponential — saw a 1.2 s spike), not mona.
  So this is a fixed-overhead win, not a scaling fix.
- **Seeds for grilling:** _(tbd)_ — libmona API surface vs the current `-w` text
  parse; also drops the temp-file write + `-w` table parsing, or keep those?

## Done

### The pending `\cl` notes on `\cref{lem:outdep-diagonal}` / `\cref{lem:outdep-transducer}` — **DONE 2026-07-31**
- **Outcome:** both written into `latex/main.tex`, uncommitted, by the
  `/theory-review` run under `/code-reviewer` on the Phase 3 diff. The
  diagonal note landed **verbatim** as drafted here (reflexivity of "reachable",
  the $\lnot X[!]\mathtt{tt}$ witness for why an irreflexive reading is unsound,
  the empty-$\liveset{s}$-at-a-live-$s$ case, and $\liveset{s}=\emptyset$ at
  non-live $s$) — the draft is dropped from this file rather than duplicated.
- **Plus one that was not drafted here:** a second note on
  `\cref{lem:outdep-transducer}`, recording that "the defaulted letters lie
  outside $L(\varphi)$" is a statement about **prefixes** (an uncovered
  observation is one whose every completion goes to a non-live state), that
  "lose the system the game" is read under the system-controlled-termination
  semantics `\cref{def:probDef}`'s note leaves open, and a two-direction sketch
  of equirealizability keyed on totality and on winning strategies never
  leaving the live part.
- **Anchor drift, already repaired:** the two notes shifted the commented-out
  input-dependency block by +8 lines; `scripts/check-main-tex-refs.py --fix`
  rewrote the affected citations across `docs/GLOSSARY.md` and
  `docs/prd/output-dependencies-tool.md` in the same working tree. Still
  **unpushed** to Overleaf (fetch first, never force).

### `OtfMtdfaProduct` — Method 3.1, the on-the-fly DFA product — **DONE 2026-07-29**
- **Outcome: POSITIVE — the first method to beat the standing champion.** Landed in
  `0ce5fab` with all four gates closed and `ctest` 420/420. Benchmarked against
  `MtdfaProduct` over two controlled families (same $\varphi_n$, same game, only the
  $\cons$ pruning differs, state counts validated *before* timing per the
  `MtnfaProduct` lesson): **up to 5488x faster** where $\cons$ prunes — and *flat*
  (0.68 ms -> 0.94 ms) while `MtdfaProduct` goes exponential — at a **shrinking**
  1.4x-2.7x cost where it prunes nothing. Full tables in
  `docs/prd/otf-mtdfa-product.md` "Benchmark results, 2026-07-29".
- **What it actually beats:** not the product — `spot::product` prunes fine (14
  states at $n=12$) — but the **materialization** of the $2^n$-state goal that
  Method 2 must build first. Exactly `main.tex:335`'s `\na`.
- **What it cost:** one deliberate deviation from `\cref{alg:otfdfa_product}` (I5:
  collapse to the accepting sink once $\varphi$ is irrevocably satisfied), which
  makes $L(P)$ a strict superset of the paper's product language — equirealizable,
  same controller, but **only** because termination is system-controlled. Now
  documented on the declaration itself.
- **Left open:** Phase 2 and Method 3.2, both in Now/next; `--minimize-mtdfa`
  silently ignored for `--otf-mtdfa-product`; state numbering depends on
  unspecified C++ argument-evaluation order (shared with `src/mtnfa_product.cpp`).
- **Theory review found no code-bug**, but did find one in the *paper*: the
  state-keyed $F_P$ of `\cref{alg:otfdfa_product}` is unsound. Four `\cl` notes
  written into `latex/main.tex`, **unpushed**.

### `MtnfaProduct` — Method 1 in the mtdfa representation — **DONE 2026-07-28**
- **Outcome: landed, fully reviewed, and benchmarked — the benchmark verdict is
  NEGATIVE.** `MtnfaProduct` + `mtnfa_product_to_mtdfa` + `--mtnfa-product` all ship;
  glossary / tests / code-review (domain + generic) / theory-review all closed;
  `ctest` 400/400. **Method 1's late determinization does not pay:** `MtdfaProduct`
  wins every measured instance by 9×–3000×, and the fused product determinization
  never yields fewer states than `spot::ltlf_to_mtdfa` (exactly 2× more on the family
  where the Goal NFA is genuinely small). The mtdfa *representation* of Method 1 is
  still worth keeping — ~1.7× over explicit `NfaProduct` when the NFA is small, 12× at
  `game_solving` — but 16× **slower** than `NfaProduct` when it is not. Keep it as the
  paper's NFA route and as a differential oracle; not a default. Numbers, instance
  design and a re-run trap (the intuitive NFA-blowup family degenerates — its NFA is
  as big as its DFA, because `ltlf_to_nfa` is mirror-based) in
  `docs/prd/mtnfa-product.md` "Benchmark results, 2026-07-28".
- **Follow-ups it leaves open** (all recorded in that PRD): the F2 precondition is
  still `assert`-only; the generated corpus never reaches the multi-block
  `delta_edges` path (its transducers are all out-degree 1); `--minimize-mtdfa` is
  silently ignored for `--mtnfa-product`; `README.md`'s wired-flag list is stale.
- **History (as-planned):**
- **PRD:** `docs/prd/mtnfa-product.md` (draft, grilled 2026-07-27) — ready for
  `/glossary` then concurrent `/developer` + `/test-writer`. Both seeds below were
  settled in the grill: the product stays symbolic and the transducer states are
  **tracked alongside** the goal subset (the `(R,q_{in},q_{out})` key, `main.tex:241`),
  **not** folded into the terminal; `turn_order.hpp` is reused exactly as
  `MtdfaProduct` does.
- **Intent:** once the MTNFA representation lands (`docs/prd/mtnfa.md` — the data
  structure + construction + determinize-to-`mtdfa` + isolated `product_xor`
  oracle), build the full Method-1 mtdfa synthesis method: the **symbolic**
  NFA×transducer product (cons filter + transducer-state tracking), determinize the
  product MTNFA into a `spot::mtdfa`, wired to `solve_mtdfa` (landed). Ships as a
  `Synthesis` class `MtnfaProduct` + `--mtnfa-product` CLI + the three canonical
  benchmark stages. **Depends on:** the MTNFA PRD, explicit `NfaProduct` (reference
  oracle), `solve_mtdfa`.
- **Why split off:** the MTNFA PRD de-risks the bespoke set-terminal apply +
  determinizer in isolation on the goal NFA alone; this item then adds the
  well-understood product/cons layer (mirrors `MtdfaProduct`'s route) + method
  wiring on top.
- **Seeds for grilling:**
  - Product stays symbolic: cons = `emits_region(q_in) & emits_region(q_out)`
    restricting live letters; the reachability invariant makes determinized states
    $(R, q_{in}, q_{out})$. Fold transducer successors into the terminal, or track
    them alongside?
  - Comparisons this unlocks: vs `MtdfaProduct` (method axis — does NFA-product's
    determinize step beat DFA-product avoiding it?), vs `NfaProduct` (representation
    axis).
  - Reuse `turn_order.hpp` (`require_turn_order_aps`) as the `solve_mtdfa`
    precondition, exactly as `MtdfaProduct` does.


### Explicit `NfaProduct` — Method 1 (`alg:nfa_product`), the paper's NFA route
- **Intent:** build Method 1 explicitly (`NfaProduct`, glossary *the five
  methods*): the explicit NFA×transducer product over `ltlf_to_nfa` (landed),
  `NfaToDfa` subset determinization into a `twa_graph`, then `solve_dfa`. This is
  the paper's actual Method 1 and, downstream, the **reference oracle +
  representation baseline** the mtdfa route (`MtnfaProduct`) is graded against.
  Independent — needs only `ltlf_to_nfa` + the existing
  `build_product`/`solve_dfa` machinery; does **not** depend on the MTNFA PRD.
- **Outcome:** shipped 2026-07-17 (`41af246` implementation, gates closed in
  `df232a6`, merged `c2fb7a9`). PRD `docs/prd/nfa-product.md` **implemented** —
  `nfa_to_dfa` + `build_product_nondet` (Phase 1) then `NfaProduct` +
  `--nfa-product` + the canonical bench stages with `determinize` as a nested
  sub-span under `product_construction` (settling the stage-mapping seed). $N$ is
  completed before the product (`complete_here`), which is also what exposed the
  `main.tex` `\algname{NfaToDfa}` empty-subset gap now tracked under **Later**.
  Its domain review also found the `materialize_product` acceptance-mark bug
  (pre-existing, deferred to **Later**).
- **Seeds for grilling:** _(resolved in the PRD grill)_

### Replace the explicit-DFA scaffolding with MTDFA — `MtdfaProduct` (2nd impl of Method 2)
- **Intent:** `ltlf_to_dfa` already builds a `spot::mtdfa` via `ltlf_to_mtdfa`,
  then throws the symbolic form away (`as_twa` + `complete_here` path-enumeration
  blowup). Keep the MTDFA all the way through — goal, product, and game stay MTBDD
  arrays, solved with Spot's own MTDFA game solver the way `ltlfsynt` does — added
  as a **second implementation of Method 2** alongside `DfaProduct` (left untouched
  to preserve the differential).
- **Outcome:** shipped, both phases, PRD `docs/prd/mtdfa-product.md` **CLOSED
  2026-07-16**, all gates clean. **Phase 1** (`61b1ad0`): `emits_dfa` (the
  *Output-agreement automaton*, no rejecting sink — skip-not-sink, faithful to
  `alg:dfa_product`'s partial $\delta$), `turn_order.hpp`
  (`register_turn_order_aps` / `require_turn_order_aps` — turn order rides the BDD
  variable order, Ifree strictly above every controllable), `solve_mtdfa` (decision
  2: pinned $\Iknown,\Oknown$ made **controllable** then projected strategy-side,
  theory-reviewed equivalent to `solve_dfa`'s arena-side projection), `MtdfaProduct`,
  and the `--mtdfa-product` CLI flag. The `backprop_nodes=true` segfault was
  root-caused to **upstream Spot #639, fixed in 2.15** (not our bug) — `CMakeLists`
  now requires `libspot >= 2.15`. **Phase 2** (`d1b0355`, gate bookkeeping
  `0d2f93c`): the three canonical `BenchTimer` stages wired
  (`automaton_construction` = `ltlf_to_mtdfa` **alone** — the measured win) plus a
  `minimize_mtdfa` knob (own span, default off). Suite green **307/307**;
  `/code-reviewer` + `/code-review` + `/theory-review` all clean. **Benchmark
  validated** the core claim live vs `DfaProduct` on realizable + unrealizable
  instances: faster at every stage — `automaton_construction` ~1.2–1.7× (grows with
  goal-DFA size), `product_construction` / `game_solving` 5–46× by staying symbolic;
  verdicts never disagreed. `minimize_mtdfa` shows no downstream payoff at these
  sizes (adjacent free real estate, as predicted). Follow-ups still open under
  **Later**: size metrics + the other-four-methods stage-mapping (benchmarking item).

### Symbolic DFA-product construction (skip the minterm loop)
- **Intent:** replace Method-2 `DfaProduct`'s exponential minterm loop
  (enumerate $v\in2^{\mathcal I\cup\mathcal O}$, group into guarded edges —
  $2^{|\mathcal I\cup\mathcal O|}$ by design) with **symbolic BDD-guard algebra**:
  compute successors and the $\cons$ filter directly on edge-guard BDDs, never
  materialising a letter. Promoted to #1 (2026-07-10) because **benchmarking** is
  the tool's eventual purpose, so the letter loop stopped being an acceptable
  baseline.
- **Outcome:** shipped in two phases (PRD `docs/prd/symbolic-dfa-product.md`,
  `implemented (Phase 1 + Phase 2)`, all four gates clean). Phase 1 (`326136c`):
  the symbolic `Transducer` contract — `emits_region(q)` (region form of `emits`)
  + `delta_edges(q)` (edge-partition form of `delta`) on the base class,
  implemented in `OutputLabeledTransducer`, with the Phase-1 contract-equivalence
  oracle (`tests/symbolic_transducer_contract_test.cpp`). Phase 2 (`d88904c`):
  `build_product_symbolic` (guard = $g_{goal}\wedge\bigwedge_i(g_i\wedge
  \texttt{emits\_region}(q_i))$, cost = product of out-degrees) + `ProductGuards`
  / `to_guard_map` / `materialize_product`, `DfaProduct::synthesize` rewired off
  the minterm loop. The **build-equivalence metamorphic oracle**
  (`tests/product_build_equivalence_test.cpp` + generated-corpus body) asserts
  `build_product_symbolic == to_guard_map(build_product(...))` (BDD-equal game),
  bug-injection-verified non-vacuous. Suite green 226/226, verdicts byte-identical
  (invariant 4). `/code-reviewer` + `/code-review` clean (2 low-severity considers
  applied), `/theory-review` blessed the symbolic-$\cons$ region faithful to
  `\cref{def:consistency}` by minterm distributivity (with a `\cl` traceability
  note pushed to Overleaf, `43b15f4`). The per-letter core is **kept** (backs
  `verify_controller` + the oracle reference). Two follow-ups logged under
  **Later**: the symbolic `verify_controller` ν-fixpoint (deliberately out of
  scope) and benchmarking (the driver this rewrite serves).

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
