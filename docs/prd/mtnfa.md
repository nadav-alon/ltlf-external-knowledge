# PRD: MTNFA — a multi-terminal NFA representation, determinizable into `spot::mtdfa`

**Status:** draft
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
- [ ] tests           — unit + oracle coverage
- [ ] code-review     — domain (/code-reviewer) + generic (/code-review)
- [ ] theory-review   — code ↔ math faithfulness vs main.tex

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

**Freeze confidence: tentative.** The `Mtnfa` type and both bespoke MTBDD applies
are invented here; implementation may reshape them. Hence **sequential** workflow —
`/developer` first, `/test-writer` binds to the landed signatures.

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
  detail::StateSetPool pool;            // interprets the terminals; OWNED
  spot::bdd_dict_ptr dict;
  std::vector<spot::formula> aps;       // APs registered (phi's support on dict)
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
