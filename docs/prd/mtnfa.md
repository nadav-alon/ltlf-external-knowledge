# PRD: MTNFA — a multi-terminal NFA representation, determinizable into `spot::mtdfa`

**Status:** implemented — Phase 1 (`StateSetPool`) landed,
`src/state_set_pool.cpp` + `include/ltlf_ek/detail/state_set_pool.hpp`.
Phase 2 (`Mtnfa` + construction + `mtnfa_to_mtdfa`) landed 2026-07-17,
`src/mtnfa.cpp` + `include/ltlf_ek/mtnfa.hpp` (uncommitted at PRD-edit
time); the isolated `product_xor` oracle itself is `/test-writer`'s job
(see `tests` gate below), not yet written.
**Interface:** adds the `Mtnfa` representation type + `ltlf_to_mtnfa` (construction)
+ `mtnfa_to_mtdfa` (determinization); **not** a `Synthesis` method. The Method-1
mtdfa synthesis method (`MtnfaProduct`) and the explicit `NfaProduct` are separate
backlog items that build on this.
**Recommended workflow:** **sequential** — freeze confidence is *tentative*: the
`Mtnfa` type and the two bespoke MTBDD applies are **invented here** (Spot's mtdfa
module has no NFA/subset-construction analog), so `/developer` lands the real
signatures first and `/test-writer` binds to them after.
**main.tex ref:** §`nfa` (`\cref{nfa}`), `\cref{alg:ltlftonfa}` (the NFA $N$ this
lifts), and `\cref{alg:nfa_product}` line `alg:nfa_product:determinize`
(`\algname{NfaToDfa}`, the subset construction this realizes at the mtdfa
representation); the reachability-invariant note at `main.tex:241`. The **MTNFA
representation itself has no `main.tex` symbol** — like *MTDFA* it is a code-only
data structure on the *Representation* axis (the `\na` at `main.tex:335`,
*"This likely requires adjusting the definitions for MTDFA usage"*, is the closest
gesture, and it is about Method 3, not a commitment to this).

**Gates:**
- [x] glossary        — *closed 2026-07-17* (`/glossary`, uncommitted): new
  *MTNFA* entry (with `Mtnfa`; `detail::StateSetPool` folded in as un-entried infra,
  the `bench.hpp` precedent), and two *Algorithms* entries — *Goal MTNFA
  construction* (`ltlf_to_mtnfa` / `nfa_to_mtnfa`) and *Goal automaton
  determinization* (`mtnfa_to_mtdfa`, cross-referencing the still-future explicit
  `NfaToDfa`). All rejected-synonym lines set. `/developer` binds to these names.
- [x] tests           — *closed 2026-07-17* (`/test-writer`, uncommitted): Phase-1
  apply units extended (>64-element set proving no bitmask, commutativity,
  `bdd_varnum`/interning stability) + new `tests/mtnfa_test.cpp` — the MONA-gated
  60-case corpus `product_xor` oracle vs `spot::ltlf_to_mtdfa`, an ungated
  hand-built-NFA oracle vs independent subset-simulation, membership fuzz,
  construction/determinize structural free-riders, the `phi=0`/`phi=1` edges, and
  **four negative controls** (all confirmed discriminating). The suite caught a
  second real lifetime bug (see *Developer comments*, 2026-07-17 entry 3);
  362/362 green after the fix.
