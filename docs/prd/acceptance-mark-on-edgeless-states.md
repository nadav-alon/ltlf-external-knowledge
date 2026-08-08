# PRD: Acceptance mark on edgeless states

**Status:** draft
**Interface:** adds `detail::ensure_acceptance_readable`; **no** `Synthesis` signature change
**Recommended workflow:** concurrent — the *Interfaces & types* freeze is **high** (the helper is a thin wrapper over `spot::twa_graph::new_edge`, every parameter a Spot type), and `src/`+`include/` vs `tests/` are disjoint territories
**main.tex ref:** `\cref{alg:dfa_product}` line `alg:dfa_product:final`; `\cref{def:consistency}`; §`Transducers` (the partiality paragraph, "In the methods below we dont assume totality of the transducers")

**Gates:**
- [ ] glossary        — new terms in docs/GLOSSARY.md C++ column
- [ ] tests           — unit + oracle coverage
- [ ] code-review     — domain (/code-reviewer) + generic (/code-review)
- [ ] theory-review   — code ↔ math faithfulness vs main.tex

**Unattended-ready:** yes — every decision below was closed in the grill of
2026-08-08. The one speculative element (the predicted `ltlfsynt` divergence) is
handled by the Stop-list rather than by guessing.

## Stop-list

An unattended run must **STOP and report** rather than proceed if any of these
holds. None of them is a decision `/developer` or `/test-writer` may take.

1. **The divergence fixture's observed verdicts differ from the prediction.**
   §"Test oracles" O5 predicts `ltlf-ek-synth` → UNREALIZABLE and `ltlfsynt` →
   REALIZABLE on the $\varphi = X[!]\mathtt{tt}$ witness. If `ltlf-ek-synth`
   reports **REALIZABLE**, the *semantics decision* of this PRD (§"Behaviour",
   reading A) is **falsified** — do **not** adjust the code to match, do **not**
   flip the expectation. Stop, and report the observed pair prominently.
2. **`run_faithfulness_guard` fails on the fixture's $(\Tin, \psiin)$ pair.**
   Then the witness is a $\psiin$ **mis-encoding**, not a soundness boundary —
   the exact 2026-07-05 delay-fixture trap. The divergence claim is void. Stop;
   do not weaken the guard and do not commit the fixture.
3. **Any test fails that is not named in §"Tests this PRD knowingly changes".**
   A verdict change outside that list means the fix reaches further than this
   PRD's analysis. Report the failing test; do not edit it to pass.
4. **Adopting the helper in `nfa_to_dfa` / `reverse_dfa_to_nfa` changes any
   existing verdict.** Those two sites are believed already-correct, so the
   adoption must be a pure refactor. A behaviour change there means the idiom is
   not equivalent to the open-coded version — stop and report.

## Goal

