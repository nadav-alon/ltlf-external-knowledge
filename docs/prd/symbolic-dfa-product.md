# PRD: Symbolic DFA-product construction (skip the minterm loop)

**Status:** draft
**Interface:** rewrites `DfaProduct`'s product construction; adds symbolic
accessors to the `Transducer` base class; adds `build_product_symbolic` +
a shared guard-map product representation
**main.tex ref:** §`fulldfa` (Method 2), Algorithm `alg:dfa_product`
("DFA Product"), `\cref{def:consistency}` (§203); reuses `\cref{def:enabled}`

**Gates:**
- [x] glossary        — `emits_region`, `delta_edges`, `build_product_symbolic`,
  `ProductGuards`/`materialize_product`/`to_guard_map`, and the build-equivalence
  oracle note all added to docs/GLOSSARY.md (2026-07-12, pre-implementation —
  names ratified for `/developer`)
- [ ] tests           — unit + oracle coverage
- [ ] code-review     — domain (/code-reviewer) + generic (/code-review)
- [ ] theory-review   — code ↔ math faithfulness vs main.tex

## Goal

Replace Method 2's exponential minterm loop with a **symbolic** product
construction. Today `DfaProduct::synthesize` (`src/dfa_product.cpp`) builds the
product by enumerating every full letter $v\in2^{\mathcal I\cup\mathcal O}$
(`LetterAlphabet`, `src/product.cpp`), testing each against `emits`+`delta` in
`agreeing_successor`, then **re-compressing** those letters back into
per-destination BDD guards (`guards[dst] |= letters[idx]`). That round-trip is
$2^{|\mathcal I\cup\mathcal O|}$ by construction — an unacceptable baseline now
that benchmarking is the tool's eventual purpose (this item was promoted to
Now/next #1 for exactly that reason, `docs/BACKLOG.md`).

The symbolic build computes the per-destination guards **directly** on edge-guard
BDDs — never materialising an individual letter — by intersecting the Goal DFA's
edge guards, each transducer's $\delta$-edge guards, and each transducer's
$\lambda$-agreement region. Cost becomes the product of out-degrees (each an
$O(1)$ BDD-and), not $2^{|\mathcal I\cup\mathcal O|}$.

