# PRD: Controller verifier (`verify_controller`) + CLI `--model-check`

**Status:** implemented
**Interface:** new free function `verify_controller` (library, `include/ltlf_ek/verify_controller.hpp`); new `Role::t_c` + `controller_as_transducer` materializer; wires the deferred CLI `--model-check` flag. **Not** a `Synthesis` method.
**main.tex ref:** `\cref{def:probDefTransducer}` (the postcondition being checked, §129–131), the turn order + *agreement* (§81, §88), `\cref{def:consistency}` / `\cons` (§149–157), the controller signature `$\lambda_C:Q_C\times2^{\mathcal I}\to2^{\Ofree}$` (§125). Fulfils the deferred **oracle #2** of `docs/prd/dfa-product.md` and the deferred **`--model-check`** of `docs/prd/cli-wrapper.md` (both stand; this PRD does not supersede them).

**Gates:**
- [x] glossary        — new terms landed in docs/GLOSSARY.md C++ column (`verify_controller`, `Role::t_c`, `controller_as_transducer`, `VerifyResult`/`Witness`, "Controller verifier"), verified spelled-exact.
- [x] tests           — `tests/verify_controller_test.cpp` (12 cases: oracles #1-#5 — ok/lasso/dead-end unit fixtures, positive metamorphic vs `DfaProduct` incl. knowledge-sensitivity flip and empty-Ofree, the `X[!] o` reachability-vs-inclusion discriminator, lambda-flip + edge-redirect discriminating negatives with witness-replay self-consistency, `controller_as_transducer` round-trip, validation throws); `Role::t_c` unit fixtures added to `tests/transducer_io_test.cpp` (`SigmaSlicesDirect.TCIsIAndOfree`, `ParseTransducer.RoleTcSigma0IsInputsSigma1IsOutputFree`); oracle #6 (CLI end-to-end) extended in `tests/ltlf_ek_synth_test.cpp` (SAFE self-check agreeing with the library, hand-broken UNSAFE `--controller` + witness, malformed `--controller` file exit 2) on top of the two pre-existing subprocess tests. Full suite green, 186/186; branch `master`, uncommitted.
- [x] code-review     — domain (/code-reviewer) **and** generic (/code-review, high effort) both **clean** (2026-07-06): no must-fix, no shipping bug. The two real bugs (unrealizable-self-check `nullopt` deref; mismatched-iterator hang in `PrintWitness`) were fixed before review; the diff carries the fixed code. One non-blocking *consider* tracked in `docs/BACKLOG.md`: the throwaway letter `registrar` in `verify_controller` is the sole owner of some AP registrations feeding the returned `Witness` bdds — safe at every current call site (the CLI's `ap_registrar` / the tests' transducers independently keep I∪O registered), latent only for a library caller that doesn't; harden with a documented precondition.
- [x] theory-review   — code ↔ math faithfulness vs main.tex: **clean, no code-bug** (theory-reviewer, 2026-07-06). All four flagged questions faithful/sound — F_φ mark parity with `solve_dfa` (both read `state_is_accepting` off the same `ltlf_to_dfa`), virtual-start split collapses to `init ∉ Bad` given `ltlf_to_mtdfa`'s non-accepting init, `agree` is the correct 4-way lift of `def:consistency`+§88+§125, DFA-completeness assumption sound (`complete_here` ⇒ `dfa_delta` nullopt branch is defensive dead code). One `underspecified` note stays in `main.tex` (author's open `\na` at §96, termination reading); reviewer recommends confirm-not-modify, no `\cl` committed. Branch `master`, uncommitted.

## Goal

Give the project its **internal linchpin correctness oracle**: given a Goal
formula `$\varphi$`, the partition, the two knowledge transducers `$\Tin,\Tout$`,
and a synthesized controller `$T_C$`, decide whether `$T_C$` actually solves
`\cref{def:probDefTransducer}` — *every trace that agrees with `$\Tin,\Tout,T_C$`
satisfies `$\varphi$`*. Reusable by every method (a post-condition on any
`Controller`), and it un-defers the CLI `--model-check` flag
(`docs/prd/cli-wrapper.md`).