- [ ] code-review     — domain (/code-reviewer) + generic (/code-review)
- [x] theory-review   — *closed 2026-07-17* (`/theory-review`, uncommitted):
  **clean, no code-bug.** All three seeded questions dispositioned: (1) subset-
  construction faithfulness **blessed** ($L(\texttt{mtnfa\_to\_mtdfa}(N))=L(N)$
  verified link-by-link), no new `\na`; (2) $\varepsilon$-convention **agrees** —
  but the PRD's argument runs backwards (see below); (3) `thm:nfa-mirror-size`
  status-noted, unchanged. Four follow-ups left OPEN, none blocking:
  **F1** (`underspecified`) `\algname{NfaToDfa}` is never defined in `main.tex` —
  a drafted `\cl` for after `main.tex:241` is in the review; load-bearing for
  `MtnfaProduct`, worth pinning before it lands.
  **F2** (`underspecified`, latent) `mtnfa_to_mtdfa` silently drops an accepting
  *initial* state — unreachable via `ltlf_to_mtnfa` (fresh $s_{N,0}$) but
  `nfa_to_mtnfa`/`mtnfa_to_mtdfa` are public and take arbitrary `twa_graph`s.
  Fix: document the precondition + `assert(!nfa.accepting[nfa.initial])`.
  **F3** (`underspecified`) `nfa_to_mtnfa`'s two unstated `state_is_accepting`
  preconditions (throws without `prop_state_acc()`; returns **false** for an
  $F_N$ state with no out-edges — defended today only by accident of
  `reverse_dfa_to_nfa`'s `bddfalse`-guarded self-loop).
  **F4** (note) the PRD's floated cross-call `set_union` memo would be
  **unsound**, not merely a perf tweak: the id-keyed memo is safe *only* because
  per-call scoping keeps operands GC-reachable. Strike the "compatible
  follow-up" phrasing; a persistent memo needs `bdd`-valued keys.
- [x] code-review     — domain (/code-reviewer) *closed 2026-07-17*: **clean, no
  must-fix.** Shared-dict usage, mtdfa terminal encoding (`2j+b` / `bddfalse`
  sink), the `register_proposition` ownership fix (idempotent, `~mtdfa()`-paired),
  and per-call memo scoping all confirmed sound; glossary-conformant. One
  *consider* (non-blocking): `mtnfa.cpp:133` `const unsigned i` is `assert`-only →
  `-Wunused-variable` under `NDEBUG`; add `(void)i;`. Theory-review already ran
  clean separately (not re-spawned). Generic `/code-review` lens: not separately
  run — recommend before committing, though the domain pass covered the semantic
  surface.

## Goal

Method 1 keeps the Goal automaton **nondeterministic** (the single-exponential NFA
$N$, `ltlf_to_nfa` — landed) and determinizes only the *product* later, so the
blowup stays exponential in $\varphi$ alone. To run Method 1 in the **mtdfa**
*Representation* (the way `MtdfaProduct` runs Method 2), $N$ needs a symbolic form
comparable to *MTDFA* — one MTBDD per state — that can be **subset-determinized
into a `spot::mtdfa`** consumable by `solve_mtdfa`. Spot provides no such thing:
`spot::mtdfa` terminals encode a **single** destination ($2d+b$), which is exactly
what makes them deterministic; an NFA needs `δ_N(s,v)` to be a **set**.

This PRD delivers that representation — the **MTNFA** (`Mtnfa`): per-state MTBDDs
whose terminals encode **sets** of NFA states via an interned index — plus its
construction (lift `ltlf_to_nfa(φ)`) and its **determinization into `spot::mtdfa`**
(symbolic subset construction). It deliberately stops there: **no transducers, no
$\cons$ filter, no `Synthesis` method, no CLI, no benchmarking.** That surface is
the follow-on `MtnfaProduct` (top of `docs/BACKLOG.md`). Stopping at determinize is
what makes this PRD *self-validating in isolation*: determinizing the Goal NFA
alone yields a DFA for $L(\varphi)$, checkable against Spot's **independent**
`spot::ltlf_to_mtdfa(φ)` by BDD-exact XOR-emptiness — the strongest possible oracle
for the one genuinely bespoke thing here (the set-terminal machinery), with nothing
else in the blast radius.

**This PRD supersedes nothing.** It relates to the *Later* backlog item
"Investigate Nondeterministic Decision Diagrams for representing the NFA" — the
MTNFA is a concrete, canonical answer to that investigation's core question (it is a
multi-terminal *deterministic-branching* diagram with set-valued leaves, so it keeps
BDD canonicity, unlike the general nBDD/nFBDD the item worried would lose it).

## Ubiquitous-language terms used

