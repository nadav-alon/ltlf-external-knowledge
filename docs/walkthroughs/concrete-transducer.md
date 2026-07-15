# As-built: Concrete Transducer construction path

Spec: `docs/prd/concrete-transducer.md` · Implemented at `2b45755`
(`Transducer::dict()` added later by the dfa-product PRD).

## Major components (dependency order)

- **`Transducer` abstract base** (glossary: *transducer*) —
  `include/ltlf_ek/transducer.hpp:30`. Four pure virtuals: `initial_state`,
  `dict`, `delta(q, v)`, `lambda(q, v)`. Partiality is `std::optional`
  (`nullopt` = undefined, `def:consistency`); `lambda` takes the **full letter**
  (§87 abuse-of-notation) so `t_in`/`t_out` are interchangeable at the
  interface. The header comment (`:22-29`) carries the un-typeable contract:
  a `nullopt` from delta *or* lambda ⇒ non-enabled ⇒ treated like a `cons`
  failure, and callers may only dereference `delta`'s result after the
  enabled test. `dict()` (`:42`) is Spot plumbing (shared `bdd_dict`), not a
  domain concept — no glossary entry. Deliberately no acceptance-shaped
  member: a transducer has no F.

- **`OutputLabeledTransducer`** (glossary: *output-labeled transducer*) —
  `include/ltlf_ek/output_labeled_transducer.hpp:31`,
  `src/output_labeled_transducer.cpp`. Concrete impl: a `twa_graph` used
  *purely as a transition structure* (ω-acceptance ignored — documented
  loudly, `hpp:17-20`) + one BDD per state over Σ0∪Σ1 for λ + the
  `sigma0_cube`/`sigma1_cube` slices (glossary: *observed / produced slice*;
  accessors on the concrete class only, `hpp:55-56`).
  - `delta` (`cpp:31-47`): unique edge with `(v & e.cond) != bddfalse`; zero
    matches = `nullopt` (incomplete twa ⇒ partial δ), two+ = **throw**
    (nondeterminism is a construction bug, not partiality).
  - `lambda` (`cpp:49-65`): `bdd_restrict(out_[q], bdd_exist(v, sigma1_cube_))`
    then `bdd_exist(·, sigma0_cube_)`. **Not** the PRD's sketch
    (`v & sigma0_cube_` collapses to `bddfalse` on negative Σ0 polarities —
    see PRD "Developer comments", 2026-07-03). Two distinct `nullopt` paths:
    state-level (`out == bddfalse`, `:51`) and per-observation
    (`r == bddfalse` after restrict, `:62`).
  - Constructor invariant (`cpp:17`): `lambda_by_state.size() == num_states()`.

- **`consistent(...)`** (glossary: *consistency (cons)* — the only spelling) —
  `src/consistency.cpp:17-24`. Σ1-agnostic: since `v` is a minterm and each λ
  returns a cube over its own Σ1, "Σ1 slice equals committed output" is
  `(v & *out) != bddfalse` — no knowledge of Iknown/Oknown needed. `nullopt` λ
  ⇒ `false` (enabled subsumes cons). Covers only the **λ half** of `enabled`;
  δ-definedness is the caller's job (`consistency.hpp:19-23`). Preconditions:
  shared dict, full-minterm letter.

- **Enabled discipline at the call site** — `src/dfa_product.cpp:118-140`.
  `d_in && d_out && consistent(...)` on `:125` *is* `def:consistency`. `delta` may
  be *called* on any letter (total C++ function); the **dereference** `*d_in`
  happens only inside the guarded branch — the fix for the pre-`optional`
  latent bug (shared by main.tex pseudocode, `\cl`-flagged). A non-enabled
  letter is *skipped* (contributes no product transition) — the same filter
  Methods 1/3 use; `dfa_product.cpp` no longer routes it to a sink (de-sinked
  per `docs/prd/drop-method2-sink.md`). For total transducers the conjunction
  degrades to plain `cons`.

## Tests

- **Unit — `tests/output_labeled_transducer_test.cpp`**: 3-state fixture,
  σ0={a}, σ1={b}, extra history var `c` in neither slice. Pins: init state;
  δ follows the guard (ignores b,c); δ `nullopt` on missing edge / edgeless
  state; λ relation encoding (`b<->a`, constant `b`); λ `nullopt` at
  `bddfalse` state; both-`nullopt` letter; **abuse-of-notation** (flipping
  c and even b never changes λ); **λ ⊆ Σ1** (`bdd_exist(out, σ1) == bddtrue`).
- **Unit — `tests/consistency_test.cpp`**: shared-dict one-state fixtures
  (`t_in`: b:=a; `t_out`: e:=true with **empty Σ0**, `sigma0=bddtrue`).
  Consistent iff both known slices match; false on either mismatch; false on
  undefined λ (`def:consistency`).
- **Domain oracles**:
  - *Monolithic baseline* — **covered**:
    `dfa_product_test.cpp:160` (`EmptyKnowledgeMatchesMonolithicBaseline`),
    trivial transducers with `sigma1=bddtrue` (also the empty-Σ1 edge case).
    Caveat: Spot baseline is Moore-flavored; `ControllerMayReadCurrentInput`
    (`:177`) documents the deliberate Mealy divergence.
  - *Integration* — **covered**: `KnowledgeTurnsUnrealizableIntoRealizable`
    (`:193`) runs real `OutputLabeledTransducer`s through `DfaProduct`.
  - *Metamorphic cross-method equivalence* — **pending** (only Method 2
    exists; fixtures ready).
  - *Controller verifier* (every trace agreeing with `t_in, t_out, T_C`
    satisfies φ) — **not implemented**; closest is
    `RealizableControllerCarriesAStrategy` (`:141`), existence only.
- **Gaps** (all three closed 2026-07-04, six tests added to
  `output_labeled_transducer_test.cpp`):
  1. ~~Per-observation λ `nullopt` (`output_labeled_transducer.cpp:62`)~~ —
     `LambdaIsNulloptOnAnUncommittedObservationEvenWhenTheStateIsDefined`
     (+ the defined-side twin), fixture `out_[0] = a & b`.
  2. ~~Both throws~~ — nondeterministic-guard (`cpp:40`, thrown at *call*
     time in `delta`, not construction — see PRD dev comments 2026-07-04)
     and constructor size mismatch (`cpp:18`).
  3. ~~Empty Σ0 direct test~~ —
     `LambdaWithEmptySigma0IgnoresTheLetterAndAlwaysReturnsTheConstantCube`.
