# PRD: MTDFA product — Method 2 over `spot::mtdfa`

**Status:** Phase 1 implemented — **green checkpoint not yet met, but the
blocker is gone** (merged 2026-07-15, `61b1ad0`; sink dropped 2026-07-15;
Spot >= 2.15 required 2026-07-15). `emits_dfa`, `turn_order.hpp`
(`register_turn_order_aps` / `require_turn_order_aps`), `solve_mtdfa`,
`MtdfaProduct` and the CLI wiring all land and build clean. **The
`backprop_nodes=true` SEGFAULT is RESOLVED**: it was an upstream Spot bug
(issue #639, fixed in Spot 2.15), *not* ours — `CMakeLists.txt` now requires
`libspot >= 2.15` and `solve_mtdfa.cpp` did not change, so the linear-time claim
is intact. All three SEGFAULTing tests pass, `GeneratedCorpus.MetamorphicRoundTrip`
included. `ctest` is now **296/302 passed, 6 failed** — all 6 are `EmitsDfa`
unit tests asserting the now-superseded "complete + rejecting sink" contract
(expected fallout of the PRD-change below — `/test-writer`'s to update, and
verified pre-existing rather than upgrade fallout). See "Phase 1 blocker" for the
root cause and for three earlier claims it corrects, and "Developer comments /
PRD disagreements" for implementation-level findings the frozen contract didn't
fully pin.
**Interface:** implements `Synthesis` as `MtdfaProduct` — a **second
implementation of Method 2** (`alg:dfa_product`), not a sixth method. Selected by
the CLI flag `--mtdfa-product`. `DfaProduct` and `ltlf_to_dfa` are left untouched.
**Recommended workflow:** **concurrent** (Phase 1 onward) — Phase 0 pinned the
four Spot-behaviour unknowns; every signature now falls out of the glossary types
(thin wrappers over existing Spot constructs), so the freeze is *high* and
`/developer` + `/test-writer` bind to it in parallel. **Phase 0 moved the contract
in two ways the PRD did not predict** (the dropped `ltlf_to_mtdfa` wrapper; the
`Controller` split-arena precondition) — read *Interfaces & types* and *Phase 0 —
answers* before binding.
**main.tex ref:** §Method 2 (`\cref{fulldfa}`, `alg:dfa_product`); $\cons$
(`\cref{def:consistency}`, §203); *enabled* (`\cref{def:enabled}`, §107–116);
$T_C$'s interface (`\cref{def:probDefTransducer}`, §129); the projection `\na`
(`main.tex:300`) and its commented-out `\cl` argument (`main.tex:302–303`).
No new algorithm — a representation change to an existing one.

**Gates:**
- [x] glossary        — *closed 2026-07-15* after Phase 0/Q2 re-opened it: new
  ***Turn order*** entry (canonical names `register_turn_order_aps` /
  `require_turn_order_aps`), *Goal DFA construction*'s wrapper placeholder
  retired, *Letter alphabet* / *Game solving* cross-refs, and the Mealy-commitment
  `\na` added to *Open theory questions*. See *Ubiquitous-language terms used*.
  *Landed 2026-07-14:* *Output-agreement automaton* (`emits_dfa`), *MTDFA*,
  *Representation*, `solve_mtdfa` + the mtdfa rows on *Product* / *Goal DFA
  construction*, and two *Open theory questions* cross-refs. Renamed this PRD's
  `cons_dfa` → `emits_dfa`. (`/glossary`; branch `master`, uncommitted.)
- [ ] tests           — unit + oracle coverage
- [ ] code-review     — domain (/code-reviewer) + generic (/code-review)
- [ ] theory-review   — code ↔ math faithfulness vs main.tex

## Goal

`ltlf_to_dfa` (`src/ltlf_to_dfa.cpp:14`) **already builds a `spot::mtdfa`** via
`spot::ltlf_to_mtdfa`, then immediately throws the symbolic form away:
`as_twa(state_based=true)` + `complete_here` explode it into an explicit
`twa_graph`. Spot's own header says of `as_twa` — *"The conversion can be costly,
since it requires creating BDD-labeled transitions for each path between a root
and a leaf of the state array"* — i.e. we pay a path-enumeration blowup to build
scaffolding that the rest of the pipeline then works symbolically over anyway.

This PRD adds `MtdfaProduct`: a path that keeps the MTDFA all the way through —
$A_\varphi$, the product, and the game stay MTBDD arrays, solved with Spot's MTDFA
game solver (`spot/twaalgos/ltlf2dfa.hh`, `\cite duret.25.ciaa`), the same
machinery `ltlfsynt` runs on. It continues the arc `docs/prd/symbolic-dfa-product.md`
started: that one removed the minterm loop from the **product**; this one removes
the explicit materialisation from the DFA **construction**, the other half of the
same cost. `docs/prd/benchmarking.md` having landed makes `automaton_construction`
a measured stage, so the win is observable rather than asserted.

**This PRD supersedes nothing.** `docs/prd/dfa-product.md` (Method 2, explicit) and
`docs/prd/symbolic-dfa-product.md` (its symbolic product build) both stay live and
unmodified: they are the differential this route is graded against. Keeping both
sides preserves the ability to localise a regression to construction vs product vs
solving, instead of moving both sides at once.

## Ubiquitous-language terms used

Existing, unchanged: *Goal formula* ($\varphi$), *Free/Known inputs/outputs*
($\Ifree,\Iknown,\Ofree,\Oknown$), *Governed variables* ($\mathcal{V}$),
*External knowledge strategy* ($\Tin,\Tout$), *Controller* ($T_C$),
*Transducer*, *Transition function* (`delta_edges`), *Output agreement*
(`emits_region`), *Consistency* ($\cons$), *Product*, *Game solving*
(`solve_dfa`), *Cube*, *Letter*, *Canonical benchmarking stage*,
*Controller verifier*, *Generated corpus & its grading modes*.

**Landed 2026-07-14 by `/glossary`** — no gaps remain; `/developer` may bind to
these names as canonical:

- **Output-agreement automaton** — `emits_dfa(tau, dict)` → `spot::twa_graph_ptr`.
  Filed as the **automaton form** under the existing *Output agreement (emits)*
  entry, beside per-letter `emits` and symbolic `emits_region`.
  **Renamed from this PRD's original `cons_dfa`**, which was a category error: $\cons$
  is the **two-transducer** conjunction and `emits` is the per-transducer conjunct,
  so a one-transducer object may not wear a `cons` name — the *Output agreement*
  entry's "Do not call it" line rejects `consistent`/`consistent_with` by name for
  exactly this. **$\cons$ has no automaton form**, precisely as it has no symbolic
  form: it emerges from the intersection (see Decision 1). `cons_dfa` / *Consistency
  automaton* are now recorded as rejected synonyms.
- **MTDFA (multi-terminal DFA)** — `spot::mtdfa_ptr`. New entry under *Automata &
  Transducers*, contrasting *NFA / DFA for the Goal* (explicit, **state-based**
  acceptance) with the MTDFA's **transition-based** terminals.
- **Representation** — new *prose note* under *The five methods*. **Deviation from
  the agreed shape:** the table gained a per-representation **column**, not a row —
  a sixth row would assert a sixth method, the exact claim we are avoiding. Five
  rows still mean five methods; `MtdfaProduct` is the Method-2 × mtdfa **cell**.
- **`solve_mtdfa`** — added to *Game solving (SolveDfa)* as the mtdfa sibling of
  `solve_dfa`, recording that the two discharge `main.tex:300`'s projection `\na`
  by different routes.
- ***Product*** and ***Goal DFA construction*** each gained their mtdfa row. The
  latter's mtdfa wrapper is **deliberately unnamed** pending Phase 0/Q2 — with a
  note that `ltlf_to_mtdfa` is unavailable as its name (Spot's own primitive).

**Landed 2026-07-15 by `/glossary`** — the Phase 0/Q2 re-open, now closed:

- ***Turn order*** — **new entry** (*Problem Definition*), anchored at
  `main.tex` **§83** (the canonical per-step order; *named* "turn order" at §107;
  Mealy-committed by §100's `\na`) — **not** §86 as this PRD said, which is $S_C$'s
  signature. The concept was already used undefined in `main.tex`, the glossary,
  `transducer.hpp:20` and `solve_dfa.cpp:54`; Q2 made it load-bearing. It records
  turn order's **two encodings**: *structural* on the explicit route
  (`split_2step`), and *nothing but the BDD variable order* on the mtdfa one.
- **Canonical names — `/glossary` overrode this PRD's placeholders.**
  `register_turn_order_aps` / `require_turn_order_aps` replace
  `register_ek_ap_order` / `assert_mtdfa_ap_order`: `ek` stutters inside
  `ltlf_ek::`, and `assert` implies an `NDEBUG`-compiled-out check when the
  function **throws**. The old names are now recorded as rejected synonyms. This
  PRD has been updated throughout; **bind to the glossary names.**
- **The checker enforces the *necessary* rule, not the canonical order** —
  $\Ifree$ strictly above every controllable, exactly what Phase 0 proved; order
  among controllables stays free. A check demanding the registrar's
  $\Ifree,\Iknown,\Ofree,\Oknown$ sequence would reject correct programs.
- *Goal DFA construction*'s "deliberately unnamed wrapper" placeholder (above) is
  **retired**: there is no wrapper to name.
- ***Open theory questions*** gained **Mealy is baked into the signatures**
  (`main.tex:100` `\na`) — see *Open theory questions touched* below.

Still required of `/developer`: reword `cli.hpp:37`'s "the five methods" contract
comment. No new *Canonical benchmarking stage* — see "Benchmarking" below.

## Behaviour / semantics (from main.tex)

`alg:dfa_product` must hold, in this representation:

1. **The product.** $P = A_\varphi \times \Tin \times \Tout$ with
   $F_P = F_D \times Q_{in} \times Q_{out}$ (`alg:dfa_product:final`) — acceptance
   is decided by the **Goal component alone**; the transducers contribute filtering
   only, never acceptance. Route (a) realises this as a **language intersection**:
   the *Output-agreement automaton* accepts on all of $Q$, so intersecting cannot make a
   $\varphi$-accepting word rejecting, nor a $\varphi$-rejecting word accepting.
2. **The $\cons$ filter** (`alg:dfa_product:cons`, `\cref{def:consistency}` §203):
   a letter survives iff its $\mathcal{V}$-variables are exactly what $\Tin,\Tout$
   output. Symbolically that is `emits_region(q_in) & emits_region(q_out)`, exactly
   as `build_product_symbolic` already computes it.
3. **Non-enabled letters** (`\cref{def:enabled}`, §107–116) are *skipped* in
   `main.tex`. Route (a) instead sends them to a **rejecting sink**. These coincide
   **given decision 2 below**: a $\neg\cons$ letter is only ever reachable by a
   *system* move, and reaching the rejecting sink loses — the same outcome as
   having no move at all. This equivalence is load-bearing and is a
   `/theory-review` item.
4. **$T_C$'s interface** (`\cref{def:probDefTransducer}`, §129): $\lambda_C : Q_C
   \times 2^{\mathcal{I}} \to 2^{\Ofree}$. `controller_as_transducer`
   (`synthesis.hpp`) reads $\lambda_C$ off the strategy's edge guards **expecting a
   relation over $\Ifree \times \Ofree$**, so step 5's projection is *mandatory*,
   not cosmetic.
5. **Projecting the governed variables.** `main.tex:300` (`\na`) asserts the game
   "can project these variables out without loss"; the argument is drafted in the
   commented-out `\cl` at `main.tex:302–303`. `solve_dfa` does this **arena-side**
   (`bdd_exist` per edge guard, `src/solve_dfa.cpp:49`). This PRD reaches the same
   endpoint **strategy-side** — see decision 2. Same result, different route;
   flagged for `/theory-review`, not claimed as conformance.

### Decision 1 — route (a): the product is a language intersection

Build, per transducer, its *Output-agreement automaton* as an explicit `twa_graph`
(cheap and near-trivial — $\delta$ is already deterministic over the full
$2^{\mathcal{I}\cup\mathcal{O}}$, `transducer.hpp:15`), lift it with
`twadfa_to_mtdfa`, and intersect with `spot::product`:

```
P = product(product(ltlf_to_mtdfa(phi, dict), twadfa_to_mtdfa(emits_dfa(t_in))),
            twadfa_to_mtdfa(emits_dfa(t_out)))
```

**Why not our own product over `states[]`.** The rejected alternative — porting
`build_product_symbolic`'s guard logic onto the goal MTDFA's `states[]` array —
looks like reuse but is not: an MTDFA product must **rewrite terminals**
($2d+b \to 2\cdot\mathrm{idx}\langle d,d_{in},d_{out}\rangle+b$), and **Spot
exposes no public API for that**. The only terminal machinery is inside
`ltlf_translator`, which the header marks *"Semi-internal… Do not rely on the
interface to be stable"*, or BuDDy-level `bddExtCache` work. Route (a) uses only
`SPOT_API` surface (`ltlf_to_mtdfa`, `twadfa_to_mtdfa`, `product`) and lets Spot
own terminal encoding, state numbering, and `fuse_same_bdds`.

**Intersection, not implication.** `spot::product_implies` is the **monolithic**
shape and is the wrong operator here. Implication is only needed when $\Iknown$ is
a *free* environment move that could be broken to win vacuously; under decision 2
it is not an environment move at all. This is **not** blocked on the monolithic
conjecture (`main.tex:133`) — and the transducer$\to\psiin$ star-free obstruction
does not bite, because it blocks an $\text{LTL}_f$ *formula*, not a DFA
($\text{LTL}_f \subsetneq$ regular), and the *Output-agreement automaton* is a DFA.

### Decision 2 — the pinned variables become *controllable*, not projected

Spot's MTDFA solver offers exactly one knob, `set_controllable_variables`; every
variable is either the system's or the environment's. Leaving $\Iknown$ as the
environment's is **fatal** — the env picks a $\neg\cons$ value, the run hits
`bddfalse`, and everything reports unrealizable. And projecting on the MTBDD is not
available: the destination lives *inside the terminal*, so `bdd_exist` over
$\Iknown$ would quantify across destinations.

Therefore: `set_controllable_variables($\Ofree \cup \Iknown \cup \Oknown$)`.

**Soundness.** At a product state $\langle s,q_{in},q_{out}\rangle$ the MTBDD is
non-`bddfalse` only on the $\cons$ region. Once the env has played $\Ifree$:
$\cons$ holds only when $\Iknown = \lambda_{in}(q_{in},\Ifree)$ — **exactly one**
value, since `parse_transducer` validates $\lambda$-functionality — and likewise
$\Oknown = \lambda_{out}(q_{out}, \mathcal{I}\cup\Ofree)$, one value per $\Ofree$.
Any other choice lands on `bddfalse` and loses. So the system's only real freedom
is $\Ofree$; $\Iknown,\Oknown$ are *functions of moves already made*. Realizability
is preserved both ways: restrict any MTDFA-game winning strategy to its $\Ofree$
component to get an EK winning strategy, and conversely extend any EK winning
strategy by the pinned values.

**Turn order permits it** (`main.tex` §83, Mealy — glossary *Turn order*):
$\lambda_{in}$ observes
$\Sigma_0 = \Ifree$, so $\Iknown$ is determined once the env has moved;
$\lambda_{out}$ observes $\Sigma_0 = \mathcal{I}\cup\Ofree$, and $\Ofree$ is being
chosen in the same turn. Both fall inside the system's turn.

**Cleanup.** The resulting Mealy machine over-emits $\Iknown,\Oknown$, so project
them out of it — legal, because `mtdfa_strategy_to_mealy` returns a **`twa_graph`**,
where `cond` and `dst` are separate fields. This is the existing `solve_dfa.cpp:49`
idiom, applied one stage later.

## Interfaces & types

**FROZEN — Phase 0 landed 2026-07-15.** Q1–Q4 are answered below (*Phase 0 —
answers*) and folded in here. Phase 0 also turned up **two findings the PRD did
not anticipate**, both of which move this contract:

- Q2's ordering fix **cannot** live in an `ltlf_ek::ltlf_to_mtdfa` wrapper
  (`register_ap` is idempotent, so a call at `synthesize` time is a no-op). That
  conditional header is **dropped**; a `register_turn_order_aps` /
  `require_turn_order_aps` pair replaces it, and **`cli.cpp`'s AP registration
  order changes**.
- `Controller` carries an undocumented **split-arena precondition** that
  `MtdfaProduct` cannot meet, so **`controller_as_transducer` changes**.

```cpp
// include/ltlf_ek/emits_dfa.hpp                                        [new file]
//
// The Output-agreement automaton for ONE transducer (docs/GLOSSARY.md "Output
// agreement (emits)", automaton form): the DFA accepting exactly those words
// whose every letter agrees with tau's lambda along tau's run.  This is the
// automaton form of emits_region --- NOT of cons, which is the two-transducer
// conjunction and has no automaton form (\cref{def:consistency} §203); the cons
// filter emerges from intersecting the two, see Decision 1.  Built on `dict` ---
// the SAME spot::bdd_dict as tau and the Goal automaton.
//
// Phase 0/Q1: marks acceptance STATE-BASED and MUST call prop_state_acc(true)
// explicitly --- twadfa_to_mtdfa branches on that property and would otherwise
// read the marks as final transitions (ltlf2dfa.cc:3001).  Must also be
// genuinely deterministic: twadfa_to_mtdfa THROWS otherwise (ltlf2dfa.cc:2958).
spot::twa_graph_ptr emits_dfa(const Transducer& tau,
                              const spot::bdd_dict_ptr& dict);

// include/ltlf_ek/turn_order.hpp                                       [new file]
// (docs/GLOSSARY.md "Turn order" --- the header is named for the concept, not
//  for the mtdfa encoding of it; the explicit route's encoding lives in
//  LetterAlphabet.)
//
// Phase 0/Q2.  The MTDFA game reads turn order off the BDD VARIABLE ORDER: a
// controllable variable above an uncontrollable one is a Moore move.  Under
// decision 2 the controllables are Ofree u Iknown u Oknown, so the contract is
//
//     every Ifree variable strictly ABOVE every controllable variable
//
// --- necessary AND sufficient (probed both ways; the controllables may be in
// any relative order among themselves).  Violating it does not crash: it
// silently returns a WRONG "unrealizable".  Spot's own ltlfsynt does the same
// thing for the same reason (bin/ltlfsynt.cc:472-477).
//
// register_ap is IDEMPOTENT, so the order can only be established BEFORE the
// first registration of each AP --- i.e. at dict setup, not inside a method.
// Hence the split: a registrar for dict setup, and a checker for the method.

// Register vars' APs on `dict` in the MTDFA-game-correct order: Ifree (sorted),
// then Iknown, Ofree, Oknown.  Call at dict creation, BEFORE parsing any
// transducer or building any automaton.  No-op for an AP already registered ---
// which is exactly why it cannot repair a bad order, only establish a good one.
void register_turn_order_aps(const VariablePartition& vars,
                          const spot::bdd_dict_ptr& dict);

// Throws std::invalid_argument iff some Ifree variable sits at or below a
// controllable one.  Turns a silent wrong verdict into a loud failure.
void require_turn_order_aps(const VariablePartition& vars,
                           const spot::bdd_dict_ptr& dict);

// include/ltlf_ek/solve_mtdfa.hpp                                     [new file]
//
// SolveDfa(P) for the MTDFA representation (docs/GLOSSARY.md "Game solving").
// `product` is the intersected MTDFA; `vars` supplies the free/known split.
// nullopt = unrealizable.  Sibling of solve_dfa, NOT a replacement.
std::optional<Controller> solve_mtdfa(const spot::mtdfa_ptr& product,
                                      const VariablePartition& vars);

// include/ltlf_ek/mtdfa_product.hpp                                   [new file]
//
// Method 2 (alg:dfa_product) over the MTDFA representation.  Shape forced by
// Synthesis; identical signature to DfaProduct.  Calls require_turn_order_aps
// (on t_in.dict()) FIRST, then spot::ltlf_to_mtdfa(phi, dict) directly --- see
// Phase 0/Q2 for why there is no ltlf_ek::ltlf_to_mtdfa wrapper.
class MtdfaProduct final : public Synthesis {
 public:
  std::optional<Controller> synthesize(const spot::formula& phi,
                                       const VariablePartition& vars,
                                       const Transducer& t_in,
                                       const Transducer& t_out) override;
};

// include/ltlf_ek/cli.hpp                                          [modified]
//
// make_synthesis_method gains "mtdfa-product" -> MtdfaProduct.  The doc comment
// at cli.hpp:37 ("the five methods") MUST be reworded: this is a sixth *flag*
// over five methods.  Known trade-off, accepted: if main.tex:335's MTDFA-for-
// Method-3 ever lands, this shape yields names like "--mtdfa-otf-dfa-product".
// Out of scope here.

// src/ltlf_ek_synth.cpp:344-345                                    [modified]
//
// Phase 0/Q2.  Today registers `inputs()` then `outputs()`; `inputs()` is
// Ifree u Iknown as ONE sorted std::set, so an Iknown name that sorts before an
// Ifree name lands ABOVE it --- and Iknown is CONTROLLABLE under decision 2.
// Probed: that yields a spurious "unrealizable".  Replace both loops with
//     register_turn_order_aps(partition, dict);
// For V = {} the resulting order is IDENTICAL to today's, so DfaProduct is
// perturbed only on EK problems --- and only in which of several equally valid
// controllers it returns, since split_2step/solve_game carry turn order
// structurally and do not read the BDD order.

// src/synthesis.cpp:24 (controller_as_transducer)                  [modified]
//
// Phase 0/Q4 follow-up.  It unconditionally calls spot::unsplit_2step, which
// requires a SPLIT arena.  DfaProduct's Controller is split (it comes from
// solved_game_to_mealy); mtdfa_strategy_to_mealy's output is NOT, and throws
// "get_state_players(): state-player property not defined, not a game?".
// Unsplit only when the "state-player" named property is present:
//
//     spot::twa_graph_ptr g = controller.strategy;
//     if (g->get_named_prop<std::vector<bool>>("state-player"))
//       g = spot::unsplit_2step(g);
//
// This makes Controller's real contract explicit: a Mealy machine, split or
// not.  synthesis.cpp is NOT in the byte-for-byte-unchanged list, so this is in
// scope; DfaProduct's path is unaffected (the property is always present).
```

**No `ltlf_ek::ltlf_to_mtdfa` wrapper.** The PRD offered one conditionally, to
pre-register APs Ifree-first. Phase 0/Q2 found ordering **is** load-bearing but
the wrapper **cannot deliver it**: `register_ap` is idempotent, and by the time
`synthesize` runs the CLI has already registered every AP (`ltlf_ek_synth.cpp:342-345`,
before any transducer is parsed). A wrapper could only validate — so it is
replaced by `register_turn_order_aps` (dict setup) + `require_turn_order_aps` (method
precondition), and `MtdfaProduct` calls `spot::ltlf_to_mtdfa(phi, dict)` directly.

**If implementation proves this contract wrong:** that is a PRD-change event —
update this section and propagate to any in-flight test branch; the developer does
not silently re-shape the interface on its own branch.

## Implementation phases

### Phase 0 — Spot-behaviour probes — **DONE 2026-07-15** ✅

Probed against the **linked** Spot, and cross-read against the source tree at
`/home/cowclaw/spot` (*newer than the linked library*, so the source is
corroboration, and every claim below is one the probe confirmed on the actual
linked lib). Probe was throwaway and is **not** committed.

> **Two caveats added 2026-07-15, both worth knowing before trusting a probe.**
> (1) The probes ran against **2.14.x**; the project now requires **Spot >= 2.15**
> (*Phase 1 blocker*). The pinned answers below still hold — the suite is green
> on 2.15.1 apart from the unrelated `EmitsDfa` contract fallout, and
> `GeneratedCorpus.MetamorphicRoundTrip` exercises Q2–Q4 end to end — but they
> are **not re-probed**, and 2.15 *did* change this area's API
> (`ltlf_to_mtdfa_for_synthesis` reordered its arguments; `ltlf_to_mtdfa` gained
> a trailing bool; `dfs_strict_node_backprop` was removed). Re-probe before
> leaning on any answer here for new work.
> (2) This section originally said the linked library was `libspot 2.14.4.dev`.
> **It was not** — `/usr/local`'s 2.14.4 supplied the *headers* while
> `LD_LIBRARY_PATH` forced a **2.14.5.dev** library onto the loader at runtime.
> "The probe confirmed it on the actual linked lib" was true; the *name* given to
> that lib was wrong. `ldd` the probe binary; never trust
> `pkg-config --modversion`.

#### Q1 — `twadfa_to_mtdfa` honours `prop_state_acc`; **both** conventions work

`ltlf2dfa.cc:3001-3017` decides the terminal's acceptance bit by the property:

```cpp
bool sbacc = twa->prop_state_acc().is_true();
... sbacc ? bdd_terminal(2*dst + twa->state_is_accepting(e.dst))   // dest-state acc
          : bdd_terminal(2*dst + !!e.acc);                         // final transition
```

Probe (reference `ltlf_to_mtdfa("last letter is a")`, compared by
`product_xor(...)->is_empty()`, with a `G(a)`-vs-`F(a)` **negative control** to
prove the oracle discriminates): the two *matched* pairs round-trip, the two
*mismatched* pairs do not. So the property is read, and either convention is
usable.

**Decision:** `emits_dfa` marks **state-based** and calls `prop_state_acc(true)`
**explicitly** — consistent with `ltlf_to_dfa`'s `as_twa(state_based=true)`.
Signature unchanged, as predicted.

Two hazards found on the way, both new to this PRD:

- **Accepting-sink absorption** (`ltlf2dfa.cc:2983-2997`). A **non-initial** state
  with a `bddtrue` self-loop carrying a **non-empty** `acc` mark is absorbed into
  the `bddtrue` constant and gets no root. Harmless for `emits_dfa` (such a state
  really is an accepting sink), and the initial state is exempt (`i == init` is
  tested first) — but it silently makes naive round-trip fixtures
  non-discriminating, which is exactly how a first pass at this probe produced a
  confident wrong answer. **Note for `/test-writer`: do not build `emits_dfa`
  fixtures whose only accepting state is a `bddtrue` self-loop.**
- **`is_deterministic` is a hard precondition** (`ltlf2dfa.cc:2958`):
  `twadfa_to_mtdfa` **throws** `std::runtime_error` on a non-deterministic input.
  `emits_dfa`'s "deterministic by construction" claim is therefore load-bearing at
  runtime, not just on paper.
- `emits_dfa`'s **rejecting** sink is **not** absorbed (absorption needs a
  non-empty mark), so it becomes a real root. Correct, but wasted; **skip creating
  the sink state entirely when no `¬covered(q)` edge is ever added.**

#### Q2 — ordering **is** load-bearing; the wrapper is dropped for a helper + assert

`phi = G(a <-> b)`, `a` uncontrollable, `b` controllable. Mealy ⇒ realizable,
Moore ⇒ unrealizable. **Only the BDD order differs:**

| BDD order | verdict |
|---|---|
| `a`@0, `b`@1 (Ifree first) | **realizable** ✅ correct |
| `b`@0, `a`@1 (Ofree first) | **unrealizable** ❌ silently Moore |

Corroborated by an **independent external oracle** — Spot's own `ltlfsynt`
(`bin/ltlfsynt.cc:472-477`): *"For Mealy semantics, inputs should appear first in
the MTBDDs. For Moore semantics, outputs should be first. Pre-registering those
variables will ensure that."* (Its `spot::bdd_dict_preorder` is just
`make_bdd_dict()` + RAII ownership — **not** a different ordering mechanism, so
our shared-dict idiom is fine.)

**The default order is unsafe.** With no pre-registration Spot's order is
alphabetical-ish, so correctness depends on whether input names happen to sort
before output names: `G(z <-> a)` with `z` = Ifree, `a` = Ofree returns
**unrealizable — the wrong answer** (`a` takes level 0).

**The CLI's current order is unsafe for `MtdfaProduct`.** `ltlf_ek_synth.cpp:344-345`
registers `inputs()` then `outputs()`, and `inputs()` = $\Ifree \cup \Iknown$ as
one sorted `std::set`. Decision 2 makes $\Iknown$ **controllable**, so an $\Iknown$
name sorting before an $\Ifree$ name lands above it. Probed on an EK-shaped spec
(`G(b <-> z) & G(a <-> b)`; $\Ifree=\{z\}$, $\Iknown=\{b\}$, $\Ofree=\{a\}$;
controllable $=\{a,b\}$; EK-realizable):

| registration order | verdict |
|---|---|
| `z, b, a` (Ifree first) | **realizable** ✅ |
| `b, z, a` (**today's CLI**) | **unrealizable** ❌ spurious |

**The rule — necessary and sufficient:** every $\Ifree$ variable strictly above
every controllable ($\Ofree \cup \Iknown \cup \Oknown$). Order *among* the
controllables is free — probed both ways (`z|b,a` and `z|a,b`), both realizable.

**Effect (the PRD predicted "only Q2 can change a signature" — it did):** the
conditional `ltlf_ek::ltlf_to_mtdfa` wrapper is **dropped**. It cannot work:
`register_ap` is idempotent, so pre-registering at `synthesize` time is a no-op
against an order the CLI already fixed. Replaced by `register_turn_order_aps` (dict
setup) + `require_turn_order_aps` (loud precondition), and `cli.cpp` changes. See
*Interfaces & types*.

#### Q3 — unrealizable ⇔ `states[0] == bddfalse`; `num_roots() == 0` **never** happens

`ltlf2dfa.cc:3574-3579`, the `backprop_nodes=true` path pinned in step 2:

```cpp
if (!enc.backprop.winner(0))   // root 0 = the initial state
{
  res->states.push_back(bddfalse);   // exactly ONE root, bddfalse
  res->names.push_back(formula::ff());
  return res;
}
```

Probed over realizable / unrealizable / collapsed `a & !a` / collapsed `1`:
`is_empty()` and `states[0] == bddfalse` agree in **all four**, and `num_roots()`
is **always ≥ 1** — so the PRD's suggested `num_roots() == 0` test is **wrong**.

**Decision:** use `strategy->num_roots() == 0 || strategy->states[0] == bddfalse`
— O(1) and exactly the implemented contract. (`is_empty()` also works but is
`!bdd_find_leaf(states, leaf_is_accepting)` (`ltlf2dfa.cc:2677-2680`): it scans
every node and answers *language*-emptiness rather than "is the initial state
winning". Those coincide for a strategy, but only incidentally.) The `num_roots()`
disjunct is defensive only.

**Also:** on the unrealizable path Spot calls **neither** `register_all_propositions_of`
**nor** `set_controllable_variables` on the result — so do not touch the returned
object beyond the emptiness test.

#### Q4 — `loop=true`. Predicted mechanism confirmed; predicted *symptom* refuted

`phi = F(b)`, $\Ifree=\{a\}$, $\Ofree=\{b\}$ — a spec that gets *fulfilled*, which
is what makes `loop` observable at all:

| | machine | $\lambda_C$ | `verify_controller` |
|---|---|---|---|
| `loop=false` | 2 states; state 1 has `1 -> 1 cond=1` | **non-functional** (4 (in,out) pairs for 2 inputs; `rel = bddtrue`) | **ok = 1** |
| `loop=true` | 1 state; `0 -> 0 cond=b` | **functional** ✅ | ok = 1 |

**Decision:** `loop=true`, exactly as the PRD reasoned — `loop=false`'s free-choice
state makes $\lambda_C$ a relation, not a function, violating
`\cref{def:probDefTransducer}`'s $\lambda_C : Q_C \times 2^{\mathcal{I}} \to 2^{\Ofree}$.

**Correction to this PRD:** *"If the round-trip oracle fails in Phase 1, this is
the first suspect"* is **wrong — it will not fail.** `verify_controller` returned
`ok = 1` for **both** values. That is not a verifier bug: `loop=false` only jumps
to a free-choice state once $\varphi$ is already fulfilled, so every continuation
still satisfies $\varphi$ and the verifier legitimately passes. The discriminator
is **$\lambda$-functionality**, which is what `parse_transducer` validates — a
`loop=false` controller would be caught on a `--controller` round-trip through
`transducer_io`, **not** by `verify_controller`.

#### Q4 follow-up (NEW) — `Controller` has an undocumented split-arena precondition

`controller_as_transducer` (`src/synthesis.cpp:24`) calls `spot::unsplit_2step`
unconditionally. Probed: `DfaProduct`'s `Controller` **has** the `"state-player"`
property (it comes from `solved_game_to_mealy` on a split arena);
`mtdfa_strategy_to_mealy`'s output does **not**, and throws
*"get_state_players(): state-player property not defined, not a game?"*.

This **blocks Phase 1**: the corpus oracle calls `verify_controller(..., *b)`,
whose `Controller&` overload delegates to `controller_as_transducer`.

**Decision (agreed):** unsplit only when the `"state-player"` property is present.
See *Interfaces & types*. `src/synthesis.cpp` is not in the byte-for-byte-unchanged
list; `DfaProduct`'s path is unaffected.

**Green checkpoint:** ✅ Q1–Q4 answered above; *Interfaces & types* updated; no
production code landed.

### Phase 1 — `emits_dfa` + `MtdfaProduct` + `solve_mtdfa` + CLI flag

`/developer` and `/test-writer` run **concurrently** on separate worktrees off the
frozen contract (`src/`+`include/` vs `tests/` — disjoint; never `git add -A` while
both are live). The launcher owns integration: merge both, build, run `ctest` once
in the foreground.

**Green checkpoint:** `ctest` green, including the full generated corpus running
both methods (see *Test oracles*) and `--mtdfa-product` reachable from the CLI.

### Phase 2 — benchmarking wiring + `minimize_mtdfa` knob

- Wire the three `BenchTimer` scopes per *Benchmarking* below.
- Add `minimize_mtdfa` (Moore minimisation, cheap on this data structure) as its
  **own knob, default off**, measured separately. It is adjacent free real estate,
  not part of the core claim — keep it separable so its effect is attributable.

**Green checkpoint:** `--benchmark` emits the three canonical stages for
`MtdfaProduct`; `automaton_construction` is directly comparable against
`DfaProduct`'s; the minimisation knob is measured on its own.

## Benchmarking

**No new `Stage` registry value.** The existing three map cleanly, and keeping
`automaton_construction` meaning exactly *"build the Goal automaton"* in **both**
methods matters — it is the one number this whole PRD exists to move.

| `Stage` | `DfaProduct` | `MtdfaProduct` |
|---|---|---|
| `automaton_construction` | `ltlf_to_mtdfa` + `as_twa` + `complete_here` | `ltlf_to_mtdfa` **only** ← the measured win |
| `product_construction` | `build_product_symbolic` + `materialize_product` | `emits_dfa` ×2 + `twadfa_to_mtdfa` ×2 + `product` ×2 |
| `game_solving` | `solve_dfa` | `mtdfa_winning_strategy` + `mtdfa_strategy_to_mealy` + projection |

The *Output-agreement automaton* build is attributed to `product_construction`, **not**
`automaton_construction` — it is EK-crossing work, and letting it into the Goal-build
number would muddy the only cross-method comparison that matters here.

## `emits_dfa` — pinned specification

Bespoke (no `main.tex` counterpart), so specified past sketch level.

- **States:** one per `tau` state $q \in [0, \texttt{num\_states})$. Initial
  state = `tau.initial_state()`. **No sink state** (dropped 2026-07-15 — see
  *Phase 1 blocker*; the sink was wasted work, and `main.tex` skips non-enabled
  letters) — the automaton is deliberately incomplete.
- **Edges:** for each $q$, for each `(g, d)` in `tau.delta_edges(q)`, add
  $q \xrightarrow{\;g\ \&\ \texttt{tau.emits\_region}(q)\;} d$. **Skip the edge if
  that conjunction is `bddfalse`** (no dead edges). **No other edge is added** —
  in particular no $\neg\mathit{covered}(q)$ edge to a sink: a letter not covered
  by any edge is a missing edge, an implicit reject, so the language is
  unchanged.
- **Acceptance:** every $q \in [0,\texttt{num\_states})$ accepting.
  **Phase 0/Q1: mark STATE-BASED and call `prop_state_acc(true)` explicitly** —
  i.e. every out-edge of every $q$ carries the mark. `twadfa_to_mtdfa` branches
  on that property and would otherwise read the marks as final transitions.
- **Result type:** `spot::twa_graph_ptr`, deterministic **but not complete** by
  construction — `delta_edges` is deterministic per `Transducer`'s contract and
  ANDing `emits_region` only shrinks guards, so determinism is preserved, but a
  $q$ with no covering letter simply has no outgoing edge. **Determinism is a
  runtime precondition, not just a claim:** `twadfa_to_mtdfa` throws
  `std::runtime_error` on a non-deterministic input (Phase 0/Q1,
  `ltlf2dfa.cc:2958`); it does not require completeness.
- **Determinism of the build:** fully deterministic; `delta_edges` order is the
  transducer's own. No seed, no iteration to a fixpoint, no bounds — a single pass
  over $Q$, each with a single pass over its out-edges.
- **Language claim (the unit oracle):**
  $L(\texttt{cons\_dfa}(\tau)) = \{\, w : \text{the run of } \tau \text{ on } w
  \text{ is defined and every letter agrees with } \lambda \text{ at its state}\,\}$.

## `solve_mtdfa` — pinned specification

Bespoke (decision 2 has no `main.tex` counterpart), so every Spot argument is
pinned rather than left to be guessed.

0. **`require_turn_order_aps(vars, product->get_dict())`** — Phase 0/Q2. A bad AP
   order does not crash; it returns a plausible, wrong "unrealizable". Fail loudly
   instead.
1. `product->set_controllable_variables(cube($\Ofree \cup \Iknown \cup \Oknown$))`
   — decision 2. Build the cube with the existing `solve_dfa.cpp` idiom; if the set
   is empty, pass `bddtrue`.
2. `strategy = spot::mtdfa_winning_strategy(product, /*backprop_nodes=*/true)`.
   **`true`** — the header says it builds a `backprop_graph` giving *"a linear-time
   resolution"*, versus `false`'s in-place refinement that *"does not have linear
   complexity"*. This PRD's whole claim is cost, so take the linear one.
3. **Unrealizable ⇒ `nullopt`.** Phase 0/Q3 pins the test:
   `strategy->num_roots() == 0 || strategy->states[0] == bddfalse`. **Not**
   `num_roots() == 0` alone — that never happens. Touch nothing else on the
   returned object: Spot leaves it without registered APs or controllable vars on
   that path.
4. `mealy = spot::mtdfa_strategy_to_mealy(strategy, /*labels=*/false,
   /*loop=*/true)`. **`loop=true`** — Phase 0/Q4: `loop=false` adds a free-choice
   state whose `bddtrue` out-edge makes $\lambda_C$ non-functional. `labels=false`:
   state names are $\text{LTL}_f$ formulas we never read, and the header says
   dropping them saves work.
5. **Project** $\Iknown,\Oknown$ out of every edge: `e.cond = bdd_exist(e.cond,
   known_cube)` — the `src/solve_dfa.cpp:49` idiom, one stage later. Drop any edge
   whose guard becomes `bddfalse`.
6. `spot::set_synthesis_outputs(mealy, ofree_cube)`, then return
   `Controller{mealy}`. This `mealy` is **already unsplit** (no `"state-player"`
   property) — unlike `solve_dfa`'s, which is a split arena. Do **not** re-split
   it; `controller_as_transducer` is being taught to unsplit only when split
   (Phase 0/Q4 follow-up, *Interfaces & types*).

**Determinism:** no seed, no randomness; a single pass over the strategy's edges.

## Edge cases

- **$\lambda$ undefined at $q$** ⇒ `emits_region(q)` = `bddfalse` ⇒ every edge from
  $q$ is `bddfalse` and is skipped ⇒ $q$ has **no outgoing edges**. Correct: no
  letter is $\cons$ at $q$, and the missing edges are an implicit reject of
  every letter (sink dropped 2026-07-15; no sink is materialised).
- **Partial transducers / Case A.** Decision 2 leans on an assumption `solve_dfa`
  does *not*: that a legal $\Iknown$ always **exists** after any env $\Ifree$ move.
  If $\lambda_{in}$ were partial at a reachable $(q_{in},\Ifree)$, the system would
  have no legal move and **lose**, whereas `\cref{def:enabled}` says the letter is
  *skipped*. Under the Case-A totality the project commits to (glossary,
  *Partial transducers — resolved*, §107–116), $\lambda_{in}$ is total on reachable
  states and this is moot. **Stated, not inherited silently** — a `/theory-review`
  item.
- **`emits_dfa` accepts the empty word** (initial state $\in Q$, all accepting).
  Harmless: the product is a language *intersection*, and $L(\varphi)$ excludes
  $\varepsilon$ (non-empty traces; `1` rejects the empty word), so the intersection
  does too. Do not "fix" this by making the initial state rejecting — that would
  break the *Output-agreement automaton*'s stated language.
- **Empty $\Ofree$.** `set_controllable_variables` receives $\Iknown \cup \Oknown$
  only; if that is also empty it receives `bddtrue`. Smoke-test both, mirroring the
  existing empty-`Ofree` cases in `tests/ltlfsynt_oracle_test.cpp`.
- **Empty universe** ($\mathcal{I}\cup\mathcal{O} = \emptyset$): $\Sigma =
  \{\texttt{bddtrue}\}$.
- **$\varphi = 1$ / $\varphi = 0$.** `ltlf_to_mtdfa`'s `detect_empty_univ` may
  collapse these to a single `bddtrue`/`bddfalse` state before any product happens.
  **Confirmed in Phase 0 — no special-casing needed.** $\varphi = 1$ collapses to a
  single `bddtrue` root, $\varphi = 0$ and $\varphi = a \wedge \neg a$ to a single
  `bddfalse` root; `spot::product` accepts a collapsed operand in **either**
  argument position, and `solve_mtdfa`'s Q3 test then reports realizable /
  unrealizable respectively. Keep the fixtures as regressions.
- **Unrealizable** ⇒ `mtdfa_winning_strategy` returns a strategy with exactly one
  root, `states[0] == bddfalse` ⇒ `nullopt` (Phase 0/Q3).
- **`ltlf_to_mtdfa_for_synthesis` is NOT used.** It takes `outvars` + a `backprop`
  mode and can collapse to a single `bddtrue`/`bddfalse` state (realizability-only).
  That is the **monolithic** route — it bakes in a game the EK product has not
  filtered yet, so per-state EK filtering would be impossible. Use plain
  `spot::ltlf_to_mtdfa`. Recorded so it is not "discovered" as an optimisation later.

## Test oracles (for /test-writer)

Bind to the frozen contract above; the domain oracles parallelize regardless.

1. **Full generated corpus, both methods** (`tests/ltlfsynt_oracle_test.cpp`,
   glossary *Generated corpus & its grading modes*). Roughly doubles corpus
   runtime; earns it, because route (a) leans on Spot semantics that Phase 0 probes
   but does not prove:
   ```
   for each generated (phi, partition, Tin):
     a = DfaProduct  .synthesize(...)
     b = MtdfaProduct.synthesize(...)
     EXPECT_EQ(a.has_value(), b.has_value())              // cross-method metamorphic
     if (a) EXPECT_TRUE(verify_controller(..., *a).ok)    // round-trip
     if (b) EXPECT_TRUE(verify_controller(..., *b).ok)    // round-trip
     EXPECT_EQ(b.has_value(), ltlfsynt_verdict(...))      // differential (via CLI)
   ```
   The differential drives the **binary** as a subprocess, so it needs
   `--mtdfa-product` — a Phase 1 dependency, not Phase 2.
2. **`emits_dfa` unit tests** — the language claim above, per fixture: total $\lambda$,
   partial $\lambda$ (sink reachable), $\lambda$ undefined at a state (all-`bddtrue`
   sink edge), single-state transducer. Determinism + completeness as structural
   free-riders.
   **Phase 0/Q1 trap — read before writing fixtures.** `twadfa_to_mtdfa` absorbs a
   **non-initial** state with an **accepting `bddtrue` self-loop** into the
   `bddtrue` constant (`ltlf2dfa.cc:2983`). A fixture whose accepting structure is
   just such a sink round-trips under *either* acceptance convention and therefore
   tests **nothing** — this exact shape produced a confident wrong answer during
   Phase 0. Prefer fixtures with no `bddtrue` self-loop (e.g. "the last letter
   is `a`"), and pair any language-equivalence oracle with a **negative control**
   that must fail, so a vacuous oracle is detectable.
3. **AP-order tests** (Phase 0/Q2) — the one bug class here that is silent:
   - `require_turn_order_aps` **throws** when an $\Ifree$ var sits below a
     controllable (build the bad order deliberately).
   - The EK regression the order exists for: $\Ifree=\{z\}$, $\Iknown=\{b\}$,
     $\Ofree=\{a\}$, $\varphi$ Mealy-realizable — `MtdfaProduct` must agree with
     `DfaProduct`, not report a spurious unrealizable.
   - `register_turn_order_aps` is a no-op on an already-registered AP (idempotence is
     *why* the order must be set at dict setup).
4. **Hand-authored `MtdfaProduct` fixtures** — mirror the existing Tables A–E rows,
   including the two Mealy-only payoff rows: they are exactly what would catch a
   turn-order error in decision 2.
5. **NOT the build-equivalence oracle.** It diffs `ProductGuards`
   (`docs/prd/symbolic-dfa-product.md`); route (a) never produces one. Winning
   strategies are non-unique, so `Controller`s are **not** comparable either — the
   cross-method oracle compares **realizability verdicts**, not controllers.

## Open theory questions touched

Flagged for `/theory-review`; not resolved here.

- **The projection `\na` (`main.tex:300`)** asserts the game "can project these
  variables out without loss", argued in the commented-out `\cl` at
  `main.tex:302–303`. Decision 2 reaches the same endpoint **strategy-side**
  (pin as forced system moves, project from the strategy) rather than **arena-side**
  (project from the guards, as `solve_dfa` does). Same result, different
  justification — needs review, not an assumed conformance.
- **Skip vs rejecting sink** (`\cref{def:enabled}`, §107–116). `main.tex` *skips* a
  non-enabled letter; route (a) sends it to a rejecting sink. Equivalent only under
  decision 2's forced-move argument. This is the single load-bearing semantic claim
  of this PRD.
- **Turn order is carried by the BDD variable order** (NEW — Phase 0/Q2).
  Decision 2 argues "turn order permits it" from `main.tex` §83 (the canonical
  *Turn order*; Mealy: $\lambda_{in}$ observes $\Sigma_0 = \Ifree$). Phase 0 found
  that in this representation §83 is **not** enforced by any structure — it is
  enforced *only* by the BDD variable order, and inverting that order silently
  yields Moore semantics and a wrong verdict (probed). So `require_turn_order_aps`
  is not a defensive nicety: it is **the** artifact keeping the code faithful to
  §83, and it deserves review as such. Contrast `solve_dfa`, where `split_2step`
  makes turn order structural and the BDD order is irrelevant. Cross-check that the
  $\Ifree$-above-controllables rule is exactly §83's turn order and not an
  approximation of it.
- **Trace-termination semantics** (`main.tex:96` `\na`; glossary *Open theory
  questions*). `solve_dfa` and the *Controller verifier* already share the
  system-controlled-termination reading. `mtdfa_winning_strategy` brings **Spot's
  own** reading — if it differs, cross-method agreement fails for a *semantic*, not
  a bug, reason. Weak evidence it agrees: `ltlfsynt` runs this machinery, and the
  existing differential already passes for `DfaProduct`.
- **`emits_region` ↔ `\cref{def:consistency}` faithfulness** — already flagged by
  `docs/prd/symbolic-dfa-product.md`; `emits_dfa` inherits it, now one level further
  from the definition (region → automaton).
- **MTDFA for Method 3** (`main.tex:335` `\na`: *"This likely requires adjusting the
  definitions for MTDFA usage"*). Out of scope; noted because it is evidence that
  MTDFA is a representation axis crossing methods, which the chosen sixth-flag CLI
  shape does not model.

## Definition of done

- [x] Phase 0's Q1–Q4 answered and written back into *Interfaces & types*
      (2026-07-15).
- `emits_dfa`, `solve_mtdfa`, `MtdfaProduct`, `register_turn_order_aps` +
  `require_turn_order_aps` land; `ltlf_to_dfa`, `DfaProduct`,
  `build_product_symbolic` and `verify_controller` are **byte-for-byte unchanged**.
  **`src/synthesis.cpp` (`controller_as_transducer`) and `src/ltlf_ek_synth.cpp`
  (AP registration order) DO change** — Phase 0/Q2 and Q4-follow-up; neither is in
  the unchanged list.
- `--mtdfa-product` wired into `make_synthesis_method`; `cli.hpp:37`'s "five
  methods" comment reworded.
- The AP-order contract holds: `register_turn_order_aps` at every dict-setup site
  (CLI **and tests**), `require_turn_order_aps` guarding `MtdfaProduct`, and a test
  that a violated order **throws** rather than returning a wrong verdict.
- `ctest` green, including the full corpus over both methods.
- `docs/GLOSSARY.md` carries *Output-agreement automaton*, *MTDFA*, *Representation*,
  `solve_mtdfa`, and the five-methods table's *implementation of Method 2* row.
- `--benchmark` emits the three canonical stages for `MtdfaProduct`;
  `automaton_construction` is comparable to `DfaProduct`'s, and the delta between
  them is the reported result of this PRD.
- `minimize_mtdfa` available as a separately-measured knob, default off.
- All four gates ticked with refs.

## Developer comments / PRD disagreements

**2026-07-15, `/developer` (Phase 1 blocker fix):**

- **`emits_dfa` no longer materialises a rejecting sink — deviates from the
  original frozen *pinned specification*, which mandated one.** This was
  *believed* to be the fix for *Phase 1 blocker — the `backprop_nodes=true`
  segfault*; **it was not** (see that section: the blocker was an upstream Spot
  bug, fixed in Spot 2.15). The change stands anyway, on its own merits — the
  sink was wasted work and `main.tex` *skips* non-enabled letters. The result is
  now **deterministic
  but not complete**: a state with no covered letter simply has no outgoing
  edge, an implicit reject, so `L(emits_dfa(tau))` is unchanged. The frozen
  **signature** does not move. Rewrote *`emits_dfa` — pinned specification*
  (dropped the sink-state/sink-edge bullets, "deterministic and complete" →
  "deterministic but not complete") and *Edge cases* (λ-undefined-at-q now
  gets no outgoing edges, not a `bddtrue` edge to a sink) to match. `solve_mtdfa`'s
  `backprop_nodes=true` is unchanged — flipping it to `false` would dodge the
  bug by forfeiting the linear-time claim this PRD exists to measure, so it was
  explicitly not taken (see "Phase 1 blocker"). That restraint paid off: the
  real fix was upstream, and `true` never had to move. **Only 2 of the 3 named
  SEGFAULTing tests were fixed** by this change (`B_XBang_not_k`,
  `C_XBang_k_iff_a`); `GeneratedCorpus.MetamorphicRoundTrip` kept SEGFAULTing at
  the same site via a trigger confirmed unrelated to this sink — cleared only by
  requiring Spot >= 2.15.
  **Theory note, not acted on:** dropping the sink may *retire* rather than
  discharge this PRD's "skip vs rejecting sink" load-bearing semantic claim
  (`main.tex` \cref{def:enabled} already *skips* a non-enabled letter; an
  incomplete automaton now does too, literally) — `/theory-review` item, `main.tex`
  untouched here.

**2026-07-15, `/developer` (Phase 1):**

- **`emits_dfa`'s "one state per q in [0, num_states)" is not implementable
  against the frozen `Transducer` interface, and the correct fix is BFS, not a
  new accessor.** `transducer.hpp` exposes `initial_state()`, `delta(q, v)`,
  `lambda(q, v)`, `emits_region(q)`, `delta_edges(q)`, `dict()` --- no
  `num_states()`, so there is no way to iterate a literal `[0, num_states)`
  range for an arbitrary `Transducer`. `emits_dfa` instead discovers tau's
  state set by BFS from `tau.initial_state()`, following `delta_edges`'
  destinations, assigning each newly-seen tau state a fresh index in its own
  `spot::twa_graph` as it is discovered. This is not a signature change (no
  frozen type moved) and is arguably *more* correct than the literal reading:
  it visits exactly the reachable states, so an unreachable state (which
  contributes nothing to the language) costs nothing, and an unreachable
  rejecting sink never gets created --- exactly the dead-root Phase 0/Q1
  already told us to avoid by other means. Confirmed independently by the
  concurrent `/test-writer` branch against the same header before this PRD
  entry was written.
- **`register_turn_order_aps`'s registrar cannot be transient, and the frozen
  `void(vars, dict)` signature has no way to return a handle for the caller to
  keep alive.** `spot::bdd_dict::register_proposition` is ref-counted per
  owner (`spot/twa/bdddict.cc:106-125,223-272`): when the *last* owner of an
  AP unregisters, the mapping is erased from `var_map` and the BuDDy variable
  slot is returned to that dict's own free list, available for a *different*
  formula's next allocation. The PRD's own reasoning ("register_ap is
  idempotent, so this can only establish the order before the first
  registration") is only true if `register_turn_order_aps`'s own registration
  is *still present* (not yet unregistered) when `t_in`/`t_out`/the Goal
  automaton later re-register the same AP names — i.e. the establishing
  registrar must outlive the call, exactly like Spot's own
  `bdd_dict_preorder` (`bin/ltlfsynt.cc:469`, a stack-scoped object spanning
  the whole synthesis call) and this project's pre-existing `ap_registrar`
  idiom (`ltlf_ek_synth.cpp`, kept alive for the whole CLI run). A registrar
  that dies at `register_turn_order_aps`'s own return would let that ordering
  guarantee depend on the *accidental* re-registration order of later
  consumers instead — fragile, and not what the "idempotent no-op" argument
  actually requires. Chosen fix: `register_turn_order_aps` keeps its scratch
  `spot::twa_graph_ptr` alive in a function-local `static
  std::vector<spot::twa_graph_ptr>` (never cleared), so it only ever
  unregisters at process exit — after every real consumer on that dict has
  come and gone. This is a deliberate, bounded, per-call footprint (one empty
  `twa_graph` plus a handful of `bdd_map` entries, never the transducers/DFAs/
  controllers built *on* the dict, which are unaffected and free normally);
  the dict itself is kept alive longer than its natural scope, which is a
  non-issue for the CLI (one dict per process) and a small, bounded cost for
  a test binary calling this once per generated-corpus case. If corpus scale
  ever makes this cost material, the fix is a signature change (an explicit
  persistent-registrar out-parameter or object) — a PRD-change event, not a
  quiet workaround.
- **Minor, non-signature additions beyond the pinned snippets:**
  `MtdfaProduct::synthesize` also calls `validate_product_inputs` (phi's APs
  ⊆ I∪O, one shared `bdd_dict`) before `require_turn_order_aps`, mirroring
  `DfaProduct`'s own validation preamble — the frozen class comment doesn't
  mention it, but nothing in Decision 1/2 forbids it either.
  `ltlf_ek_synth.cpp`'s `kMethodFlags` list (used by `ParseArgs`, not by
  `make_synthesis_method`) also gained `"mtdfa-product"` — without it,
  `--mtdfa-product` would throw `UsageError("unrecognised flag")` before ever
  reaching `make_synthesis_method`, so this was necessary for the PRD's own
  green checkpoint ("`--mtdfa-product` reachable from the CLI").

## Phase 1 blocker — the `backprop_nodes=true` segfault — **RESOLVED 2026-07-15** ✅

**It was an upstream Spot bug, it is fixed upstream, and the fix costs this PRD
nothing.** Spot issue **#639** ("segfault in ltlfsynt on unrealizable case"),
fixed by **e4452735e** (2026-02-08), shipped in **Spot 2.15**. `CMakeLists.txt`
now requires `libspot >= 2.15`. Building against 2.15.1 clears the blocker with
**zero change to `src/solve_mtdfa.cpp`**: `backprop_nodes=true` stays pinned, so
the linear-time resolution — *this PRD's entire claim* — is intact, and
`backprop=false` never had to be taken.

**All three SEGFAULTing tests now pass**, including the one the sink fix could
not clear:

| test | pre-2.15 | on Spot 2.15.1 |
|---|---|---|
| `GeneratedCorpus.MetamorphicRoundTrip` | SEGFAULT | **Passed** |
| `…/MtdfaKnownInputTest…/B_XBang_not_k` | SEGFAULT (also cleared by the sink fix) | Passed |
| `…/MtdfaKnownInputTest…/C_XBang_k_iff_a` | SEGFAULT (also cleared by the sink fix) | Passed |

`ctest` is **296/302**. The remaining 6 are the `EmitsDfa` contract fallout
(*Still open*, below) — not Spot's, and verified pre-existing.

### Root cause — pinned (2026-07-15, `/grill-debug`)

`mtdfa_winning_strategy_by_backprop` encodes roots under `stop_asap`: the loop
**breaks as soon as backprop state 0 (root 0) is determined** — by design. Only
the roots reached before that point ever enter `rootnum_to_backprop_state`; on
the 10-root reproducer, exactly **3** of them (roots 0, 1, 3 — gdb:
`mNumElements = 3`). The finalize loop that follows nevertheless runs over
**all 10** roots and calls `root_winner(term / 2)` on roots never encoded.

Pre-2.15, `root_winner` guarded that lookup with **`assert()` only** — a no-op
under `NDEBUG`, which every release build defines:

```cpp
// pre-2.15 (the crashing code)
bool root_winner(unsigned root_number) const
{
  auto it = rootnum_to_backprop_state.find(root_number);
  assert(it != rootnum_to_backprop_state.end());   // compiled out under NDEBUG
  return backprop.winner(it->second);              // dereferences end()
}
```

`find()` misses, returns robin_hood's **`end()` sentinel**, and `it->second`
reads the info-array bytes as a state index — `3,014,656` into a 22-element
vector, hence the fault at `winner`'s bitfield. Spot 2.15 replaces the assert
with a real runtime guard returning `-1`, and the callers test `<= 0`.
Upstream's own rationale matches the observed mechanism exactly: *"would
occasionally query the winner of some indeterminate vertices. In that case, it
is OK to assume that the winner is input: the vertex would have been determined
if it was useful."*

**Established by A/B/A against a pristine 2.15.1 build — executed, not read:**
pristine survives the reproducer; reverting **only** that guard brings the
SIGSEGV back; restoring it fixes it again.

### Corrections to the diagnosis this section replaces

The earlier passes had the *shape* right (upstream bug; `strategy_finalize`; the
early-breaking encode loop as prime suspect — which was the correct lead) but
three specific claims were **wrong**, and each is instructive:

- **"`root_winner()` is itself guarded (returns `-1` on a `find()` miss), so the
  root's map entry *exists* but resolves out-of-range."** Backwards. The entry
  does **not** exist — the `-1` guard is precisely what 2.15 *added*. The
  out-of-range index is `end()`'s own memory misread as a value.
- **"A Spot version gap — hypothesis REFUTED … relinking is not the fix."**
  Exactly inverted. The "two-line difference" that pass dismissed **is** the fix.
  The `2.15.1` source was *read*, never *executed*; `~/spot` was never built.
- **"The linked library is `libspot 2.14.4.dev`."** The *headers* were 2.14.4
  (`/usr/local`); the library actually crashing was **2.14.5.dev**, forced onto
  the loader by `LD_LIBRARY_PATH` (`~/.zshrc`). So the source comparison was run
  against a tag that was not the crashing code at all. `CMakeLists.txt` now emits
  `DT_RPATH` (which outranks `LD_LIBRARY_PATH`) so this cannot recur — and the
  standing lesson is: `ldd` the binary before diagnosing, never trust
  `pkg-config --modversion`.

### What the trigger was NOT — every row was an experiment, and all of them hold

The rows below were **correct** and remain so; the root cause explains why none
of them was *the* trigger. All that matters is whether **some root goes
unencoded** before `stop_asap` fires. Sink, root count and `X[!]` were each
merely ways of arranging that — which is exactly why the trigger resisted
pinning.

- **the materialised rejecting sink** — sink-free products crash too.
- **root count alone** — sink-free 5-root products survive, while the 5-root
  *sink* product crashes.
- **`X[!]` / forced continuation** — the corpus trigger `p1 | p6 | F(Gp1 & Fp7)`
  has none. (The `X[!]` correlation was real *within* the sink cases:
  `strategy_finalize` early-exits on accepting terminals via `term & 1` without
  ever touching `global_backprop`, so `!k`/`F(!k)`/`G(!k)` never reach the fault
  and unrealizable goals collapse first.)
- **the trivial `t_out` operand** (all-accepting `bddtrue` self-loop).
- **the goal automaton alone** — no product, no crash.

**`backprop=false`** did survive every observed case, but it was always a
**workaround with a real price** — the header promises backprop is a
*"linear-time resolution"* against `false`'s *"does not have linear
complexity"*. It is now moot, and it was never applied: `src/solve_mtdfa.cpp`
has always passed `true`.

### The sink removal stands — on its own merits

Unaffected by the resolution, and still right:

- it **costs nothing** — Phase 0/Q1 already called the sink "correct, but wasted";
- **plausibly improves faithfulness.** `main.tex` **skips** a non-enabled letter
  (`\cref{def:enabled}`, §107–116); an incomplete automaton literally skips it,
  where the sink was an encoding we had to *argue* equivalent. That argument is
  this PRD's "single load-bearing semantic claim" — dropping the sink may retire
  it rather than discharge it. **`/theory-review` item**, not a claim to assume.

It was **never** a fix for the segfault, and nothing here should be read as
saying so.

### Still open — the `emits_dfa` contract fallout

**Contract impact — a PRD-change event, already applied.** *`emits_dfa` — pinned
specification* mandated the sink and called the result "deterministic **and
complete** by construction"; *Edge cases* said a λ-undefined `q` gets "a single
`bddtrue` edge to the sink". Both were rewritten (sink bullets dropped;
"deterministic but not complete"; λ-undefined `q` now gets **no outgoing
edges**). The frozen **signature** did not move.

**This is the only thing left before the PRD's green checkpoint.** 6 `EmitsDfa`
unit tests assert the superseded contract and are red — notably `GraphAccepts`
throws *"emits_dfa's result is expected complete"*, so the **language oracle
currently cannot run**. That means "dropping the sink leaves the language
unchanged" is **argued, not verified**. `/test-writer` must teach the helper that
a missing edge is a reject, and re-establish the language claim.

These 6 are **not** fallout from the Spot upgrade — confirmed by building
pristine `HEAD` against the old Spot in a throwaway worktree: the same 6 fail.
They date from `e7dcd7b`, which dropped the sink without updating the tests.

### Why the corpus oracle earned its cost — twice over

Kept, because the lesson outlives the bug. The two hand-written fixtures are
`X[!]` rows in the Mealy/weak-X-sensitive region, and `B_XBang_k` **passed**
while `B_XBang_not_k` segfaulted, so the trigger was already narrower than "uses
`X[!]`" — a thinner hand-picked corpus ships this. More important: the
**generated** corpus is the *only* thing that caught the sink-free trigger. Had
it not been in the suite, the sink fix would have turned the suite green and the
real bug would have shipped behind a plausible, wrong root cause. The generator
found a case (`p1 | p6 | F(Gp1 & Fp7)` over an 8-variable partition with a
3-state `t_in`) that no one would have hand-written.

**A second lesson, about process rather than coverage.** The wrong root cause
held for a day because it explained every case then in evidence. Two things
broke it: an *experiment* (`/developer` stashing the sink fix and reproducing the
crash 3/3 anyway), and finally *executing* the allegedly-identical 2.15 source
instead of reading it. Reading source that "looks the same" is not evidence —
and neither is a version number from `pkg-config` when `LD_LIBRARY_PATH` is in
play.
