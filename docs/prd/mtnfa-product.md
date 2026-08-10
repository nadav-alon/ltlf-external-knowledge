# PRD: `MtnfaProduct` — Method 1 in the mtdfa representation

**Status:** **DONE — landed, all gates closed, benchmark-validated (and the benchmark
verdict is negative).** `include/ltlf_ek/mtnfa_product.hpp` + `src/mtnfa_product.cpp`
(the fused BFS `mtnfa_product_to_mtdfa` + `MtnfaProduct::synthesize`), wired into
`CMakeLists.txt`, `src/cli.cpp`/`cli.hpp` **and `src/ltlf_ek_synth.cpp`'s
`kMethodFlags`** (`--mtnfa-product`); `ctest` green (400/400). Glossary, tests,
code-review (domain + generic) and theory-review all closed; see the three findings
sections below.

**The benchmark answers this PRD's central question in the negative: Method 1's late
determinization does NOT pay.** `MtdfaProduct` beats `MtnfaProduct` on every instance
measured, 9×–3000×, and `mtnfa_product_to_mtdfa` never produces *fewer* states than
`spot::ltlf_to_mtdfa` (exactly 2× more on the family where the NFA is genuinely small).
The mtdfa *Representation* of Method 1 is still worth having — it beats explicit
`NfaProduct` ~1.7× when the Goal NFA is small, and 12× at `game_solving` — but it is
16× **slower** than `NfaProduct` when the NFA is not small. Keep `MtnfaProduct` as the
paper's NFA route and as a differential oracle; do not make it a default. Full numbers,
instance design and a trap to avoid on re-run: "Benchmark results, 2026-07-28".
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
`\cref{def:consistency}` (§203) and the reachability-invariant note at `main.tex:253`.
The mtdfa *Representation* itself has **no `main.tex` symbol** (the `\na` at
`main.tex:350` gestures at MTDFA, but for Method 3).

**Gates:**
- [x] glossary        — new terms in docs/GLOSSARY.md C++ column (already landed by
      the grill-prd session that wrote this PRD, 2026-07-27)
- [ ] tests           — unit + oracle coverage
- [x] code-review     — domain (/code-reviewer) + generic (/code-review), **both
      re-run 2026-07-27.** Domain: three must-fix, **all three fixed** (D1 CLI flag,
      D2 glossary, D3 out-degree coverage). Generic: **no correctness bug**; two of
      its findings were defects in those same-day fixes and were fixed immediately.
      See the two findings sections below. Its sharpest finding — the
      transducer-determinism precondition being `assert`-only — was **also fixed**:
      it is now a `std::runtime_error` on the raw `delta_edges` guards, matching
      `build_product_symbolic`. A few documentation-level *consider* items remain.
- [x] theory-review   — code ↔ math faithfulness vs main.tex, **re-run 2026-07-27,
      clean: no `code-bug`.** Findings below ("Theory-review findings, 2026-07-27"):
      one `doc-bug` (this PRD's *Behaviour* §3 justification, and the
      already-drafted-but-unapplied `\cl` in `docs/prd/nfa-product.md` it agrees
      with, both assert a sink-vs-skip distinction the *game* erases) and two
      `underspecified` (F1 `\algname{NfaToDfa}`, plus the state-based-vs-
      transition-based $F_P$ gap). One `\cl` note drafted for `main.tex`, unapplied.

**Review passes attempted 2026-07-27 — ALL THREE ABORTED, no findings produced.**
**(Superseded for `/theory-review` and the *domain* `/code-reviewer`, both re-run
2026-07-27 — see their findings sections below. Only the generic `/code-review`
is still genuinely unreviewed. Both salvaged leads below are now CLOSED: the
out-degree lead was real, see domain finding D3; the sink-vs-skip lead is
settled, see F5.)**
`/code-reviewer` (domain), `/code-review` (generic) and `/theory-review` were launched
concurrently against `4a1e997..f043912` and all three were killed mid-run by an API
session limit, so the gates above stay unchecked and **nothing was reviewed**. Do not
read the aborted state as a clean pass. Two salvageable leads, recorded only so the
re-run does not re-derive them — these are *where each reviewer was*, **not** findings:

