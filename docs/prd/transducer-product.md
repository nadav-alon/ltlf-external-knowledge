# PRD: Reusable transducer product (Goal automaton × N transducers)

**Status:** implemented — 57aab06 (Phase 1) + Phase 2 (this commit)
**Interface:** new `product.hpp` core — `agreeing_successor` (lazy per-letter step) + `build_product` (eager driver → neutral map); consumed by `DfaProduct::synthesize` and `verify_controller`
**main.tex ref:** `alg:dfa_product` (Method 2 product), $\cons$ = `\cref{def:consistency}` (with its partiality note); the 4-way verifier product is code-only (`docs/prd/controller-verifier.md`)

**Gates:**
- [x] glossary        — new terms in docs/GLOSSARY.md C++ column
- [x] tests           — unit + oracle coverage (Phase 1: see Developer comments below)
- [x] code-review     — Phase 2 (verify_controller migration): domain + generic clean
- [x] theory-review   — Phase 2 clean (no code-bug); filter equivalence + def:probDefTransducer preserved

## Goal
The product-of-Goal-automaton-with-transducers construction is currently
hand-rolled twice: `DfaProduct::synthesize` (`src/dfa_product.cpp`) crosses the
Goal DFA with $\Tin,\Tout$, and `verify_controller` (`src/verify_controller.cpp`)
crosses it with $\Tin,\Tout,T_C$. The two share the same worklist BFS, the same
`all_letters` enumeration, near-identical `dfa_delta` helpers, the same
AP-⊆-$\mathcal{I}\cup\mathcal{O}$ / shared-`bdd_dict` validation, and the same
per-letter *enabled* filter — differing only in **arity** (3 vs 4 components) and
in **materialization** (a `spot::twa_graph` game arena vs an in-memory map for a
$\nu$-fixpoint). This PRD extracts one reusable product over a **Goal automaton ×
a list of transducers**, with a lazy per-letter core (that Methods 3.1–3.3 can
later drive on the fly) and one thin eager driver returning a
materialization-neutral map that each consumer post-processes. It is a
**behaviour-preserving refactor**: it changes no method's result, only removes
the duplication. It refactors the internals of `docs/prd/dfa-product.md` and
`docs/prd/controller-verifier.md` without superseding either.

## Ubiquitous-language terms used
- **Product** — `docs/GLOSSARY.md` "Product" ($P$, states $S\times Q_{in}\times
  Q_{out}$). This PRD **generalizes** its state to $S\times Q_1\times\cdots\times
  Q_n$ and adds the C++ identifiers `ProductState` (now a struct),
  `agreeing_successor`, `build_product`. **Glossary update required.**
- **Consistency ($\cons$)** — `docs/GLOSSARY.md` "Consistency (cons)". Concept
  **unchanged**; `consistent` is re-implemented to delegate to the new atom (an
  implementation note in the entry, not a concept change).
- **Output agreement (`emits`)** — now in `docs/GLOSSARY.md`: the per-transducer
  λ-agreement atom (one conjunct of $\cons$, `\cref{def:consistency}`); $\cons$ is
  now `emits(t_in) && emits(t_out)`. NB `main.tex` has **no** `def:consistency` label;
  the "enabled"/skip notion is the unlabeled partiality note after
  `\cref{def:consistency}` (see the glossary-drift flag at handoff).
- **Observed / produced slice ($\Sigma_0/\Sigma_1$)**, **Letter**, **Cube** —
  `docs/GLOSSARY.md`, used as-is.
- **Goal DFA construction (`LtlfToDfa`)**, **Game solving (`SolveDfa`)**,
  **Controller verifier** — used as-is; unchanged by this refactor.

## Behaviour / semantics (from main.tex)
Preserve, verbatim, the invariants both call sites already satisfy:

1. **Enabled filter** (the skip rule of `\cref{def:consistency}` + its partiality note). A full letter $v$ is enabled
   at a product state iff, for **every** transducer $\tau_i$ in the product,
   $\delta_i$ and $\lambda_i$ are defined at $(q_i,v)$ **and** $v$ lies in the
   $\Sigma_1$ cube $\lambda_i(q_i,v)$ commits to; a non-enabled letter is
   **skipped** (contributes no product edge), the filter all methods share.
