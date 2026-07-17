# PRD: NfaProduct — explicit Method 1 (NFA product), the paper's NFA route

**Status:** implemented — Phase 1 (`nfa_to_dfa` + `build_product_nondet`,
uncommitted); Phase 2 (`NfaProduct` + CLI, uncommitted) landed 2026-07-17.
**Interface:** implements `Synthesis` as `NfaProduct` (the **explicit** *Representation*
of Method 1); adds the generic subset construction `nfa_to_dfa` and the
nondeterministic product-guard builder `build_product_nondet`. **Independent of
`docs/prd/mtnfa.md`** — it needs only the landed `ltlf_to_nfa` plus the existing
`build_product`/`materialize_product`/`solve_dfa` machinery, and touches no mtdfa
code. It is, downstream, the **reference oracle + representation baseline** the
mtdfa route (`MtnfaProduct`, `docs/BACKLOG.md`) will be graded against.
**Recommended workflow:** **sequential** — freeze confidence is *tentative*: the
public `NfaProduct : Synthesis` surface is high-confidence (identical shape to
`DfaProduct`), but the two genuinely new internals (`nfa_to_dfa`,
`build_product_nondet`) are invented here and their signatures may shift, so
`/developer` lands them first and `/test-writer`'s per-function tests bind after.
The domain oracles (differential, metamorphic, cross-method) parallelize
regardless — they bind to the public `synthesize` interface and the math.
**main.tex ref:** Method 1 §`nfa`, `\cref{alg:nfa_product}` (lines
`alg:nfa_product:cons`, `alg:nfa_product:determinize`, `alg:nfa_product:solve`),
`\cref{def:consistency}`, `\cref{alg:ltlftonfa}` (the NFA $N$ this consumes), and
the **reachability-invariant note** at `main.tex:241`
($R\times\{q_{in}\}\times\{q_{out}\}$).

**Gates:**
- [x] glossary        — *closed 2026-07-17* (`/glossary`, uncommitted): folded the
  explicit `nfa_to_dfa` into *Goal automaton determinization* (now bicameral,
  explicit + mtdfa), added `build_product_nondet` to *Product*, and repointed the
  *Goal DFA construction* "still future" note. All rejected-synonym lines set.
- [x] tests           — *closed 2026-07-17*: Phase-1 structural (`nfa_to_dfa_test.cpp`,
  `product_test.cpp`) + the isolated determinize oracle with a **discriminating**
  negative control; Phase-2 cross-method (vs `DfaProduct`), metamorphic
  (`verify_controller`), corpus differential (`--nfa-product` vs `ltlfsynt`), and
  the bench-span shape check. `ctest` 358/358. The cross-method oracle **caught a
  real defect** on first run (the `materialize_product` empty-`ap()` bug — see the
  CONTRACT CHANGE note in *Interfaces & types*), which is precisely its purpose.
- [~] code-review     — **domain (/code-reviewer) run 2026-07-17; generic
  (/code-review) NOT run.** Domain review found the diff clean on `Synthesis`
  conformance, BDD/Spot idiom, the `bddfalse`-self-loop precedent, and glossary
  naming. Two findings actioned: the stale GLOSSARY entries are fixed (this
  commit); the one **must-fix** — `materialize_product` dropping $F_P$ on an
  edgeless accepting product state — is **pre-existing, affects `DfaProduct`
  equally, and was deliberately DEFERRED** to `docs/BACKLOG.md` *Later* (it changes
  shipped `DfaProduct` verdicts on partial transducers, so it wants its own grill).
  **Known-live bug at merge; both our oracles are structurally blind to it** (the
  cross-method oracle because both methods share the broken function and agree; the
  corpus because `random_tin` is total by construction). Not re-litigated here.
  Lower-severity, left open: no `goal is COMPLETE` precondition throw in
  `build_product_nondet` (both sibling builders have one); `nfa_to_dfa.hpp`'s
  undocumented state-based-acc *input* precondition.