- **Domain review** was checking `trivial_transducer`'s **out-degree** in order to
  assess a coverage claim in `tests/mtnfa_product_test.cpp`. The likely concern: the
  `delta_edges` × `delta_edges` cartesian product in *Novel mechanisms* (b).3 is only
  meaningfully exercised when a transducer has out-degree > 1, so fixtures built on
  `trivial_transducer` may leave the multi-block path (and hence the disjointness
  assert) untested. Worth confirming on re-run.
- **Theory review** had reached *"I have everything I need"* and was doing a final
  check on **how the explicit route's game reads a missing move**, having concluded
  that the sink-vs-skip argument (*Behaviour* §3) turns on exactly that. That is a
  sharper framing than this PRD's own: it says the claim's soundness hinges on
  comparing `solve_dfa`'s treatment of an absent arena move against `solve_mtdfa`'s
  `bddfalse`. Start the re-run there.

Re-run all three after the limit resets; the generic `/code-review` is user-triggered.

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
  reachability invariant `main.tex:253` — the product determinizations `NfaProduct` /
  `MtnfaProduct`"); it should now name the function.
- Nothing else is new. The fused pass introduces **no** new domain type — no
  `ProductState`/`ProductGuards` analog exists or is wanted on this route (the same
  absence `docs/prd/mtdfa-product.md` records for `MtdfaProduct`).

## Behaviour / semantics (from main.tex)

`MtnfaProduct` must realize `\cref{alg:nfa_product}` — every line of it — with the
Goal automaton, the product, and the game all held in the mtdfa *Representation*.

**1. The product ($P$, `main.tex:233–244`).** For a product state
$\langle s, q_{in}, q_{out}\rangle$ and a letter $v$,
$$\delta_{prod}(\langle s,q_{in},q_{out}\rangle, v) = \begin{cases}\{\langle s', \delta_{in}(q_{in},v), \delta_{out}(q_{out},v)\rangle : s'\in\delta_N(s,v)\} & \text{if } \cons(q_{in},q_{out},v)\\ \emptyset & \text{otherwise,}\end{cases}$$
with $F_P = F_N \times Q_{in} \times Q_{out}$ — acceptance depends on the **goal**
component alone. $\cons$ is applied as a **region intersection**
`emits_region(q_in) & emits_region(q_out)`, never per letter; this is the same
symbolic reading `build_product_symbolic` uses, already blessed against
`\cref{def:consistency}` by the `\cl` note at `main.tex:228–230` (minterm
distributivity). Generalized to $n$ transducers it is
$\bigwedge_k$ `taus[k]->emits_region(q[k])`, matching `product.hpp`'s existing
generalization of $S\times Q_1\times\cdots\times Q_n$.

**2. The determinization (`alg:nfa_product:determinize`).** `\algname{NfaToDfa}`
applied to $P$: a state is a set of $P$-states, the initial one is
$\{\langle s_{N,0},q_{in,0},q_{out,0}\rangle\}$, the successor is the union of the
members' successors, and a set is accepting iff it meets $F_P$. By the reachability
invariant (`main.tex:253`) — $\Tin,\Tout$ deterministic ⇒ every reachable subset of
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
  is that moment. A `\cl` note for after `main.tex:253` is already drafted in that
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
- **Governed-variable projection** (`main.tex:315` `\na`, supporting argument commented
  out at `main.tex:317–318`) — inherited unchanged from `solve_mtdfa`; this PRD adds a
  second consumer of the strategy-side projection, no new content.
- **Trace-termination semantics** (`main.tex:98` `\na`) — `solve_mtdfa` carries Spot's
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
- ~~A live benchmark run comparing `MtnfaProduct` against `MtdfaProduct` (method axis)
  and `NfaProduct` (representation axis) on realizable **and** unrealizable instances,
  as `docs/prd/mtdfa-product.md` Phase 2 did — this is the comparison the method exists
  to enable, and it is what tells us whether Method 1's late determinization pays.~~
  **DONE 2026-07-28 — see "Benchmark results" below. The answer is no.**
- `docs/BACKLOG.md`'s `MtnfaProduct` item moves to **Done** with its outcome; the
  Method 3 item becomes top priority.