**The verifier is built directly on the `\cref{def:probDefTransducer}`
agreement postcondition and uses the monolithic-reduction conjecture
(`main.tex:135`, `$\psiin\rightarrow(\varphi\land\psiout)$`) *nowhere*.** That is
deliberate: the conjecture is an *existential/realizability* statement, and it is
exactly what the **external `ltlfsynt` oracle** (`docs/prd/ltlfsynt-oracle.md`,
implemented) already exercises. A verifier built on the conjecture would be a
second copy of that oracle through the same reduction — inheriting its blind
spots instead of catching them, and unable to certify a *specific fixed* `$T_C$`
(the conjecture speaks about *whether a controller exists*, not *whether this one
is correct*). Staying conjecture-free keeps this a genuinely independent second
oracle, and it turns the verifier into a tool that could later *test* the
conjecture (cross-check its verdicts against the monolithic reduction) rather
than assume it.

## Ubiquitous-language terms used

From `docs/GLOSSARY.md` unless flagged:

- **Goal formula** `$\varphi$` → `phi`; **Inputs/Outputs** split
  `$\Ifree,\Iknown,\Ofree,\Oknown$` → `VariablePartition`.
- **Controller** `$T_C$` → `Controller` (§"Controller (system strategy)").
- **External knowledge strategy** `$\Tin,\Tout$` → `Transducer` (`t_in`/`t_out`).
- **Agreement** → `agrees`/the per-letter **Consistency** `$\cons$` →
  `consistent(t_in, q_in, t_out, q_out, v)` (§"Consistency (cons)").
- **Enabled letter** → `\cref{def:consistency}`.
- **NFA/DFA for the Goal** → `ltlf_to_dfa` (§"Goal DFA construction").
- **Role** / `sigma_slices` → §"Role".
- **Output-labeled transducer** → `OutputLabeledTransducer`.

**Glossary gaps to close (run `/glossary` before `/developer`):**