- [ ] theory-review   — code ↔ math faithfulness vs main.tex. **Started 2026-07-17,
  stopped early (unfinished) — NOT closed.** Still owes a verdict on the
  load-bearing non-$\cons$-skip vs $\cons$-dead-sink distinction (Behaviour §1), the
  `main.tex:241` reachability invariant, and the $\varepsilon$-convention.

## Goal

Method 1 keeps the Goal automaton **nondeterministic** — the single-exponential
NFA $N$ (`ltlf_to_nfa`, landed) — forms the product $P=N\times\Tin\times\Tout$
keeping only $\cons$-consistent transitions, **subset-determinizes** the product
into a DFA $D$, then solves the reachability game on $D$. Determinizing only the
*product* (never $\varphi$'s full DFA) is what keeps the blowup single-exponential
in $\varphi$ and polynomial in the transducers (`main.tex:241`).

This PRD delivers Method 1 in the **explicit** *Representation* (the way
`DfaProduct` delivers Method 2): a `Synthesis` class `NfaProduct` over
`spot::twa_graph`. It is the paper's *actual* Method 1, and its role is
deliberately as much **oracle as feature** — the correctness-obvious explicit
baseline that the forthcoming symbolic mtdfa route (`MtnfaProduct`) is
cross-checked against on realizability verdicts. It reuses everything it can:
`ltlf_to_nfa`, `LetterAlphabet`, `emits`, `Transducer::delta`, `ProductGuards` +
`materialize_product`, and `solve_dfa`. Only two pieces are new — a generic
explicit subset construction `nfa_to_dfa` (the still-future explicit
`\algname{NfaToDfa}` the glossary reserves), and a nondeterministic product-guard
builder `build_product_nondet`.

**This PRD supersedes nothing.** It is the explicit sibling of the mtdfa-route
work: the `docs/prd/mtnfa.md` representation PRD and its follow-on `MtnfaProduct`
(both `docs/BACKLOG.md`) realize the *same* `\algname{NfaToDfa}` symbolically;
this one realizes it explicitly and independently, and lands first as their oracle.

## Ubiquitous-language terms used

Existing, unchanged: *Goal formula* ($\varphi$), *NFA / DFA for the Goal* ($N$),
*Goal NFA construction* (`ltlf_to_nfa`), *Goal DFA construction* (`ltlf_to_dfa`,
the oracle side), *Consistency* ($\cons$, `consistent`), *Output agreement*
(`emits`), *Product* (`ProductState`, `ProductGuards`, `materialize_product`,
`build_product`), *Letter alphabet* (`LetterAlphabet`), *Game solving*
(`solve_dfa`), *Controller* / *Controller verifier* (`verify_controller`),
*Canonical benchmarking stage* (`Stage`), *Generated corpus*. The class name
**`NfaProduct`** is already fixed by the *five methods* table (explicit Method 1).

**Glossary — landed 2026-07-17 by `/glossary`** (no gaps remain; `/developer` binds
to these names as canonical):

- **`nfa_to_dfa`** — the **explicit** `\algname{NfaToDfa}` (`alg:nfa_product:determinize`)
  as a generic subset construction `twa_graph` → `twa_graph`. Folded into the
  *Goal automaton determinization* entry, now **bicameral** (explicit `nfa_to_dfa`
  + mtdfa `mtnfa_to_mtdfa`), exactly like *Goal DFA construction* / *Game solving*.
  The entry captures the **∅-skip vs `bddfalse`-sink** substrate asymmetry (why the
  explicit route needs `spot::complete_here` and the mtdfa route does not).
- **`build_product_nondet`** — the per-letter **nondeterministic** product-guard
  builder (multi-destination), added to the *Product* entry beside `build_product`
  / `build_product_symbolic`.

## Behaviour / semantics (from main.tex)

The pipeline is `\cref{alg:nfa_product}`, realized as:

1. **NFA + completion.** $N=\algname{LtlfToNfa}(\varphi)$ (`ltlf_to_nfa`, landed —
   nondeterministic, **not** completed, sole final state $F_N=\{s_{D,0}\}$). Because
   $N$ is not complete, a $\cons$-consistent letter on which the goal *dies*
   ($\delta_N(s,v)=\emptyset$) is otherwise indistinguishable, inside a pre-built
   product, from a **non-$\cons$** letter — yet the two must differ in the game:
   non-$\cons$ is **impossible** (skip, no edge), $\cons$-but-dead is
   **env-playable and losing** (edge to a rejecting sink). We restore the
   distinction by **completing $N$** with `spot::complete_here` into $N_c$
   (a fresh non-accepting sink, $\delta$ total) *before* the product — exactly as
   `ltlf_to_dfa` completes $A$ for Method 2 — so that in the product cons-dead
   becomes a real successor $\{(\text{sink},q_{in}',q_{out}')\}$ (kept) while
   non-$\cons$ letters are dropped by the $\cons$ filter (absent). This is what
   makes `NfaProduct` agree with `DfaProduct`, whose $A$ is likewise complete.

2. **Product $P$ (`alg:nfa_product:cons`).** $P=N_c\times\Tin\times\Tout$ with
   $S_P=S_{N_c}\times Q_{in}\times Q_{out}$, initial
   $\langle s_{N,0},q_{in,0},q_{out,0}\rangle$, $F_P=F_{N}\times Q_{in}\times
   Q_{out}$ (state-based), and
   $\delta_{prod}(\langle s,q_{in},q_{out}\rangle,v)=\{\langle
   s',\delta_{in}(q_{in},v),\delta_{out}(q_{out},v)\rangle : s'\in\delta_{N_c}(s,v)\}$
   **iff** $\cons(q_{in},q_{out},v)$, else $\emptyset$ (`main.tex:198`–`\cref{alg:nfa_product}`).
   $P$ is **nondeterministic** (many $s'$ per letter). It is built as a
   `ProductGuards` map — one source $\to$ per-destination OR'd guard, which
   represents multiple same-letter destinations natively — then materialized to a
   `twa_graph` by the existing `materialize_product` (state-based Büchi, $F_P$ on
   the acc flag).

3. **Determinize $D=\algname{NfaToDfa}(P)$ (`alg:nfa_product:determinize`).**
   Generic explicit subset construction: a DFA state is a set
   $R\subseteq S_P$; $R_0=\{\langle s_{N,0},q_{in,0},q_{out,0}\rangle\}$;
   $\delta_D(R,v)=\bigcup_{p\in R}\delta_P(p,v)$; $R$ accepting iff
   $R\cap F_P\neq\emptyset$. The **empty subset is skipped** (missing edge =
   reject) — so non-$\cons$ letters (no $P$ edge) contribute nothing, while
   cons-dead letters land in the non-empty $\{\langle\text{sink},\dots\rangle\}$
   subset (a real, reachable, non-accepting DFA state that loops to itself). By the
   **reachability invariant** (`main.tex:241`) every reachable $R$ has the form
   $R'\times\{q_{in}\}\times\{q_{out}\}$ for a single transducer-state pair;
   `nfa_to_dfa` does not *rely* on this (it is a generic subset construction) but it
   is what bounds $D$ at $2^{|S_N|}\cdot|Q_{in}|\cdot|Q_{out}|$ and is a free
   structural oracle (below).

4. **Solve $C=\algname{SolveDfa}(D)$ (`alg:nfa_product:solve`).** `solve_dfa(D, vars)`
   unchanged — $D$ is a state-based-Büchi `twa_graph` in exactly the shape
   `DfaProduct`'s product hands it (non-$\cons$ letters skipped, so nothing to drop;
   §Game solving). `nullopt` = unrealizable.

**Empty word / non-empty traces.** $L(\varphi)$ excludes $\varepsilon$; this is
inherited entirely from $N$ (`ltlf_to_nfa`, $s_{N,0}\notin F_N$) and $D$'s initial
subset $\{s_{N,0}\}\times\dots$ is therefore non-accepting — no new decision here.

## Interfaces & types

**Freeze confidence: tentative.** `NfaProduct : Synthesis` is high-confidence (a
verbatim copy of `DfaProduct`'s shape), but `nfa_to_dfa` and `build_product_nondet`
are invented here and may be reshaped by implementation. Hence **sequential**.

```cpp
// include/ltlf_ek/nfa_to_dfa.hpp                                     [new, Phase 1]
//
// NfaToDfa (alg:nfa_product:determinize) in the EXPLICIT representation
// (docs/GLOSSARY.md "Goal automaton determinization"): a GENERIC explicit subset
// construction, twa_graph NFA -> twa_graph DFA.  Partition-agnostic and
// transducer-agnostic --- it enumerates full minterms over the input's own
// registered APs (nfa->ap()), computes each subset's per-letter union of
// successors, and SKIPS the empty subset (missing edge = reject, incomplete
// output --- no sink of its own).  A subset is accepting iff it contains an
// accepting state of `nfa`; output is state-based Büchi (the abused-DBA
// convention of ltlf_to_dfa / mtdfa::as_twa), deterministic, on nfa's bdd_dict.
// Reachable-subset BFS from {init}; never returns nullptr.
namespace ltlf_ek {
spot::twa_graph_ptr nfa_to_dfa(const spot::twa_graph_ptr& nfa);
}  // namespace ltlf_ek

// include/ltlf_ek/product.hpp  (add beside build_product / _symbolic) [new, Phase 1]
//
// Nondeterministic per-letter product-guard builder (docs/GLOSSARY.md "Product"):
// like build_product but the Goal automaton is an NFA, so a letter yields MANY
// goal successors.  Worklist BFS from `init` over alphabet.letters(); for each
// reachable <s, q_in, q_out> and letter v: cons filter via emits(t_in,q_in,v) &&
// emits(t_out,q_out,v) with delta_in/delta_out defined (else SKIP v); then for
// EVERY goal successor s' in delta_N(s,v), OR v into
// guards[<s', delta_in(q_in,v), delta_out(q_out,v)>].  Node acc flag =
// goal->state_is_accepting(s) (F_P = F_N x Q x Q).  Emits the shared ProductGuards
// so materialize_product turns it into a NONDETERMINISTIC twa_graph (multiple
// edges per source/letter).  Precondition: `goal` is COMPLETE (the caller passes
// complete_here(N)); a set-valued goal-successor helper (goal_delta_set, inline or
// beside goal_delta) collects { dst : v ⊨ edge.cond }.
namespace ltlf_ek {
ProductGuards build_product_nondet(const spot::twa_graph_ptr& goal,
                                   const std::vector<const Transducer*>& taus,
                                   const ProductState& init,
                                   const LetterAlphabet& alphabet);
}  // namespace ltlf_ek

// include/ltlf_ek/product.hpp  --- CONTRACT CHANGE, 2026-07-17     [Phase 2 fix]
//
// materialize_product gains a `vars` parameter and now REGISTERS the universe's
// APs on the product graph it builds.  Previously it made the graph via
// make_twa_graph(dict) and only attached BDD guards --- the guards referenced
// variables numbered in the shared dict, but the graph never DECLARED them, so
// product->ap() came back EMPTY.  DfaProduct never noticed: solve_dfa
// re-registers vars' APs on its own game graph rather than trusting P->ap().
// NfaProduct is the first caller to feed a materialized product into something
// that TRUSTS ap() --- nfa_to_dfa enumerates full minterms over `nfa->ap()`, so
// an empty list collapsed the whole game to the single letter bddtrue (it
// reported phi="o" REALIZABLE and returned a controller verify_controller
// rejects).  Caught by the Phase-2 cross-method oracle vs DfaProduct, exactly
// the regression class it exists for.  Fixing materialize_product (rather than
// patching the NfaProduct call site, or making nfa_to_dfa take explicit letters)
// keeps nfa_to_dfa's partition-agnostic contract intact and stops the product
// graph misreporting its own alphabet for every future caller.  Every call site
// updated; behaviour-preserving for DfaProduct/MtdfaProduct (it only declares
// APs those graphs already use).
namespace ltlf_ek {
spot::twa_graph_ptr materialize_product(const ProductGuards& pg,
                                        const ProductState& init,
                                        const spot::bdd_dict_ptr& dict,
                                        const VariablePartition& vars);  // NEW
}  // namespace ltlf_ek

// include/ltlf_ek/nfa_product.hpp                                    [new, Phase 2]
//
// Method 1 --- NFA product (main.tex §nfa, alg:nfa_product), EXPLICIT
// representation.  Builds N via ltlf_to_nfa, completes it, forms the
// nondeterministic product with T_in/T_out skipping non-cons letters
// (def:consistency), subset-determinizes it (nfa_to_dfa), then solves the game.
// The reference/baseline route for the mtdfa MtnfaProduct.
namespace ltlf_ek {
class NfaProduct final : public Synthesis {
 public:
  std::optional<Controller> synthesize(const spot::formula& phi,
                                       const VariablePartition& vars,
                                       const Transducer& t_in,
                                       const Transducer& t_out) override;
};
}  // namespace ltlf_ek

// cli.hpp / cli.cpp  --- make_synthesis_method: add "--nfa-product" -> NfaProduct,
// mirroring "--dfa-product" -> DfaProduct (the known method×representation flag
// wart, docs/GLOSSARY.md "the five methods"; accepted, not re-litigated here).
```

The assembled `NfaProduct::synthesize` body (three benchmarking spans):

```cpp
const std::vector<const Transducer*> taus{&t_in, &t_out};
validate_product_inputs(phi, vars, taus);
const spot::bdd_dict_ptr dict = t_in.dict();

spot::twa_graph_ptr nfa;                       // Stage::automaton_construction
{ BenchTimer t(Stage::automaton_construction);
  nfa = ltlf_to_nfa(phi, dict); }

spot::twa_graph_ptr D;                         // Stage::product_construction
{ BenchTimer t(Stage::product_construction);
  spot::complete_here(nfa);                    // N -> N_c (rejecting sink, δ total)
  const LetterAlphabet alphabet(vars, nfa);
  const ProductState init{nfa->get_init_state_number(),
                          {t_in.initial_state(), t_out.initial_state()}};
  const ProductGuards pg = build_product_nondet(nfa, taus, init, alphabet);
  const spot::twa_graph_ptr P = materialize_product(pg, init, dict, vars);
  { BenchTimer sub("determinize");             // free-form nested sub-span
    D = nfa_to_dfa(P); } }

BenchTimer t(Stage::game_solving);             // Stage::game_solving
return solve_dfa(D, vars);
```

**If implementation proves this contract wrong:** that is a PRD-change event —
update this section and propagate to the in-flight test branch; the developer does
not silently re-shape the interface. `build_product_nondet`'s exact parameter list
(alphabet passed vs built internally) and the `goal_delta_set` helper's home are
the most likely churn; `NfaProduct : Synthesis` is frozen.

## Implementation phases

Two phases; each compiles green and is independently testable. Phase 1 lands and
directly tests the two invented pieces before the method wiring depends on them.

- **Phase 1 — `nfa_to_dfa` + `build_product_nondet`.** The generic subset
  construction and the nondeterministic product-guard builder, plus a
  `spot::complete_here` call site is *not* yet needed (Phase 2 uses it). **Green
  checkpoint:** (a) the **isolated determinize oracle**
  `nfa_to_dfa(ltlf_to_nfa(φ))` is finite-word language-equal to `ltlf_to_dfa(φ)`
  over the generated corpus (MONA-gated), via the *same* `L(·)=L(φ)` equivalence
  mechanism the landed `ltlf_to_nfa` tests already use; (b) structural unit tests
  on hand-built NFAs (subset states, accepting rule, ∅-skip, a nondeterministic NFA
  whose two successors merge into one subset); (c) `build_product_nondet` on a
  hand-built NFA + two toy transducers produces the expected multi-destination
  edges, and every reachable product subset carries a **single** $(q_{in},q_{out})$
  pair (the `main.tex:241` reachability invariant, as an assertion/test). *May stub:*
  the `NfaProduct` class and CLI.
- **Phase 2 — `NfaProduct : Synthesis` + CLI + cross-method oracles.** The method
  body above, the `--nfa-product` flag, and the three benchmarking spans. **Green
  checkpoint:** `NfaProduct` and `DfaProduct` return the **same realizability
  verdict** on the generated corpus (cross-method oracle); each realizable
  `NfaProduct` controller passes `verify_controller` (metamorphic); the corpus
  **differential** (`ltlf-ek-synth --nfa-product` vs Spot's `ltlfsynt`) agrees; the
  benchmark report emits `automaton_construction` / `product_construction`
  (+ `determinize` sub-span) / `game_solving`.

## Novel mechanisms — pinned to code

Both new pieces are bespoke (no Spot analog for our finite-acceptance rule; no
`main.tex` beyond the black-box names), so each is pinned past sketch level.

**(a) `nfa_to_dfa` — generic explicit subset construction.**
- **State:** a subset as a **sorted, de-duplicated `std::vector<unsigned>`** of
  input state ids, interned to an output DFA state id via
  `std::map<std::vector<unsigned>, unsigned>`. Seed the worklist with
  `R0 = { nfa->get_init_state_number() }` at output id 0 (so `states[0]` is the
  initial state, as `solve_dfa` expects).
- **Letters:** enumerate **full minterms** over `nfa->ap()` (the automaton's own
  registered APs) — $2^{|\text{ap}|}$ of them; empty AP set ⇒ the single letter
  `bddtrue`. (Reference-baseline enumeration; the symbolic route is `MtnfaProduct`'s
  job, out of scope.) For each subset $R$ and minterm $v$:
  $R'=\{\,d : \exists s\in R,\ \exists\text{ edge } s\xrightarrow{c}d \text{ with }
  v\models c\,\}$, sorted+de-duplicated.
- **∅-skip:** if $R'=\emptyset$, **emit no edge** (incomplete output; missing edge =
  reject). Otherwise intern-or-enqueue $R'$ and add edge $R\xrightarrow{v}R'$.
- **Acceptance:** output state $R$ is accepting iff **some** $s\in R$ has
  `nfa->state_is_accepting(s)`; build a **state-based Büchi** `twa_graph`
  (`set_buchi` / the abused-DBA convention `ltlf_to_dfa` uses, so `solve_dfa` and
  the isolated oracle read it identically).
- **Determinism:** BFS discovery order; sorted subset keys; **no seed, no
  randomness**. Never returns `nullptr` (`φ=0` ⇒ a single non-accepting initial
  state with no outgoing edges).

**(b) `build_product_nondet` — nondeterministic per-letter product.**
- **Driver:** worklist BFS from `init` over `alphabet.letters()`; visited set on
  `ProductState`. Reuses `ProductState{unsigned goal; std::vector<unsigned> taus}`
  unchanged (each *product* state is a single triple; nondeterminism is in the
  edges, not the state).
- **Per letter $v$ at $\langle s,q_{in},q_{out}\rangle$:** the $\cons$ filter is
  `emits(t_in, q_in, v) && emits(t_out, q_out, v)` **and** `delta_in`/`delta_out`
  defined (`std::optional` non-null); a failing filter **skips $v$** (no edge —
  the non-$\cons$/undefined case of `\cref{def:consistency}`). On pass: for **every**
  goal successor $s'\in\delta_N(s,v)$ (a set-valued `goal_delta_set`: the dsts of
  $s$'s out-edges whose guard $v$ satisfies — with `goal` complete this is
  non-empty), `guards[<s', *delta_in, *delta_out>] |= v`.