Existing, unchanged: *Goal formula* ($\varphi$), *NFA / DFA for the Goal* ($N$,
`ltlf_to_nfa`), *MTDFA* (`spot::mtdfa`), *Letter* / *Letter alphabet*, *Cube*,
*Representation*, *Automaton reversal* (`reverse_dfa_to_nfa`, the NFA's builder).

**Glossary — landed 2026-07-17 by `/glossary`** (no gaps remain; `/developer` may
bind to these names as canonical):

- **MTNFA (multi-terminal NFA)** — the representation; C++ `Mtnfa`. New entry under
  *Automata & Transducers*, the nondeterministic sibling of *MTDFA*: same per-state
  MTBDD shape, but terminals denote **sets** of states (interned index), not a
  single destination. On the *Representation* axis, code-only, no `main.tex` symbol.
- **`ltlf_to_mtnfa`** — build the `Mtnfa` for $\varphi$ (compose `ltlf_to_nfa` +
  the lift). Filed alongside *Goal NFA construction*.
- **`nfa_to_mtnfa`** — the lift of an explicit `twa_graph` NFA into an `Mtnfa`
  (the NFA analog of Spot's `twadfa_to_mtdfa`; **our** function, Spot has none).
- **`mtnfa_to_mtdfa`** — the subset determinization `Mtnfa` → `spot::mtdfa`; realizes
  `\algname{NfaToDfa}` (`alg:nfa_product:determinize`) at the mtdfa representation,
  applied to the Goal NFA alone. Filed under *Game solving* neighbourhood / a new
  *Goal automaton determinization* entry.
- **`StateSetPool`** (Phase-1 substrate) — the set-terminal interning table + the
  **union apply**. Internal `detail` machinery; may or may not warrant a glossary
  entry (cf. BuDDy `Cube` primitives get one, but `bench.hpp` infra does not) —
  `/glossary`'s call.

## Behaviour / semantics (from main.tex)

The representation is code-only, but its two operations must be faithful:

1. **Construction faithfulness.** `nfa_to_mtnfa(N)` must denote **exactly**
   $\delta_N$: for every state $s$ and letter $v$, the set named by the terminal
   that `states[s]` reaches on $v$ equals $\delta_N(s,v)$ (`\cref{alg:ltlftonfa}`'s
   $N$, $\delta_N : S_N \times 2^{\mathcal I\cup\mathcal O} \to 2^{S_N}$,
   `main.tex:198`). A letter covered by **no** out-edge maps to $\emptyset$ — $N$ is
   partial/uncompleted (`alg:nfa_product` tolerates an empty $\delta_N(s,v)$), and
   the representation must preserve that (empty set, **not** a sink state).

2. **Determinization faithfulness.** `mtnfa_to_mtdfa` is the subset construction
   `\algname{NfaToDfa}` (`alg:nfa_product:determinize`) applied to the Goal NFA:
   a DFA state is a **set** $R\subseteq S_N$, the initial DFA state is
   $\{s_{N,0}\}$, $\delta_D(R,v)=\bigcup_{s\in R}\delta_N(s,v)$, and $R$ is accepting
   iff $R\cap F_N\neq\emptyset$. The result recognizes $L(N)=L(\varphi)$ over
   non-empty traces (`\cref{alg:ltlftonfa}`), so it is language-equal to
   `spot::ltlf_to_mtdfa(φ)` — the oracle below. (In the paper `NfaToDfa` runs on the
   **product** $P$; running it on $N$ alone is the isolated, transducer-free special
   case that makes this PRD independently testable. Generalizing the *same*
   determinizer to the $(R,q_{in},q_{out})$ product states is `MtnfaProduct`'s job,
   backed by the reachability invariant `main.tex:241`.)

3. **Empty word / non-empty traces.** $L(\varphi)$ excludes $\varepsilon$
   (`1` rejects the empty word; project commitment). $s_{N,0}\notin F_N$
   (`F_N=\{s_{D,0}\}\neq\{s_{N,0}\}`), so the initial DFA state $\{s_{N,0}\}$ is
   non-accepting and $\varepsilon$ is rejected — matching `ltlf_to_mtdfa(φ)`.

## Interfaces & types

**Freeze confidence: RE-FROZEN 2026-07-17 — this block now reflects what actually
landed** (Phases 1 + 2 both implemented). The original tentative freeze was
reshaped by implementation in exactly two ways, both recorded in *Developer
comments / PRD disagreements* and both folded into the block below: `Mtnfa::pool`
is now `mutable`, and `Mtnfa` gained a seventh field `source_nfa`. The three
function signatures survived unchanged. **`/test-writer` binds to the block
below.**

```cpp
// include/ltlf_ek/detail/state_set_pool.hpp                          [new, Phase 1]
//
// The set-terminal substrate for MTNFA (docs/GLOSSARY.md "MTNFA").  Owns the
// interning table mapping a terminal int <-> a sorted set of NFA-state indices
// (index 0 == the empty set), and the memoized UNION APPLY over MTBDDs whose
// leaves are such terminals.  `detail` (unit-tested from tests/, mona_dfa.hpp
// precedent); no public/glossary commitment on the exact members.
namespace ltlf_ek::detail {
class StateSetPool {
 public:
  StateSetPool();                       // interns {} as index 0 up front

  // Intern a sorted, de-duplicated state-set; returns its stable terminal index.
  unsigned intern(std::vector<unsigned> sorted_states);
  const std::vector<unsigned>& set_of(int terminal_index) const;

  // The bespoke core: MTBDD union.  `a`, `b` are MTBDDs whose every leaf is
  // bdd_terminalpp(idx) for an idx in THIS pool.  Returns the MTBDD whose leaf
  // on each letter is the UNION of a's and b's leaf-sets there.  Memoized on the
  // unordered {a.id(), b.id()} pair (union is commutative + idempotent).
  bdd set_union(const bdd& a, const bdd& b);

  // Convenience: the MTBDD  ite(guard, {state}, {})  --- terminal {state} where
  // `guard` holds, empty-set terminal elsewhere.  Used by the lift.
  bdd guarded_singleton(const bdd& guard, unsigned state);
};
}  // namespace ltlf_ek::detail

// include/ltlf_ek/mtnfa.hpp                                          [new, Phase 2]
//
// MTNFA: the multi-terminal NFA (docs/GLOSSARY.md "MTNFA").  One MTBDD per NFA
// state over the letter alphabet 2^{I u O}; terminals name SETS of successor
// states via `pool`.  Nondeterministic sibling of spot::mtdfa.  Built on the
// SAME spot::bdd_dict as the transducers and every letter.
namespace ltlf_ek {
struct Mtnfa {
  std::vector<bdd> states;              // states[s] : MTBDD, set-valued terminals
  std::vector<bool> accepting;          // accepting[s] == (s in F_N)
  unsigned initial = 0;                 // index of s_{N,0}
  // MUTABLE (re-freeze 2026-07-17): mtnfa_to_mtdfa is frozen as `const Mtnfa&`
  // but must intern newly-unioned successor sets into THIS pool as it
  // determinizes --- the terminals in `states` are only meaningful relative to
  // it.  Logical constness.
  mutable detail::StateSetPool pool;    // interprets the terminals; OWNED
  spot::bdd_dict_ptr dict;
  std::vector<spot::formula> aps;       // APs registered (phi's support on dict)

  // ADDED (re-freeze 2026-07-17): keeps the source twa_graph alive so the AP
  // variables `states`' BDDs reference stay registered on `dict` for this
  // Mtnfa's lifetime.  spot::bdd_dict frees an owner's variables when the LAST
  // reference to that owner dies; without this, ltlf_to_mtnfa's temporary
  // ltlf_to_nfa result died on return and a later dict->register_ap could alias
  // those variable numbers.  Mirrors OutputLabeledTransducer::delta_dfa_
  // (output_labeled_transducer.hpp:65).  Internal book-keeping --- NOT a field
  // the oracle reads.
  spot::twa_graph_ptr source_nfa;
};

// Lift an explicit deterministic-or-not twa_graph NFA into an Mtnfa.  For each
// state s: fold every out-edge (cond, dst) via pool.set_union of
// pool.guarded_singleton(cond, dst) --- overlapping guards MERGE (that overlap is
// nondeterminism), an uncovered letter stays the empty-set terminal.  accepting[s]
// = nfa->state_is_accepting(s); initial = nfa->get_init_state_number().  The NFA
// analog of spot::twadfa_to_mtdfa (ours; Spot has no twanfa_to_mtnfa).
Mtnfa nfa_to_mtnfa(const spot::twa_graph_ptr& nfa);

// LtlfToNfa in the mtdfa representation: ltlf_to_nfa(phi, dict) then nfa_to_mtnfa.
// Same (phi, dict) shape + shared-dict precondition as ltlf_to_nfa; APs come from
// phi's support.  MONA-backed (via ltlf_to_nfa) --- runtime dep on `mona`.
Mtnfa ltlf_to_mtnfa(const spot::formula& phi, const spot::bdd_dict_ptr& dict);

// NfaToDfa (alg:nfa_product:determinize) at the mtdfa representation, applied to
// the Goal NFA alone: symbolic subset construction.  Returns a spot::mtdfa (states
// = reachable subsets, states[0] = {initial}, terminal 2*idx+b, bddfalse = the
// empty-set/rejecting sink) recognizing L(nfa).  nullptr is NEVER returned.
spot::mtdfa_ptr mtnfa_to_mtdfa(const Mtnfa& nfa);
}  // namespace ltlf_ek
```

**If implementation proves this contract wrong:** that is a PRD-change event —
update this section and re-freeze; the developer does not silently re-shape the
interface. (`detail::StateSetPool`'s members may churn freely; only `mtnfa.hpp`'s
three signatures + the `Mtnfa` fields the oracle reads are the frozen surface.)

## Implementation phases

Two phases; each compiles green and is independently testable. Phase 1 quarantines
the one bespoke, refcount-fragile piece behind direct unit tests **before** anything
depends on it.

- **Phase 1 — the set-terminal substrate (`StateSetPool`).** The interning table +
  `set_union` + `guarded_singleton`, nothing automaton-shaped. **Green checkpoint:**
  direct unit tests on the apply — e.g. `set_union(ite(x,{1},{2}), ite(x,{2},{3}))`
  ≡ `ite(x,{1,2},{2,3})` (compare by walking terminals via `bdd_get_terminal` +
  `set_of`); $\emptyset$-terminal is the identity; idempotence
  (`set_union(a,a)==a`); commutativity (`==`, BuDDy canonicalizes so this is exact);
  a set that grows past 64 elements (proving the index encoding is **not** a bitmask);
  and a no-leak / stable-`bdd_varnum` sanity pass. *May stub:* everything else — no
  `Mtnfa`, no determinize.
- **Phase 2 — `Mtnfa` + construction + `mtnfa_to_mtdfa` + isolated oracle.** The
  type, `nfa_to_mtnfa`, `ltlf_to_mtnfa`, and the determinizer. **Green checkpoint:**
  `product_xor(mtnfa_to_mtdfa(ltlf_to_mtnfa(φ)), spot::ltlf_to_mtdfa(φ)).is_empty()`
  over the generated corpus (MONA-gated), with a **negative control** proving the
  XOR oracle discriminates (a deliberately wrong determinization, or two
  known-different formulas, gives a non-empty XOR).

## Novel mechanisms — pinned to code

Everything here is bespoke (no `main.tex`, no Spot analog), so each is pinned past
sketch level per the "grill to the code" rule.

**(a) Terminal encoding.** Every leaf of an `Mtnfa` state MTBDD is
`bdd_terminalpp(idx)` where `idx` is a `StateSetPool` index; **`idx == 0` is the
empty set**. `bddfalse`/`bddtrue` do **not** appear inside an `Mtnfa`'s `states[]`
— $\emptyset$ is a genuine set-terminal (index 0), so the union apply has a single
uniform terminal∪terminal base case. (`bddfalse` reappears only in the *output*
`spot::mtdfa`, as its rejecting sink — see (c).) Sets are stored **sorted +
de-duplicated**, so interning is canonical: equal sets ⇒ equal index ⇒ physically
equal MTBDDs ⇒ BDD equality/caching stays valid.

**(b) The union apply (`set_union`).** Memoized binary recursion, classic BDD-apply
shape, all in Spot's `bdd` C++ type so **intermediates are RAII-refcounted** (no
manual `bdd_addref`/`bdd_delref` — the main plumbing hazard, removed by construction):
- `a == b` ⇒ return `a` (idempotent short-circuit).
- both terminals ⇒ `bdd_terminalpp(intern(merge(set_of(a), set_of(b))))`, a sorted
  set-merge.
- else split on the **topmost** variable `v` (lower `bdd_level`; a terminal counts
  as below every variable): let `a_lo,a_hi` be `a`'s cofactors at `v`
  (`bdd_low/bdd_high(a)` if `bdd_var(a)==v`, else `a` for both), likewise `b`; return
  `bdd_ite(bdd_ithvarpp(v), set_union(a_hi,b_hi), set_union(a_lo,b_lo))`.
- **Memo** on the order-normalized `{a.id(), b.id()}` pair → polynomial in the input
  MTBDDs' node counts, not exponential in letters.
Fully deterministic: no seed, no randomness; a fixpoint-free single recursion.

**(c) The determinizer (`mtnfa_to_mtdfa`).** A **BFS over reachable subsets**, each
a sorted `std::vector<unsigned>` interned to an output-`mtdfa` root index (a
`map<vector<unsigned>, unsigned>` distinct from the pool):
- Seed the queue with `R0 = {nfa.initial}` at output index 0 (⇒ `states[0]` is the
  initial state, as `solve_mtdfa` and Spot expect).
- Dequeue `R` (output index `i`): its **successor MTBDD** is
  `rowSet = fold(set_union, {nfa.states[s] : s ∈ R})` — one MTBDD over letters whose
  every leaf is the successor subset $\bigcup_{s\in R}\delta_N(s,v)$.
- **Relabel** `rowSet` into a real `spot::mtdfa` row by a second memoized MTBDD
  recursion (unary map, same skeleton as (b)): at each set-terminal with pool index
  `t`, let `S = pool.set_of(t)`; if `S` is empty ⇒ **`bddfalse`** (rejecting sink);
  else `j =` intern-or-enqueue `S` as an output state, `b = any(nfa.accepting[s]
  for s in S)`, ⇒ `bdd_terminalpp(2*j + b)`. Rebuild internal nodes with `bdd_ite`.
  Store as `out->states[i]`.
- `out->aps = nfa.aps`; `out->names` left **empty** (numeric printing; the labels
  are unused downstream). Never returns `nullptr`; a `φ=0` NFA yields the single
  rejecting-sink mtdfa (see edges).
The `2*j+b` / `b = destination-accepting` convention is Spot's state-based mtdfa
terminal encoding (`ltlf2dfa.cc` Q1 in `docs/prd/mtdfa-product.md`;
`solve_mtdfa.cpp:52` reads `states[0]==bddfalse`). BFS discovery order is
deterministic; no seed.

## Edge cases

- **Uncovered letter / partial $\delta_N$.** A state with no out-edge on a letter
  region keeps the $\emptyset$-terminal there ⇒ successor subset $\emptyset$ ⇒
  `bddfalse` in the output ⇒ implicit reject. **No sink state in the `Mtnfa`**; the
  only sink is `bddfalse` in the output mtdfa. Faithful to $N$'s non-completion.
- **$\varphi = \mathtt{1}$ (`tt`).** $N$ rejects $\varepsilon$, accepts every
  non-empty trace; determinized mtdfa language-equals `ltlf_to_mtdfa(1)`. Oracle
  covers it.
- **$\varphi = \mathtt{0}$ (`ff`).** $L(N)=\emptyset$; every reachable subset is
  non-accepting (or the reversal purged everything), so every terminal is `bddfalse`
  ⇒ the mtdfa is a single rejecting sink. Must not crash / must not return
  `nullptr`.
- **Empty universe** ($\mathcal I\cup\mathcal O=\emptyset$): $\Sigma=\{\texttt{bddtrue}\}$,
  each `states[s]` is a bare terminal (no variable nodes); the applies' terminal
  base case handles it.
- **Set grows past machine-word width.** Deliberately exercised in Phase 1 — the
  interned index makes $|S_N|$ unbounded; a bitmask-in-the-int encoding would
  silently corrupt here and is rejected.
- **Initial state accepting.** Out of scope (non-empty traces ⇒ $s_{N,0}\notin F_N$),
  but note the mtdfa transition-based encoding cannot mark $\varepsilon$-acceptance
  on `states[0]` alone; if a future non-empty-trace relaxation needs it, that is a
  new decision, not a silent gap.
- **MONA absent.** `ltlf_to_mtnfa` inherits `ltlf_to_nfa`'s `mona` runtime
  dependency; the corpus oracle is `MONA_FOUND`-gated exactly like the existing
  `ltlf_to_nfa` tests. `nfa_to_mtnfa` / `mtnfa_to_mtdfa` on a hand-built `twa_graph`
  need no `mona` and always run.

## Test oracles (for /test-writer)

- **Phase 1 — direct apply unit tests.** As in the Phase-1 checkpoint: union
  identities on hand-built set-terminal MTBDDs, compared by walking terminals; the
  >64-element set; idempotence/commutativity; leak/`bdd_varnum` sanity.
- **Phase 2 primary — isolated determinization oracle.** Over the generated corpus
  (`tests/ltlfsynt_oracle_test.cpp` generators, reused in-file), MONA-gated:
  `product_xor(mtnfa_to_mtdfa(ltlf_to_mtnfa(φ)), spot::ltlf_to_mtdfa(φ)).is_empty()`.
  Two **independent** constructions (MONA-reverse-lift-determinize vs Spot's direct
  `ltlf_to_mtdfa`) agreeing on the language is the strong oracle. **Negative
  control** required (mtdfa-product Phase-0/Q1 lesson: a naive XOR oracle can be
  silently non-discriminating) — a `G(a)`-vs-`F(a)` style mismatch, or an
  intentionally-broken determinization, must give a **non-empty** XOR.
- **Construction structural free-riders.** `nfa_to_mtnfa(N)`: `states.size() ==
  N->num_states()`; `accepting` / `initial` match `N`; on a hand-built small NFA
  with overlapping guards, the terminal sets equal $\delta_N(s,v)$ on sampled
  letters (pins the union-merge-on-overlap behaviour).
- **Determinize structural.** Output is a well-formed `spot::mtdfa` over exactly
  `φ`'s support APs; `states[0]` is the initial subset; the `φ=0` single-sink and
  `φ=1` shapes as in *Edge cases*.
- **Membership fuzz (scale).** For random non-empty traces `w`: the determinized
  mtdfa accepts `w` iff `ltlf_to_mtdfa(φ)` does — catches edge letters on formulas
  too large to XOR-compare exactly.

## Open theory questions touched

- **`thm:nfa-mirror-size` proof "To be determined"** (single-exponential $N$) — not
  an implementation blocker; inherited from `docs/prd/ltlf-to-nfa.md`, leave for
  `/theory-review`.
- **Subset-construction faithfulness of the *representation*.** `mtnfa_to_mtdfa` is
  textbook `NfaToDfa`, but done symbolically with bespoke terminals; `/theory-review`
  should confirm $L(\texttt{mtnfa\_to\_mtdfa}(N)) = L(N)$ against
  `alg:nfa_product:determinize` (the oracle *verifies* it empirically; theory-review
  blesses the argument). No new `\na`.
- **Non-empty-trace / $\varepsilon$ convention** (`main.tex:96` `\na`, tracked in
  glossary *Open theory questions*): the determinized mtdfa's $\varepsilon$-rejection
  must agree with `ltlf_to_mtdfa`'s De Giacomo–Vardi reading — the oracle checks it,
  but flag for `/theory-review` as the same convention `ltlf-to-nfa.md` flagged.

## Definition of done

- `include/ltlf_ek/detail/state_set_pool.hpp` + `include/ltlf_ek/mtnfa.hpp`
  (+ `src/`) land; tree compiles green.
- Phase 1 apply unit tests + Phase 2 isolated `product_xor` oracle (with negative
  control) + structural free-riders pass; `ctest` green.
- Glossary updated (`/glossary`): *MTNFA* / `Mtnfa`, `ltlf_to_mtnfa`,
  `nfa_to_mtnfa`, `mtnfa_to_mtdfa`, and a call on `StateSetPool`.
- `code-review` (domain + generic) + `theory-review` gates ticked; the subset-
  construction-faithfulness and $\varepsilon$-convention questions dispositioned.
- The two `docs/BACKLOG.md` follow-ons (`NfaProduct`, `MtnfaProduct`) remain
  accurate against what landed.

## Developer comments / PRD disagreements

**2026-07-17 (test gate — entry 3: the SECOND lifetime bug).** `/test-writer`'s
dedicated regression test (`MtnfaBddDictLifetime`, the very test the Phase-2
developer flagged as worth adding) **failed on arrival**, proving the Phase-2
`source_nfa` fix was *incomplete*. `source_nfa` keeps $\varphi$'s AP variables
registered on `dict` only while the **`Mtnfa` struct** lives. But
`mtnfa_to_mtdfa`'s returned `spot::mtdfa` built its rows with `bdd_ite` over
those variable numbers while claiming **no ownership stake of its own** — so the
natural calling pattern `mtnfa_to_mtdfa(ltlf_to_mtnfa(phi, dict))`, which
discards the `Mtnfa` temporary and keeps only the `mtdfa_ptr`, left the mtdfa
holding BDDs over variable numbers `dict` was free to recycle on the next
`register_ap` (exactly what `spot::ltlf_to_mtdfa` — the oracle — does next).
Isolated three-pattern probe confirmed it: `Mtnfa` kept alive ⇒ XOR empty ✓;
`Mtnfa` discarded ⇒ XOR **non-empty** ✗. Nothing in the frozen contract
documented an obligation to keep the `Mtnfa` alive afterwards, so this was a
genuine bug, not a misuse.

**Fix (landed in `src/mtnfa.cpp`, `mtnfa_to_mtdfa`):** the output mtdfa now
registers its own APs — `dict->register_proposition(ap, out.get())` for each of
`out->aps`. `register_proposition` returns the **already-assigned** variable
number and merely adds `out.get()` to that variable's owner list, so the numbers
already baked into the rows stay valid; the pairing unregister is
`spot::mtdfa`'s own destructor (`dict_->unregister_all_my_variables(this)`,
`ltlf2dfa.hh:130` — the class is *designed* to own its stake, which is why this
is the idiom `spot::twadfa_to_mtdfa` honours and not a leak). This does **not**
re-trip `~bdd_dict()`'s `assert_emptiness()` (the trap that ruled out Phase 2's
rejected `register_all_variables_of` "permanent claim" alternative), precisely
because the destructor pairs it. Full suite 362/362 green. Signature unchanged;
no re-freeze needed — but note the invariant now holds *without* any caller
obligation, which is the point.