2. **$\cons$** (`\cref{def:consistency}`) is exactly the $\{\Tin,\Tout\}$ instance of that per-letter
   λ-agreement: $\cons=(v\cap\Iknown=\lambda_{in})\wedge(v\cap\Oknown=\lambda_{out})$.
   The new atom is one conjunct of it, per transducer. For the verifier's fourth
   component $T_C$ ($\Sigma_1=\Ofree$) the same atom is the $v\cap\Ofree=\lambda_C$
   check — **not** $\cons$ (since $\Ofree\notin V$), so it must **not** be folded
   into the `consistent` function.
3. **λ-agreement is $\Sigma_1$-agnostic** (consistency.cpp:17 comment): a full
   letter $v$ agrees with a committed cube `out` exactly when `(v & out) !=
   bddfalse`; the atom never needs to know which variables $\Sigma_1$ is.
4. **Goal automaton is deterministic and (post-`ltlf_to_dfa`) complete.** The
   product navigates it as a bare transition structure (acceptance ignored for
   $\delta$; `state_is_accepting` read separately for $F_P$), the
   `OutputLabeledTransducer::delta` idiom. $F_P=F_D\times Q_1\times\cdots$
   (`alg:dfa_product:final`): a product state is accepting iff its Goal component
   is.
5. **Non-empty-trace / partiality semantics** are unchanged — the atom returns
   false on a `nullopt` λ, `delta` `nullopt` skips, exactly as today
   (`\cref{def:consistency}` partiality note; glossary "Partial transducers — resolved").

## Interfaces & types

### New — `include/ltlf_ek/consistency.hpp` (the atom; cons delegates)
```cpp
// emits(t, q, v)  [glossary: "Output agreement (emits)"]: the per-transducer
// λ-agreement atom — one conjunct of cons (def:consistency), per transducer.
// λ-ONLY (no delta):
//   t.lambda(q,v) defined  &&  (v & *lambda) != bddfalse.
// nullopt λ (undefined) => false (non-enabled; def:consistency partiality note).
bool emits(const Transducer& t, unsigned q, bdd v);

// consistent is now exactly emits(t_in) && emits(t_out) — same def:consistency
// concept, same signature, no behaviour change; delta-definedness stays the
// caller's concern (glossary "Consistency" note).
bool consistent(const Transducer& t_in, unsigned q_in,
                const Transducer& t_out, unsigned q_out, bdd v);
```

### New — `include/ltlf_ek/product.hpp`
```cpp
// Product state over a Goal automaton × an ordered list of transducers.
// taus[i] is the state of the i-th transducer in the SAME order the caller
// passes the transducer list to build_product / agreeing_successor.
struct ProductState {
  unsigned goal;
  std::vector<unsigned> taus;
};  // needs operator< (lexicographic goal then taus) and operator== for map keys.

// Goal automaton δ as a bare (deterministic) transition structure — acceptance
// ignored. nullopt = no matching edge. Shared spelling of both files' dfa_delta.
std::optional<unsigned> goal_delta(const spot::twa_graph_ptr& goal, unsigned s,
                                   bdd v);

// Every full letter v ∈ 2^{I∪O} over io_vars, LSB-first in io_vars order — the
// accepted exponential baseline (\For of alg:dfa_product; symbolic build
// deferred). The CALLER owns io_vars ORDERING: the verifier lists Ifree first so
// a letter's low bits are its Ifree combo (see build_product edge indices).
std::vector<bdd> all_letters(const std::vector<int>& io_vars);

// Lazy per-letter core (also serves on-the-fly Methods 3.x later).
// For each τ_i: emits(τ_i, state.taus[i], v) AND δ_i defined; then goal_delta.
// Returns the successor ProductState iff v is enabled at `state`, else nullopt.
// goal_must_be_complete: if the transducer filter passes but goal_delta MISSES,
//   true  => throw std::runtime_error (DfaProduct's completeness invariant),
//   false => return nullopt (a legitimate non-agreement, the verifier's case).
// The goal edge is consulted ONLY after the transducer filter passes — so the
// throw fires exactly as DfaProduct's dfa_delta does today (enabled letters only).
std::optional<ProductState> agreeing_successor(
    const spot::twa_graph_ptr& goal,
    const std::vector<const Transducer*>& taus,
    const ProductState& state, bdd v, bool goal_must_be_complete);

// One product node: its Goal-acceptance flag and its agreeing edges. Each edge
// stores the letter's INDEX into `letters` (not the bdd): DfaProduct ORs
// letters[idx] into a per-dst guard; the verifier reads idx & ifree_mask for the
// Ifree combo — preserving the verifier's bitmask bucketing with zero extra bdd
// ops. Edge order follows `letters`; at most one edge per Ifree combo holds by
// λ-determinism (build_product does not enforce it — see Edge cases).
struct ProductNode {
  bool acc;
  std::vector<std::pair<std::size_t, ProductState>> edges;
};

// Eager driver: worklist BFS from `init`, calling agreeing_successor over
// `letters`, returning the whole reachable product as a neutral map. No visitor
// — both current consumers materialize the entire product anyway (DfaProduct a
// twa_graph, the verifier a global ν-fixpoint), so pull beats push.
std::map<ProductState, ProductNode> build_product(
    const spot::twa_graph_ptr& goal,
    const std::vector<const Transducer*>& taus,
    const ProductState& init, const std::vector<bdd>& letters,
    bool goal_must_be_complete);
```

