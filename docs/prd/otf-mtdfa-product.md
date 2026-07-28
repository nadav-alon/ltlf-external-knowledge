# PRD: `OtfMtdfaProduct` — Method 3.1, the on-the-fly DFA product (mtdfa representation)

**Status:** draft
**Interface:** implements `Synthesis` as `OtfMtdfaProduct` (the **mtdfa**
*Representation* cell of Method 3.1); adds the public wrapper type
`ForwardProgression` (realizing `\cref{alg:fp}`), the fused builder
`otf_product_to_mtdfa`, and the `--otf-mtdfa-product` CLI flag. Phase 2 adds the
fused construct-and-solve path `otf_solve_fused` behind an `--otf-solve` knob.
**Recommended workflow:** **sequential** — freeze confidence is *tentative*.
The `Synthesis`-level surface is high-confidence (forced by the base class,
identical to `MtnfaProduct`) and `otf_product_to_mtdfa` falls straight out of the
landed `mtnfa_product_to_mtdfa`, but `ForwardProgression` is a **new type invented
here**, and that is exactly what made `docs/prd/mtnfa.md` re-freeze twice. So
`/developer` lands Phase 1 first and `/test-writer`'s **per-function** tests bind
after. The **domain oracles parallelize regardless** — they bind to
`synthesize` and the math, never to internals.
**main.tex ref:** §`otf` (`\cref{otf}`) and §`otfdfa` (`\cref{otfdfa}`),
`\cref{alg:otfdfa_product}` — in particular lines `alg:otfdfa_product:uncons`
(the $\cons$ filter), `:fp` (`\algname{FP}`), `:in_succ`/`:out_succ`,
`:states:insert`, `:final_insert` ($F_P$) and `:solve` (`\algname{SolveDfa}`) —
plus `\cref{alg:fp}` (the `FP` stub this realizes), `\cref{def:consistency}`
(§203), and the `\na` pair at `main.tex:333`/`main.tex:335`.

**Gates:**
- [x] glossary        — *closed 2026-07-28* (`/glossary`, this commit). Mostly
      **corrections**, not additions: *Forward progression* and *Canonical
      representative* both named C++ functions that this design proves must never
      exist (`progress(psi, w)`, `canonical(psi)`) and were rewritten. Added
      `otf_product_to_mtdfa` to *Product*, `OtfMtdfaProduct` to the *five methods*
      table (explicit cell marked unbuilt), updated *Representation*, the
      CLI-flag-wart paragraph (the wart went live exactly where it was predicted
      to), and three *Open theory questions* entries — including a stale
      "only Method 2 has one today" left behind by the `MtnfaProduct` landing.
      **No code renames implied:** both retired names were reserved-only and
      never implemented (grep-verified). `otf_solve_fused` deliberately NOT
      minted — Phase 2's name is not canonical yet.
- [ ] tests           — unit + oracle coverage
- [ ] code-review     — domain (/code-reviewer) + generic (/code-review)
- [ ] theory-review   — code ↔ math faithfulness vs main.tex

## Goal

Build **Method 3.1**: the product of the Goal automaton with $\Tin,\Tout$
constructed **on the fly** by forward progression, so that only the goal states
reachable *under* the $\cons$ filter are ever built — instead of materializing the
whole Goal automaton first and intersecting afterwards, as Method 2 does
(`docs/prd/mtdfa-product.md`). The method is realized in the **mtdfa**
*Representation*, which is what the `\na` at `main.tex:335` anticipates
(*"This likely requires adjusting the definitions for MTDFA usage"*); the
explicit cell of Method 3.1 is deliberately left unbuilt.

The scientific question this exists to answer is **"does laziness pay?"**, and it
is asked against `MtdfaProduct` — which beat `MtnfaProduct` on **every** instance
measured, by 9×–3000×, in the `docs/prd/mtnfa-product.md` benchmark, making it the
standing champion to beat. Holding the solver, the
substrate and the state-interning rule constant, the only difference between
`MtdfaProduct` and `OtfMtdfaProduct` is *when* goal states get built. Method 1
asked the analogous question about late determinization and the answer was **no**;
this PRD is built so that a negative answer here is equally attributable.

Phase 2 additionally discharges the *other* `\na` (`main.tex:333`): on-the-fly
product construction that still solves the game at the end is *"missing a hanging
fruit optimization"*. It lands as a **knob**, not a replacement, so Phase 1's
build-then-solve path stays live as Phase 2's internal differential oracle and as
the control arm of the measurement.

