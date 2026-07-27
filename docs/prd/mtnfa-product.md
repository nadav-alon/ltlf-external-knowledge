# PRD: `MtnfaProduct` — Method 1 in the mtdfa representation

**Status:** implemented — `include/ltlf_ek/mtnfa_product.hpp` + `src/mtnfa_product.cpp`
(the fused BFS `mtnfa_product_to_mtdfa` + `MtnfaProduct::synthesize`), wired into
`CMakeLists.txt` and `src/cli.cpp`/`cli.hpp` (`--mtnfa-product`); tree compiles,
`ctest` green (381/381, concurrent `/test-writer` suite not yet merged into this
worktree).
**Interface:** implements `Synthesis` as `MtnfaProduct` (the **mtdfa** *Representation*
cell of Method 1, alongside the explicit `NfaProduct`); adds one public free function
`mtnfa_product_to_mtdfa` and the `--mtnfa-product` CLI flag.
**Recommended workflow:** **concurrent** — freeze confidence is *high*: the `Synthesis`
signature is forced by the base class, and `mtnfa_product_to_mtdfa` falls straight out
of landed glossary types (`mtnfa_to_mtdfa`'s shape + `product.hpp`'s `taus` vector +
`VariablePartition`). No new struct is invented here, which is what made
`docs/prd/mtnfa.md` re-freeze twice.
**main.tex ref:** §`nfa` (`\cref{nfa}`), `\cref{alg:nfa_product}` — in particular
line `alg:nfa_product:cons` (the $\cons$ filter), `alg:nfa_product:determinize`
(`\algname{NfaToDfa}`) and `alg:nfa_product:solve` (`\algname{SolveDfa}`) — plus
`\cref{def:consistency}` (§203) and the reachability-invariant note at `main.tex:241`.
The mtdfa *Representation* itself has **no `main.tex` symbol** (the `\na` at
`main.tex:335` gestures at MTDFA, but for Method 3).

**Gates:**
- [x] glossary        — new terms in docs/GLOSSARY.md C++ column (already landed by
      the grill-prd session that wrote this PRD, 2026-07-27)
- [ ] tests           — unit + oracle coverage
- [ ] code-review     — domain (/code-reviewer) + generic (/code-review)
- [ ] theory-review   — code ↔ math faithfulness vs main.tex

## Goal

Method 1 (`\cref{alg:nfa_product}`) keeps the Goal automaton **nondeterministic** and
determinizes only *after* crossing it with the knowledge transducers, so the subset
blowup is paid on the $\cons$-pruned product rather than on $\varphi$ up front. That
method ships today only in the **explicit** *Representation* (`NfaProduct`, landed
2026-07-17), which materialises a `twa_graph` product and subset-determinizes it with
`nfa_to_dfa`. This PRD delivers the same method in the **mtdfa** *Representation* —
the way `MtdfaProduct` is Method 2's mtdfa cell — so the automata stay MTBDD arrays
end to end and the game is solved by `solve_mtdfa`.

The substrate already exists: `docs/prd/mtnfa.md` landed the *MTNFA* (`Mtnfa`), its
construction (`ltlf_to_mtnfa`), and its determinization into a `spot::mtdfa`
(`mtnfa_to_mtdfa`) — but deliberately stopped before any transducer, $\cons$ filter,
`Synthesis` class or CLI. This PRD adds exactly that missing layer, and it is the
**last** of the three backlog items that the MTNFA work was split into.