- **Acceptance flag:** `goal->state_is_accepting(s)` per source ($F_P=F_N\times
  Q_{in}\times Q_{out}$, state-based).
- **Output:** the shared `ProductGuards` (so `materialize_product` builds a
  nondeterministic `twa_graph` and the type stays diff-comparable with the
  deterministic builders); **deterministic**, no seed.

## Edge cases

- **Non-$\cons$ vs $\cons$-dead.** The load-bearing case (see Behaviour §1): $N$ is
  completed so $\cons$-dead → a kept `{sink}` subset, non-$\cons$ → skipped. A
  regression here flips a realizability verdict and is caught by the cross-method
  oracle vs `DfaProduct`.
- **$\varphi=\mathtt{0}$ (`ff`).** $L(N)=\emptyset$; `nfa_to_dfa` yields a
  single non-accepting initial DFA state (no edges); the product is empty of
  accepting states ⇒ `solve_dfa` reports **unrealizable** (matching `DfaProduct`).
  Must not crash / not `nullptr`.
- **$\varphi=\mathtt{1}$ (`tt`).** Every non-empty trace accepted; realizable
  trivially; agrees with `DfaProduct` and `ltlfsynt`.
- **Nondeterministic $N$ (many successors).** The point of the method — a letter
  whose two goal successors are both in $R$ merges into one $R'$ (subset union);
  Phase-1 structural test covers it.