**2026-07-17 (Phase 1 landing).** No disagreements with the frozen contract:
`StateSetPool`'s constructor, `intern`, `set_of`, `set_union`, and
`guarded_singleton` landed with exactly the signatures in "Interfaces &
types". Two implementation choices the PRD left open (both within the
"members may churn freely" latitude), recorded for traceability:

- **Memo scope is per-call, not cross-call.** `set_union`'s "Novel mechanisms
  (b)" memo (order-normalized `{a.id(), b.id()}` -> result) is a fresh
  `std::unordered_map` allocated inside each top-level `set_union` call and
  discarded on return, via a private recursive `UnionRec` worker — not a
  member field persisted across calls. This satisfies the PRD's complexity
  requirement ("polynomial in the input MTBDDs' node counts, not exponential
  in letters") for a single call; it does not additionally amortize repeated
  `set_union` calls across different top-level pairs (e.g. across the BFS in
  Phase 2's determinizer). If Phase 2's profiling shows that matters, a
  member-scoped memo is a compatible follow-up (same public signature).
- **Explicit `LevelOf` helper for "a terminal counts as below every
  variable".** Rather than relying on `bdd_level()`'s own (undocumented in
  the installed header) treatment of terminal nodes, `set_union`'s topmost-
  variable split uses a small `LevelOf(const bdd&)` that returns
  `std::numeric_limits<int>::max()` for a terminal and `bdd_level(x)`
  otherwise — making the PRD's stated invariant true by construction instead
  of by assumption about BuDDy's internals.

**2026-07-17 (Phase 2 landing).** `include/ltlf_ek/mtnfa.hpp` +
`src/mtnfa.cpp` land with the three signatures verbatim from "Interfaces &
types" (`nfa_to_mtnfa`, `ltlf_to_mtnfa`, `mtnfa_to_mtdfa`), and `Mtnfa`'s six
frozen fields (`states`, `accepting`, `initial`, `pool`, `dict`, `aps`) all
present with the documented semantics. Two forced deviations, both
PRD-change events (the PRD's "reshape" latitude is limited to
`StateSetPool`'s members — these touch `Mtnfa` itself and its
`mtnfa_to_mtdfa` signature's usability, so are flagged here rather than
silently absorbed):

- **`Mtnfa::pool` had to become `mutable`.** `mtnfa_to_mtdfa(const Mtnfa&
  nfa)` is frozen as taking `nfa` by `const&`, but the BFS determinizer must
  `nfa.pool.set_union(...)` to fold multi-state subsets' successor MTBDDs —
  `set_union` is a mutating (interning) call, and it must land in `nfa`'s
  OWN pool (the terminals in `nfa.states` are only meaningful relative to
  that specific pool; a fresh/copied pool would not recognize them). Marking
  `pool` `mutable` is the standard C++ idiom for logical constness on an
  owned cache/interning table and requires no signature change; the field is
  still named `pool`, still `detail::StateSetPool`, still owned.
- **A 7th field, `source_nfa` (`spot::twa_graph_ptr`), had to be added to
  `Mtnfa`.** This is a genuine, empirically-discovered correctness bug,
  not a style choice: `nfa_to_mtnfa` builds `states` from BDDs whose AP
  variables were registered on `dict` by its `nfa` argument
  (`spot::bdd_dict::register_ap(ap, nfa.get())`); `spot::bdd_dict` frees an
  owner's variable numbers when the *last* reference to that owner object
  dies (`unregister_all_my_variables`, called from `twa_graph`'s
  destructor). `ltlf_to_mtnfa(phi, dict)` passes the just-constructed
  `ltlf_to_nfa(phi, dict)` as a **temporary** into `nfa_to_mtnfa`; without
  keeping a reference to it, that twa_graph is destroyed the instant
  `nfa_to_mtnfa` returns, freeing its AP variable numbers back to `dict`
  while `Mtnfa::states` still structurally references them by index. A
  *later* unrelated `dict->register_ap` call on the same dict (e.g.
  `spot::ltlf_to_mtdfa(phi, dict)` building the independent oracle
  afterward, exactly the Phase-2 checkpoint's own oracle shape) can then
  reuse/alias those freed numbers for what it thinks are the same or
  different APs, silently corrupting any later BDD operation that compares
  the two (`product`, `product_xor`, ...). Reproduced concretely: `phi =
  "!(a & b) | c"` gave a spurious non-empty `product_xor` **only** when
  built via the one-line `ltlf_to_mtnfa` composition (temporary `nfa`), and
  correctly reported empty when the caller kept `ltlf_to_nfa`'s result alive
  in a named variable across both `nfa_to_mtnfa` and the oracle call — the
  smoking gun for a variable-aliasing bug, not a determinization bug. Tried
  and rejected: registering a permanent claim under a stable key (e.g.
  `dict.get()`) via `register_all_variables_of` and never unregistering —
  `bdd_dict::~bdd_dict()` calls `assert_emptiness()` and **aborts**
  (`SIGABRT`, confirmed empirically) if any owner's claim is still
  outstanding at dict-destruction time, so a permanent leak is not a safe
  option. The landed fix instead mirrors the codebase's existing pattern for
  this exact hazard (`OutputLabeledTransducer::delta_dfa_`,
  `output_labeled_transducer.hpp:65`): `Mtnfa` keeps a
  `spot::twa_graph_ptr source_nfa` member
  pointing at the graph `nfa_to_mtnfa` was built from, so the graph's own
  (correctly paired) register/unregister lifecycle is tied to the `Mtnfa`'s
  lifetime instead of to some other object's. `source_nfa` is internal
  book-keeping, not part of the six fields the Phase-2 oracle is specified
  to read (`states`, `accepting`, `initial`, `pool`, `dict`, `aps`); it
  does not change any of their names, types, or semantics.
- **Flag for `/test-writer`:** the reproduction above should become a
  regression test — `ltlf_to_mtnfa` called directly (not decomposed into
  `ltlf_to_nfa` + `nfa_to_mtnfa` with the intermediate kept alive), followed
  by an oracle call on the same `dict`, over at least this witnessing
  formula. The existing generated-corpus oracle loop (fresh `dict` per
  formula, `ltlf_to_mtnfa` called once) already exercises this path and
  passed after the fix, but a small dedicated regression test pins the
  *mechanism*, not just an incidental corpus formula.
- **Flag for `/theory-review`:** none — this is a C++/BDD-manager lifetime
  fix, not a `main.tex` semantics question; no theory content changed.