## Benchmark results, 2026-07-28 — **late determinization does not pay**

Live `--benchmark` sweep over the CLI, three methods × two scaling families × three
knowledge regimes, realizable **and** unrealizable in every family; min of 2 runs,
20 s timeout. Harness and raw JSON are not committed (throwaway); everything needed
to reproduce is below.

**Headline: on the method axis `MtdfaProduct` wins on every single instance measured,
by 9× to 3000×.** Method 1's late determinization is not merely a wash here — it is
the dominant cost. On the representation axis the answer is *conditional*: the mtdfa
route beats the explicit one when the Goal NFA is genuinely small, and loses badly
when it is not.

**Instance design — the first family was WRONG, and the state counts caught it.** The
obvious pick, $\varphi_n = F(v \wedge X[!]^n\,v)$, is the textbook NFA-vs-DFA blowup —
but `ltlf_to_nfa` is *mirror*-based (`reverse_dfa_to_nfa`), so a small NFA needs a small
**reverse**-language DFA, which that family does not have. Measured directly: its NFA is
**1027 states at $n=10$**, the same order as its DFA, so it never tested the hypothesis
at all. The family that does is

$$\varphi_n \;=\; F\bigl(v \wedge X[!]^n(\neg X[!]\,\mathbf{1})\bigr)
\qquad\text{“}v\text{ holds exactly } n \text{ strong-steps before the LAST position”}$$

whose reverse language ("the $(n{+}1)$-th letter is $v$") is deterministic in $O(n)$.
Measured: NFA $n{+}3$, `spot::ltlf_to_mtdfa` $2^n$. **Anyone re-running this must check
the NFA size first — the intuitive family silently degenerates.**

**State counts (the decisive number; timing alone cannot separate "slower" from
"builds more").** On the mirror-small family, `mtnfa_product_to_mtdfa` produces
**exactly $2\times$** the states of `spot::ltlf_to_mtdfa` at every $n$ (2049 vs 1024 at
$n=10$); on the degenerate family it produces the *same* count (1026 vs 1024). It never
produces fewer. The hoped-for win — $\cons$-filtering prunes the subset space *before*
determinization — never materializes, because the subset construction is unminimized
while Spot's DFA is not.

| family, $n$ | NFA | `ltlf_to_mtdfa` | `mtnfa_product_to_mtdfa` |
|---|---|---|---|
| mirror-small, 10 | 13 | 1024 | 2049 |
| degenerate, 10 | 1027 | 1024 | 1026 |

**Stage breakdown — Method 1's promise is real but small, and it is swamped.**
Mirror-small, $n=12$ (ms):

| | `automaton_construction` | `product_construction` | `game_solving` | total |
|---|---|---|---|---|
| `MtnfaProduct` | **4.1** | 126.2 | 1.1 | 135.0 |
| `MtdfaProduct` | 8.4 | **1.0** | **0.6** | **12.5** |
| `NfaProduct` | 3.6 | 215.4 (`determinize` 215.3) | 13.1 | 235.2 |

Method 1 *does* build its Goal automaton ~2× cheaper than Method 2 (4.1 vs 8.4 ms) —
the linear NFA vs the exponential DFA, exactly as predicted. But it defers the
exponential work into `product_construction`, where it costs 126 ms, and the net is a
**11× loss**. Method 2's `ltlf_to_mtdfa` stays symbolic and never pays for the blowup
at all.

**Representation axis — conditional, and worth keeping.** Against `NfaProduct` on the
same method, the mtdfa route wins where it should: the fused pass is 1.7× faster than
the explicit `determinize` (126 vs 215 ms) and `game_solving` is **12× faster** (1.1 vs
13.1 ms), for ~1.7× overall. **But on the degenerate family it is 16× SLOWER** (16.4 s
vs 0.98 s at $n=10$): (b).1's per-state `set_union` fold over $R$ costs
$O(|R|\times\text{MTBDD})$, and when the NFA is itself exponential the subsets are
large. `NfaProduct` is the better Method-1 implementation whenever the Goal NFA is not
small.