Two comparisons this unlocks, both of which are the point of building it: against
`MtdfaProduct` on the **method** axis (does determinizing the *product* beat Method
2's avoiding determinization altogether?) and against `NfaProduct` on the
**representation** axis (does staying symbolic beat the explicit `twa_graph`
product + `nfa_to_dfa`?). `NfaProduct`'s own benchmark already found that its
scaling cost is the in-process `nfa_to_dfa` subset determinization, not MONA
(`docs/BACKLOG.md`, "Link `libmona` directly", 2026-07-18 note) — which is precisely
the stage this PRD replaces with a symbolic one.

**This PRD supersedes nothing.** It **depends on** `docs/prd/mtnfa.md` (the `Mtnfa`
type and `StateSetPool`) and uses `docs/prd/nfa-product.md`'s `NfaProduct` as its
reference oracle; both are implemented.

## Ubiquitous-language terms used

Existing, unchanged: *Goal formula* ($\varphi$), *MTNFA* (`Mtnfa`, `ltlf_to_mtnfa`),
*MTDFA* (`spot::mtdfa`), *Goal automaton determinization* (`mtnfa_to_mtdfa`),
*Product*, *Consistency* ($\cons$), *Output agreement* (`emits_region`), *Transition
function* (`delta_edges`), *Transducer*, *Observed / produced slice*, *Turn order*
(`require_turn_order_aps`), *Game solving* (`solve_mtdfa`), *Representation*,
*Closed universe of APs* (`VariablePartition::universe()`), *Canonical benchmarking
stage*, *Controller verifier*, *Generated corpus*.

**Glossary gaps — run `/glossary` before or alongside `/developer`:**

- **`MtnfaProduct`** — fills the currently-`—` **mtdfa** cell of the *NFA product*
  (Method 1) row in the *five methods* table. That table's standing note ("Only
  Method 2 has an mtdfa implementation today") becomes stale on landing and must be
  updated; the accompanying **known wart** about `make_synthesis_method` selecting a
  *cell* rather than a method is unchanged by this (a second mtdfa cell makes the
  wart more visible, not worse — `--mtnfa-product` is still unambiguous because the
  representation is baked into the `Mtnfa` type name).
- **`mtnfa_product_to_mtdfa`** — needs an entry, and the interesting part is that it
  spans **two** existing entries at once: it is both the *Product* (mtdfa
  representation, Method 1) **and** the *Goal automaton determinization*
  (`\algname{NfaToDfa}` applied to the product $P$, not to $N$ alone). The two are
  **fused into one pass** here (see *Novel mechanisms*), so one identifier realizes
  both algorithm steps. *Goal automaton determinization*'s mtdfa bullet already
  anticipates this ("generalizes to the $(R,q_{in},q_{out})$ product states under the
  reachability invariant `main.tex:241` — the product determinizations `NfaProduct` /
  `MtnfaProduct`"); it should now name the function.
- Nothing else is new. The fused pass introduces **no** new domain type — no
  `ProductState`/`ProductGuards` analog exists or is wanted on this route (the same
  absence `docs/prd/mtdfa-product.md` records for `MtdfaProduct`).

## Behaviour / semantics (from main.tex)

`MtnfaProduct` must realize `\cref{alg:nfa_product}` — every line of it — with the
Goal automaton, the product, and the game all held in the mtdfa *Representation*.

**1. The product ($P$, `main.tex:217–229`).** For a product state
$\langle s, q_{in}, q_{out}\rangle$ and a letter $v$,
$$\delta_{prod}(\langle s,q_{in},q_{out}\rangle, v) = \begin{cases}\{\langle s', \delta_{in}(q_{in},v), \delta_{out}(q_{out},v)\rangle : s'\in\delta_N(s,v)\} & \text{if } \cons(q_{in},q_{out},v)\\ \emptyset & \text{otherwise,}\end{cases}$$
with $F_P = F_N \times Q_{in} \times Q_{out}$ — acceptance depends on the **goal**
component alone. $\cons$ is applied as a **region intersection**
`emits_region(q_in) & emits_region(q_out)`, never per letter; this is the same
symbolic reading `build_product_symbolic` uses, already blessed against
`\cref{def:consistency}` by the `\cl` note at `main.tex:213–215` (minterm
distributivity). Generalized to $n$ transducers it is
$\bigwedge_k$ `taus[k]->emits_region(q[k])`, matching `product.hpp`'s existing
generalization of $S\times Q_1\times\cdots\times Q_n$.

**2. The determinization (`alg:nfa_product:determinize`).** `\algname{NfaToDfa}`
applied to $P$: a state is a set of $P$-states, the initial one is
$\{\langle s_{N,0},q_{in,0},q_{out,0}\rangle\}$, the successor is the union of the
members' successors, and a set is accepting iff it meets $F_P$. By the reachability
invariant (`main.tex:241`) — $\Tin,\Tout$ deterministic ⇒ every reachable subset of
$P$ has the form $R\times\{q_{in}\}\times\{q_{out}\}$ for a **single** transducer-state
pair — the determinized state is carried as the triple $(R, q_{in}, q_{out})$, i.e.
a subset of $S_N$ **plus** one state per transducer. This is not an optimization to
be rediscovered later; it is the paper's own complexity argument, and it is what
makes the pool's interned sets stay sets of **goal** states.

**3. Skip = reject, and therefore no completion of $N$.** `\cref{alg:nfa_product}`
*skips* a non-$\cons$ letter (`\cref{def:consistency}`, partiality clause: a missing
$\delta$ or $\lambda$ is equivalent to an inconsistent letter). In the mtdfa
representation a skipped letter is spelled `bddfalse`, which **is** the rejecting sink
— completion is implicit (glossary *MTDFA*). Two consequences the implementer must
not "fix":

- **Do not call `spot::complete_here`.** `NfaProduct` must complete $N$ because
  `nfa_to_dfa` *skips* the empty subset, which would otherwise make a $\cons$-passing
  letter on which the goal dies indistinguishable from a non-$\cons$ letter. Here both
  land on `bddfalse` already, so there is nothing to disambiguate and no completion to
  pay for. There is no `Mtnfa` completion primitive and none is wanted.
- **This is the "sink-both" reading** of the empty subset that `docs/BACKLOG.md`
  flags as underspecified in `main.tex` (the `\algname{NfaToDfa}` black box states no
  rule for $\emptyset$). The claim this PRD makes — and hands to `/theory-review` — is
  that sink-both is **sound in the mtdfa representation for two representation-specific
  reasons**: (a) `solve_mtdfa` makes $\Iknown,\Oknown$ **controllable** (Decision 2,
  `docs/prd/mtdfa-product.md`), and $\cons$ pins exactly one legal value of each given
  the earlier moves, so losing on a non-$\cons$ letter never costs the system anything
  — it always has the pinned alternative; and (b) acceptance is **transition-based**,
  so a win is banked on the terminal of the transition *arriving* at a state, not read
  off that state's out-edges. Neither reason transfers to the explicit route, which is
  exactly why `NfaProduct` needs completion and this does not. `MtdfaProduct` already
  ships on reading (a) — `emits_dfa` is deliberately incomplete and
  `spot::twadfa_to_mtdfa` turns its missing edges into `bddfalse` — with
  corpus-validated verdicts against `DfaProduct`.

**4. Game solving (`alg:nfa_product:solve`).** `\algname{SolveDfa}` is `solve_mtdfa`,
unchanged. Its *Turn order* precondition applies: this route reads the Mealy move
order off the **BDD variable order alone**, and a violation returns a wrong
"unrealizable" rather than failing, so `require_turn_order_aps(vars, dict)` must run
**before** any automaton is built on `dict` — the same placement `MtdfaProduct` uses.

## Interfaces & types

**Freeze confidence: high.** Both signatures fall out of landed glossary types: the
`Synthesis` override is forced by the base class, and `mtnfa_product_to_mtdfa` is
`mtnfa_to_mtdfa`'s shape (`const Mtnfa&` → `spot::mtdfa_ptr`) plus `product.hpp`'s
`std::vector<const Transducer*>` plus `VariablePartition`. No new struct is
introduced. **`/test-writer` binds to the block below.**

```cpp
// include/ltlf_ek/mtnfa_product.hpp                                       [new]
#pragma once

#include <optional>
#include <vector>

#include <spot/twaalgos/ltlf2dfa.hh>

#include "ltlf_ek/mtnfa.hpp"
#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/transducer.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// The cons-filtered product of the Goal MTNFA with the knowledge transducers,
// subset-determinized into a spot::mtdfa --- alg:nfa_product lines
// :cons and :determinize FUSED into one symbolic pass (see the PRD's "Novel
// mechanisms").  Method 1's product in the mtdfa Representation; the sibling of
// mtnfa_to_mtdfa, which is the same determinization applied to the Goal NFA
// alone.
//
// `goal`  : the Goal MTNFA (ltlf_to_mtnfa(phi, dict)).
// `taus`  : the knowledge transducers, T_in then T_out --- generalized to n
//           the way build_product / build_product_symbolic / build_product_nondet
//           are; the determinized state carries one state per element.
// `vars`  : supplies the output mtdfa's `aps` = universe() (see below).
//
// Preconditions:
//   - `goal`, every tau, and `vars` share ONE spot::bdd_dict (goal.dict).
//   - !goal.accepting[goal.initial] (mtnfa_to_mtdfa's F2 precondition, same
//     reason: R0 is seeded at output index 0 and never rediscovered as a
//     destination, so an accepting initial state would be silently dropped).
//     ltlf_to_mtnfa always satisfies it (fresh non-accepting s_{N,0}).
//   - Every tau is deterministic (delta_edges' guards pairwise disjoint) ---
//     asserted, see "Novel mechanisms (d)".
// Does NOT check the Turn order AP-ordering contract: that is solve_mtdfa's
// precondition, discharged by MtnfaProduct::synthesize.  Language equality is
// independent of the BDD variable order, so a direct caller comparing languages
// (the oracle) needs no ordering.
//
// nullptr is NEVER returned.
spot::mtdfa_ptr mtnfa_product_to_mtdfa(const Mtnfa& goal,
                                       const std::vector<const Transducer*>& taus,
                                       const VariablePartition& vars);

// Method 1 (main.tex §nfa, \cref{alg:nfa_product}) over the mtdfa Representation
// (docs/GLOSSARY.md "Representation") --- a SECOND implementation of Method 1,
// not a sixth method; NfaProduct is left untouched and is the differential this
// route is graded against.
//
// Shape forced by Synthesis; identical signature to NfaProduct / MtdfaProduct.
class MtnfaProduct final : public Synthesis {
 public:
  std::optional<Controller> synthesize(const spot::formula& phi,
                                       const VariablePartition& vars,
                                       const Transducer& t_in,
                                       const Transducer& t_out) override;
};

}  // namespace ltlf_ek
```

`MtnfaProduct::synthesize`'s body is pinned to this shape (mirroring
`NfaProduct::synthesize` and `MtdfaProduct::synthesize` line for line):

```cpp
const std::vector<const Transducer*> taus{&t_in, &t_out};
validate_product_inputs(phi, vars, taus);          // product.hpp preamble
const spot::bdd_dict_ptr dict = t_in.dict();
require_turn_order_aps(vars, dict);                // BEFORE any automaton is built

Mtnfa goal;
{ BenchTimer t(Stage::automaton_construction); goal = ltlf_to_mtnfa(phi, dict); }

spot::mtdfa_ptr product;
{ BenchTimer t(Stage::product_construction);
  product = mtnfa_product_to_mtdfa(goal, taus, vars); }

BenchTimer t(Stage::game_solving);
return solve_mtdfa(product, vars);
```

**CLI:** `make_synthesis_method("mtnfa-product")` → `MtnfaProduct`, i.e. the flag
`--mtnfa-product`. `cli.hpp`'s doc-comment ("`dfa-product`, `mtdfa-product`, and
`nfa-product` are wired today … the other three recognised method names throw") must
be updated; the `minimize_mtdfa` knob stays `MtdfaProduct`-only and is ignored here
(a `minimize_mtdfa` pass over this product is *adjacent free real estate*, out of
scope — `docs/prd/mtdfa-product.md` measured no downstream payoff at these sizes).

**Benchmarking (`docs/prd/benchmarking.md`), and the stage-mapping question it
settles for this route.** Three canonical `Stage`s, each emitted exactly once, in
order: `automaton_construction` = `ltlf_to_mtnfa` **alone** (mirroring
`MtdfaProduct`'s choice of `ltlf_to_mtdfa` alone — the goal build is the measured
axis, and letting EK-crossing work in would muddy the comparison);
`product_construction` = `mtnfa_product_to_mtdfa`; `game_solving` = `solve_mtdfa`.

There is deliberately **no nested `determinize` sub-span**, and that absence is a
finding, not an oversight: `NfaProduct` emits one because its product and its
determinization are two separate objects built in sequence, whereas here they are a
single fused pass with no intermediate product to time. So the open benchmarking
question — *"determinization runs after the product: its own `Stage` or folded into
`product_construction`?"* (`docs/BACKLOG.md`, benchmarking item) — gets **different
answers per representation**, and that is the honest answer rather than a
convention to be forced: explicit ⇒ separable sub-span, mtdfa ⇒ not separable. Record
it; do not invent a synthetic split.

**If implementation proves this contract wrong:** that is a PRD-change event — update
this section and propagate to the in-flight test branch; the developer does not
silently re-shape the interface on its own branch.

## Novel mechanisms — pinned to code

The fused pass is bespoke (no `main.tex` pseudocode for it, no Spot analog), so it is
pinned past sketch level. It is a direct generalization of `mtnfa_to_mtdfa`
(`src/mtnfa.cpp`), and the implementer should read that function first — the BFS
skeleton, the `RelabelRec` shape, the terminal encoding and the AP-ownership fix all
carry over.

**(a) The BFS key.** A determinized product state is
`struct Key { std::vector<unsigned> R; std::vector<unsigned> q; }` — `R` a **sorted,
de-duplicated** subset of $S_N$, `q` one transducer state per element of `taus`, in
`taus` order. Keys are interned in a `std::map<Key, unsigned>` (lexicographic on
`(R, q)`) mapping to the output mtdfa's state index, with a `std::deque<Key>` work
list — the same `subset_index` / `pending` pair `mtnfa_to_mtdfa` uses, widened by the
`q` component. `R` is **never empty** (only non-empty subsets are enqueued, see (c)).
Seed: `R0 = {goal.initial}`, `q0[k] = taus[k]->initial_state()`, at output index 0 —
so `states[0]` is the initial state, as `solve_mtdfa` and Spot expect. Assert
`!goal.accepting[goal.initial]` up front (the F2 precondition).

**(b) One dequeued state's row.** For `Key{R, q}` at output index `i`:

1. `row_set = fold(goal.pool.set_union, {goal.states[s] : s in R})` — one MTBDD over
   letters whose leaf at $v$ is the set-terminal for $\bigcup_{s\in R}\delta_N(s,v)$.
   Uses the landed `StateSetPool::set_union` unchanged, on `goal`'s **own** pool (the
   terminals in `goal.states` are meaningful only relative to it — this is why
   `Mtnfa::pool` is `mutable`).