### Optional shared validation helper (`product.hpp`)
Both call sites run the same preamble (formula APs ⊆ $\mathcal{I}\cup\mathcal{O}$;
all transducers share one `bdd_dict`). Extract once and reuse in both phases:
```cpp
// Throws std::invalid_argument on an out-of-universe AP or a dict mismatch.
void validate_product_inputs(const spot::formula& phi,
                             const VariablePartition& vars,
                             const std::vector<const Transducer*>& taus);
```

### Consumers (positional `taus`, unchanged externally)
- `DfaProduct::synthesize`: `taus = {&t_in, &t_out}`, `goal_must_be_complete =
  true`. Post-process the map: for each node, group `edges` by destination
  (`guard |= letters[idx]`), emit one `new_edge(src, dst, guard, mark)` with
  `mark = acc ? {0} : {}`; then `solve_dfa`. Public signature and result
  unchanged.
- `verify_controller`: `taus = {&t_in, &t_out, &t_c}`, `goal_must_be_complete =
  false`, `letters = all_letters(io_vars)` with $\Ifree$ **first**. Rebuild its
  `StateInfo` from the map: `acc` copied; `edges[idx & ifree_mask] = {letters[idx],
  succ}` (keep first on the λ-determinism invariant); `has_dead_end` = some
  $\Ifree$ combo with no edge. `compute_bad` / `extract_witness` / the
  virtual-start split are **unchanged**.

## Implementation phases

- **Phase 1 — core + `consistent` delegation + DfaProduct migration.** Add the
  atom to `consistency.{hpp,cpp}` and re-implement `consistent` as
  `emits(t_in) && emits(t_out)`. Add `include/ltlf_ek/product.hpp` +
  `src/product.cpp` with `ProductState`, `goal_delta`, `all_letters`,
  `agreeing_successor`, `ProductNode`, `build_product`, and
  `validate_product_inputs`. Migrate `DfaProduct::synthesize` onto
  `build_product` + the group-by-dst post-process. **Green checkpoint:** the
  existing DfaProduct tests, the consistency tests, and the generated-corpus /
  differential oracle all pass unchanged; the tree compiles. `verify_controller`
  is untouched (still on its own product) and stays green. *Stubs nothing.*