**External knowledge did not rescue it.** Three regimes: (A) trivial transducers;
(B) `t_in` commits $k \leftrightarrow i$ — this one is a **null test**, since the goal
never mentions $k$, so $\cons$ restricts letters without killing any goal branch, and
timings match regime A to within noise; (C) `t_in` pins $k=\text{false}$ with goal
$F(k \wedge X[!]^n k)$, so $\cons$ kills **every** goal branch — the maximally
Method-1-favourable case. `MtdfaProduct` still won regime C (13 vs 216 ms at $n=12$).

**Verdict agreement: perfect.** All three methods agreed on every instance of every
family in every regime (~100 instances) — the cross-method oracle passing live, at
sizes the unit corpus never reaches.

**What this means for the method.** `MtnfaProduct` is correct, and it is the right
implementation of Method 1 when the Goal NFA is small — but Method 1 itself is not
competitive with Method 2 in this toolchain, because `spot::ltlf_to_mtdfa` never
materializes the blowup that Method 1 exists to defer. Keep it as the paper's NFA route
and as the differential oracle it already serves as; do not make it a default. The
`thm:nfa-mirror-size` proof being "To be determined" is now more interesting, not less:
the mirror construction is what decides whether Method 1 has a favourable regime at all.

## Domain code-review findings, 2026-07-27 (`/code-reviewer`, `4a1e997..f043912`)

Delivered: three must-fix, four *consider*. **All three must-fix were fixed the same
day, 2026-07-27** (resolutions recorded inline below); the four *consider* are left
open as non-blocking. The gate box stays unticked because the generic `/code-review`
is a separate, user-triggered gate and is still owed. `ctest` green after the fixes
(398/398).

**D1 (must fix) — `--mtnfa-product` is unreachable from the CLI.**
`src/ltlf_ek_synth.cpp:53`'s `kMethodFlags` was never extended, so `ParseArgs`
(`src/ltlf_ek_synth.cpp:136`) throws `UsageError` before `make_synthesis_method` is
ever consulted: `build/ltlf-ek-synth --mtnfa-product …` → `usage error: unrecognised
flag: --mtnfa-product`, exit 2. `include/ltlf_ek/cli.hpp:43` claims the flag is "wired
today" and this PRD puts it in scope (*Interfaces & types*, **CLI**), so the claim is
currently false. It is invisible to `ctest`: `tests/mtnfa_product_test.cpp:383` exercises
`make_synthesis_method("mtnfa-product")` directly, and `tests/ltlf_ek_synth_test.cpp` has
no end-to-end run for it. Note the *Definition of done* names only `src/cli.cpp`, which
is why the second site was missed — fix the DoD wording too. While there,
`src/ltlf_ek_synth.cpp:48-52`'s comment still says "six flags" and that only
`dfa-product`/`mtdfa-product` are wired (the `nfa-product` half of that staleness
pre-dates this PRD).

  - **FIXED 2026-07-27.** `"mtnfa-product"` added to `kMethodFlags`, and the comment
    above it rewritten to say out loud that a new flag needs **two** edits and that
    `ParseArgs` rejects an unlisted flag before the factory is consulted. Regression
    guard added: `LtlfEkSynth.EveryWiredMethodFlagIsAcceptedEndToEndAndAgreesOnThe`
    `Verdict` (`tests/ltlf_ek_synth_test.cpp`) drives **all four** wired flags through
    the real binary and asserts `REALIZABLE` + exit 0, so the next second-implementation
    flag is one line and cannot ship unreachable. The *Definition of done*'s "`src/cli.cpp`
    updated" wording is what misled the implementer — read it as *both* CLI sites.
  - **Follow-up from the generic `/code-review`, same day:** that guard was written as
    one `MONA_FOUND`-gated loop, so a mona-less build skipped it **entirely** — including
    for `--dfa-product` / `--mtdfa-product`, which need no mona, i.e. exactly the
    configuration where an unreachable-flag bug would ship unnoticed. Split into
    `EveryWiredMonaFreeMethodFlagIsAcceptedEndToEnd` (unconditional) and
    `EveryWiredNfaRouteMethodFlagIsAcceptedEndToEnd` (`MONA_FOUND`-gated) over a shared
    helper. When adding a flag, put it in the loop that matches its mona dependency.