## Ubiquitous-language terms used

Existing, used as-is: *Goal formula* ($\varphi$), *Inputs / Outputs*,
*Free / Known inputs / outputs* ($\Ifree,\Iknown,\Ofree,\Oknown$), *Governed
variables* ($\mathcal{V}$), *Closed universe of APs*, *Turn order*, *Controller*,
*Transducer*, *Transition function (delta)* / `delta_edges`, *Output function
(lambda)*, *Output agreement* (`emits` / `emits_region`), *Consistency* ($\cons$),
*Letter*, *Cube*, *MTDFA*, *Product*, *Game solving* (`solve_mtdfa`),
*Representation*, *Canonical benchmarking stage*, *Controller verifier*,
*Generated corpus*.

**Glossary gaps — `/glossary` must run before `/developer`.** Two existing
entries are made *wrong* by this design, not merely extended:

- **Forward progression** currently gives the C++ name `progress(psi, w)` — a
  **per-letter** function. No such function exists or will: `FP` is realized as a
  **whole-alphabet row** (one MTBDD covering every letter at once). Rewrite the
  C++ column to `ForwardProgression::progress_row(psi)` and record the per-letter
  form as *derived* (`bdd_restrict(row, v)` + `decode`), not as an API.
- **Canonical representative** currently gives `canonical(psi)`. There is no such
  function and there must not be one: $[\psi]$ is **Spot's** propositional-
  equivalence representative (`propeq_representative`, `ltlf2dfa.cc:825`), which
  the translator applies when it *mints a terminal*, so `terminal_to_formula(t)`
  already returns $[\psi]$. Rewrite the entry to say the canonicalization is
  inherited and that re-canonicalizing on top would de-sync us from the baseline.

New terms to add:

- **`ForwardProgression`** — the project's `\cref{alg:fp}` realization; the single
  seam over Spot's semi-internal `ltlf_translator`.
- **`OtfMtdfaProduct`** — new cell in the *five methods* table: Method 3.1, mtdfa
  column (the explicit column stays `—`).
- **`otf_product_to_mtdfa`** — under *Product*: Method 3.1's fused build, the
  third sibling of `build_product_symbolic` / `mtnfa_product_to_mtdfa`.
- **`otf_solve_fused`** (Phase 2) — under *Game solving*: construction and solving
  fused, early-aborting on `backprop_graph`.
- The **CLI-flag wart** entry needs updating: the glossary predicted it "becomes
  live at Method 3". It did, and the resolution taken here is to keep the flat
  shape because the automaton type stays *in the name* (`--otf-mtdfa-product`),
  leaving `--otf-dfa-product` reserved-not-wired for a future explicit build.

## Behaviour / semantics (from main.tex)

`\cref{alg:otfdfa_product}` is a worklist BFS. Everything below is that algorithm
unless marked **DEVIATION**, and every deviation carries its justification.

**I1 — the state.** A product state is $\langle[\psi], q_{in}, q_{out}\rangle$
(`alg:otfdfa_product:stack_def`), initial
$\langle[\varphi], q_{in,0}, q_{out,0}\rangle$. Generalized to $n$ transducers as
`taus`, the way `build_product*` and `mtnfa_product_to_mtdfa` already are.

**I2 — the $\cons$ filter.** A letter $v$ with
$\lnot\cons(q_{in},q_{out},v)$ is **skipped**, contributing no transition
(`alg:otfdfa_product:uncons`, `\cref{def:consistency}` §203). A missing $\delta$
or $\lambda$ is equivalent to an inconsistent letter (the partiality clause), so a
$\lambda$-undefined transducer state contributes nothing. Applied symbolically as
the region $\bigwedge_k$ `emits_region(q[k])`, exactly as
`mtnfa_product_to_mtdfa` does.