- **Phase 2 — verify_controller migration.** Replace `build_product` (the
  verifier's own) + `agreeing_successor` locals with the shared core; rebuild
  `StateInfo` from the neutral map as above. Delete the now-dead duplicate
  helpers. **Green checkpoint:** verifier unit tests, controller-verifier
  oracle, and the metamorphic round-trip (`synthesize`→`verify_controller`) pass
  unchanged; the tree compiles.

Each phase leaves the tree compiling and independently testable.

## Edge cases
- **Empty `taus`** (`n = 0`): product is the Goal automaton alone; the filter is
  vacuously true; every letter with a defined `goal_delta` yields an edge. Not a
  real caller, but `build_product` must not crash (loop over zero transducers).
- **`goal_must_be_complete = true` + a miss on an enabled letter** ⇒ throw
  (DfaProduct's invariant; never fires because `ltlf_to_dfa` is complete).
- **`goal_must_be_complete = false` + miss** ⇒ that letter is a non-agreement,
  skipped (the verifier's `dfa_delta` returning `nullopt` today).
- **Partial transducers**: `emits` false on `nullopt` λ; `delta` `nullopt`
  skips — identical to current behaviour (`\cref{def:consistency}` partiality note).
- **λ non-determinism guard**: build_product records **all** agreeing edges; if
  two letters in the same $\Ifree$ combo agreed (a λ-determinism violation) the
  verifier keeps the first, exactly as its current `if (!info.edges[idx])` does.
- **Unrealizable / no edges**: nodes with empty `edges`; DfaProduct → `solve_dfa`
  reports `nullopt`; the verifier → dead-ends. Both paths unchanged.
- **Init state accepting / dead-end**: the verifier's virtual-start split (init
  Acc is irrelevant; a first-successor into Bad or a dead end ⇒ not ok) is
  preserved verbatim — this refactor does not touch it.
- **Edge/letter index coupling**: `ProductNode.edges` indices are into the exact
  `letters` vector passed to `build_product`; a consumer must use the **same**
  vector to resolve them (both do).

## Test oracles (for /test-writer)
- **Regression is the primary net** — this is behaviour-preserving. Every
  existing test must stay green with no expected-value edits: DfaProduct tests,
  consistency tests, controller-verifier tests, and especially the generated
  corpus oracle (differential vs Spot's `ltlfsynt` + metamorphic
  `synthesize`→`verify_controller` round-trip). A diff in any of these means the
  refactor changed behaviour.
- **`emits` unit fixtures**: λ defined & letter in cube ⇒ true; λ defined &
  letter outside cube ⇒ false; λ `nullopt` ⇒ false. **`consistent` unchanged**:
  its current fixtures must pass verbatim (proves delegation is behaviour-preserving).
- **`agreeing_successor` fixtures**: a small Goal DFA × 2 transducers — an
  enabled letter yields the right successor tuple; a $\cons$-violating letter ⇒
  `nullopt`; a partial-δ letter ⇒ `nullopt`; a goal miss ⇒ throw when
  `goal_must_be_complete`, `nullopt` otherwise.
- **`build_product` fixtures**: on a tiny fixed product, assert the reachable
  `ProductState` set, each node's `acc`, and its `edges` (index → successor).
- **Cross-check (metamorphic)**: `build_product` with `taus = {t_in, t_out}` +
  group-by-dst must reproduce DfaProduct's old arena up to state renumbering —
  covered transitively by the unchanged `solve_dfa` outcomes in existing tests.

## Open theory questions touched
None new. This is a code-level generalization faithful to existing definitions
($\cons$ = `\cref{def:consistency}`, `alg:dfa_product`); the 4-way verifier
product it also unifies is already code-only (`docs/prd/controller-verifier.md`).
`/theory-review` should confirm the per-transducer atom conjunction reduces to
$\cons$ for $\{\Tin,\Tout\}$ and to the verifier's filter for
$\{\Tin,\Tout,T_C\}$, and that folding the goal-miss policy behind
`goal_must_be_complete` preserves each method's semantics. Does **not** touch the
tracked stubs (`FP`, aggregation overwrite, on-the-fly solving,
trace-termination).

## Definition of done
- `include/ltlf_ek/product.hpp` + `src/product.cpp` added; the atom in
  `consistency.{hpp,cpp}` with `consistent` delegating; both consumers migrated
  (Phase 1 DfaProduct, Phase 2 verify_controller); the duplicate `all_letters` /
  `dfa_delta` / worklist / validation removed from both.
- Tree compiles green; all pre-existing tests + oracles pass unchanged; new unit
  tests for `emits`, `agreeing_successor`, `build_product` land.
- `docs/GLOSSARY.md` updated: "Product" C++ column gains `ProductState` /
  `agreeing_successor` / `build_product`; a **new** entry for the atom (final
  name via /glossary); "Consistency" gets the delegation note.
- `/theory-review` confirms the generalization is faithful.

## Developer comments / PRD disagreements

**2026-07-08 — Phase 1 landed, no PRD deviations.** Implemented exactly as
specified:
- `emits(t, q, v)` added to `consistency.{hpp,cpp}`; `consistent` now
  delegates to `emits(t_in, ...) && emits(t_out, ...)` — same signature, same
  behaviour (regression suite is the proof).
- `include/ltlf_ek/product.hpp` + `src/product.cpp`: `ProductState` (with free
  `operator<`/`operator==`, lexicographic on `(goal, taus)`), `goal_delta`,
  `all_letters`, `agreeing_successor`, `ProductNode`, `build_product`,
  `validate_product_inputs` — signatures and semantics as pinned (edges keyed
  by index into the caller's `letters`; `goal_must_be_complete` gates
  throw-vs-nullopt on a goal miss, consulted only after the transducer filter
  passes).
- `DfaProduct::synthesize` migrated onto `build_product` + the group-by-dst
  post-process (`guard |= letters[idx]`, `mark = acc ? {0} : {}`), then
  `solve_dfa`. Public signature and result unchanged; the local
  `all_letters`/`dfa_delta` duplicates removed. Also swapped in
  `validate_product_inputs` for the hand-rolled AP/dict preamble (PRD's
  "Optional shared validation helper" — taken as in-scope for Phase 1 since
  DfaProduct is the phase's only consumer).
- `src/product.cpp` added to `CMakeLists.txt`.

**Green checkpoint met:** `cmake --build build -j` compiles clean; full
`ctest` run is 189/189 passing (DfaProduct, consistency, and the
generated-corpus/differential/metamorphic oracles all green with no expected-
value edits). `verify_controller` (`src/verify_controller.cpp`) was not
touched and still runs its own local product — Phase 2 scope.

**Glossary gate ticked** — `emits`, `ProductState`, `agreeing_successor`,
`build_product` were already present in `docs/GLOSSARY.md` ("Output agreement
(emits)" and "Product" entries) before this session; no new identifiers were
introduced beyond what's documented there. `tests`, `code-review`, and
`theory-review` gates are left unchecked for `/test-writer`, `/code-review`
+ `/code-reviewer`, and `/theory-review` respectively.

**Next:** Phase 2 (`verify_controller` migration onto the shared
`build_product`/`agreeing_successor` core) is not started.

**2026-07-08 — `/test-writer`, Phase 1 unit coverage.** Added the "Test
oracles" unit fixtures for the three new functions, per the PRD:
- `emits` (`tests/consistency_test.cpp`): 3 new `TEST(Emits, ...)` cases reuse
  the existing `Consistent` fixture (`t_in` commits `b<->a`, `t_out` commits
  `e:=true`) one transducer at a time — letter agrees with the committed cube
  ⇒ true, disagrees ⇒ false, `nullopt` λ ⇒ false. The pre-existing `Consistent`
  fixtures were left untouched and still pass, confirming `consistent`'s
  delegation to `emits(t_in) && emits(t_out)` is behaviour-preserving.
- `agreeing_successor` / `build_product` (new `tests/product_test.cpp`, added
  to the `unit_tests` target): a hand-built 2-state Goal DFA (`o`-guarded
  0→1 accepting sink) crossed with a trivial transducer and a 2-state
  Oknown transducer (commits `o:=true`/`o:=false`) gives 5
  `agreeing_successor` cases (enabled → correct successor, cons-violating →
  `nullopt`, partial-δ → `nullopt` via a dedicated delta-only-partial
  fixture, goal-miss → throw when `goal_must_be_complete`, `nullopt`
  otherwise) and 2 `build_product` cases (the tiny fixed product's full
  reachable-state set / `acc` / `edges`, plus the PRD's `n=0` empty-`taus`
  edge case). All values hand-traced against the fixture, not re-derived from
  another oracle.
- Build green, full suite **199/199** (up from the Phase 1 checkpoint's
  189/199 — the 10 new tests above), no existing test's expected value
  touched.
- Out of scope, left for Phase 2 / later: `verify_controller`'s still-local
  product (untouched), the generated-corpus/differential/metamorphic oracles
  (already green, not extended), `DfaProduct`-specific tests (unchanged).
- `tests` gate ticked above. `code-review` and `theory-review` remain open.

**2026-07-08 — Phase 2 landed (`src/verify_controller.cpp` migration), one
naming deviation.** Implemented as specified:
- Deleted the file's local `dfa_delta`, `all_letters`, `agreeing_successor`,
  and local `build_product` — all superseded by `include/ltlf_ek/product.hpp`.
- Added a reshaping helper, named `build_verifier_graph` per the PRD's
  recommendation (**not** a local `build_product`, to avoid shadowing
  `ltlf_ek::build_product` with different semantics). It calls
  `ltlf_ek::build_product(dfa, {&t_in, &t_out, &t_c}, init, letters,
  goal_must_be_complete=false)` with `io_vars` Ifree-first (unchanged AP
  registration order) and reshapes the returned
  `map<ltlf_ek::ProductState, ltlf_ek::ProductNode>` into the file's
  tuple-keyed `map<ProductState, StateInfo>`: `acc` copied, each edge bucketed
  at `idx & ifree_mask` with keep-first tie-break (reproduces the old
  ordering, since `build_product`'s BFS appends edges in ascending
  `letters`-index order), `has_dead_end` = any Ifree combo with no edge.
- **Deviation from the PRD's shadowing note:** the PRD says the local
  `using ProductState = std::tuple<...>` (nested anonymous namespace) "must
  keep shadowing `ltlf_ek::ProductState` for the unqualified uses in
  `StateInfo`/`compute_bad`/`extract_witness`." That shadowing does hold
  *inside* the anonymous namespace (unqualified lookup finds the inner
  `using` first and never considers the outer `ltlf_ek::ProductState`
  struct). It does **not** hold in `verify_controller`'s own body: that
  function lives directly in `namespace ltlf_ek`, and an anonymous
  namespace's members are injected into the enclosing namespace as if by a
  `using`-directive, so an unqualified `ProductState` there is genuinely
  **ambiguous** between the tuple alias and the `product.hpp` struct (a hard
  compile error, not a silent wrong pick). Fixed by spelling
  `std::tuple<unsigned, unsigned, unsigned, unsigned>` explicitly for the
  local `init` in `verify_controller`, and `auto` for the `graph`/`bad`
  locals there — `build_verifier_graph`/`compute_bad`'s return types are
  unaffected since those functions are still defined (and their signatures
  resolved) inside the anonymous namespace where the shadow is unambiguous.
  Behaviour is unchanged; this is purely a lookup-visibility fix the PRD's
  phrasing didn't anticipate.
- Left the hand-rolled validation preamble (AP-⊆-I∪O, shared-`bdd_dict`)
  untouched per the locked decision in the plan (`jaunty-coalescing-dijkstra.md`):
  do not swap in `validate_product_inputs` for Phase 2, zero message drift.
  `compute_bad`, `extract_witness`, the virtual-start / non-empty-trace
  split, and `controller_as_transducer` are byte-for-byte unchanged.
- **Green checkpoint met:** `cmake --build build -j` compiles clean; full
  `ctest` is **199/199** passing, no expected value moved (verifier unit
  tests, controller-verifier oracle, and the metamorphic
  `synthesize`→`verify_controller` round-trip are the behaviour-preservation
  proof).
- `code-review` and `theory-review` gates reset to `[ ]` above (Phase 2's
  semantic change to `verify_controller` needs its own pass). Not committed —
  left for review first.
- Out of scope, left for follow-up: new tests for `build_verifier_graph`
  (`/test-writer`), the repo-wide dangling-`def:consistency` citation sweep, any
  `main.tex` change.