2. `cons = AND_k taus[k]->emits_region(q[k])`. If `cons == bddfalse`, the row is
   `bddfalse` (no letter is consistent here — a $\lambda$-undefined transducer state);
   push it and continue. This short-circuit is an optimization **and** the natural
   reading of the partiality clause.
3. `row = bddfalse`. For each combination in the cartesian product of
   `taus[k]->delta_edges(q[k])` — element $k$ contributing $(g_k, d_k)$ —
   let `g = cons & AND_k g_k`. Skip if `g == bddfalse`. Otherwise **mask `row_set`
   to `g` before relabeling**: `row_set_g = bdd_ite(g, row_set, bdd_terminalpp(0))`,
   then `row = bdd_ite(g, Relabel(row_set_g, d), row)`, with `Relabel` as in (c).
   **Corrected 2026-07-27** (see *Developer comments / PRD disagreements*):
   `Relabel` must never be called on the unrestricted `row_set` here — it walks the
   *whole* MTBDD and interns a `Key{S, d}` at every non-empty set-terminal it
   reaches, including branches that occur only **outside** `g` (where the true
   successor vector is a different `d`). Masking to `g` first replaces those
   out-of-`g` branches with the empty-set terminal, which `Relabel` already maps to
   `bddfalse` without interning anything — so only genuinely `d`-reachable subsets
   get enqueued.
