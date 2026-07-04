# PRD: DfaProduct (Method 2 — DFA product)

**Status:** implemented — `src/dfa_product.cpp` + `src/ltlf_to_dfa.cpp` + `src/solve_dfa.cpp` (+ headers, `Transducer::dict()`) (branch `master`, uncommitted)
**Interface:** implements `Synthesis` as `DfaProduct`; adds the shared black-box helpers `ltlf_to_dfa` (`LtlfToDfa`) and `solve_dfa` (`SolveDfa`)
**main.tex ref:** §`fulldfa` (Method 2), Algorithm `alg:dfa_product` ("DFA Product"); relies on `def:enabled`, `lem:sink_skip`, `def:probDef`/`def:probDefTransducer`

**Gates:**
- [x] glossary        — new terms in docs/GLOSSARY.md C++ column (`ltlf_to_dfa`, `solve_dfa`, `ProductState`, `kSink` all already present; no new domain identifier introduced — `Transducer::dict()` is Spot/BuDDy infrastructure, not a domain concept)
- [x] tests           — `tests/ltlf_to_dfa_test.cpp` + `tests/dfa_product_test.cpp` (59 tests green; branch `master`, uncommitted). Covers: LtlfToDfa (dict/alphabet/determinism/completeness/state-based-acc/final-state classification incl. non-empty-trace `1`/`0`), realizability verdicts, empty-Ofree, validation throws, Mealy convention, monolithic baseline (turn-order-invariant subset), knowledge-sensitivity flip. **Deferred:** the trace-level controller verifier (oracle #2 — needs reachability-under-adversarial-env, not a naive language-inclusion check) and cross-method equivalence (needs Methods 1/3)
- [x] code-review     — domain (/code-reviewer, branch `master` uncommitted): no must-fix; invariants (⊥-sink routing, `enabled`=δ∧cons, F_P state-based Büchi, shared dict), glossary, interface conformance all clean. The one "consider" is **resolved**: `solve_dfa` now throws if the sink named prop is absent, and the key is the shared `kSinkProperty` constant (test `SolveDfa.ThrowsWhenProductLacksSinkProperty`). Generic /code-review not yet run.
- [x] theory-review   — code ↔ math faithfulness (/code-reviewer → theory-reviewer, branch `master` uncommitted): **no code-bug**; product/F_P/sink, `enabled`, projection+drop-sink+determinism, Mealy/§86, reachability-lift all faithful. The two `underspecified` main.tex doc-gaps are **resolved**: `\cl` notes added (drop-sink necessity after §fulldfa `\cl`; who-fixes-trace-length after `def:probDef`). `lem:sink_skip` `\na[inline]{Rewrite}` caveat engaged (author's own marker; proof body relied-on is valid).

## Goal

Implement Method 2 end-to-end: the first complete `$\varphi$` → `Controller`
synthesis path in the project, and (per `main.tex` §`fulldfa`) the "good
comparison point" against which the other methods are measured. Given a Goal
formula `$\varphi$`, a `VariablePartition`, and the two external knowledge
transducers `$\Tin,\Tout$`, `DfaProduct::synthesize` builds the full DFA `$A$`
for `$\varphi$`, forms the product `$P = A\times Q_{in}\times Q_{out}\cup\{\bot\}$`
that routes every non-`$\cons$` letter to the failing sink `$\bot$`, solves the
resulting game, and returns a `Controller` (or `nullopt` if unrealizable).

Because this is the first method to run to completion, it also introduces the
two reusable black boxes named in `alg:dfa_product` — `ltlf_to_dfa`
(`LtlfToDfa`) and `solve_dfa` (`SolveDfa`) — as thin wrappers over Spot, in
their own headers so Methods 1 and 3 can reuse them.

## Ubiquitous-language terms used

All from `docs/GLOSSARY.md` unless flagged:

- **DFA product** / `DfaProduct` (§"The five methods", Method 2).
- **Product** / `ProductState` and the `$P$` construction (§"Product").
- **Sink (⊥)** / `kSink` — absorbing non-accepting self-loop (§"Sink").
- **Consistency (cons)** / `consistent(t_in, q_in, t_out, q_out, v)` (§"Consistency").
- **Enabled** — `def:enabled`; `cons` + delta/lambda-definedness (used in prose).
- **Controller** / `Controller`, `$T_C$`, `$\lambda_C:Q\times2^{\mathcal I}\to2^{\Ofree}$` (§"Controller").
- **NFA / DFA for the Goal** / `LtlfToDfa` wrapper (§"NFA / DFA for the Goal").
- **Free / known inputs & outputs** / `VariablePartition` accessors (§"…split").
- **Letter**, **Cube**, **Σ₀/Σ₁** / `sigma0_cube`, `sigma1_cube` (§"Automata & Transducers").
- **Transducer**, **delta**, **lambda** / `Transducer::delta` / `::lambda`.

**Glossary gaps to close (run `/glossary` before `/developer` finalises these):**

- **`LtlfToDfa`** — §"NFA / DFA for the Goal" names the *wrapper* but gives it no
  canonical C++ identifier. This PRD proposes `ltlf_to_dfa`; ratify it.
- **`SolveDfa`** — `alg:dfa_product` line `alg:dfa_product:solve` uses
  `\algname{SolveDfa}`, but it has **no glossary entry at all**. This PRD
  proposes `solve_dfa`; add the entry.

## Behaviour / semantics (from main.tex)

The construction is `alg:dfa_product`, to be matched line-for-line. Let

```
A = (S_D, 2^{I∪O}, δ_D, s_{D,0}, F_D)         (the DFA for φ)
```

Then the product DFA is (`main.tex` §`fulldfa`)

```
P = (S_D × Q_in × Q_out ∪ {⊥},  2^{I∪O},  δ_Dprod,  ⟨s_{D,0}, q_{in,0}, q_{out,0}⟩,  F_D × Q_in × Q_out)
```

with, for every `$v\in2^{\mathcal I\cup\mathcal O}$`,

```
δ_Dprod(⟨s, q_in, q_out⟩, v) = ⟨δ_D(s,v), δ_in(q_in,v), δ_out(q_out,v)⟩   if cons(q_in, q_out, v)
                             = ⊥                                          otherwise
δ_Dprod(⊥, v)                = ⊥                                          for every v   (self-loop)
```

Invariants that MUST hold:

1. **`$\cons$` is the only filter, and `enabled` subsumes it** (`def:enabled`):
   a letter takes the product branch iff `$\delta_{in},\lambda_{in},\delta_{out},
   \lambda_{out}$` are all defined at `$v$` **and** `$\cons$` holds. Dereference
   `delta` only after the enabled test passes (never call `*t_in.delta(...)`
   before checking it is non-`nullopt`) — see `consistency.hpp` and
   `transducer.hpp`. A non-enabled letter → `$\bot$`.
2. **Final set** `$F_P = F_D\times Q_{in}\times Q_{out}$` (`alg:dfa_product:final`).
   The product state is accepting iff its DFA component is; `$\bot\notin F_P$`.
3. **Sink** `kSink` self-loops on every letter and is non-accepting
   (`alg:dfa_product:self_loop`).
4. **Sink/skip equivalence** (`lem:sink_skip`): the `$\bot$`-sink is provably
   **unreachable** in real play (the governed variables are pinned, not free), so
   it lies outside the winning arena. Correctness of Method 2 rests on this
   lemma — which is itself marked `\na[inline]{Rewrite}` in `main.tex` (see Open
   theory questions).

**Construction strategy (decided):** build `$P$` as an explicit
`spot::twa_graph` over `ProductState = (s, q_in, q_out)` plus `kSink`, reusing
the same `twa_graph` substrate `OutputLabeledTransducer` already uses. The
transition loop **enumerates full letters** `$v\in2^{\mathcal I\cup\mathcal O}$`
exactly as `alg:dfa_product`'s `\For`, drives the per-letter APIs
(`t_in.delta`/`t_out.delta`/`consistent`), and **groups letters that share a
destination into one guarded edge** (the OR of their letters). The exponential
letter loop is the accepted, documented baseline cost — the symbolic
alternative is deferred (docs/BACKLOG.md → "Symbolic DFA-product construction").

**`LtlfToDfa` (decided):** `ltlf_to_dfa(phi, dict)` = `spot::ltlf_to_mtdfa`
followed by `mtdfa::as_twa()`, yielding an explicit deterministic `twa_graph`
whose finiteness lives in **acceptance marks, not an extra AP** — so the DFA's
alphabet is exactly `$\mathcal I\cup\mathcal O$` and the product/`cons` stay
clean. The DFA MUST be built on the **same `bdd_dict`** as `$\Tin,\Tout$`.

**`SolveDfa` (decided, with a deferred sub-decision):** `solve_dfa(product,
vars)` marks `$\Ofree$` as the synthesis outputs, forms a turn-ordered game
arena (Spot: `set_synthesis_outputs` → `split_2step` → `solve_game`), and — if
the system wins from the initial state — extracts the controller via
`spot::solved_game_to_mealy`, storing that strategy `twa_graph` in
`Controller::strategy`. Otherwise returns `nullopt` (unrealizable).

- **Pinned known variables (resolved by `/theory-review`):** `$\Iknown,\Oknown$`
  are *pinned*, not free (`lem:sink_skip`: "the only free moves are `$\Ifree$`
  and `$\Ofree$`"). `solve_dfa` existentially projects `$\Iknown\cup\Oknown$` out
  of the product guards before splitting (safe because `$\cons$` makes each
  pinned value unique given the free choices), and solves the game over
  `$\Ifree$` (env) vs `$\Ofree$` (system). **This does not conflict with
  `def:probDefTransducer`'s `$2^{\mathcal I}$` interface:** `$\Iknown$` is
  informationally redundant to the controller — `$v\cap\Iknown=\lambda_{in}(q_{in},
  v\cap\Ifree)$` and `$q_{in}$` is carried in the product state — so the
  `$\Ifree$`-controller **lifts** to the required
  `$Q_C\times 2^{\mathcal I}\to 2^{\Ofree}$` interface by ignoring the
  `$\Iknown$` bits, yielding a faithful `$T_C$`. `synthesize` MUST perform that
  lift so the returned `Controller` conforms to `def:probDefTransducer`. See the
  `\cl` note in `main.tex` §`fulldfa` (after `lem:sink_skip`).

**Controller (decided):** `synthesize` stores the `spot::solved_game_to_mealy`
strategy `twa_graph` directly in `Controller::strategy`; `$\lambda_C$` is **read
off that machine's edges** (the `$\Ofree$` slice of the matched edge guard) —
the same idiom `OutputLabeledTransducer` uses (delta via edges, output derived).
No new `Controller` fields beyond what the verifier oracle needs to simulate it.
(Note: `spot::solved_game_to_mealy` returns what Spot calls a "Mealy machine";
that is Spot's API name for the strategy graph — our domain term stays
`Controller`, never "Mealy machine", per the glossary.)

## Interfaces & types

New / touched signatures (snake_case free functions, matching `parse_transducer`
/ `consistent` / `collect_aps` convention):

```cpp
// include/ltlf_ek/ltlf_to_dfa.hpp   (new)  — LtlfToDfa black box
// Deterministic DFA for the LTLf Goal φ, built on `dict`; finiteness in
// acceptance marks (no extra AP).  Thin wrapper: ltlf_to_mtdfa + mtdfa::as_twa.
spot::twa_graph_ptr ltlf_to_dfa(const spot::formula& phi,
                                const spot::bdd_dict_ptr& dict);

// include/ltlf_ek/solve_dfa.hpp     (new)  — SolveDfa black box
// Solve the product game; nullopt = unrealizable.  `vars` supplies the Ofree
// output partition.  Solves over the projected Ifree/Ofree arena, then lifts
// the controller to the 2^I interface of def:probDefTransducer (see the
// resolved arena-partition note in main.tex §fulldfa).
std::optional<Controller> solve_dfa(const spot::twa_graph_ptr& product,
                                    const VariablePartition& vars);

// include/ltlf_ek/dfa_product.hpp   (exists; implement the stub)
class DfaProduct final : public Synthesis {
  std::optional<Controller> synthesize(const spot::formula& phi,
                                       const VariablePartition& vars,
                                       const Transducer& t_in,
                                       const Transducer& t_out) override;
};
```

- **`ProductState`** — the `(s, q_in, q_out)` key (glossary §"Product"). Introduce
  as a small struct/tuple with a state-index map into the product `twa_graph`;
  reserve one index for `kSink`.
- **`kSink`** — the reserved sink state index (glossary §"Sink").
- **Reused as-is:** `consistent(...)`, `OutputLabeledTransducer`,
  `VariablePartition` (`input_free/…/output_known`, `inputs()`, `known()`),
  `Controller`.
- **Black boxes — implemented now (scope = full end-to-end):** both `ltlf_to_dfa`
  and `solve_dfa` are real, not stubs.
- **`bdd_dict` discipline:** `synthesize` builds the DFA on `$\Tin$`/`$\Tout$`'s
  shared `bdd_dict`; the product edge guards, `sigma*_cube`s, and every letter
  must share it.

## Edge cases

- **Unrealizable** → `synthesize` returns `nullopt` (system loses the initial
  state of the game).
- **Trivially-false `$\varphi$`** (`$\bot$` / rejecting DFA) → unrealizable →
  `nullopt`. **Trivially-true `$\varphi$`** (`$\top$`) → single accepting DFA
  state → realizable with any output.
- **Empty `$\mathcal V$` (no knowledge, `$\Iknown=\Oknown=\emptyset$`)** →
  `$\cons$` is trivially true, `$P$` collapses to `$A$` crossed with
  single-state transducers → the **monolithic baseline** regime.
- **Empty `$\Ofree$`** (controller controls nothing) → degenerate but solvable;
  realizable iff `$\varphi$` holds under the pinned strategies for every env
  input; the strategy has empty output. **Empty `$\Ifree$`** → env has no move;
  likewise solvable.
- **Sink is constructed but unreachable in play** (`lem:sink_skip`) — build it
  anyway (self-loop, non-accepting); do not "optimise it away", so the graph
  matches `alg:dfa_product`.
- **Partial transducers** — non-enabled (undefined `delta`/`lambda`) letters are
  routed to `kSink`, exactly like a `$\cons$` failure (`def:enabled`; resolved
  in glossary "Open theory questions" → Case-A regime).
- **Validation policy (decided):** up front, throw `std::invalid_argument` with a
  clear message when (a) an atomic proposition of `$\varphi$` is outside
  `$\mathcal I\cup\mathcal O$` (`collect_aps(phi)` ⊄ partition), or (b) the DFA
  and the two transducers do not share one `bdd_dict`. Assume the deeper
  transducer contracts (delta determinism, lambda functionality, state coverage)
  — `OutputLabeledTransducer` / `parse_transducer` already own those.

## Test oracles (for /test-writer)

1. **Unit fixtures** — hand-built tiny explicit DFA + small `$\Tin/\Tout$` with a
   known product structure and realizability verdict: at least one realizable and
   one unrealizable instance, the empty-`$\mathcal V$` collapse, and a case
   asserting the sink is present, self-looping, non-accepting, and unreachable.
2. **Controller verifier** (linchpin correctness oracle) — check the `def:probDef`
   postcondition: every trace that agrees with `$\Tin,\Tout,T_C$` satisfies
   `$\varphi$`. Implement by composing controller ∩ `$\Tin$` ∩ `$\Tout$` ∩
   `$\neg\varphi$` and asserting language emptiness. New, reusable by every later
   method.
3. **Monolithic baseline (empty `$\mathcal V$`)** — with trivial transducers,
   `DfaProduct`'s realizability verdict must match plain LTLf synthesis on
   `$\varphi$` (Spot's own game solve on the bare DFA / `ltlsynt`-equivalent).
4. **Knowledge-sensitivity metamorphic oracle** — the same `$\varphi$` flips
   verdict when a variable moves from free to known:
   - `$\varphi = (\text{X}\,i)\leftrightarrow o$`, `$\mathcal I=\{i\}$`,
     `$\mathcal O=\{o\}$`.
   - With `$i\in\Ifree$` (no knowledge): **unrealizable** — the system cannot
     predict the next input.
   - With `$i\in\Iknown$` and `$\Tin$` committing `$G(i)$` (always `$i$`):
     **realizable** — the controller knows `$\text{X}\,i$` and sets `$o$`.
   - Assert the verdict flip, and (via oracle 2) that the realizable controller
     verifies.

Cross-method metamorphic equivalence is **not yet available** (Method 2 is the
only method); add it once Method 1 / 3.x land.

## Open theory questions touched

- **Arena input partition** (RESOLVED by `/theory-review`, 2026-07-04) — solve the
  game over the projected `$\Ifree$`/`$\Ofree$` arena, then **lift** the resulting
  controller to `def:probDefTransducer`'s `$2^{\mathcal I}$` interface (ignore the
  redundant `$\Iknown$` bits). Sound because `$\Iknown$` is a function of
  `$(\Ifree, q_{in})$` already in the state, so projection loses no winning
  strategy (same pinning as `lem:sink_skip`). Recorded as a `\cl` note in
  `main.tex` §`fulldfa`. **Actionable for `/developer`:** `synthesize` must apply
  the lift. The coupled `$F_P$`-reachability → `solve_game` acceptance encoding is
  a normal implementation detail, no theory blocker.
- **`lem:sink_skip` is marked `\na[inline]{Rewrite}`** in `main.tex` — Method 2's
  correctness (that `$\bot$` is unreachable and sink/skip are interchangeable)
  rests on this lemma while its proof is flagged for rewrite. Confirm the lemma
  holds as relied upon.
- `FP` (forward progression) and aggregation stubs are **not** touched — Method 2
  uses the explicit DFA, no progression.

## Definition of done

- `include/ltlf_ek/ltlf_to_dfa.hpp` + `src/ltlf_to_dfa.cpp`,
  `include/ltlf_ek/solve_dfa.hpp` + `src/solve_dfa.cpp`, and the implemented
  `src/dfa_product.cpp` compile (CMake, Spot via pkg-config) with no stub throw
  remaining.
- `DfaProduct::synthesize` runs `$\varphi$` → `Controller` end-to-end: returns a
  `Controller` on realizable inputs and `nullopt` on unrealizable ones.
- Product is the explicit `twa_graph` of `alg:dfa_product`: `$F_P$` correct,
  `kSink` self-looping/non-accepting, `$\cons$`/enabled the only filter, built on
  the shared `bdd_dict`.
- All four test oracles pass (unit fixtures, controller verifier, monolithic
  baseline, knowledge-sensitivity flip).
- Glossary gaps closed via `/glossary`: canonical C++ identifiers for
  `LtlfToDfa` (`ltlf_to_dfa`) and `SolveDfa` (`solve_dfa`) added.
- The controller returned by `synthesize` conforms to `def:probDefTransducer`'s
  `$2^{\mathcal I}$` interface: the game is solved over `$\Ifree$`/`$\Ofree$` and
  the strategy **lifted** to `$2^{\mathcal I}$` (per the resolved arena-partition
  theory question / the `\cl` note in `main.tex` §`fulldfa`).

## Developer comments / PRD disagreements

- **2026-07-04 — `Transducer::dict()` accessor added (interface extension).**
  `synthesize` receives only `const Transducer&`, but the PRD requires it to
  build the Goal DFA on `$\Tin,\Tout$`'s shared `bdd_dict` (and to validate the
  two transducers agree on one). The base `Transducer` exposed no way to reach
  that dict. Rather than `dynamic_cast` to `OutputLabeledTransducer` (breaking
  the abstraction), I added `virtual spot::bdd_dict_ptr dict() const = 0;` to
  `Transducer`, implemented as `delta_dfa_->get_dict()`. This is Spot/BuDDy
  infrastructure, not a domain concept, so it takes no glossary entry (documented
  as such at the declaration). Methods 1 & 3 will reuse it.

- **2026-07-04 — Reachability encoding of `SolveDfa` (implementation detail, as
  the PRD anticipated).** `solve_dfa` realises the `$F_P$`-reachability objective
  as a Büchi game by making every final product state an **absorbing accepting
  self-loop** (the standard reachability→Büchi reduction), then running
  `set_synthesis_outputs`→`split_2step`→`solve_game`→`solved_game_to_mealy`. The
  `$\Iknown,\Oknown$` projection is done by `bdd_exist` over the known cube on
  each guard, **and** by dropping edges into `kSink` (unreachable by
  `lem:sink_skip`) — dropping is necessary because the *projected* sink guard
  would otherwise become satisfiable for some `$(\Ifree,\Ofree)$` and spuriously
  re-admit `$\bot$`. `kSink` is located via the `twa_graph` named property
  `"ltlf-ek-sink"` set by the product builder (the `solve_dfa` signature takes no
  sink index). This is faithful to the PRD's decided pipeline; recorded because
  the drop-`kSink` step is not spelled out in the PRD's projection prose.

- **2026-07-04 — Oracle #4 (knowledge-sensitivity flip) does NOT flip as written;
  its example rests on strong-next intuition.** The PRD's oracle uses
  `$\varphi=(\text{X}\,i)\leftrightarrow o$` and predicts *unrealizable* when
  `$i\in\Ifree$`. Verified end-to-end, that instance is **realizable** even with
  no knowledge: Spot's `ltlf_to_mtdfa` reads `X` as **weak** next, and LTLf
  synthesis lets the system terminate the trace — so the system stops at length 1
  (no next step), where `$\text{X}\,i$` is vacuously true and it sets `$o$`
  accordingly. Confirmed by inspecting the DFA: after one letter the `X i`
  automaton is already in an accepting state. The verdict therefore does not flip
  when `$i$` moves to `$\Iknown$` (both realizable). A genuine flip needs a
  formula unrealizable under weak-next/early-stop — e.g. use **strong** next
  `X[!]`: `$\varphi=(\text{X[!]}\,i)\leftrightarrow o$` (or a formula forcing the
  trace to continue past where the input matters). Left for `/test-writer` to pick
  a correct discriminating formula; the pipeline itself is verified sound on the
  adversarial cases (`X[!] i`, `i & X[!] i` → unrealizable; `X[!] o`, `G(i->o)` →
  realizable).

- **2026-07-04 — flagged for `/theory-review`:** `SolveDfa` commits to the
  standard **system-controls-termination reachability** semantics of LTLf
  synthesis (system may stop the trace at any accepting state). `def:probDef`
  quantifies over "every trace that agrees" without stating who fixes the trace
  length; the reachability reading is the mainstream one (De Giacomo–Vardi) and is
  what "solve the resulting game" in `alg:dfa_product` implies, but it should be
  confirmed against `def:probDef`, since it is what makes weak-`X` formulas like
  the oracle-#4 example realizable. (No divergence from an explicit `main.tex`
  statement — this is a semantic gap being filled the standard way.)