- **Empty universe** ($\mathcal I\cup\mathcal O=\emptyset$): `LetterAlphabet` is
  `{bddtrue}` (size 1) and `nfa_to_dfa` enumerates the single letter; both drivers
  handle the bare-terminal case.
- **Partial transducers.** `emits`/`delta` return `nullopt` on undefined states ⇒
  the letter is skipped (`\cref{def:consistency}` partiality clause), same as all
  methods; no special handling.
- **Unrealizable.** `solve_dfa` returns `nullopt`; `NfaProduct::synthesize`
  propagates it.
- **MONA absent.** `ltlf_to_nfa` (hence `NfaProduct` and the isolated oracle) carry
  `ltlf_to_nfa`'s `mona` runtime dependency; those tests are `MONA_FOUND`-gated
  exactly as the existing `ltlf_to_nfa` tests. `nfa_to_dfa` / `build_product_nondet`
  on hand-built `twa_graph`s need no `mona` and always run.
- **Accepting dead-end subset (`nfa_to_dfa`).** The same shape flagged in
  `tests/reverse_dfa_to_nfa_test.cpp`'s `AcceptingDeadEnd` fixture recurs one
  level up: a subset $R$ can be accepting (some $s\in R$ has
  `nfa->state_is_accepting(s)`) yet have every letter $\emptyset$-skipped, so
  it gets **zero** real out-edges in the output DFA. Since
  `spot::twa_graph::state_is_accepting` reads a state-based mark off its
  **first out-edge** (returning `false` with none), such a state would
  silently read back non-accepting without a fix. `nfa_to_dfa` applies the
  same defensive fix `reverse_dfa_to_nfa` uses: a `bddfalse`-guarded self-loop
  carrying the Final mark, added only when a subset is accepting and no real
  edge was emitted for it — never taken by any real letter, but gives Spot an
  edge whose mark to read. Verified by hand (both the merge-into-one-subset
  case and this dead-end case) with ad hoc smoke drivers before handoff; not a
  PRD-contract change (the PRD's "Acceptance" bullet already states the
  intended rule, this just names the encoding needed to make Spot honor it
  uniformly) — flagged here so `/test-writer` includes a dedicated structural
  case for it (mirroring the reversal fixture).