4. `assert(subset_index.at(Key{R,q}) == out->states.size());`
   `out->states.push_back(row);` (BFS dequeue order assigns indices 0,1,2,…, so this
   holds; inline the lookup into the `assert` so it vanishes under `NDEBUG`).

Everything a letter is *not* covered by — outside `cons`, outside every
`delta_edges` guard ($\delta$-undefined), or landing on an empty successor subset —
is `bddfalse`. That is skip = reject, per *Behaviour* §3.

**(c) `Relabel(row_set, d)`** — a unary, memoized MTBDD map, the same skeleton as
`src/mtnfa.cpp`'s `RelabelRec`, parameterized by the successor transducer-state
vector `d`. At a set-terminal with members `S = goal.pool.set_of(bdd_get_terminal(n))`:
if `S` is empty ⇒ **`bddfalse`** (the rejecting sink); otherwise
`j =` intern-or-enqueue `Key{S, d}`, `b = any(goal.accepting[s] for s in S)` — the
goal component alone, since $F_P = F_N\times Q_{in}\times Q_{out}$ — and the result is
`bdd_terminalpp(2 * j + b)`. Internal nodes are rebuilt with
`bdd_ite(bdd_ithvarpp(bdd_var(n)), Relabel(bdd_high(n)), Relabel(bdd_low(n)))`.