**I3 — the successor.** $([\psi'],b) \gets \algname{FP}([\psi],v)$
(`:fp`), $q'_k \gets \delta_k(q_k,v)$ (`:in_succ`, `:out_succ`).

**I4 — acceptance is transition-based. DEVIATION from `:final_insert`.**
`\cref{alg:otfdfa_product}` inserts the *successor state*
$\langle[\psi'],q'_{in},q'_{out}\rangle$ into $F_P$, making $F_P$ a set of
**states**. That is **lossy**: $b$ is not a function of $[\psi']$. Verified
against the linked libspot:

| source $[\psi]$ | letter $v$ | $[\psi']$ | $b$ |
|---|---|---|---|
| `G(a -> Xb)` | `{!a,!b}` | `G(a -> Xb)` | $\top$ |
| `X[!] G(a -> Xb)` | `{!a,!b}` | `G(a -> Xb)` | $\bot$ |

If both transitions occur in one product, a state-keyed $F_P$ marks that state
accepting and the product **over-accepts**. This is the same hazard the author
already flagged for Method 3.2 (`main.tex:434`, the "aggregated final-state
overwrite" `\na`), but it bites the **un-aggregated** method too. The mtdfa
substrate encodes acceptance on the transition ($2d+b$ terminals, see *MTDFA*), so
this implementation is **strictly more faithful to $\text{LTL}_f$ than the
pseudocode**. Flagged for `/theory-review` as a **doc-bug** candidate.

**I5 — the sinks. DEVIATION (no counterpart in main.tex).**
`\cref{alg:otfdfa_product}` has no sinks; a goal row has three leaf kinds.
- `bddfalse` ($\varphi$ irrevocably violated) ⇒ the rejecting sink. Language-exact
  — a dead product state is never accepting either way.
- terminal $t$ ⇒ intern the successor product state, emit $2j+(t\,\&\,1)$.
- `bddtrue` ($\varphi$ irrevocably satisfied) ⇒ **collapse to the accepting sink;
  stop exploring that branch.** This is the method's largest pruning win and is
  what Spot's own construction does.

  **What the collapse costs, stated precisely:** $L(P)$ then accepts continuations
  that violate $\cons$ *after* the satisfaction point, so $L(P)$ is
  "traces satisfying $\varphi$, $\cons$-enforced up to irrevocable satisfaction"
  rather than $\cons$-enforced throughout. **Realizability verdicts and
  `verify_controller` are unaffected**: the accepting transition *into* the sink is
  what wins the game and it is marked accepting in either encoding, $\varphi$ holds
  on *every* continuation from a `bddtrue` leaf, and the extracted `Controller`
  only ever emits $\Ofree$. **Soundness of the collapse depends on
  system-controlled termination** (`main.tex:96`'s `\na`, the De Giacomo–Vardi
  reading this project already commits to) — if that reading changed, this
  deviation would have to be revisited. Consequence for `/test-writer`: **do not
  write a language-equality oracle on $P$ itself.**

**I6 — solve at the end (Phase 1).** $C \gets \algname{SolveDfa}(P)$
(`:solve`), realized as `solve_mtdfa(product, vars)`. Phase 2 fuses this; see
*Implementation phases*.

**I7 — $[\psi]$ and state interning.** Three layers, of which only the third is
ours to choose (all three are Spot's, `ltlf2dfa.cc`). **Line numbers below are
from the 2.15.1 release tarball** (`~/spot-2.15.1.tar.gz`), which is the version
actually linked — **not** from the `~/spot` checkout, which is 39 commits ahead
*and* carries uncommitted edits, and whose numbers are off by ~250 lines here:
1. **propeq representative** (`:825`) — light rewrites ($G\alpha\wedge\alpha\equiv
   G\alpha$, $F\alpha\vee\alpha\equiv F\alpha$, $(\alpha U \beta)\vee\beta\equiv
   \alpha U \beta$, …), then encoding the formula as a BDD over its maximal
   *temporal* subformulas as atoms, interned by that encoding. Applied when a
   terminal is minted, so `terminal_to_formula(t)` **already returns** $[\psi]$.
   **Do not re-canonicalize.**
2. **worklist dedup** (`:1848`) — subsumed by our own interning map.
3. **`fuse_same_bdds`** (`:1925`, Spot default **on**) — states with an identical
   row MTBDD are merged. **Applied componentwise here**: the product state is
   interned on `(row($[\psi]$), q)`. Sound (equal goal rows + equal transducer
   states ⇒ bisimilar) and it keeps the goal dimension merging *exactly as
   `MtdfaProduct`'s baseline does*, without which a state-count comparison would
   measure our interning rather than the method.

**I8 — turn order.** `require_turn_order_aps(vars, dict)` is a precondition of
`solve_mtdfa` and must be called in `synthesize` **before any automaton is built**
(the `MtnfaProduct::synthesize` order). A violation does not crash — it silently
reinterprets the game as Moore and returns a wrong "unrealizable".

## Interfaces & types

**Freeze confidence: tentative.** The `Synthesis` surface is forced by the base
class and `otf_product_to_mtdfa` mirrors the landed `mtnfa_product_to_mtdfa`, but
`ForwardProgression` is invented here and Phase 2's `otf_solve_fused` seam is the
least settled thing in this document. Judged *tentative* on the
`docs/prd/mtnfa.md` precedent: that PRD re-froze twice, and the distinguishing
feature was exactly this — **a new type being invented**.

### `include/ltlf_ek/progression.hpp` (new, Phase 1)

```cpp
// Forward progression (main.tex \cref{alg:fp}) --- the project's single seam
// over spot::ltlf_translator, whose own header marks it "Semi-internal ... Do
// not rely on the interface to be stable".  Confining it here means a Spot bump
// breaks ONE file, not the product BFS.
class ForwardProgression {
 public:
  // simplify_terms is pinned true (Spot's default) and deliberately NOT a
  // parameter: layer-1 of the [psi] canonicalization depends on it, and a
  // false setting would silently coarsen nothing and inflate state counts.
  explicit ForwardProgression(const spot::bdd_dict_ptr& dict);

  ForwardProgression(const ForwardProgression&) = delete;
  ForwardProgression& operator=(const ForwardProgression&) = delete;

  // FP over the WHOLE letter alphabet at once: the one-step successor MTBDD of
  // psi.  Leaves are bddfalse | bddtrue | a terminal 2*idx+b (see I5).
  // Memoized inside the translator, so repeated calls on the same formula are
  // free.  NOT const: it memoizes and may register APs on the dict.
  bdd progress_row(const spot::formula& psi);

  // Decode one terminal leaf value into ([psi'], b).  psi' is already the
  // propeq representative (I7.1).  Precondition: `terminal` came from
  // bdd_get_terminal on a leaf of a row THIS object produced.
  //
  // Implement as: { tr.terminal_to_formula(t), t & 1 }.
  //
  // TRAP, cost one debugging cycle while writing this PRD: Spot's
  // ltlf_translator::leaf_to_formula(int b, int v) is NOT the terminal decoder,
  // despite the name.  Its FIRST parameter is the leaf KIND, not the acceptance
  // bit: b==0 returns {ff,false}, b==1 returns {tt,true}, and only b>=2 falls
  // through to {terminal_to_formula(v), v & 1}.  So leaf_to_formula(0, t)
  // silently returns (false, false) for EVERY terminal -- a plausible-looking
  // wrong answer, not a crash.  Handle the two constant leaves before calling
  // anything, then use terminal_to_formula + (t & 1) directly.
  std::pair<spot::formula, bool> decode(int terminal) const;

 private:
  spot::ltlf_translator translator_;
};
```

There is **no per-letter entry point**: the per-letter form of `\cref{alg:fp}` is
derived as `decode(bdd_get_terminal(bdd_restrict(progress_row(psi), v)))`, with
the two constant leaves handled first. `/test-writer` asserts at that granularity
(see *Test oracles*).

**Dropped from the interview sketch:** a `hand_over_aps` member. It is
unnecessary — the product registers every AP of `vars.universe()` with the output
`spot::mtdfa` as owner *before* building any row, exactly as
`mtnfa_product_to_mtdfa` does, and $\varphi$'s APs are a subset of the universe
(guaranteed by `validate_product_inputs`). That covers every variable the rows
reference, so the translator's destructor
(`dict_->unregister_all_my_variables(this)`, `ltlf2dfa.cc:749`) cannot pull a
variable out from under the returned automaton.

### `include/ltlf_ek/otf_mtdfa_product.hpp` (new, Phase 1)

```cpp
// Method 3.1's product: the cons-filtered product of the Goal automaton with the
// knowledge transducers, with the Goal side built ON THE FLY by forward
// progression --- alg:otfdfa_product, fused so that no Goal automaton object
// ever exists.  Sibling of mtnfa_product_to_mtdfa (which fuses cons with subset
// determinization); this one fuses cons with the Goal CONSTRUCTION itself.
//
// `phi`  : the Goal formula.
// `taus` : the knowledge transducers, T_in then T_out --- the same n-transducer
//          generalization build_product* uses; the product state carries one
//          state per element.
// `vars` : the closed AP universe; supplies the output mtdfa's `aps`.
// `dict` : the shared bdd_dict (phi carries none, unlike mtnfa's Mtnfa::dict).
//
// Preconditions:
//   - every tau and `vars` share `dict`   (validate_product_inputs)
//   - phi's APs are a subset of vars.universe()
//   - every tau is deterministic (delta_edges guards pairwise disjoint) ---
//     CHECKED, throws std::runtime_error, same wording as
//     mtnfa_product_to_mtdfa / build_product_symbolic
// Does NOT check the Turn order contract: that is solve_mtdfa's precondition,
// discharged by OtfMtdfaProduct::synthesize.  nullptr is NEVER returned.
spot::mtdfa_ptr otf_product_to_mtdfa(const spot::formula& phi,
                                     const std::vector<const Transducer*>& taus,
                                     const VariablePartition& vars,
                                     const spot::bdd_dict_ptr& dict);

// Method 3.1 (main.tex §otfdfa) in the mtdfa Representation.  A method, not a
// representation variant of an existing one --- the fifth row's first cell.
class OtfMtdfaProduct final : public Synthesis {
 public:
  // otf_solve (Phase 2): fuse game solving into the construction and abort as
  // soon as the initial state is determined.  Default OFF --- the proven
  // build-then-solve path stays the default until the benchmark says otherwise,
  // the same discipline MtdfaProduct's `minimize_mtdfa` knob follows.
  explicit OtfMtdfaProduct(bool otf_solve = false);

  std::optional<Controller> synthesize(const spot::formula& phi,
                                       const VariablePartition& vars,
                                       const Transducer& t_in,
                                       const Transducer& t_out) override;
 private:
  bool otf_solve_;
};
```

### Phase 2 addition (same header) — **least settled; expect revision**

```cpp
// Construction and solving fused: the BFS feeds a spot::backprop_graph as it
// discovers rows and stops as soon as the initial state is determined.
// nullopt = unrealizable.
std::optional<Controller> otf_solve_fused(const spot::formula& phi,
                                          const std::vector<const Transducer*>& taus,
                                          const VariablePartition& vars,
                                          const spot::bdd_dict_ptr& dict);
```

### CLI wiring

`--otf-mtdfa-product` → `OtfMtdfaProduct`. `kMethodFlags`
(`src/ltlf_ek_synth.cpp:58`) gains `"otf-mtdfa-product"`; `make_synthesis_method`
(`src/cli.cpp:82`) gains the case and **drops nothing** — `"otf-dfa-product"`
stays in `kRecognisedNotWired`, still meaning the unbuilt *explicit* Method 3.1.
Phase 2 extends `make_synthesis_method(method_flag, minimize_mtdfa, otf_solve)`
with a defaulted third parameter and adds the `--otf-solve` flag.

**If implementation proves this contract wrong:** that is a PRD-change event —
update this section and propagate to any in-flight test branch; the developer does
not silently re-shape the interface on its own branch.

## Implementation phases

### Phase 1 — the fused on-the-fly product, solved at the end

Lands `ForwardProgression`, `otf_product_to_mtdfa`, `OtfMtdfaProduct` (knob
present but always `false`), CLI flag, bench spans.

**The BFS, pinned.** Mirror `src/mtnfa_product.cpp` structurally; the goal part of
the key changes from a state subset $R$ to a row BDD.

```
Key { bdd row; std::vector<unsigned> q; }        // ordered by (row.id(), q)
```

Keying on the **bdd object** (not a bare id) is load-bearing twice over: it is the
fuse of I7.3, and holding the handle is what stops BuDDy recycling the id — the
`docs/prd/mtnfa.md` F4 hazard, and the same reason Spot's own `bdd_to_state` is
`unordered_map<bdd, int, bdd_hash>`.

0. Register every AP of `vars.universe()` on the output `spot::mtdfa` **first**
   (ownership, above); set `out->aps` sorted by formula id.
1. Seed index 0 with `Key{fp.progress_row(phi), q0}`, `q0[k] =
   taus[k]->initial_state()`. Index 0 must be the initial state — `solve_mtdfa`
   and Spot both assume it.
2. Dequeue `Key{row, q}`. Compute `cons = AND_k taus[k]->emits_region(q[k])`.
   If `cons == bddfalse`, push `bddfalse` as this state's row and continue — no
   letter is consistent here, which is the partiality clause, not an error.
3. Otherwise take the cartesian product of `taus[k]->delta_edges(q[k])`
   (the `ForEachCombination` recursion), seeded with `cons` and short-circuiting
   on `bddfalse`. Check per-tau guard disjointness on the **raw** guards and
   **throw** on overlap.
4. Per combination `(g, d)`: **mask before walking** —
   `row_g = bdd_ite(g, row, bddfalse)`. This is not an optimization; walking the
   unmasked `row` interns successor states for branches occurring only *outside*
   `g`, where the true successor vector is a different `d`. Those states are
   unreachable, so the language is unaffected — but they get enqueued and
   expanded, silently destroying reachability pruning, which is this method's
   entire selling point. (This is the PRD-change event `docs/prd/mtnfa-product.md`
   recorded on 2026-07-27; it is pre-paid here.) Note `bdd_ite`, **not** `&` —
   `&` is not meaningful on multi-terminal BDDs.
5. Relabel `row_g` recursively (`bdd_var`/`bdd_high`/`bdd_low`, terminal check),
   memoized on `node.id()` with a **fresh memo per call** (F4 again — a memo
   hoisted to loop scope to "amortize" would be unsound, not a tweak):
   `bddfalse -> bddfalse`; `bddtrue -> bddtrue` (I5); terminal `t` ->
   `decode(t)` gives `(psi', b)`, intern `Key{fp.progress_row(psi'), d}` as index
   `j`, emit `bdd_terminalpp(2*j + b)`.
6. Accumulate `row_out = bdd_ite(g, relabelled, row_out)` across combinations
   (exact because combination guards are pairwise disjoint, per step 3).
7. Push `row_out`; assert its index equals the dequeue index.

**Determinism.** No randomness anywhere: the worklist is FIFO, the interning map
is a `std::map` with a total order on `(row.id(), q)`, `delta_edges` returns a
fixed order, and BDD operations are canonical. The same
`(phi, vars, t_in, t_out)` on the same `bdd_dict` must produce a byte-identical
`spot::mtdfa` across runs — assert this rather than assuming it, since `row.id()`
ordering makes state *numbering* depend on BuDDy node allocation, which is stable
within a process but is the one place drift could enter.

**Phase 1 behaviour of the knob.** `OtfMtdfaProduct(true)` must
`throw std::logic_error` in Phase 1 ("otf_solve not implemented until Phase 2"),
not silently fall back to the build-then-solve path — a silent fallback would make
Phase 2's differential oracle vacuously pass.

**No final remap pass.** Spot needs one (`ltlf2dfa.cc:2035`) because it decides
fusing at *expansion* time, after terminals were already minted against formula
ids. Here the row is computed at *discovery*, so the final index is known when the
terminal is emitted — one pass, as in `mtnfa_product_to_mtdfa`. In Phase 1 this
costs nothing: every discovered state is expanded anyway.

**Green checkpoint:** compiles; `ctest` green; `--otf-mtdfa-product` runs
end-to-end; cross-method verdict agreement with `MtdfaProduct` and `DfaProduct` on
the generated corpus; `verify_controller` accepts every controller produced;
corpus differential against `ltlfsynt` passes; bench emits exactly
`product_construction` and `game_solving`.

### Phase 2 — fused construct-and-solve, behind `--otf-solve`

Lands `otf_solve_fused` + the knob + CLI flag. The BFS of Phase 1 additionally
feeds a `spot::backprop_graph` (`spot/twaalgos/backprop.hh`, public in 2.15.1 and
incremental by design: `new_state(owner)`, `new_edge(src,dst)` returning *"true if
the status of src is now known"*, `freeze_state(state)` = *"call once the
successors of a state have all been declared"*, plus a per-state counter of
unknown successors). Model the node-per-MTBDD-node mapping on
`mtdfa_to_backprop`, which is what gives linear-time resolution; node ownership
follows the variable's controllability, which is **precisely where the *Turn
order* obligation becomes load-bearing again**.

Stop as soon as `is_determined(initial)`. Unrealizable ⇒ return `nullopt`
immediately — the largest single win available.

**DEFERRED TO PHASE 2's OWN GRILL, with rationale.** The precise
backprop-node mapping — one node per MTBDD node vs per product state, how
terminals and the two constant leaves become nodes, exactly when `freeze_state`
fires for a row, and whether to hand-roll the incremental feed at all rather than
call the **public** `mtdfa_to_backprop(dfa, early_stop=true)` on the product as it
grows — is **not** pinned here, and `/developer` must not guess it. Two reasons
this is a deliberate deferral rather than a hole: the choice wants Phase 1's
measured numbers to justify it (if construction is not the bottleneck, the whole
phase is moot), and settling it needs a read of `mtdfa_to_backprop`'s **body**,
which means unpacking `~/spot-2.15.1.tar.gz` — **not** reading `~/spot`, which is
both a `2.15.1.dev` checkout **39 commits ahead of the linked library** *and*
carries an **uncommitted local edit** to `ltlf2dfa.cc` (a `domain_knowledge`
parameter that is in no commit and does not match its own committed header).
Phase 2 therefore **starts with its own grill**, not with code.

**Two things Phase 2 must decide, flagged now rather than discovered:**

1. **Partial product.** With early abort, discovered-but-unexpanded states have no
   row. Filling them with `bddfalse` before strategy extraction is **sound** —
   making unexplored states losing can only hurt the system, and the system
   already won without them — but it is an invariant to state and test, not to
   assume.
2. **Interning may have to move.** Phase 1 keys on `(row, q)`, which forces
   computing `progress_row(psi')` at *discovery*. Under early abort that is wasted
   work for states never expanded. The fix, if it measures, is Spot's own
   two-map scheme: intern on the **propeq formula** at discovery, fuse on the
   **row** at expansion — which then *reintroduces* the final terminal-remap pass
   Phase 1 avoids (`ltlf2dfa.cc:2035`), because a terminal is minted before its
   destination's fusing is known. This is a real trade, not an oversight: measure
   before taking it, and if taken, it is a PRD-change event.

**Green checkpoint:** compiles; `ctest` green; **verdicts identical to Phase 1**
(`--otf-solve` on vs off) across the whole generated corpus; states expanded with
the knob on is ≤ states expanded with it off, on every corpus case;
`verify_controller` accepts controllers from both paths.

## Edge cases

- **$\varphi$ irrevocably satisfied at once** (`progress_row(phi) == bddtrue`,
  e.g. $\varphi=\mathtt{1}$): state 0's row is the $\cons$ region relabelled to
  `bddtrue`. Realizable iff some letter is $\cons$-consistent.
- **$\varphi$ unsatisfiable** (`progress_row(phi) == bddfalse`): state 0's row is
  `bddfalse`; `solve_mtdfa` reports unrealizable via its documented test
  (`num_roots() == 0 || states[0] == bddfalse`).
- **$\cons$ dead at the initial state** (no letter consistent — e.g. a
  $\lambda$-undefined $q_{in,0}$): row `bddfalse`, verdict **unrealizable**.
  Note this is arguably vacuous-true under `\cref{def:probDefTransducer}` (no trace
  agrees, so *every* agreeing trace satisfies $\varphi$). **Do not resolve here** —
  it is pre-existing and shared with `MtdfaProduct`/`MtnfaProduct`, so the binding
  requirement is only that all three **agree**, which is directly testable.
- **Partial transducers** — a missing $\delta$ or $\lambda$ is an inconsistent
  letter (`\cref{def:consistency}` partiality clause); `emits_region` is
  `bddfalse` where $\lambda$ is undefined and `delta_edges` simply omits uncovered
  letters, so both fall out of the symbolic filter with no special case.
- **Non-deterministic transducer** (overlapping `delta_edges` guards) —
  `std::runtime_error`, not an assert: `Transducer` is a public virtual interface,
  so a violating subclass would otherwise get a silently wrong language under
  `NDEBUG`.
- **Empty $\Ifree$ / empty $\Oknown$ / empty universe** — no special case; the
  cartesian product degenerates to one combination and `cons` to `bddtrue`.
- **Turn-order violation** — `require_turn_order_aps` throws
  `std::invalid_argument` before anything is built.
- **Unrealizable** — `nullopt` from `solve_mtdfa` (Phase 1) or from early
  determination (Phase 2).

## Test oracles (for /test-writer)

**Unit — `ForwardProgression`** (bind to the frozen signature; these are the
per-function tests that wait for the sequential order):
- Per-letter fixtures via `bdd_restrict` + `decode`, taken from the probe run
  while writing this PRD and **already verified against the linked libspot**:
  `FP(b | Xc, {b})` → `bddtrue`; `FP(a, {!a})` → `bddfalse`;
  `FP(Xb, {!b})` → `(b, ⊤)` (**weak X**);
  `FP(X[!]G(a -> Xb), {!a,!b})` → `(G(a -> Xb), ⊥)`;
  `FP(G(a -> Xb), {!a,!b})` → `(G(a -> Xb), ⊤)`. The last two together are the
  **discriminating pair for I4** — same successor, different bit. Assert exactly
  that, so a regression to state-based acceptance cannot pass.
- **Isolated oracle:** drive `progress_row` alone in a plain worklist to rebuild
  the whole Goal MTDFA of $\varphi$ (no transducers, no $\cons$), and assert
  language-equivalence with `spot::ltlf_to_mtdfa(phi, dict)`. This is the
  `docs/prd/nfa-product.md` "isolated determinize oracle" idiom; give it a
  **discriminating negative control** (a mutated $\varphi$ that must fail).

**Unit — `otf_product_to_mtdfa`:** state 0 is the initial state; `out->aps` is
`vars.universe()` sorted; a `bddfalse` row for a $\cons$-dead state; the
overlapping-`delta_edges` throw; and a fusing assertion — two goal formulas with
equal rows and equal transducer states occupy **one** product state.

**Domain oracles (parallelize regardless — bind to `synthesize` and the math):**
- **Cross-method:** `OtfMtdfaProduct` vs `MtdfaProduct` vs `DfaProduct` vs
  `NfaProduct` must agree on realizability across the generated corpus. The
  `MtdfaProduct` pair is the load-bearing one: same solver, same substrate, same
  interning rule, so a disagreement isolates *laziness*.
- **Metamorphic round-trip:** `synthesize` → `verify_controller` accepts.
- **Corpus differential:** `--otf-mtdfa-product` vs `ltlfsynt`.
- **Bench-span shape:** exactly `product_construction` + `game_solving`, and
  **no** `automaton_construction`.
- **Phase 2 internal differential:** `--otf-solve` on vs off, same class, same
  inputs — identical verdicts, and states-expanded(on) ≤ states-expanded(off).

**Explicitly NOT an oracle:** language equality of $P$ against
$L(\varphi)\cap\{\cons\text{-consistent traces}\}$. The I5 accepting-sink collapse
makes it false by design. Cross-check **verdicts**, as the mtdfa route already
does (`docs/GLOSSARY.md` *Product*: the build-equivalence oracle does not apply to
mtdfa builds).

## Open theory questions touched

Flag, do not resolve — these are `/theory-review`'s.

- **`FP` is a `TODO`** (`\cref{alg:fp}`). This PRD *realizes* it as Spot's LTLf
  progression. `/theory-review` should confirm that is the intended `FP` — in
  particular the **weak `X`** reading (`FP(Xb, {!b}) = (b, ⊤)`), which matches
  this project's committed semantics but is not written in `main.tex`.
- **State-keyed $F_P$ is lossy** (`alg:otfdfa_product:final_insert`) — the new
  finding of I4. Likely a **doc-bug**; note it also strengthens the existing
  Method 3.2 `\na` at `main.tex:434` from "am I unsure?" to "demonstrably yes".
- **On-the-fly game solving** (`main.tex:333` `\na`) — Phase 2 implements it; the
  *definitional* half stays open.
- **MTDFA definitions for Method 3** (`main.tex:335` `\na`) — this PRD is that
  adjustment, made in code first. `main.tex` still commits to no MTDFA definition.
- **Trace-termination semantics** (`main.tex:96` `\na`) — newly load-bearing: the
  I5 accepting-sink collapse is sound *because* termination is system-controlled.
- **Governed-variable projection** (`main.tex:300` `\na`) and **Mealy-only
  signatures** (`main.tex:100` `\na`) — inherited unchanged via `solve_mtdfa` and
  the *Turn order* contract.

## Definition of done

- `/glossary` has run **first**: *Forward progression* and *Canonical
  representative* rewritten (their current C++ names are wrong), the five-methods
  table has `OtfMtdfaProduct` in the mtdfa column, `otf_product_to_mtdfa` is under
  *Product*, and the CLI-flag-wart entry records that the wart went live and how it
  was resolved.
- Phase 1 and Phase 2 both land, each at its own green checkpoint; the tree
  compiles and `ctest` is green after each.
- `--otf-mtdfa-product` (and `--otf-solve`) wired through `kMethodFlags`,
  `make_synthesis_method` and `--help`; `--otf-dfa-product` still
  reserved-not-wired.
- Unit tests + all domain oracles above pass.
- All four gates ticked with refs.
- **Benchmarked, and the verdict written down here** — against `MtdfaProduct`
  above all, with instance families where the $\cons$ filter prunes a lot and
  where it prunes nothing. As with `docs/prd/mtnfa-product.md`, a **negative**
  result is a real result and is recorded in this file, not buried.