**Scope (decided in the grill):** the rewrite targets **`DfaProduct` only** —
the benchmarked synthesis path. The per-letter core (`build_product`,
`agreeing_successor`, `LetterAlphabet`) is **kept**, because (a) it is the
`verify_controller` ν-fixpoint's engine, whose whole data structure is a
**per-$\Ifree$-combo** enumeration (`StateInfo::edges` indexed by
`ifree_index`, `src/verify_controller.cpp`) — not a symbolic fixpoint — and (b)
it is the reference build for the **build-equivalence metamorphic oracle** below.
A symbolic rewrite of the verifier's fixpoint is explicitly **out of scope** and
logged as a separate Later backlog item; the Method-2 win does not depend on it.
This PRD **supersedes the deferral note** in `docs/prd/dfa-product.md`
("Construction strategy … The exponential letter loop is the accepted,
documented baseline cost — the symbolic alternative is deferred").

## Ubiquitous-language terms used

All from `docs/GLOSSARY.md` unless flagged:

- **DFA product** / `DfaProduct` (§"The five methods", Method 2).
- **Product** / `ProductState`, `build_product`, `agreeing_successor` (§"Product").
- **Consistency (cons)** / `consistent(...)` (§"Consistency (cons)", `\cref{def:consistency}` §203).
- **Output agreement (emits)** / `emits(t, q, v)` (§"Output agreement (emits)") —
  the per-transducer λ-agreement atom this PRD lifts to a **region**.
- **Transition function (delta)** / `Transducer::delta(q, v)` (§"…(delta)") —
  this PRD lifts to a **symbolic edge partition**.
- **Enabled** — `\cref{def:enabled}`; $\cons$ + δ/λ-definedness (used in prose).
- **Letter alphabet** / `LetterAlphabet` (§"Letter alphabet") — **retained** for
  the verifier + reference build; **not** used by the symbolic path.
- **Cube**, **Letter**, **Σ₀/Σ₁** / `sigma0_cube`, `sigma1_cube`, `lambda_by_state_`.
- **Goal DFA construction** / `ltlf_to_dfa` (§"Goal DFA construction").
- **Game solving (SolveDfa)** / `solve_dfa` (§"Game solving (SolveDfa)") — **unchanged**.

**Glossary gaps to close (run `/glossary` before/after `/developer`):**

- **`emits_region(q)`** — the region (whole-set) form of `emits`; §"Output
  agreement (emits)" defines the per-letter atom only. This PRD proposes the name;
  add it under that entry's C++ column.
- **`delta_edges(q)`** — the symbolic edge-partition form of `delta`;
  §"Transition function (delta)" names only the per-letter `delta(q,v)`. Add.
- **`build_product_symbolic`** — the symbolic Product builder; §"Product" lists
  `build_product`/`agreeing_successor` only. Add.
- **`ProductGuards`** (shared per-dst guard map) + **`materialize_product`** —
  the neutral product-guard representation and its game-`twa_graph` materialiser;
  add to §"Product".
- **Build-equivalence metamorphic oracle** — a *methodology* term (like the
  corpus grading modes, §"Generated corpus & its grading modes"): a metamorphic
  cross-check between **two builds of the same method**. It is **NOT** the
  corpus's "**differential**" (which grades *realizability* against `ltlfsynt`).
  Add a one-line note so the terms do not drift.

## Behaviour / semantics (from main.tex)

The symbolic build must construct **the same product $P$** as `alg:dfa_product`
and its per-letter implementation — it is an optimisation, not a new automaton.
$P$ is uniquely determined by $(A,\Tin,\Tout,\cons)$: its reachable states are the
$\langle s,q_{in},q_{out}\rangle$ tuples reached by BFS, and for each
$\langle\text{src},\text{dst}\rangle$ the guard is the OR of exactly the enabled
+ consistent letters heading there. There is **no** ⊥-sink (Method 2 is a
*direct* product per `docs/prd/drop-method2-sink.md`).

**The symbolic reformulation.** For a product state $\langle s,q_{in},q_{out}\rangle$:

- **Symbolic $\cons$ region** (`\cref{def:consistency}` §203, region form): the
  set of letters consistent at $(q_{in},q_{out})$ is
  $\texttt{emits\_region}(q_{in})\ \wedge\ \texttt{emits\_region}(q_{out})$, one
  BDD over $\mathcal I\cup\mathcal O$. This is faithful because the λ-functionality
  invariant (owned by `parse_transducer` / `OutputLabeledTransducer`) makes
  `emits(t,q,v)` ⟺ $v$'s $(\Sigma_0,\Sigma_1)$ slice ∈ `lambda_by_state_[q]`, so
  the whole region **is** `lambda_by_state_[q]` (see *Interfaces & types*). This
  is the "symbolic `cons`" the backlog seed asks to reconcile with the math —
  flagged for `/theory-review` (Open theory questions).
- **Symbolic successors:** for each transducer $t_i$, iterate
  `delta_edges(q_i)` = its $\delta$ partition as $(g_i, q_i')$ pairs; iterate the
  Goal DFA's out-edges $(g_{goal}, s')$. For each combination the destination is
  $\langle s', q_1', q_2'\rangle$ and its guard is
  $$g_{goal}\ \wedge\ \big(g_1\wedge\texttt{emits\_region}(q_1)\big)\ \wedge\ \big(g_2\wedge\texttt{emits\_region}(q_2)\big).$$
  Accumulate (OR) guards per destination; drop `bddfalse` combinations. Push new
  destinations onto the BFS worklist.

Invariants that MUST hold (mirroring `docs/prd/dfa-product.md` and
`alg:dfa_product`):

1. **$\cons$ is the only filter**, and `enabled` subsumes it
   (`\cref{def:enabled}`): a letter takes a product edge iff both transducers'
   δ **and** λ are defined at it and $\cons$ holds. δ-partiality is handled
   structurally — an undefined-δ letter is covered by **no** `delta_edges` guard,
   so it contributes to no edge (exactly the per-letter "skip", `def:enabled`).
   λ-partiality is handled by `emits_region` (an undefined λ excludes the letter,
   see below). A non-enabled letter simply appears in no destination guard;
   there is no sink.
2. **Final set** $F_P = F_D\times Q_{in}\times Q_{out}$
   (`alg:dfa_product:final`): a product state is accepting iff its Goal component
   is (`goal->state_is_accepting(s)`), materialised as state-based Büchi exactly
   as `src/dfa_product.cpp` does today.
3. **Goal completeness.** `ltlf_to_dfa` returns a complete DFA, so the Goal
   out-guards partition $\Sigma$ and the per-`src` union of $g_{goal}$ is
   `bddtrue`; ANDing them into the enabled region therefore loses no enabled
   letter (the symbolic analogue of the per-letter `goal_must_be_complete=true`
   throw). Assert this once (per Goal state, $\bigvee g_{goal}=\mathtt{bddtrue}$)
   so a non-complete Goal is caught rather than silently dropping letters.
4. **Same game out.** The materialised game `twa_graph` must be **identical** to
   the per-letter build's (same states, acc, per-edge guard BDDs) — enforced by
   the build-equivalence oracle. `solve_dfa` and its $\Iknown/\Oknown$ projection
   + $\Ifree/\Ofree$ lift are **unchanged** (the symbolic build still emits guards
   over the full $\mathcal I\cup\mathcal O$ with the known variables pinned by
   $\cons$, so `solve_dfa`'s existing `bdd_exist` projection applies verbatim).

## Interfaces & types

**Phase 1 — symbolic contract on the `Transducer` base class** (`transducer.hpp`).
Two pure virtuals, each the symbolic (whole-set) form of an existing glossary
concept:

```cpp
// Symbolic 'emits' (docs/GLOSSARY.md "Output agreement (emits)", region form):
// the BDD over I∪O of every letter whose Sigma1 slice agrees with lambda at q.
// For OutputLabeledTransducer this is exactly lambda_by_state_[q] (the stored
// output relation over Sigma0∪Sigma1) --- the lambda-functionality invariant
// makes region membership <=> per-letter emits(t,q,v).  bddfalse when lambda is
// undefined at q (matches emits's nullopt => false).
virtual bdd emits_region(unsigned q) const = 0;

// Symbolic 'delta' (docs/GLOSSARY.md "Transition function (delta)", partition
// form): the deterministic delta out of q as (guard, dst) pairs --- for
// OutputLabeledTransducer, its twa_graph out-edges (acceptance ignored).  A
// letter covered by no returned guard is delta-undefined there (partial
// transducer), handled structurally by contributing to no product edge.
virtual std::vector<std::pair<bdd, unsigned>> delta_edges(unsigned q) const = 0;
```

`OutputLabeledTransducer` (the **only** concrete `Transducer` — also what
`controller_as_transducer` and `trivial_transducer` return) implements both
trivially from state it already holds: `emits_region(q)` returns
`lambda_by_state_[q]`; `delta_edges(q)` returns `{(e.cond, e.dst)}` over
`delta_dfa_->out(q)`. No fallback default on the base (a per-letter fallback
would hide the perf cliff, and every transducer is materialised anyway — the
"on-the-fly" in Method 3 is the **Goal DFA** via progression, not the
transducers).

**Phase 2 — shared product representation + symbolic build** (`product.hpp` /
`src/product.cpp`):

```cpp
// Neutral per-dst guard map: for each reachable ProductState, its Goal-acceptance
// flag and, per destination ProductState, the accumulated (OR'd) edge guard.
// Both builds emit THIS type, so they are directly comparable.
struct ProductGuards {
  std::map<ProductState, std::pair<bool, std::map<ProductState, bdd>>> nodes;
};

// Symbolic build: BFS from `init`, computing per-dst guards directly via
// delta_edges/emits_region + Goal out-edges.  Assumes a complete Goal DFA
// (asserts it).  No LetterAlphabet, no minterm loop.  DfaProduct-only.
ProductGuards build_product_symbolic(
    const spot::twa_graph_ptr& goal,
    const std::vector<const Transducer*>& taus, const ProductState& init);

// Compress the per-letter build's ProductNode edges into the shared type (the
// existing `guards[dst] |= letters[idx]` loop, extracted) --- reference side of
// the build-equivalence oracle.
ProductGuards to_guard_map(const std::map<ProductState, ProductNode>& graph,
                           const LetterAlphabet& alphabet);

// Materialise the game twa_graph from the shared type (state-based Büchi, F_P
// on the acc flag) --- the single place product states/guards become an
// automaton, called by DfaProduct.
spot::twa_graph_ptr materialize_product(const ProductGuards& pg,
                                        const spot::bdd_dict_ptr& dict);
```

`src/dfa_product.cpp` is rewired: `build_product_symbolic` → `materialize_product`
→ `solve_dfa`. It no longer constructs a `LetterAlphabet` or runs the
`guards[dst] |= letters[idx]` loop.

**Reused unchanged:** `consistent`/`emits` (per-letter, still used by the
verifier and as the oracle reference via `build_product`), `solve_dfa`,
`ltlf_to_dfa`, `validate_product_inputs`, `verify_controller`, `LetterAlphabet`.

## Implementation phases

- **Phase 1 — symbolic contract.** Add `emits_region`/`delta_edges` pure virtuals
  to `Transducer`; implement in `OutputLabeledTransducer`. Nothing consumes them
  yet. **Green checkpoint:** compiles; new unit tests assert, over a spread of
  fixtures, that `emits_region(q)` agrees with per-letter `emits(t,q,v)` on every
  letter of a `LetterAlphabet`, and that `delta_edges(q)` agrees with
  `delta(q,v)` on every letter (guard partition ⟺ per-letter successor). Existing
  suite stays green.

- **Phase 2 — symbolic build + rewire + oracle.** Add `ProductGuards`,
  `build_product_symbolic`, `to_guard_map`, `materialize_product`; rewire
  `DfaProduct::synthesize` onto the symbolic path. **Green checkpoint:** compiles;
  all existing `dfa_product` tests pass unchanged (same verdicts, because the
  game is identical); the build-equivalence metamorphic oracle passes on unit
  fixtures and on the generated corpus.

Each phase leaves the tree compiling and independently testable.

## Edge cases

- **Empty universe ($\mathcal I\cup\mathcal O=\emptyset$)** — Goal over a
  one-letter alphabet, single-state transducers; `emits_region` and Goal guards
  are `bddtrue`; product guard is `bddtrue`. No minterm loop to degenerate.
- **Empty $\Ifree$ / empty $\Ofree$** — no special handling in the build (guards
  are over whatever variables exist); `solve_dfa` handles the degenerate arena as
  today.
- **Empty $\mathcal V$ (no knowledge)** — trivial transducers: `emits_region` is
  `bddtrue`, `delta_edges` a single self-loop $(\mathtt{bddtrue},q_0)$; $P$
  collapses to $A$ — the monolithic baseline regime, same as the per-letter path.
- **Partial transducers** — undefined δ: letter covered by no `delta_edges` guard
  ⇒ no edge. Undefined λ at $q$: `emits_region(q)=\mathtt{bddfalse}$ ⇒ every guard
  through $q$ is `bddfalse` ⇒ dropped. Both match the per-letter "skip"
  (`\cref{def:enabled}`, Case-A regime).
- **Non-complete Goal** — assertion (invariant 3) throws rather than silently
  dropping enabled letters (symbolic analogue of the per-letter completeness
  throw). `ltlf_to_dfa` never produces one; the assert guards a future
  regression.
- **Determinism** — `delta_edges` guards out of one state are disjoint
  (deterministic δ) and Goal out-guards are disjoint (deterministic DFA), so no
  destination guard double-counts a letter; the accumulated OR is exact.
- **Validation** — `validate_product_inputs` (APs ⊆ $\mathcal I\cup\mathcal O$;
  one shared `bdd_dict`) runs first, unchanged.

## Test oracles (for /test-writer)

1. **Phase-1 contract equivalence (unit)** — for a spread of transducers
   (`OutputLabeledTransducer` from fixtures, `trivial_transducer`,
   `controller_as_transducer`): over every letter of a `LetterAlphabet`, assert
   `(v & emits_region(q)) != bddfalse` ⟺ `emits(t,q,v)`, and that the unique
   `delta_edges` guard satisfied by `v` names the same `dst` as `delta(q,v)`
   (and that an undefined `delta(q,v)` ⟺ no guard covers `v`).
2. **Build-equivalence metamorphic oracle (the linchpin for this rewrite)** —
   for the same inputs, assert `build_product_symbolic(...)` equals
   `to_guard_map(build_product(...), alphabet)`: identical set of reachable
   `ProductState`s, identical `acc` flag on each, and for every
   `⟨src,dst⟩` a **BDD-equal** guard (`==`; BuDDy canonicalises, so structural
   equality is semantic equality). This is the direct check on the rewrite's
   likeliest bug class (lost/mis-grouped transitions, the `|=`→`=` seeded bug).
   It compares the **game (arena)**, not realizability and not a controller
   (controllers are non-unique winning strategies — not comparable). Run on
   dedicated unit fixtures **and** wired into the generated corpus
   (`tests/ltlfsynt_oracle_test.cpp`) as a cheap, library-only, self-labeling
   body over every `(phi, partition, Tin)` case.
3. **Realizability oracles unchanged (regression)** — the existing `DfaProduct`
   suite (controller verifier, monolithic baseline, knowledge-sensitivity flip,
   `ltlfsynt` differential) must pass **byte-for-byte the same verdicts** on the
   symbolic path, since invariant 4 makes the game identical. These catch "wrong
   verdict"; the build-equivalence oracle catches "right verdict, wrong game".

No cross-method equivalence is added here (still Method-2-only).

## Open theory questions touched

- **Symbolic $\cons$ region vs the per-letter `\cref{def:consistency}` (§203).**
  The build uses `emits_region(q_in) & emits_region(q_out)` as the whole-region
  $\cons$, justified by the λ-functionality invariant (region membership ⟺
  per-letter `emits`). `main.tex` states $\cons$ per-letter only; the region
  reformulation is a code-level equivalence to confirm — flagged for
  `/theory-review` (likely a `\cl` note candidate near `\cref{def:consistency}`
  or §`fulldfa`; do **not** resolve here). This is the backlog seed "a whole-region
  version must be reconciled with the math".
- **Goal-completeness reliance** — invariant 3 assumes `ltlf_to_dfa` is complete;
  the per-letter path already relies on this (`goal_must_be_complete`). Same
  standing assumption, now asserted symbolically. No new `main.tex` gap.
- `solve_dfa`'s $\Iknown/\Oknown$ projection + $2^{\mathcal I}$ lift is
  **unchanged** (the symbolic build emits the same full-$\mathcal I\cup\mathcal O$
  pinned guards), so no new theory there — the resolved arena-partition `\cl`
  note in §`fulldfa` still applies verbatim.

## Definition of done

- **Phase 1:** `emits_region`/`delta_edges` on `Transducer` +
  `OutputLabeledTransducer`; contract-equivalence unit tests green; suite green.
- **Phase 2:** `ProductGuards` + `build_product_symbolic` + `to_guard_map` +
  `materialize_product` land; `DfaProduct::synthesize` runs the symbolic path with
  **no** `LetterAlphabet` / minterm loop; the build-equivalence oracle passes on
  fixtures and on the generated corpus; the full existing `DfaProduct` suite
  passes with identical verdicts.
- The per-letter core (`build_product`, `agreeing_successor`, `LetterAlphabet`)
  remains intact and in use by `verify_controller` and the oracle reference.
- Glossary gaps closed via `/glossary` (`emits_region`, `delta_edges`,
  `build_product_symbolic`, `ProductGuards`/`materialize_product`, the
  build-equivalence-oracle note).
- `/theory-review` confirms the symbolic $\cons$ region is faithful to
  `\cref{def:consistency}`.
- **No benchmarking in this PRD** (decided): construction-time measurement is
  deferred wholesale to the Benchmarking backlog item so all benchmarking shares
  one uniform design; correctness rests on the build-equivalence + realizability
  oracles. A symbolic rewrite of the `verify_controller` ν-fixpoint is likewise
  out of scope, logged as a separate Later backlog item.