## Test oracles (for /test-writer)

- **Phase 1 primary — isolated determinize oracle.** `nfa_to_dfa(ltlf_to_nfa(φ))`
  finite-word language-equals `ltlf_to_dfa(φ)` over the generated corpus,
  MONA-gated, via the equivalence mechanism the landed `ltlf_to_nfa` tests already
  establish for `L(N)=L(φ)`. This exercises the one genuinely bespoke Phase-1 piece
  (the subset construction) against an **independent** trusted DFA. A **negative
  control** (a deliberately broken determinization, or two known-different formulas)
  must report **non-equal**.
- **Phase 1 structural (no MONA).** `nfa_to_dfa` on hand-built NFAs: ∅-skip, the
  accepting-iff-intersects-finals rule, subset merge on a nondeterministic branch,
  `states[0]` is `{init}`, determinism/completeness-modulo-skip.
  `build_product_nondet` on a hand-built NFA + toy `t_in`/`t_out`: expected
  multi-destination edges; and every reachable product subset carries a **single**
  $(q_{in},q_{out})$ pair (the `main.tex:241` reachability invariant).
- **Phase 2 — cross-method realizability oracle.** For each generated
  $(\varphi,\text{vars},\Tin,\Tout)$, `NfaProduct` and `DfaProduct` return the
  **same** realizable/unrealizable verdict. This is the strongest Method-1 oracle
  and the reason `NfaProduct` is the baseline.