**D2 (must fix) — the glossary entries are derelict.** `docs/GLOSSARY.md:652` and `:742`
still mark `mtnfa_product_to_mtdfa` / `MtnfaProduct` **not yet implemented**; both landed
in `04462b1`. This is the *Definition of done*'s own open glossary bullet — fix via
`/glossary`.

  - **FIXED 2026-07-27.** Both markers now read **landed**. The rest of that DoD bullet
    was already satisfied at PRD time and was re-checked: `mtnfa_product_to_mtdfa` is
    entered against *Product* (`docs/GLOSSARY.md:650`) **and** *Goal automaton
    determinization* applied to $P$ (`:543`), cross-referenced rather than merged, with
    the rejected-synonym line in place. No "not yet implemented" marker remains anywhere
    in the glossary.

**D3 (must fix, test debt) — the (b).3 cartesian path is structurally untested; the
salvaged out-degree lead was REAL.** No fixture anywhere gives a transducer out-degree
> 1: `trivial_transducer` is one state with one `bddtrue` self-loop
(`src/output_labeled_transducer.cpp:93-101`), and every hand-built transducer in
`tests/mtnfa_product_test.cpp` — including `MtnfaProductCorpusTin` and the
expected-divergence `t_in` — uses a single `new_edge` per state. So `ForEachCombination`
always yields exactly **one** combination, and the cartesian product, the multi-block
`bdd_ite` accumulation and the (d) disjointness assert are never exercised.
`tests/mtnfa_product_test.cpp:228`'s claim that the determinism test "also pins that the
cartesian-product iteration order is stable" is therefore **vacuous** — there is one
block to order.

  - **It is test debt, not a latent bug — verified, do not re-derive.** Probed directly
    with this PRD's own fixture shape (goal row branching `b -> {1}` / `!b -> {2}`, a
    `t_in` with two `delta_edges` `(b,1)`/`(!b,2)`, trivial `t_out`): the multi-block
    path produces the **tight** 3 states and does not trip the assert; inserting an
    overlapping guard *does* trip it at `src/mtnfa_product.cpp:197`. Both halves of (d)
    behave as specified.
  - **FIXED 2026-07-27.** New **SECTION A2** in `tests/mtnfa_product_test.cpp`:
    `MtnfaProductMultiBlock.OutDegreeTwoTransducerExercisesTheCartesianPathAndStays`
    `ReachabilityTight` builds the (b).3 *Developer comments* fixture (goal branching
    `a&b -> {1}` / `a&!b -> {2}`, `t_in` with two `delta_edges`, trivial `t_out`) and
    asserts **`states.size() == 3`** plus a language spot-check, and
    `MtnfaProductMultiBlockDeathTest.OverlappingDeltaEdgeGuardsTripTheDisjointnessAssert`
    (`#ifndef NDEBUG`) pins the (d) assert on a nondeterministic `t_in`. The false
    coverage claim at the determinism test was rewritten to say plainly that this
    fixture has one combination and points at A2.
  - **The state-count assertion was negative-controlled, not just written.** Reverting
    the `(b).3` mask to the unmasked `Relabel(row_set, …)` makes it fail with exactly
    the predicted 5, **and nothing else in the suite fails** — which is the first
    empirical confirmation of the *Developer comments* claim that the bug is
    language-invariant. These are now the only state-count-sensitive tests in the
    suite; if one fails while the language oracles stay green, the mask is what
    regressed.
  - **Follow-up from the generic `/code-review`, same day: A2 closed D3 structurally
    but not semantically.** The first fixture gives `t_in` an empty $\Sigma_1$, so
    `emits_region == bddtrue` and $\cons$ masks nothing — the combination guards are
    the bare `delta_edges` guards. Added
    `MtnfaProductMultiBlock.MultiBlockPathIsCorrectWhenConsAlsoRestrictsEach`
    `Combination`: out-degree 2 **and** a state-dependent $\lambda$ ($\Sigma_1=\{k\}$,
    state 0 commits $k \leftrightarrow a$), so every combination guard is
    $\cons\ \&$ delta-guard, with a hand-derived membership oracle. It is a sharper
    negative control than the first — unmasked it interns $(\{1\},2,0)$, a key whose
    destination transducer state that combination's guard makes unreachable, giving 3
    states instead of 2. **Still open (not blocking):** the MONA corpus's transducers
    are all out-degree 1, so the primary `product_xor` oracle still never reaches the
    multi-block path; a corpus variant with an out-degree-2, $\cons$-restricting
    `t_in` would close that. The generic reviewer closed it out-of-tree (140 random
    differential cases, all agreeing) — evidence of correctness, but not a
    regression guard.