**Memo scope is per call — this is a correctness constraint, not a perf choice.** The
memo is keyed on `bdd::id()`, and BuDDy recycles a node id once its last handle is
released; per-call scoping is what keeps every keyed operand GC-reachable for the
memo's lifetime. A memo hoisted to member/loop scope to "amortize across states" would
be **unsound** and could return a stale MTBDD (`docs/prd/mtnfa.md` theory-review **F4**,
2026-07-17). Allocate a fresh `std::unordered_map<int, bdd>` per `Relabel` call.

**(d) Determinism and the disjointness assert.** `delta_edges` returns edges in the
transducer's own (deterministic) order and the work list is FIFO, so output state
numbering is fully deterministic — no seed, no randomness. The `bdd_ite` accumulation
in (b).3 is correct **because** the combination guards are pairwise disjoint (each
$\Tin,\Tout$ is deterministic, so its `delta_edges` guards partition the covered
region); if they overlapped, a later block would silently overwrite an earlier one.
Make that explicit: accumulate `covered` and
`assert((g & covered) == bddfalse)` before `covered |= g`. Cheap, debug-only, and it
turns "someone passed a nondeterministic transducer" from a wrong verdict into a loud
failure.

**(e) AP ownership of the returned mtdfa.** `out->aps` = `vars.universe()` mapped to
`spot::formula::ap(name)` and sorted (by formula id — `std::sort` on `spot::formula`,
mirroring `spot::mtdfa`'s documented convention and `src/mtnfa.cpp`'s `SortedAps`).
Then, for each, `goal.dict->register_proposition(ap, out.get())`. This is **not**
optional book-keeping: the rows are built with `bdd_ite` over AP variable numbers, and
without its own stake the returned mtdfa outlives its registration under the natural
calling pattern `mtnfa_product_to_mtdfa(ltlf_to_mtnfa(phi, dict), …)`, letting a later
`register_ap` recycle those numbers — observed empirically as a spurious non-empty
`product_xor` (`docs/prd/mtnfa.md` "Developer comments", entry 3, 2026-07-17).
`register_proposition` returns the already-assigned number and only adds `out.get()`
to the owner list, so numbers already baked into rows stay valid, and
`spot::mtdfa`'s destructor pairs the unregister — so this does not trip
`~bdd_dict()`'s `assert_emptiness()`.

Note `universe()` is the right source rather than `goal.aps`: `goal.aps` is only
$\varphi$'s support, but a transducer's `emits_region` / `delta_edges` may mention APs
$\varphi$ never uses, and the *Closed universe of APs* commitment makes
$\mathcal{I}\cup\mathcal{O}$ an exact superset of everything any row can reference.

**Cost.** Per determinized product state: one `set_union` fold over $|R|$ rows, plus
one `Relabel` pass per combination of transducer out-edges — i.e. the **product of the
transducers' out-degrees**, the same cost shape `build_product_symbolic` has. No
minterm enumeration anywhere; `LetterAlphabet` is **not** used on this route.

## Edge cases

- **Partial transducer, $\lambda$-undefined at a state.** `emits_region(q) ==
  bddfalse` ⇒ `cons` empty ⇒ the whole row is `bddfalse`. If that state was reached
  by an accepting terminal ($b=1$), the system has already banked the win — see the
  known-bug divergence below.
- **Partial transducer, $\delta$-undefined on some letters.** Those letters are
  covered by no `delta_edges` guard, contribute to no block, and stay `bddfalse`.
  Structural, no special case.