- **Phase 2 — metamorphic round-trip.** Each realizable `NfaProduct` controller
  passes `verify_controller(φ, vars, t_in, t_out, t_c)`.
- **Phase 2 — corpus differential.** `ltlf-ek-synth --nfa-product` agrees with
  Spot's `ltlfsynt` on the verdict (the existing `GeneratedCorpusDifferential`
  harness, adding the `--nfa-product` method).

## Open theory questions touched

- **`thm:nfa-mirror-size` proof "To be determined"** (single-exponential $N$) —
  inherited from `ltlf_to_nfa`; not an implementation blocker; leave for
  `/theory-review`.
- **`alg:nfa_product` complexity proof "To be determined"** and the
  **reachability invariant** (`main.tex:241`) — the invariant is asserted here as a
  structural test; `/theory-review` should bless that our completion-of-$N$ +
  cons-skip realization of $\delta_{prod}$ + generic subset construction faithfully
  computes `\algname{NfaToDfa}(P)` (the oracle *verifies* language empirically;
  theory-review blesses the argument, especially the **non-$\cons$-skip vs
  $\cons$-dead-sink** distinction). No new `\na`.
- **Governed-variable projection** (`main.tex:300` `\na`) and
  **trace-termination semantics** (`main.tex:96` `\na`) — `NfaProduct` inherits both
  through `solve_dfa` exactly as `DfaProduct` does; no new consumer, no new
  divergence. Listed for completeness; already tracked in the glossary *Open theory
  questions*.

## Definition of done

- `include/ltlf_ek/nfa_to_dfa.hpp`, `include/ltlf_ek/nfa_product.hpp`
  (+ `src/`), and the `build_product_nondet` addition to `product.hpp`/`product.cpp`
  land; `--nfa-product` wired in `cli.cpp`; tree compiles green.
- Phase 1 isolated determinize oracle (+ negative control) + structural tests, and
  Phase 2 cross-method / metamorphic / differential oracles pass; `ctest` green.
- Glossary updated (`/glossary`): `nfa_to_dfa` (the explicit `\algname{NfaToDfa}`,
  no longer "future") and `build_product_nondet` (Product family).
- `code-review` (domain + generic) + `theory-review` gates ticked; the
  reachability-invariant / non-$\cons$-vs-$\cons$-dead faithfulness question
  dispositioned.
- `docs/BACKLOG.md`: move the explicit `NfaProduct` item to Done; leave the
  `MtnfaProduct` follow-on accurate (it now has its reference oracle).