**Consider (non-blocking).**
- `src/mtnfa_product.cpp:187` — `covered` is `assert`-only, but `covered |= g` survives
  `NDEBUG` as a live BDD op per combination. Same class as the `b7cc9b3` follow-up;
  `#ifndef NDEBUG` it.
- `src/mtnfa_product.cpp:201-213` — 13 lines re-litigating the (b).3 PRD-change event in
  source. The argument already lives in *Developer comments*; keep ~3 lines ("mask to
  `g`, or `Relabel` interns keys off branches outside `g`") plus the pointer.
- `out->aps = vars.universe()` is sound only under the *Closed universe of APs*
  commitment, and `validate_product_inputs` (`src/product.cpp:348`) checks $\varphi$'s
  APs but never the transducers'. On **this** route specifically an out-of-universe
  transducer AP yields an mtdfa whose rows branch on a variable absent from `aps`; the
  other routes only get a strange guard. Worth naming in the header's precondition list.
- `README.md:47` still says only `--dfa-product` is wired (pre-existing).

**Checked and sound — recorded so no later pass re-derives it.** Terminal encoding
`2j+b` / `bddfalse` sink matches `mtnfa_to_mtdfa`; the per-call `memo` scope honours F4;
the `register_proposition` ownership pattern matches `src/mtnfa.cpp`'s and is
`~mtdfa()`-paired; the BFS-index assert uses the inlined `b7cc9b3` form;
$\cons=\bigwedge_k$ `emits_region` is valid *as a guard* because the relation ranges only
over $\Sigma_0\cup\Sigma_1$; `bdd_terminalpp(0)` = the empty set is guaranteed by
`StateSetPool`'s constructor; `Synthesis` conformance and `BenchTimer` stage ordering
match `MtdfaProduct`. Suite green (28/28 on the `Mtnfa*` filters after a rebuild).

## Generic code-review findings, 2026-07-27 (`/code-review 4a1e997`)

**No correctness bug in the core algorithm.** The reviewer re-derived the load-bearing
invariants independently (terminal 0 = empty set; BFS index ↔ `states.size()` because
insert order = index order = FIFO; the per-call `Relabel` memo is F4-safe because its
keys are ids of nodes kept alive by `row_set_g`; `spot::formula::operator<` is id-based
so the `aps` sort honours `mtdfa`'s convention; `~mtdfa` pairs the
`register_proposition` stake) and added one this PRD had not recorded:
**`spot::mtdfa::is_empty()`'s "all states reachable" assumption holds precisely because
of the (b).3 `g`-mask** — a second, independent reason the mask is load-bearing rather
than an optimization.

It also attacked the D3 gap empirically out-of-tree: 140 random differential cases
against `ltlf_to_mtdfa × twadfa_to_mtdfa(emits_dfa(τ))` with multi-state, out-degree-2,
$\cons$-restricting transducers (60) and partial-δ transducers (80) — `product_xor`
empty in all 140 — plus $n = 0$ and $n = 3$ transducer counts (nothing in the suite
tests $n \neq 2$, though the header claims the generalization). All correct. That is
evidence, not a regression guard; see the corpus follow-up under D3.

**Two findings were defects in the same-day D1/D3 fixes and were fixed immediately**
(recorded inline above): the MONA-gating of the CLI guard, and A2's structural-only
coverage. One was a stale SHA in D2's text (`04262b1` → `04462b1`), fixed.

**FIXED 2026-07-27 — the determinism precondition is now enforced, not asserted.**
The `assert` on the *combined* combination guards, plus its `covered` accumulator, is
replaced by a `std::runtime_error` thrown per tau on the **raw** `delta_edges` guards
as `edges[k]` is built. This is strictly stronger than what it replaced, in three ways
beyond surviving `NDEBUG`:
- it checks each transducer's own guards directly instead of inferring disjointness
  from the combined guards, so an overlap that `cons` or another tau's guards happen to
  mask apart still trips (the same reasoning `build_product_symbolic` records);
- it names the offending transducer state in the message;
- it deletes `covered` outright, which also resolves the "assert-only work survives
  `NDEBUG`" finding raised by both review passes — nothing is now computed for an
  assert's benefit.

Wording, exception type and check shape are copied from `build_product_symbolic`
(`src/product.cpp`), which performs the identical check on the identical data;
`OutputLabeledTransducer::delta` throws the per-letter analogue. The check sits inside
the `cons != bddfalse` branch on purpose: a `cons`-dead state's row is `bddfalse`
whatever the transducer does, so no language can be wrong there and (b).2's shortcut
stays a pure shortcut. `tests/mtnfa_product_test.cpp`'s death test became a plain
`EXPECT_THROW` and now runs in release builds too — which was the whole point.

**Open, non-blocking — left for a decision rather than fixed:**
- `src/mtnfa_product.cpp` — the F2 precondition (`!goal.accepting[goal.initial]`) is
  still `assert`-only and has the same release-silence shape. Consistent with
  `mtnfa_to_mtdfa`, but this is a newly-public entry point, so the two should probably
  move together rather than diverge.
- `src/cli.cpp:88` — `--minimize-mtdfa` is silently ignored for `--mtnfa-product`, so a
  benchmark run with the knob is indistinguishable from one without it (no
  `minimize_mtdfa` span, no minimisation, no error). *Interfaces & types* **CLI** does
  settle the knob as `MtdfaProduct`-only, so this is PRD-sanctioned — but the silence is
  not: rejecting the combination would cost nothing and is the honest reading.
- `README.md:46` — still says only `--dfa-product` is wired, and its
  recognised-but-unimplemented list omits both mtdfa-representation flags, so it now
  understates *and* misclassifies. Pre-existing drift this PRD widens.

**Not a finding:** the reviewer flagged the uncommitted `.claude/settings.json` reduction
to `{}` as an accidental carry-over. It is intentional and unrelated to this PRD — the
project-level `claude-opus-4-8` pin was removed on request so the user-level model
setting applies. Ignore it when reading this diff.

## Theory-review findings, 2026-07-27 (faithfulness mode, `4a1e997..f043912`)

**No `code-bug`.** `mtnfa_product_to_mtdfa` realizes `\cref{alg:nfa_product}` faithfully:
$\cons$ as the region $\bigwedge_k$ `emits_region` (blessed by `main.tex:228`'s `\cl`),
$\delta_{prod}$'s goal component as `goal.pool.set_union` over $R$,
$F_P=F_N\times Q_{in}\times Q_{out}$ as `any(goal.accepting[s] for s in S)` on the
**goal** slice alone, and `\algname{NfaToDfa}`'s subset step as the interned
$(R,q_{in},q_{out})$ key. The (b).3 masking fix is faithful and **tight**, not merely
sound: $\mathtt{bdd\_ite}(g,\ \mathtt{row\_set},\ \mathtt{terminal}(0))$ is a *reduced*
MTBDD, so a non-empty set-terminal survives in it iff it is the function's value at some
$v\models g$ — hence every interned `Key{S,d}` is a genuine $\delta_D((R,q),v)$ and the
reachable state count is exactly within `main.tex:253`'s
$2^{|S_N|}\cdot|Q_{in}|\cdot|Q_{out}|$ bound. That is positive evidence for the
`\cref{alg:nfa_product}` complexity theorem (whose own proof is still "To be determined")
in the mtdfa representation.

**F5 — sink-both is SOUND; the dilemma it was thought to escape does not exist.**
(`doc-bug`, in this PRD's *Behaviour* §3 and in the unapplied `\cl` draft in
`docs/prd/nfa-product.md` / `docs/BACKLOG.md`; **the code is right either way**.)
Settled by reading how `solve_dfa` treats an *absent* arena move, which is the
comparison the aborted run had reached. `solve_dfa` calls
`split_2step(game, complete_env=true)` (`src/solve_dfa.cpp:57`), and Spot's
`split_2step_expl_impl` (`spot/twaalgos/synthesis.cc:574`) completes the **environment**
into a player-losing sink: `all_letters` is the union of the out-edges' input
projections, and `bddtrue - all_letters` is routed to `sink_con`, which loops with the
`unsat_mark`. On the **system** side there is no completion at all: an uncovered output
is simply a move that is never offered. So the explicit route already reads a missing
move exactly the way `bddfalse` reads in the mtdfa game — env branch to `bddfalse` = the
rejecting sink = system loses; controllable branch to `bddfalse` = a move the system
never takes. Consequently:
- **skip-both is *not* "spuriously realizable"** — `complete_env` turns the skip back
  into a losing sink for every environment move.
- **sink-both is *not* "spuriously unrealizable"** — $\cons$ pins $\Iknown,\Oknown$ to
  exactly one live value, so a non-$\cons$ letter can never be forced on the system
  (this PRD's reason (a), **confirmed**), and a $\cons$-dead letter is a genuine loss.
- The real difference between the two routes is **not** sink-vs-skip but **where the
  governed variables live**: `solve_dfa` `bdd_exist`s $\Iknown,\Oknown$ out of the arena
  *before* splitting, so a non-$\cons$ letter is not a move at all; `solve_mtdfa` keeps
  them as forced controllable moves, so it *is* a (losing) move the system simply never
  picks. Both quantify the pinned variables **existentially for the system**, so they
  coincide — even if some $\lambda$ were non-functional and pinned more than one value.
- Therefore this PRD's inference *"neither reason transfers to the explicit route, which
  is exactly why `NfaProduct` needs completion and this does not"* is **wrong**:
  `spot::complete_here` in `NfaProduct` is **verdict-neutral**, a state-count cost rather
  than a correctness requirement. The *conclusion* ("do not call `complete_here` here")
  is right, and `NfaProduct` needs **no** change — but do not apply the
  `docs/prd/nfa-product.md` `\cl` draft as written, it would put a false distinction into
  `main.tex`. Superseded by the F1 note drafted below.

**F1 — `\algname{NfaToDfa}` is undefined in `main.tex`** (`underspecified`, still OPEN,
inherited from `docs/prd/mtnfa.md`; now load-bearing in **three** ways, not one). `grep`
finds the name only at `main.tex:161` (prose) and `main.tex:280` (the algorithm line).
Missing: (i) the subset-construction rule itself, (ii) the $\emptyset$ rule, and (iii)
the fact that the reachability invariant at `main.tex:253` — stated only as a *proof
note* on a theorem whose proof is "To be determined" — is what licenses carrying
$(R,q_{in},q_{out})$ as the state instead of a subset of $S_P$. A drafted `\cl` for after
`main.tex:253` is in the review report; **Overleaf-only, batched, does not block code**.

**F6 — $F_P$ is state-based in `main.tex`, transition-based in the mtdfa realization**
(`underspecified`). `\cref{alg:nfa_product}` line `alg:nfa_product:final_def` makes $F_P$
a set of *states*; `Relabel` puts the acceptance bit on the terminal of the transition
*entering* $R$. The two agree on non-empty traces **iff the empty trace is not accepted**
— i.e. exactly the `!goal.accepting[goal.initial]` (F2) precondition, which is therefore
load-bearing for more than index bookkeeping. This equivalence is nowhere in `main.tex`,
and it is precisely the axis on which the *expected-divergence* fixture makes
`MtnfaProduct` the correct method. Folded into the same drafted `\cl`.

**Nit (non-blocking, style).** `main.tex:228`'s `\cl` names only
`\texttt{build\_product\_symbolic}` as the region-intersection consumer; there are now
three (`build_product_symbolic`, `emits_dfa`, `mtnfa_product_to_mtdfa`). Its argument
(minterm distributivity) is generic and still covers this route; only the example is
stale. Batch with the F1 note.

**Also noted, benign.** `main.tex:269` initializes $\delta_{prod}$ to an "undefined
mapping" while the display at `main.tex:237–244` gives $\emptyset$ for a non-$\cons$
letter; for an NFA the two are the same object, so no verdict.

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