- **Known divergence from the explicit route — expected, and asserted.** On a
  partial transducer, `MtnfaProduct` will disagree with `NfaProduct` / `DfaProduct`
  **and be the correct one**: the mtdfa route puts the acceptance bit on the
  *incoming* terminal, so an accepting product state with no outgoing edges keeps its
  mark, whereas `materialize_product` (`src/product.cpp:341`) only attaches the mark
  inside its per-destination guard loop and Spot reads state-based acceptance off a
  state's *first out-edge* — the live bug logged under *Later* in `docs/BACKLOG.md`
  (reproduced: $\varphi=b$, $\Ofree=\{b\}$, a $\delta$-dead `t_in` state ⇒ spurious
  UNREALIZABLE). **The fix is out of scope here** (it changes shipped `DfaProduct`
  verdicts and wants its own semantics grill). Instead this PRD pins the divergence
  with a dedicated test (see *Test oracles*) so `MtnfaProduct` becomes **evidence for**
  the bug rather than a casualty of it, and the cross-method verdict oracle is scoped
  to total transducers.
- **$\varphi = \mathtt{0}$ (`ff`).** $L(N)=\emptyset$; every terminal is `bddfalse`,
  `states[0] == bddfalse`, `solve_mtdfa` reports unrealizable. Must not crash, must
  not return `nullptr`.
- **$\varphi = \mathtt{1}$ (`tt`).** Realizable whenever the transducers admit any
  consistent letter; language-equal to `MtdfaProduct`'s product on the same inputs.
- **$\cons$ empty at the initial state.** `states[0] == bddfalse` ⇒ unrealizable via
  `solve_mtdfa`'s documented test — consistent with the non-accepting-initial
  precondition, which guarantees no win can be banked before any letter is read.
- **Empty universe** ($\mathcal{I}\cup\mathcal{O}=\emptyset$): $\Sigma=\{\texttt{bddtrue}\}$,
  rows are bare terminals with no variable nodes; the `Relabel` terminal base case and
  the `bdd_ite` accumulation both handle it unchanged.
- **Goal state set larger than a machine word.** Unbounded by construction (the pool
  interns indices, not bitmasks) — already exercised by `docs/prd/mtnfa.md` Phase 1.
- **MONA absent.** `ltlf_to_mtnfa` inherits `ltlf_to_nfa`'s `mona` runtime dependency,
  so every `MtnfaProduct` test is `MONA_FOUND`-gated exactly like the existing
  `ltlf_to_nfa` / `NfaProduct` tests. A test driving `mtnfa_product_to_mtdfa` from a
  hand-built `Mtnfa` needs no `mona` and always runs.

## Test oracles (for /test-writer)