- **`Role::t_c`** — a third `Role` value with `$(\Sigma_0,\Sigma_1)=(\mathcal I,\Ofree)$`
  (the controller's align-block row, `main.tex:130–132`). The glossary "Role"
  entry currently lists only `t_in`/`t_out`; add `t_c`.
- **`controller_as_transducer`** — materialize a synthesized `Controller`'s
  strategy graph as an `OutputLabeledTransducer` (`$\Sigma_0=\mathcal I,\Sigma_1=\Ofree$`).
- **Controller verifier** / `verify_controller`, `VerifyResult`, `Witness` — a new
  testing-&-oracle domain concept (§"Testing & oracles", alongside *Faithfulness
  guard*).

## Behaviour / semantics (from main.tex)

The verifier decides the `\cref{def:probDefTransducer}` postcondition under the
**system-controlled-termination reachability** reading of `$\text{LTL}_f$`
synthesis — the same semantics `solve_dfa` commits to
(`docs/prd/dfa-product.md`, 2026-07-04 developer note; the mainstream
De Giacomo–Vardi reading). This resolves, *by matching `solve_dfa`*, the author's
`\na` note at `main.tex:98` ("the controller does not decide when the trace
ends") — the two **must** share one termination semantics or the oracle and the
method disagree by construction (flagged for `/theory-review`).

**Failure condition — reachability of `$F_\varphi$`, not language inclusion.**
Let `$A_\varphi$` be the DFA for `$\varphi$` (`ltlf_to_dfa`), accepting set
`$F_\varphi$`. Because `$T_C$` is *fixed*, the system has no remaining moves;
the only branching is the environment's `$\Ifree$`. So the check is a
**one-player (env) reachability/safety fixpoint on the product**, never a game
solve:

```
product state:  s = ⟨ s_φ, q_in, q_out, q_c ⟩          # A_φ × Tin × Tout × T_C
Acc(s)       :=  s_φ ∈ F_φ                              # a legal φ-satisfying stop
```

An **agreeing** letter `$v\in2^{\mathcal I\cup\mathcal O}$` at `s` is one that is
`$\cons$` **and** pinned by `$T_C$` **and** has all four `$\delta$` defined:

```
agree(s, v) := consistent(t_in, q_in, t_out, q_out, v)   # Iknown, Oknown pins
            ∧ (v ∩ Ofree = λ_c(q_c, v ∩ I))              # the T_C  Ofree pin
            ∧ δ_φ, δ_in, δ_out, δ_c  all defined at v
```

For a fixed env choice `$\Ifree$` the pinned knowns/outputs make **at most one**
`$v$` agree (determinism of `$\lambda_{in},\lambda_C,\lambda_{out}$`); if none
agree for some `$\Ifree$`, that env choice is a **dead-end** (the trace is forced
to end at `s`). The environment *wins* — i.e. `$T_C$` is **incorrect** — from a
state where it can keep the play out of `$F_\varphi$` forever *or* force a
non-accepting dead-end:

```
Bad = νY.  { s :  ¬Acc(s)
                ∧ (  hasDeadEnd(s)                       # ∃ Ifree with no agreeing v
                   ∨ ∃ Ifree whose agreeing successor s' ∈ Y ) }

correct(T_C)  ⇔  start ∉ Bad
```

**Non-empty-trace convention (load-bearing — the `1`-rejects-empty gotcha).**
`$\text{LTL}_f$` traces are non-empty (memory
`ltlf-weak-x-and-termination-semantics`; glossary "Open theory questions"), so
the system may **not** stop before consuming ≥1 letter — the pre-letter initial
node is **not** a valid `$\varphi$`-satisfying stop even if its `$A_\varphi$`
component is accepting. Model this with a **virtual start node** that must take
exactly one mandatory transition into the real product (whose every state
represents "≥1 letter consumed" and uses `Acc` normally):

```
correct(T_C)  ⇔  ¬hasDeadEnd(start)  ∧  ∀ Ifree: first-successor(start, Ifree) ∉ Bad
```

A later *revisit* of the initial `$A_\varphi$` state (after ≥1 letter) **is** a
legal stop — the virtual-start split gives exactly that (identity of the state
does not matter, "≥1 letter consumed" does).

**Witness (counterexample lasso).** When `$\text{start}\in\text{Bad}$`, extract a
concrete adversarial play: follow env choices staying inside `Bad` (deterministic
tie-break, e.g. lexicographically least `$\Ifree$`) until either a `$\neg F_\varphi$`
**dead-end** (prefix ends there, empty cycle) or a repeated `Bad` state
(prefix + cycle). Every `Bad` state has a `Bad`-successor or a dead-end, so in the
finite product this always terminates as a lasso.

## Interfaces & types

```cpp
// include/ltlf_ek/verify_controller.hpp   (new)

// A concrete adversarial play the controller cannot win: env's letters from the
// virtual start into a ¬F_φ cycle, or into a ¬F_φ dead-end (cycle empty).
struct Witness {
  std::vector<bdd> prefix;   // agreeing letters, start → cycle head / dead-end
  std::vector<bdd> cycle;    // repeating ¬F_φ loop; empty ⇒ dead-end at prefix end
};

struct VerifyResult {
  bool ok;                              // true ⇔ T_C solves def:probDefTransducer
  std::optional<Witness> counterexample;// set iff !ok
};

// Verify a controller (as a Role::t_c transducer) against the def:probDefTransducer
// postcondition.  Reuses ltlf_to_dfa (A_φ) and consistent (cons); builds the
// product + attractor independently.  NEVER calls solve_dfa / solve_game.
VerifyResult verify_controller(const spot::formula& phi,
                               const VariablePartition& vars,
                               const Transducer& t_in,
                               const Transducer& t_out,
                               const Transducer& t_c);

// Convenience overload: materialize a synthesized Controller as its Role::t_c
// transducer, then delegate.
VerifyResult verify_controller(const spot::formula& phi,
                               const VariablePartition& vars,
                               const Transducer& t_in,
                               const Transducer& t_out,
                               const Controller& controller);

// Materialize a synthesized Controller's strategy graph as a Role::t_c
// OutputLabeledTransducer: Σ0 = I, Σ1 = Ofree; per-state λ_C read off the Mealy
// edge outputs (the Ofree slice), δ_C off the edge dsts — the same "delta via
// edges, output derived" idiom OutputLabeledTransducer already uses.
OutputLabeledTransducer controller_as_transducer(const Controller& controller,
                                                 const VariablePartition& vars);
```

```cpp
// include/ltlf_ek/transducer_io.hpp   (extend)
enum class Role { t_in, t_out, t_c };   // t_c: Σ0 = I, Σ1 = Ofree (main.tex:127)
// sigma_slices(partition, Role::t_c) → { sigma0 = inputs(), sigma1 = output_free }
```

- **Reused as-is (independence-preserving):** `ltlf_to_dfa` (build `$A_\varphi$`),
  `consistent` (the `$\Iknown/\Oknown$` half of `agree`), `OutputLabeledTransducer`,
  `parse_transducer` (for the CLI `--controller` file, `Role::t_c`),
  `VariablePartition`, `Controller`.
- **Hand-rolled (must NOT reuse `solve_dfa`/`solve_game`):** the product traversal,
  the `Bad` `$\nu$`-fixpoint, the virtual-start non-empty-trace split, and witness
  extraction.
- **`bdd_dict` discipline:** `$A_\varphi$`, `$\Tin,\Tout,T_C$` and every letter
  share one `bdd_dict` (`Transducer::dict()`); validate as `DfaProduct` does.
- **Product-letter enumeration:** enumerate full letters `$v\in2^{\mathcal I\cup\mathcal O}$`
  and keep `agree(s,v)` — the same accepted-baseline enumeration cost as
  `DfaProduct` (symbolic version deferred, `docs/BACKLOG.md`).

### CLI `--model-check` wiring (`src/ltlf_ek_synth.cpp`)

Replace the deferral at `src/ltlf_ek_synth.cpp:215–219`. New flag
`--controller <file>` (a `Role::t_c` `%%LAMBDA` transducer file — **zero new
format**, `parse_transducer(in, partition, Role::t_c, dict)`):

```
ltlf-ek-synth --model-check [--controller F] --<method> --formula φ  <partition/knowledge flags>

  if --controller F:   t_c = parse_transducer(F, partition, Role::t_c, dict)   # check a given artifact
  else:                t_c = controller_as_transducer(method.synthesize(...).value(), vars)  # self-check
  r = verify_controller(φ, vars, t_in, t_out, t_c)
  ok    → stdout "SAFE",   exit 0
  !ok   → stdout "UNSAFE" + print witness (letters), exit 20
```

- `--model-check` short-circuits before method dispatch **only** when self-check
  is *not* requested; in self-check mode it needs the method to synthesize first
  (reconcile with `docs/prd/cli-wrapper.md` §249's short-circuit note — the
  deferral message goes away, and an unwired method flag in self-check mode still
  reports "method not yet implemented").
- Verdict word parsing / exit-code conventions mirror `--realizable`
  (0 / 20 / 2 / 1). A `--controller` file that fails to parse → usage class, exit 2.

## Edge cases

- **Empty `$\Ofree$`** — `$\lambda_C$` yields the empty cube; the `$T_C$` pin is
  vacuous; verifier still runs (controller controls nothing).
- **Empty `$\Ifree$`** — env has no branching; `Bad` reduces to the single
  deterministic run reaching `$F_\varphi$` (or hitting a dead-end).
- **Trivially-true `$\varphi$`** (`$\top$`) — first post-letter state is `Acc`;
  `ok` for any total controller. **Trivially-false `$\varphi$`** (`$\bot$`, no
  `$F_\varphi$`) — everything `$\neg\text{Acc}$` ⇒ `!ok` with a witness (correct:
  nothing solves `$\bot$`; only reachable via `--controller` since synthesis
  returns `nullopt`).
- **Partial `$\Tin/\Tout/T_C$`** — undefined `$\lambda$`/`$\delta$` ⇒ that
  `$\Ifree$` is a dead-end (`hasDeadEnd`); handled by `agree`'s definedness clause,
  consistent with `\cref{def:consistency}` (Case-A regime).
- **Controller from `--controller` that is not total on reachable inputs** — an
  undefined `$\lambda_C$` on a reachable `$\Ifree$` is a dead-end ⇒ contributes to
  `Bad` (a partial controller that abandons a play is incorrect). This is the
  right behaviour, not an error.
- **Non-empty-trace boundary** — the virtual-start split (above); a controller
  that would "win" only via the empty trace is correctly rejected.
- **Validation** — throw `std::invalid_argument` when an AP of `$\varphi$` is
  outside `$\mathcal I\cup\mathcal O$`, or the automata/transducers do not share
  one `bdd_dict` (same policy as `DfaProduct::synthesize`).
- **Determinism** — fixed letter/`$\Ifree$` enumeration order and a fixed witness
  tie-break, so verdict and witness are reproducible for tests.

## Test oracles (for /test-writer)

1. **Unit fixtures** — tiny hand-built `$A_\varphi\times\Tin\times\Tout\times T_C$`
   with a known verdict and known witness *shape*: one `ok`, one `!ok` via an
   infinite-avoidance **lasso**, one `!ok` via a `$\neg F_\varphi$` **dead-end**
   (empty cycle). Assert `counterexample` is present iff `!ok`.
2. **Positive (every `solve_dfa` controller passes)** — across the
   `tests/dfa_product_test.cpp` realizable corpus (incl. the knowledge-sensitivity
   flip), every `Controller` `DfaProduct::synthesize` returns must
   `verify_controller(...).ok`. This ties the independent oracle to `solve_dfa`'s
   verdict metamorphically.
3. **Discriminating (negative — guards the always-`ok` stub failure mode)** —
   take a verified-good controller, **mutate** it (flip one `$\lambda_C$` output
   bit / redirect an edge to a wrong successor), assert `verify_controller` now
   returns `!ok` **with a witness**; then **replay** the witness's letters through
   `$A_\varphi/\Tin/\Tout/T_C$` and assert it indeed agrees and never enters
   `$F_\varphi$` (self-consistency of the counterexample).
4. **Reachability-vs-inclusion discriminator (load-bearing, the analog of the
   `ltlfsynt` oracle's flip discipline)** — at least one fixture where a *correct*
   controller is *forced* through a `$\neg F_\varphi$` prefix before a
   `$\varphi$`-stop — crisply **`$\text{X[!]}\,o$`** (strong next: unsatisfiable at
   length 1, so the system must consume ≥2 letters and set `$o$` at step 1):
   assert `verify_controller` returns **`ok`**, proving reachability semantics
   were implemented — a naive language-inclusion check wrongly returns `!ok` here
   because the length-1 stopping point violates `$\varphi$`. (Contrast `$F(o)$` /
   `$a\,\text{U}\,o$`, which a correct controller can satisfy already at step 0 —
   not a discriminator, since no `$\neg F_\varphi$` prefix is forced.)
5. **`controller_as_transducer` round-trip** — the materialized `Role::t_c`
   transducer's `$\lambda_C/\delta_C$` reproduce the strategy graph's outputs on
   every reachable `$(q_C, i)$`.
6. **CLI end-to-end** — `--model-check --controller F` on a SAFE controller →
   stdout `SAFE`, exit 0; on an UNSAFE (hand-broken) controller → `UNSAFE` +
   witness, exit 20; self-check mode (no `--controller`) agrees with the library
   `verify_controller` on the synthesized controller; exit-code matrix (bad
   `--controller` file → 2).

## Open theory questions touched

- **Termination-semantics parity with `solve_dfa` (`main.tex:98` `\na`).** The
  verifier commits to system-controlled-termination *reachability*; `solve_dfa`
  commits to the same. `/theory-review` must confirm the two readings coincide —
  if `solve_dfa` ever produced a controller under a *different* stopping
  convention, oracle #2 (positive) would fail for a semantic, not a bug, reason.
  This does not modify `main.tex`; it confirms `def:probDef`/`def:probDefTransducer`
  against the shared reading.
- **The monolithic conjecture (`main.tex:135`)** is *not* used and *not* resolved
  here; the verifier is independent of it and could later cross-check it (future,
  `docs/BACKLOG.md`).
- **Method-2 arena input partition (`$\Ifree$` vs full `$\mathcal I$`)** —
  inherited from `docs/prd/dfa-product.md`, not resolved. The verifier reads
  `$\lambda_C$` over `$2^{\mathcal I}$` per `main.tex:127`, so it is agnostic to
  that internal solve choice.
- **Non-empty-trace / weak-`X` convention** — the verifier relies on it (virtual
  start); confirm alignment with `ltlf_to_dfa`'s acceptance marks.
- No `\na`/stub in `main.tex` is edited; `FP`/aggregation untouched.

## Definition of done

- `include/ltlf_ek/verify_controller.hpp` + `src/verify_controller.cpp` compile;
  `verify_controller` (both overloads) + `controller_as_transducer` implemented;
  `Role::t_c` + its `sigma_slices` case added.
- `verify_controller` runs the reachability `$\nu$`-fixpoint with the virtual-start
  non-empty-trace split, reusing `ltlf_to_dfa` + `consistent`, and **never** calls
  `solve_dfa`/`solve_game`.
- Returns `VerifyResult{ok, counterexample}`; witness is a valid agreeing lasso
  when `!ok`.
- CLI `--model-check` (+ `--controller`) replaces the deferral: SAFE/UNSAFE +
  witness, exit 0/20, self-check when `--controller` omitted.
- All six test-oracle groups pass — crucially the discriminating negative (oracle
  3) and the reachability-vs-inclusion fixture (oracle 4).
- Glossary updated via `/glossary`: `Role::t_c`, `controller_as_transducer`,
  `verify_controller`/`VerifyResult`/`Witness`, "Controller verifier".
- `docs/prd/cli-wrapper.md` and `docs/prd/dfa-product.md` cross-referenced (their
  deferred `--model-check` / oracle #2 now delivered here); neither superseded.

## Developer comments / PRD disagreements

- **CLI self-check on an unrealizable goal (PRD gap, 2026-07-06).** The "CLI
  `--model-check` wiring" sketch writes
  `controller_as_transducer(method.synthesize(...).value(), vars)` for self-check
  mode, which assumes `synthesize` returns a controller. When the goal is
  **unrealizable** it returns `nullopt`, so `.value()` throws. Resolved by
  guarding the `nullopt` path in self-check mode: print `UNREALIZABLE` to stderr,
  exit 20 (mirroring the default-mode unrealizable convention of
  `docs/prd/cli-wrapper.md`). No controller exists to model-check, so this is the
  natural third outcome alongside SAFE/UNSAFE; the file/`--controller` path is
  unaffected. (Surfaced by the 2026-07-05 smoke test; see `docs/BACKLOG.md`.)
