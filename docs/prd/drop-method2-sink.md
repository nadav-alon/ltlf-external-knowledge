# PRD: Drop the ⊥-sink from Method 2 (DFA product)

**Status:** implemented — `src/dfa_product.cpp` + `src/solve_dfa.cpp` (+ headers `dfa_product.hpp`, `solve_dfa.hpp`, `transducer.hpp`) + `tests/dfa_product_test.cpp` (branch `master`, uncommitted)
**Interface:** revises `DfaProduct` + `solve_dfa` (no signature change); removes the `kSinkProperty` plumbing contract
**main.tex ref:** §`fulldfa` (Method 2), Algorithm `alg:dfa_product`; deletes `lem:sink_skip`
**Revises:** `docs/prd/dfa-product.md` (implemented Method-2 PRD) — sink sections only; the rest stands

**Gates:**
- [x] glossary        — "Sink (⊥)" entry deleted; "Product" + "Partial transducers" notes de-sinked (now point to `\cref{def:consistency}`) (ref: `docs/GLOSSARY.md`, 2026-07-05)
- [x] tests           — sink-contract test deleted, suite green (163/163, one pre-existing DISABLED_ excluded-class test unaffected); `/test-writer` added two replacement tests covering the sink-free skip contract directly (`tests/dfa_product_test.cpp`, uncommitted at review time)
- [x] code-review     — domain (/code-reviewer): no must-fix, invariants/glossary/interface clean; generic (/code-review, high): no findings (removed-behavior audit confirms the sink-free arena is identical modulo the unreachable sink) (2026-07-05, branch `master` uncommitted)
- [x] theory-review   — main.tex de-sinking of §`fulldfa`, `alg:dfa_product`, and deletion of `lem:sink_skip` (ref: `latex/main.tex` §`fulldfa` Patches 1-8, 2026-07-05)

## Goal

Remove the failing sink `$\bot$` (`kSink`) from Method 2 everywhere it appears —
code, tests, docs, and the paper. Per the author, the sink was a **later
accretion** (added after an AI-chatbot conversation); it was **not** in the
original theory. Method 2's identity is a **direct product with the DFA `$A$`**
(as opposed to determinizing the Method-1 NFA), **not** a *total* DFA product —
so the totalisation the sink provides is not central to the method's existence.

Mechanically the sink is already dead weight in code: `DfaProduct` builds
`kSink`, routes every non-`$\cons$` letter to it, records its index in the
`"ltlf-ek-sink"` named property, and then `solve_dfa` **drops every edge into it**
before handing the arena to Spot (`src/solve_dfa.cpp:70-75`). The game Spot
actually solves is byte-for-byte identical whether or not the sink was ever
built. This PRD replaces "route non-`$\cons$` to `$\bot$`" with **skip**
(contribute nothing), exactly as Methods 1 and 3 already do (`def:consistency`,
§`nfa`/§`otfdfa`). It is a **behaviour-preserving refactor** — the reason its
test oracle is "the existing suite still passes unchanged."

This supersedes only the sink-related parts of `docs/prd/dfa-product.md`; that
PRD remains the archival record for the rest of Method 2.

## Ubiquitous-language terms used

All from `docs/GLOSSARY.md`:

- **DFA product** / `DfaProduct` (§"The five methods", Method 2).
- **Product** / `ProductState`, the `$P$` construction (§"Product") — its
  parenthetical "(Method 2 sends the rest to `$\bot$`)" is removed.
- **Sink (⊥)** / `kSink` (§"Sink") — **this entry is deleted** by the glossary gate.
- **Consistency (cons)** / `consistent(t_in, q_in, t_out, q_out, v)` (§"Consistency").
- **Enabled** — `def:consistency`; `$\cons$` + delta/lambda-definedness. A non-enabled
  letter is now **skipped** in Method 2 (it was: routed to `$\bot$`).
- **Game solving (SolveDfa)** / `solve_dfa(product, vars)` (§"Game solving").
- **Free / known inputs & outputs** / `VariablePartition` accessors.

No new glossary term is introduced; one is removed. (Glossary gate = a deletion,
not an addition.)

## Behaviour / semantics (from main.tex)

The invariants that MUST still hold after the change (all already true, the sink
was never load-bearing):

