# PRD: Drop the ⊥-sink from Method 2 (DFA product)

**Status:** draft
**Interface:** revises `DfaProduct` + `solve_dfa` (no signature change); removes the `kSinkProperty` plumbing contract
**main.tex ref:** §`fulldfa` (Method 2), Algorithm `alg:dfa_product`; deletes `lem:sink_skip`
**Revises:** `docs/prd/dfa-product.md` (implemented Method-2 PRD) — sink sections only; the rest stands

**Gates:**
- [ ] glossary        — remove the "Sink (⊥)" entry + de-sink the "Product" and "Partial transducers" notes
- [ ] tests           — delete the sink-contract test; behavioural suite stays green (it is the oracle)
- [ ] code-review     — domain (/code-reviewer) + generic (/code-review)
- [ ] theory-review   — main.tex de-sinking of §`fulldfa`, `alg:dfa_product`, and deletion of `lem:sink_skip`

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
(contribute nothing), exactly as Methods 1 and 3 already do (`def:enabled`,
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
- **Enabled** — `def:enabled`; `$\cons$` + delta/lambda-definedness. A non-enabled
  letter is now **skipped** in Method 2 (it was: routed to `$\bot$`).
- **Game solving (SolveDfa)** / `solve_dfa(product, vars)` (§"Game solving").
- **Free / known inputs & outputs** / `VariablePartition` accessors.

No new glossary term is introduced; one is removed. (Glossary gate = a deletion,
not an addition.)

## Behaviour / semantics (from main.tex)

The invariants that MUST still hold after the change (all already true, the sink
was never load-bearing):

1. **Skip = drop, per `def:enabled`.** A non-enabled letter (inconsistent, or —
   for a partial transducer — undefined `$\delta$`/`$\lambda$`) contributes no
   product transition. This is the Method-1/3 filter (`def:enabled`: "skipped …
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

- **Partial transducers** (`def:enabled`): a `nullopt` `$\delta$`/`$\lambda$` was
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
    sink self-loop; non-`$\cons$` becomes undefined (skip), matching `def:enabled`.
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