Three of the five `Synthesis` implementations silently drop the acceptance mark
on a product state that has **zero out-edges**, so `spot::twa_graph::state_is_accepting`
— which reads a state-based mark off the state's *first* out-edge — reads the
flag back as `false`. The mark is **lost in transit**, not deliberately
reinterpreted: `\cref{alg:dfa_product}` line `alg:dfa_product:final` defines
$F_P \gets F_D \times Q_{in} \times Q_{out}$ with no exclusion for edgeless
states. The result is that `DfaProduct`, `NfaProduct` and `MtdfaProduct` report
UNREALIZABLE where REALIZABLE is correct, whenever a **partial** $\Tin$ or
$\Tout$ strands the product run — which §`Transducers` explicitly permits ("In
the methods below we dont assume totality of the transducers").

This PRD repairs the class at all four builder sites through one named helper,
restores `emits_dfa`'s original all-states-accepting contract, and ships the
partial-transducer coverage whose absence let a second site hide for ten days
(found 2026-07-17, widened 2026-07-27).

Found by `NfaProduct`'s domain review and deliberately deferred out of
`docs/prd/nfa-product.md` to keep that PRD scoped to Method 1. This PRD does not
supersede any existing PRD; it **revises** the "Edge cases" of
`docs/prd/mtdfa-product.md` (the `emits_dfa` acceptance exception) — see
§"Tests this PRD knowingly changes".

## Ubiquitous-language terms used

All already in `docs/GLOSSARY.md`, spelled exactly — **no glossary gap, so
`/glossary` need not run before `/developer`**:

- **Product** / `ProductState`, `ProductGuards`, `materialize_product` (§"Product").
- **Consistency (`$\cons$`)** / `consistent(...)` (§"Consistency"), and **Enabled** —
  `\cref{def:consistency}`, including its partiality clause "a missing $\delta$ or
  $\lambda$ value is equivalent to an inconsistent letter".
- **Partial transducers** — `Transducer::delta` / `Transducer::lambda` returning
  `std::nullopt`, `emits_region` returning `bddfalse`.
- **Output agreement (emits)** / `emits_dfa`, `emits_region` (§"Output agreement").
- **Goal automaton determinization** / `nfa_to_dfa`; **Goal automaton reversal** /
  `detail::reverse_dfa_to_nfa` — whose glossary entry **already describes this
  idiom** ("gives an edgeless final state an explicit `bddfalse`-guarded
  self-loop so its $F_N$-membership survives a later `state_is_accepting` read").
- **DFA product / NFA product / MTDFA product / MTNFA product / OTF MTDFA
  product** — the five `Synthesis` implementations.

**`detail::ensure_acceptance_readable` deliberately gets NO glossary entry**
(decided in the grill). It is BuDDy/Spot representation mechanics, not a domain
concept — the same call `include/ltlf_ek/transducer.hpp:44` makes for `dict()`.
The glossary's math↔prose↔C++ table has no math column to fill for it.

## Behaviour / semantics (from main.tex)

**The decision, settled 2026-08-08 (reading A).** An edgeless product state
$\langle s, q_{in}, q_{out}\rangle$ — one at which *every* letter is non-enabled,
whether because $\delta$ is undefined, $\lambda$ is undefined, or $\cons$ fails —
is **accepting iff $s \in F_D$**, exactly as `alg:dfa_product:final` states. It
is not special-cased, not widened, and not an input error.

Invariants that must hold, each traced to its source:

1. **`alg:dfa_product:final`:** $F_P = F_D \times Q_{in} \times Q_{out}$. The
   transducer components contribute **no** acceptance condition — they restrict
   transitions only (line `alg:dfa_product:cons`). Therefore edgelessness, which
   is a property of $\delta_{Dprod}$, can never influence $F_P$.
2. **`\cref{def:consistency}`, partiality clause:** a missing $\delta$ **or**
   $\lambda$ value is equivalent to an inconsistent letter. The two sources of an
   edgeless state are therefore **unified** by the paper, and the code must not
   distinguish them (this is why the "λ-undefined rejects, δ-dead accepts" split
   was rejected in the grill).
3. **§`Transducers`, partiality paragraph:** partiality is totalizable "in a way that does not impact
   its behaviour in allowed traces". Reaching an edgeless state is thus a legal
   end of an allowed trace, not an error state.
4. **`emits_dfa`'s language is unchanged** — it always was
   $\{w : \tau\text{'s run on } w \text{ is defined and every letter agrees with }
   \lambda \text{ at its state}\}$. Every state is accepting. The fix makes the
   *encoding* faithful to that language; it does not redefine it.

**Not decided here, and deliberately so.** The alternative reading — that once
the environment can no longer conform to $\Tin$ the assumption $\psiin$ is
vacuously satisfied and the system wins regardless of $s$ — was considered and
**rejected** for the code. It is not dismissed as wrong: it is what the
monolithic reduction $\psiin \rightarrow (\varphi \land \psiout)$ implies, and
whether it or reading A is correct bottoms out on the **open termination
question** of the `\na` following `\cref{def:probDef}` (who decides when the
trace ends). That is a
paper-level question; see §"Open theory questions touched" and oracle O5, which
*measures* the disagreement instead of resolving it.

## Interfaces & types

**Freeze confidence: high.** Every parameter is an existing Spot type; nothing is
invented. The helper is a thin wrapper over `twa_graph::new_edge`.

New header `include/ltlf_ek/detail/acceptance.hpp`, namespace `ltlf_ek::detail`:

```cpp
// Spot carries state-based acceptance ON a state's out-edges, and
// twa_graph::state_is_accepting reads the mark off the state's FIRST out-edge.
// A state with zero out-edges therefore cannot carry a mark: the flag is
// silently read back as false.  Give such a state a bddfalse-guarded self-loop
// --- never taken by any real letter, so the language is unchanged, but it is
// an edge whose mark Spot can read.
//
// No-op when `g` already has at least one out-edge from `state`, and a no-op
// when `mark` is empty (a non-accepting edgeless state needs no carrier).
// Precondition: `g` uses state-based acceptance (prop_state_acc(true)).
void ensure_acceptance_readable(const spot::twa_graph_ptr& g, unsigned state,
                                spot::acc_cond::mark_t mark);
```

**Behaviour, pinned (this is a bespoke helper, so nothing is left to discover):**

- **No-op condition:** returns without modifying `g` if `g->out(state)` is
  non-empty, **or** if `mark == spot::acc_cond::mark_t{}`. Both conditions are
  checked; neither is an error.
- **Effect otherwise:** exactly one `g->new_edge(state, state, bddfalse, mark)`.
- **Idempotent:** a second call on the same state is a no-op, because the first
  call left an out-edge.
- **Determinism:** no iteration order dependence; the added edge is a function of
  `(state, mark)` alone.
- **Return type:** `void`. Callers do not branch on whether a carrier was added.
- **It does not compute acceptance.** The caller has already decided
  `state`'s $F_P$-membership; this helper only makes that decision readable.

**Call sites — all four builders (the two broken, and the two that already
open-code the idiom, which adopt it so the convention has one implementation):**

| File | Function | Action |
|---|---|---|
| `src/product.cpp` | `materialize_product` | **fix** — call after the guards loop for each state whose `acc` is true |
| `src/emits_dfa.cpp` | `emits_dfa` | **fix** — call after the `delta_edges` loop for each discovered state (every state is accepting) |
| `src/nfa_to_dfa.cpp` | `nfa_to_dfa` | **adopt** — replace the open-coded `if (acc && !added_real_edge)` self-loop |
| `src/nfa_to_dfa.cpp` | `detail::reverse_dfa_to_nfa` | **adopt** — replace its open-coded equivalent |

**If implementation proves this contract wrong:** that is a PRD-change event —
update this section and propagate to the in-flight test branch; the developer
does not silently re-shape the interface on its own branch.

## Edge cases

- **`materialize_product`, non-accepting edgeless state.** No carrier is added
  (the `mark_t{}` no-op). The state stays a genuine dead end and the system loses
  there. This is reading A's discriminating case — see O5.
- **`emits_dfa`, λ undefined at the initial state.** `emits_region` is `bddfalse`,
  so the state is edgeless *and* accepting; it now carries a self-loop, and
  `emits_dfa` **accepts the empty word**. Harmless for every current caller
  because $L(\varphi)$ excludes $\varepsilon$ — but that must be **re-verified by
  test**, not assumed (see O3), since it is the assumption the old exception
  rested on.
- **`emits_dfa`, δ-dead state.** Same treatment; unified by `\cref{def:consistency}`.
- **Partial $\Tout$, not only partial $\Tin$.** The pre-existing fixture only
  exercises a δ-dead $\Tin$. Both $\Tout$ cells of the matrix in O1 are new and
  are the point of the coverage.
- **Downstream consumers of a `bddfalse` edge.** `emits_dfa`'s output is fed to
  `spot::twadfa_to_mtdfa` (`MtdfaProduct`), and `materialize_product`'s to
  `solve_dfa`. A `bddfalse`-guarded edge must be inert in both. `nfa_to_dfa`
  already relies on this, so it is believed safe — but the developer must
  **confirm** it rather than assume, and Stop-list item 3 applies if a
  downstream test moves.
- **`tests/dfa_product_test.cpp:268`** carries a comment about "wholesale failure
  of the partial transducer". Re-read it before touching the code; if its
  expectation encodes the old (wrong) verdict, Stop-list item 3 applies.
- **Empty $\Ifree$ / empty $\Oknown$ partitions.** Unaffected — the helper never
  inspects the alphabet.

## Tests this PRD knowingly changes

Exactly three existing tests change. Anything else moving triggers Stop-list 3.

1. `MtnfaProductExpectedDivergence.MaterializeProductBugMakes...` —
   `tests/mtnfa_product_test.cpp:1025,1029`: both `EXPECT_FALSE` become
   `EXPECT_TRUE`, and the "KNOWN BUG / this PINS the bug" comments become
   assertions of correct behaviour. The test name should be renamed off
   "ExpectedDivergence", since after the fix all five methods agree.
2. `EmitsDfa.UndefinedAtStateHasNoOutgoingEdgesForAnyLetter` —
   `tests/emits_dfa_test.cpp:368`: the state now has **exactly one** out-edge,
   `bddfalse`-guarded, self-looping. Still one state (no sink). Every non-empty
   word is still rejected.
3. `EmitsDfa.AcceptsTheEmptyWordAcrossEveryFixture` —
   `tests/emits_dfa_test.cpp:346`: the `EXPECT_FALSE` for
   `BuildUndefinedAtStateFixture` becomes `EXPECT_TRUE`, and the exception
   paragraph (`:335-342`) is deleted. `EmitsDfa.LanguageMatchesTheRunOfTauClaim`
   (`:419`) drops its `if (word.empty() && ...) continue;` exemption.

## Test oracles (for /test-writer)

**O1 — the partiality matrix, cross-method agreement (the core oracle).** Four
fixtures, the cartesian product of {δ-dead, λ-undefined} × {$\Tin$, $\Tout$},
each built as an `OutputLabeledTransducer` (δ-dead = a state with no out-edges;
λ-undefined = `bddfalse` in the lambda vector, as
`tests/mtnfa_product_test.cpp:1010` already does). Run **all five** `Synthesis`
implementations on each and assert they return the **same** verdict.

This oracle is load-bearing here in a way it has never been: `MtnfaProduct` and
`OtfMtdfaProduct` are **immune** to the class (both key acceptance on the
incoming transition — `Mtnfa::accepting` and the $2j+b$ terminal at
`src/otf_mtdfa_product.cpp:87` respectively, and the latter handles
`cons == bddfalse` explicitly at `:171-175`), while `DfaProduct`, `NfaProduct`
(via `materialize_product`) and `MtdfaProduct` (via `emits_dfa`) are **broken**.
So the oracle is genuinely discriminating — 3 vs 2, not 5 failing identically,
which is precisely why it was blind to this bug while only the explicit route
existed.

**O2 — verdict direction, per fixture.** Agreement alone is satisfiable by all
five being wrong together. Each fixture must additionally pin the *expected*
verdict from `alg:dfa_product:final` by hand, with the reasoning in a comment.
The pre-existing repro ($\varphi = b$, $\Ofree = \{b\}$, δ-dead $\Tin$ state 1)
is REALIZABLE and is the model for the other three.

**O3 — `emits_dfa` language equivalence, exemption removed.**
`EmitsDfa.LanguageMatchesTheRunOfTauClaim` must now pass on **every** fixture for
**every** word including $\varepsilon$, with no `continue`. Its existing negative
control (`EmitsDfa.LanguageOracleIsDiscriminating`) must still fail against the
wrong reference — i.e. the oracle must not have become vacuous.

**O4 — the helper's own unit tests.** Each pinned behaviour in §"Interfaces &
types" gets one test: no-op when out-edges exist; no-op on an empty mark;
adds exactly one `bddfalse` self-loop otherwise; idempotent under a second call;
`state_is_accepting` reads `true` afterwards where it read `false` before.

**O5 — the `ltlfsynt` expected-divergence fixture (`tests/ltlfsynt_oracle_test.cpp`).**
The one speculative oracle. Witness:

- $\varphi = X[!]\mathtt{tt}$ (the trace must have length $\ge 2$),
- $\Tin$: $\delta(0,\cdot) = 1$, state 1 **δ-dead**, $\lambda_{in}(0) = a$,
- $\psiin = (k \leftrightarrow a) \wedge \lnot X[!]\mathtt{tt}$,
- $\Tout$ = `trivial_transducer`, $\psiout = \top$.

**Precondition, load-bearing:** `run_faithfulness_guard` must **pass** on
$(\Tin, \psiin)$. If it fails, the witness is a mis-encoding rather than a
boundary and Stop-list 2 applies — this is exactly how the 2026-07-05 delay
witness was retired.

**Predicted:** `ltlf-ek-synth` → UNREALIZABLE (reading A: after one step the
product state is edgeless and its goal component is non-accepting, so the system
is stuck), `ltlfsynt` on $\psiin \rightarrow \varphi$ → REALIZABLE (the system
continues to length 2, $\psiin$ becomes false, the implication holds vacuously).

If observed, this is the project's **first known divergence witness** for the
equirealizability conjecture — `docs/BACKLOG.md` "Prove the monolithic
reduction" currently records that none exists. Commit it with an `IMPORTANT`
header in the style of `MtnfaProductExpectedDivergence`: it **pins a known
boundary, not correct behaviour**, and must not be "fixed" by a later reader.
Stop-list 1 governs any other observed pair.

## Open theory questions touched

Flagged for `/theory-review`; **none** may be resolved by an unattended run.

1. **The `\na` following `\cref{def:probDef}` — who decides when the trace ends.** This PRD's
   semantics decision is a *consequence* of that open question, not independent
   of it. Under system-controlled termination the system continues past the
   assumption boundary and reading A is wrong; under "every agreeing trace must
   satisfy $\varphi$" reading A is right. Recorded, not settled.
2. **The equirealizability conjecture** (the `\na` following
   `\cref{def:probDefTransducer}`). O5 is designed to produce evidence *against* it on the
   partial-transducer fragment. A `\cl` note recording the witness belongs on
   that conjecture — but `latex/` is a submodule that builds only on Overleaf, so
   **draft the note into `docs/BACKLOG.md`** and leave `main.tex` untouched from
   a worktree (per `CLAUDE.md`).
3. **`\cref{alg:dfa_product}` is silent on edgeless states.** It is only silent,
   not ambiguous — `alg:dfa_product:final` is a plain cartesian product. But a
   `\cl` note making the edgeless case explicit would stop the next reader from
   rediscovering this. Draft, do not apply.
4. **Whether the generated corpus wants a partial-transducer (Case B) regime.**
   Deliberately **out of scope**: O5 predicts that partial $\Tin$ is exactly
   where `ltlf-ek-synth` and `ltlfsynt` diverge, so enabling Case B would make
   the corpus differential oracle fail legitimately. Case B cannot land until
   that boundary is characterised. Stays a `docs/BACKLOG.md` item.

## Definition of done

- `include/ltlf_ek/detail/acceptance.hpp` exists with the frozen signature; all
  four call sites use it and no site open-codes the idiom any more
  (`grep -rn "bddfalse" src/ | grep new_edge` returns only the helper).
- `cmake --build build -j` succeeds.
- `ctest --test-dir build` is **fully green**, including: the two flipped
  `EXPECT_TRUE`s in the renamed `MtnfaProduct…` fixture; the three knowingly
  changed tests in §"Tests this PRD knowingly changes"; and O1–O5.
- The pre-existing suite passes with **no edits beyond those three tests** —
  Stop-list 3 is the regression bar.
- `docs/prd/mtdfa-product.md`'s "Edge cases" entry recording the `emits_dfa`
  acceptance exception is updated to say the exception is **retired** and why,
  with a pointer to this PRD.
- `docs/BACKLOG.md`: the "#1 acceptance-mark bug" item moves to **Done** with the
  outcome, and the O5 result is recorded under "Prove the monolithic reduction".
- All four gates ticked with refs; `Status:` set to `implemented — <commit/PR>`.