1. **Skip = drop, per `def:consistency`.** A non-enabled letter (inconsistent, or —
   for a partial transducer — undefined `$\delta$`/`$\lambda$`) contributes no
   product transition. This is the Method-1/3 filter (`def:consistency`: "skipped …
   contributing `$\emptyset$`"). Method 2 now uses the same filter instead of the
   `$\bot$` route.

2. **The arena is unchanged.** `solve_dfa` already existentially projects the
   pinned `$\Iknown,\Oknown$` out of every guard and solves the free game over
   `$\Ifree$` (env) vs `$\Ofree$` (system). Today it additionally drops
   `$\bot$`-edges; after the change there simply are none to drop. `split_2step`
   with `complete_env=true` still completes any missing environment move exactly
   as before — the `$(\Ifree,\Ofree)$` combinations whose unique `$\cons$`
   completion is non-enabled are absent from the arena in **both** the current
   and the new code (they were routed to `$\bot$`, then dropped).

3. **Soundness of skipping.** No play traverses a non-`$\cons$` letter, because
   the governed variables are pinned to the transducer outputs
   (`$v\cap\Iknown=\lambda_{in}(\cdot)$`, `$v\cap\Oknown=\lambda_{out}(\cdot)$`).
   This was the *content* of `lem:sink_skip`; the author has decided to **delete
   the lemma entirely** rather than restate it — Method 2 relies on the same
   skip-is-sound property that Methods 1 and 3 already rely on without a lemma.

4. **`F_P` and acceptance unchanged.** `$F_P = F_D\times Q_{in}\times Q_{out}$`,
   state-based Büchi, reachability-as-Büchi in `solve_dfa`. The sink was the only
   non-`$F_P$`, self-looping absorbing state; with it gone the product has one
   fewer state and no unreachable states.

## Interfaces & types

Signatures are **unchanged**; only internals and one plumbing contract go away.

- **`solve_dfa(product, vars)`** — same signature. Remove: the `kSinkProperty`
  lookup + `std::invalid_argument` throw (`src/solve_dfa.cpp:35-42`); the
  `if (st == *sink)` branch (`:70-73`); the `if (e.dst == *sink) continue;`
  filter (`:75`). Result: `solve_dfa` becomes a **generic** free-arena game
  solver with no Method-2-specific knowledge — a welcome side effect, since
  Methods 1/3 will reuse it (no rename, no re-scoping in this PRD).

- **`include/ltlf_ek/solve_dfa.hpp`** — delete `kSinkProperty` (`:12-16`) and its
  doc block; de-sink the `solve_dfa` doc comment (`:24-30`).

- **`DfaProduct::synthesize`** (`src/dfa_product.cpp`) — same signature. Remove
  the `kSink` state + self-loop + `set_named_prop` (`:85-89`); in the per-letter
  loop, replace the `else { dst = kSink; }` branch (`:136-138`) with a **skip**
  (only accumulate a letter into `guards` when enabled). Update the
  `ProductState`/sink comment (`:23-24`, `:76-77`).

- **`include/ltlf_ek/dfa_product.hpp:10`** and **`include/ltlf_ek/transducer.hpp:27`**
  — replace "failing sink `$\bot$`" / "routed to the sink in Method 2" with
  "skipped (as in Methods 1/3)".

Black boxes (`ltlf_to_dfa`, `SolveDfa`, `progress`): no change beyond the above.

## Edge cases

- **Partial transducers** (`def:consistency`): a `nullopt` `$\delta$`/`$\lambda$` was
  routed to `$\bot$`; now it is skipped — identical arena, since `solve_dfa`
  dropped the `$\bot$`-edge anyway. The Case-A regime (partial ≡ total,
  `docs/prd/concrete-transducer.md`) is unaffected.
- **Empty `$\Ofree$`** / **unrealizable** / **trivially-false `$\varphi$`**: all
  covered by the existing behavioural tests; verdicts must not move.
- **`solve_dfa` called with an arbitrary product lacking the named property**:
  previously threw; now simply solves. The `SolveDfa.ThrowsWhenProductLacksSinkProperty`
  test is deleted because the contract it guarded no longer exists.
- **State-index shift**: `kSink` was `new_state()`'d first (index 0), init
  second. With the sink gone, init becomes index 0. `set_init_state` /
  `get_init_state_number` already carry this; `solve_dfa`'s `game->new_states(n)`
  uses `product->num_states()`, so the loop stays consistent — verify no code
  hard-codes a sink index (grep for a literal `0` sink assumption; there is none).

## Test oracles (for /test-writer)

The change is behaviour-preserving, so the **existing suite is the oracle**:

- **Regression (primary):** every non-deleted test in `tests/dfa_product_test.cpp`
  must stay green unchanged — the realizability verdicts, empty-`$\Ofree$`, Mealy
  `ControllerMayReadCurrentInput`, validation throws, and especially the two
  cross-checks that pin *behaviour*:
  - `EmptyKnowledgeMatchesMonolithicBaseline` (turn-order-invariant subset vs
    Spot's monolithic LTLf synthesis), and
  - `KnowledgeTurnsUnrealizableIntoRealizable` (metamorphic: knowledge adds power).
- **Delete:** `SolveDfa.ThrowsWhenProductLacksSinkProperty` (`:234-246`) and update
  the file-header comment (`:17-20`) from "routing non-cons letters to the sink"
  to "skipping non-cons letters".
- **Optional structural assertion:** after `synthesize`, the product has no
  unreachable absorbing non-accepting sink — i.e. `kSink` is gone. Cheap to check
  only if the builder is exposed; not required, the regression suite already
  proves the arena is identical.
- **Symbol sweep (done-check, not a test):** `grep -rn 'kSink\|ltlf-ek-sink\|sink'`
  over `include/ src/ tests/` returns nothing outside deliberately-kept prose.

## Open theory questions touched

Handed to `/theory-review` (main.tex is Overleaf-only — do **not** compile
locally; edits are `\cl`-gated; keep the §87 footnote commented; §86 is the live
anchor). This PRD records **intent**; `/theory-review` owns the actual edits and
their soundness:

- **Delete `lem:sink_skip` entirely** (author's decision) — the lemma
  (`main.tex:264-278`), its proof, and the `\na[inline]{Rewrite}` marker. Its
  content (non-`$\cons$` letters are never played) is **not** restated; Method 2
  leans on the same unstated skip-soundness as Methods 1/3.
- **De-sink §`fulldfa`:**
  - Product definition (`~:246`): drop `$\cup\{\bot\}$`; states become
    `$S_D\times Q_{in}\times Q_{out}$`.
  - `$\delta_{Dprod}$` (`~:250-257`): drop the `$\bot$` otherwise-branch and the
    sink self-loop; non-`$\cons$` becomes undefined (skip), matching `def:consistency`.
  - Remove the sink/skip-interchangeability sentence (`:261`).
  - Rework the two surviving `\cl` notes (`:282-283`, `:285-286`): the
    `$\Iknown$`-redundancy / free-arena projection argument (`:282-283`) is
    **still valid** and stays, but must be de-sinked; the "must drop `$\bot$`-edges
    not project them" note (`:285-286`) is deleted with the sink.
  - `alg:dfa_product` (`:288-315`): drop `$\cup\{\bot\}$` from the state set
    (`:297`), the `\Else … \bot` branch (`:303-304`), the sink self-loop line
    (`:307`), and the prose "sending … to a failing sink" (`:315`) → "skipping".
    The algorithm then mirrors `alg:otfdfa_product`'s `continue`-on-`$\lnot\cons$`.
- **Dangling `\cref{lem:sink_skip}` after deletion** — clean up the references in
  `docs/prd/transducer-file-format.md:253`, `docs/prd/concrete-transducer.md:185`,
  and `docs/prd/dfa-product.md` (multiple). Also verify the unrelated `$\bot$` at
  `docs/prd/transducer-file-format.md:322` is a truth-value `$\bot$`, not the sink.

These are the author's own theory; `/theory-review` should not re-derive them.

## Definition of done

- `src/dfa_product.cpp`, `src/solve_dfa.cpp`, and the three headers compile with
  **no** `kSink` / `kSinkProperty` symbol remaining; `grep` sweep clean.
- `tests/dfa_product_test.cpp`: sink-contract test deleted, header comment
  updated, **all remaining tests green** with unchanged verdicts.
- `docs/GLOSSARY.md`: "Sink (⊥)" entry removed; "Product" and "Partial
  transducers" notes de-sinked (via `/glossary`).
- `docs/prd/dfa-product.md`: `**Revised by:** docs/prd/drop-method2-sink.md`
  pointer added; sink bullets flagged as superseded (not deleted — archival).
- `docs/prd/concrete-transducer.md`, `docs/walkthroughs/concrete-transducer.md`,
  `docs/prd/transducer-file-format.md`: "skip / `kSink`" → "skip"; `lem:sink_skip`
  references removed.
- `latex/main.tex` de-sinked and `lem:sink_skip` deleted via `/theory-review`
  (separate pass; not `/developer`).
- This PRD's four gates ticked with refs.

## Developer comments / PRD disagreements

- 2026-07-05: `/developer` completed the **code + tests** slice only:
  `src/dfa_product.cpp` (drop `kSink` state/self-loop/`set_named_prop`; the
  per-letter loop now `continue`s on a non-enabled letter instead of routing to
  `dst = kSink`), `src/solve_dfa.cpp` (drop the `kSinkProperty` lookup/throw and
  the `st == *sink` / `e.dst == *sink` branches), `include/ltlf_ek/solve_dfa.hpp`
  (delete `kSinkProperty` + de-sink the doc comment), `include/ltlf_ek/dfa_product.hpp`
  and `include/ltlf_ek/transducer.hpp` (de-sink prose), and
  `tests/dfa_product_test.cpp` (delete `SolveDfa.ThrowsWhenProductLacksSinkProperty`,
  de-sink the file-header comment). Build is green; the full suite
  (161/161, including `EmptyKnowledgeMatchesMonolithicBaseline` and
  `KnowledgeTurnsUnrealizableIntoRealizable`) passes unchanged, confirming the
  refactor is behaviour-preserving as predicted. `grep -rn 'kSink\|ltlf-ek-sink'`
  over `include/ src/ tests/` returns nothing; a broader `grep -i sink` sweep
  turns up only unrelated prose (Spot's own DFA-completion sink in
  `ltlf_to_dfa.cpp`/`ltlf_to_dfa_test.cpp`, and this PRD's own new comment about
  "no sink transitions to drop").
- **Deliberately not done here** (left for the other named gates, per the
  `/developer` skill's own scope — glossary/doc-wide edits are `/glossary`'s
  job, not `/developer`'s, and no new domain identifier was added to warrant
  ticking that gate from this skill): `docs/GLOSSARY.md`'s "Sink (⊥)" entry and
  the "Product"/"Partial transducers" de-sinking; `docs/prd/dfa-product.md`'s
  "Revised by" pointer; the `lem:sink_skip` cross-reference cleanup in
  `docs/prd/concrete-transducer.md`, `docs/prd/transducer-file-format.md`, and
  `docs/walkthroughs/concrete-transducer.md`; and all of `latex/main.tex`
  (owned by `/theory-review`). These remain open items under this PRD's
  `glossary` and `theory-review` gates.
- 2026-07-05: `/test-writer` confirmed the deleted sink-contract test guarded a
  plumbing contract, not a behaviour, and added two direct replacements to
  `tests/dfa_product_test.cpp`: `DfaProduct.PartialKnownTransducerDeltaSkipsUndefinedLetters`
  (a `t_in` whose delta is undefined on `o=false`, with V = ∅ so `consistent`
  is trivially true — isolating DfaProduct's own `!d_in` skip branch from the
  lambda/cons half already covered elsewhere; shows `!o` flips
  realizable→unrealizable while `o` stays realizable, i.e. the skip is
  letter-specific, not a global failure) and
  `SolveDfa.SolvesAnArbitraryProductWithoutAnyNamedProperty` (the exact fixture
  the deleted throw-test used, now asserting no throw and the correct
  unrealizable verdict). No further gap found: the existing
  `EmptyKnowledgeMatchesMonolithicBaseline` / `KnowledgeTurnsUnrealizableIntoRealizable`
  / `ltlfsynt_oracle_test.cpp` corpus (Tables A-D use partial-delta-adjacent but
  actually-total transducers) already covers the consistency/lambda half of
  `def:consistency`; cross-method metamorphic equivalence does not yet apply since
  Methods 1/3 are unimplemented. Full suite green: 163/163 (one pre-existing
  `DISABLED_` excluded-class test unaffected, per design). `grep -rn
  'kSink\|ltlf-ek-sink'` over `include/ src/ tests/` still clean (one prose hit
  in the new test's own comment, referencing the deleted property by name).
  Ticked the `tests` gate.
- 2026-07-05: `/theory-review` (faithfulness mode) verified the code is faithful
  to the intended de-sinked math — the per-letter `continue` in
  `src/dfa_product.cpp:117-119` is the exact complement of `def:consistency`, and
  `src/solve_dfa.cpp:34-65` realizes the surviving free-arena projection note.
  The sink-vs-skip equivalence is **sound**: with the governed variables pinned
  to the transducer outputs, every realized letter is `cons` by construction, so
  `$\bot$` is unreachable and its deletion removes one unreachable, non-`$F_P$`,
  absorbing state and nothing else (the 161/161→163/163 unchanged verdicts are
  the behavioural witness). Applied the main.tex de-sink (Patches 1-8):
  `def:consistency` line-171 sink clause de-sinked; the Product definition and
  `$\delta_{Dprod}$` (dropped `$\cup\{\bot\}$`, the `$\bot$`-branch, and the
  self-loop sentence); deleted `lem:sink_skip` + proof + the `\na[inline]{Rewrite}`
  marker + the two sink-dependent `\cl` notes; de-anchored the surviving free-arena
  `\cl` note; and de-sinked `alg:dfa_product` (state set, `\Else…⊥` branch, self-
  loop line, closing prose). All edits are `\cl`-noted, one sentence per source
  line, macro-only notation. No dangling `\ref`/`\cref` remain — both
  `\cref{lem:sink_skip}` sites and the `:non_cons` / `:self_loop` label refs are
  gone. Not compiled locally (Overleaf-only); verified by reading. The
  theory-review gate is now ticked. Beyond the PRD's itemized edit list, the
  de-sink also had to fix `def:consistency` (main.tex line 171), which still said
  Method 2 "routed to the sink `$\bot$`."