- **Primary, exact — cross-representation product equality.** The strongest oracle
  available, and the reason the fused pass is a public free function:
  `mtnfa_product_to_mtdfa(ltlf_to_mtnfa(phi, dict), taus, vars)` and
  `MtdfaProduct`'s intersected product
  `spot::product(spot::product(spot::ltlf_to_mtdfa(phi, dict), twadfa_to_mtdfa(emits_dfa(t_in, dict))), twadfa_to_mtdfa(emits_dfa(t_out, dict)))`
  are DFAs for the **same language** — $L(\varphi)$ intersected with the
  $\cons$-agreeing words — by two completely independent routes (MONA + reverse + lift
  + subset-determinize vs Spot's direct translation + language intersection). So
  `spot::product_xor(a, b).is_empty()` must hold, over the *Generated corpus*,
  MONA-gated. Both sides must be built on **one shared private `bdd_dict` per case**,
  the way `tests/mtnfa_test.cpp:403` already does it — `product_xor` compares a common
  variable numbering. **Negative control required** (the `docs/prd/mtdfa-product.md`
  Phase-0/Q1 lesson: a naive XOR oracle can be silently non-discriminating) — an
  intentionally broken build, or a known-different formula pair, must give a
  **non-empty** XOR.
  - **Expect asymmetric `aps`.** This route's product declares
    `aps = vars.universe()`, while `MtdfaProduct`'s is whatever `spot::product` merged
    from $\varphi$'s support and the two `emits_dfa`s — so the two operands can carry
    *different* ap vectors for the same language, and a variable present in one and
    absent in the other is a don't-care. Assert the languages via `product_xor`, **not**
    the `aps` vectors; if `product_xor` turns out to require matching ap sets, that is a
    test-harness detail to solve in the test (widen the narrower side), not a reason to
    change the signature.
  - **Honest limit, state it in the test file:** both sides share the *skip = reject*
    convention (*Behaviour* §3), so this oracle checks the **construction**, not that
    convention. A shared semantic error would agree — the same "both methods share the
    code path, so they fail identically" blindness `docs/BACKLOG.md` records for the
    `materialize_product` bug. That is what `/theory-review` and the external
    `ltlfsynt` differential are for.
- **Isolated, transducer-free.** With trivial transducers (`trivial_transducer`),
  `product_xor(mtnfa_product_to_mtdfa(...), spot::ltlf_to_mtdfa(phi, dict)).is_empty()`
  — the product degenerates to the goal, so this pins the fusion's neutral element and
  separates a product bug from a determinization bug.
- **Cross-method realizability verdicts, total transducers only.** Over the generated
  corpus, `MtnfaProduct` must agree with `NfaProduct` (representation axis, same
  method) and with `DfaProduct` / `MtdfaProduct` (method axis). The corpus's
  `random_tin` is deterministic **and total** by construction, so this is
  well-defined; do **not** widen it to partial transducers without reading the next
  bullet.
- **The expected-divergence test (required).** One dedicated partial-transducer
  fixture — $\varphi=b$, $\Ofree=\{b\}$, a `t_in` with a $\delta$-dead state, the
  reproduction already recorded in `docs/BACKLOG.md` — asserting
  `MtnfaProduct` ⇒ REALIZABLE (correct) and `NfaProduct` ⇒ UNREALIZABLE (the known
  bug), with a comment naming the backlog item and saying which assertion to flip when
  it is fixed. Also worth asserting `MtdfaProduct` ⇒ REALIZABLE: the same
  transition-based-acceptance argument says the bug cannot bite it either, but that has
  never been tested, so this fixture would establish it. This test is the difference
  between a documented fact and a landmine.
  - **The divergence direction is predicted, not observed.** It follows from the
    transition-based-acceptance argument in *Behaviour* §3, and only the explicit
    route's half has actually been reproduced (`docs/BACKLOG.md`). If the mtdfa route
    *also* reports UNREALIZABLE here, do **not** just flip the assertion — that would
    falsify the argument this PRD's skip-=-reject reading rests on, and it belongs back
    with `/theory-review` (and in *Developer comments*) before the test is written to
    match observed behaviour.
- **Metamorphic round-trip.** `MtnfaProduct::synthesize` → `verify_controller` must
  accept the controller it produced, on both hand-written fixtures and the corpus —
  the project's linchpin internal oracle, method-agnostic by design.
- **Structural free-riders.** The three canonical `BenchTimer` stages are emitted
  once each, in order, with **no** nested `determinize` sub-span (the
  `BenchScopeIntegration` pattern from `tests/nfa_bench_test.cpp`);
  `make_synthesis_method("mtnfa-product")` builds an `MtnfaProduct`; the returned
  mtdfa's `aps` equal `vars.universe()` even when $\varphi$'s support is strictly
  smaller (the bug the three-arg signature exists to prevent — use a fixture where a
  transducer mentions an AP $\varphi$ does not); `states[0]` is the initial subset;
  `nullptr` is never returned.
- **AP-lifetime regression.** The `docs/prd/mtnfa.md` entry-3 hazard applies verbatim:
  call `mtnfa_product_to_mtdfa` on a **temporary** `Mtnfa`, discard it, then do an
  unrelated `register_ap` / `spot::ltlf_to_mtdfa` on the same `dict`, and check the
  product's language is still right. Pins the mechanism, not an incidental formula.
- **Determinism.** Two runs on the same inputs produce mtdfas with identical state
  counts and BDD-equal rows (no seed, FIFO discovery). Cheap, and it guards the
  cartesian-product iteration order.

## Open theory questions touched

- **`\algname{NfaToDfa}` is never defined in `main.tex` — now load-bearing.** This is
  `docs/prd/mtnfa.md`'s theory-review follow-up **F1** (OPEN), which explicitly said
  it was "load-bearing for `MtnfaProduct`, worth pinning before it lands" — this PRD
  is that moment. A `\cl` note for after `main.tex:241` is already drafted in that
  review, and a second one (the empty-subset rule) is drafted in
  `docs/prd/nfa-product.md` and tracked under *Later* in `docs/BACKLOG.md`. Both are
  **Overleaf-only `main.tex` edits**, batched with the next LaTeX pass; neither blocks
  code. `/theory-review` should confirm the triple-keyed determinization
  $(R,q_{in},q_{out})$ against the reachability invariant and the informal black box.
- **Is "sink-both" sound in the mtdfa representation?** The load-bearing new claim of
  this PRD (*Behaviour* §3): non-$\cons$ letters and $\cons$-dead letters both map to
  `bddfalse`, justified by $\Iknown,\Oknown$ being system-controllable in `solve_mtdfa`
  and by transition-based acceptance. `docs/BACKLOG.md` records that **no** uniform
  reading of the black box is sound in general (skip-both ⇒ spuriously realizable,
  sink-both ⇒ spuriously unrealizable), so this is a claim that the mtdfa
  representation escapes that dilemma for representation-specific reasons — not a
  claim that the dilemma was wrong. `MtdfaProduct` already depends on the same reading;
  `/theory-review` should bless or refute it **once**, for both methods.
- **Governed-variable projection** (`main.tex:300` `\na`, supporting argument commented
  out at `main.tex:302–303`) — inherited unchanged from `solve_mtdfa`; this PRD adds a
  second consumer of the strategy-side projection, no new content.
- **Trace-termination semantics** (`main.tex:96` `\na`) — `solve_mtdfa` carries Spot's
  own reading; this adds a second method depending on it agreeing with
  `verify_controller`'s. Already tracked in the glossary's *Open theory questions*; not
  re-opened here.
- **`thm:nfa-mirror-size` proof "To be determined"** — inherited from
  `docs/prd/ltlf-to-nfa.md`; not an implementation blocker.

## Definition of done

- `include/ltlf_ek/mtnfa_product.hpp` + `src/mtnfa_product.cpp` land (both the free
  function and the `Synthesis` class), `CMakeLists.txt` and `src/cli.cpp` updated,
  tree compiles green.
- The exact cross-representation `product_xor` oracle (with its negative control), the
  isolated trivial-transducer oracle, the cross-method verdict oracle (total
  transducers), the metamorphic round-trip, the expected-divergence partial-transducer
  test, the AP-lifetime regression and the structural free-riders all pass; `ctest`
  green.
- Glossary updated (`/glossary`): `MtnfaProduct` fills the *NFA product* row's mtdfa
  cell (and the "only Method 2 has an mtdfa implementation" note is corrected);
  `mtnfa_product_to_mtdfa` is entered against **both** *Product* (mtdfa) and *Goal
  automaton determinization* (mtdfa, applied to $P$), with rejected synonyms set.
- `code-review` (domain + generic) and `theory-review` gates ticked; the
  `\algname{NfaToDfa}` definition gap (F1) and the sink-both soundness claim are
  dispositioned.
- A live benchmark run comparing `MtnfaProduct` against `MtdfaProduct` (method axis)
  and `NfaProduct` (representation axis) on realizable **and** unrealizable instances,
  as `docs/prd/mtdfa-product.md` Phase 2 did — this is the comparison the method exists
  to enable, and it is what tells us whether Method 1's late determinization pays.
- `docs/BACKLOG.md`'s `MtnfaProduct` item moves to **Done** with its outcome; the
  Method 3 item becomes top priority.

## Developer comments / PRD disagreements

- **2026-07-27 — "Novel mechanisms" (b).3 over-approximated reachability
  (PRD-change event, caught in launcher review, not by any test).** The original
  (b).3 called `Relabel(row_set, d)` on the **unrestricted** `row_set` for every
  cartesian combination, instead of `row_set` masked to that combination's guard
  `g`. `Relabel` walks the whole MTBDD and interns a `Key{S, d}` at **every**
  non-empty set-terminal it reaches — including set-terminals that only occur on
  branches **outside** `g`, where the letter's actual `cons`-passing successor
  vector is a *different* `d`. The outer `bdd_ite(g, Relabel(row_set, d), row)`
  then correctly discards that out-of-`g` branch from the returned row, but by
  then `Relabel` has already pushed the spurious `Key{S, d}` onto `pending` and
  it gets processed as if reachable.
  - **Symptom.** Purely a state-count blowup, not a correctness bug: `states[0]`
    is still the true initial state, every reachable state's row is still exactly
    right, and the spurious extra states are dead weight (no combination in
    `taus` ever actually drives the BFS into them via a `d`-labeled edge from a
    reachable predecessor — they are discovered but not truly reachable under
    the product semantics). Worst case the state count degrades toward the
    unpruned $2^{|S_N|}\times|Q_{in}|\times|Q_{out}|$ bound, which is exactly the
    blowup Method 1's late determinization exists to avoid. Confirmed on a
    hand-built fixture (goal row branching `b -> {1}` / `!b -> {2}`, `t_in` with
    two `delta_edges` `(b, 5)` / `(!b, 6)`, trivial `t_out`): `states.size()`
    was 5 before the fix (2 spurious states enqueued) and 3 after (the minimal
    correct count).
  - **No PRD oracle catches this — do not treat it as covered by the test
    suite.** Language is invariant under the bug (the spurious states are
    unreachable, so they never affect a row anyone actually reads), so
    `product_xor` against `MtdfaProduct`'s intersected product passes either
    way, every cross-method/cross-representation realizability verdict agrees
    either way, and `verify_controller`'s metamorphic round-trip accepts either
    way. Only a **state-count** assertion (or a benchmark comparison against the
    theoretical bound) can detect a regression here; none of this PRD's *Test
    oracles* are state-count-sensitive. If `/test-writer` wants coverage for
    this class of bug, it needs a dedicated reachability-tightness fixture, not
    an extension of an existing verdict/language oracle.
  - **The fix.** Mask `row_set` to `g` before relabeling:
    `row_set_g = bdd_ite(g, row_set, bdd_terminalpp(0))`, then
    `Relabel(row_set_g, ...)`. Outside `g`, `row_set_g` is the empty-set
    terminal, which `Relabel`'s existing base case already maps to `bddfalse`
    without interning anything — no new machinery, same pattern `Relabel` itself
    already uses for `bdd_ite`. The per-call `memo` scope (F4) is unaffected and
    stays fresh per combination. Landed in `src/mtnfa_product.cpp`; `ctest` stays
    green (381/381) since no verdict changes.
