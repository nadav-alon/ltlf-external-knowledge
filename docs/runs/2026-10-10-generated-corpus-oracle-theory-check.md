# Theory check 2026-10-10 — generated-corpus oracle against `main.tex`

Ticket #40. Subject: `tests/ltlfsynt_oracle_test.cpp` (the generator `random_partition` / `random_tin`,
`GeneratedCorpus.MetamorphicRoundTrip`, `LtlfsyntOracleTest.GeneratedCorpusDifferential`) and the
production code those tests drive. Reference: `latex/main.tex`, read-only.

Verdicts are `faithful`, `code-bug`, `doc-bug` or `underspecified` (`docs/project-standards.md`,
**Theory fidelity**).

| # | Place the code relies on the paper | Paper | Verdict |
|---|------------------------------------|-------|---------|
| 1 | The enabled predicate | `\cref{def:consistency}` | faithful |
| 2 | Committed totality of the generated input transducer | Transducers subsection `\cl` note; `\cl` witness after `\cref{def:probDefTransducer}`; `\cref{def:consistency}` partiality clause | underspecified |
| 3 | The controller postcondition | `\cref{def:probDefTransducer}`, note after `\cref{def:probDef}` | underspecified |
| 4 | The Mealy observed slice | `\Sigma_0`/`\Sigma_1` of `S_C` (`\cref{definition}`) | faithful |

No `code-bug`, so no code or test changes accompany this report. Nothing under `latex/` changed.
The tests named below are unchanged by this PR; no test run is claimed here.

## 1. Enabled predicate — faithful

**Relied on by:** `emits` and `consistent` in `src/consistency.cpp:15,24`, reached by every
product build the generated corpus drives, and by `random_tin` (`tests/ltlfsynt_oracle_test.cpp:2161`),
whose per-state `lambda` is the relation `ifree_cube ∧ iknown_cube`, OR'd over the `Ifree` cubes.

**Paper:** `cons(q_in, q_out, v) = (v ∩ Iknown = λ_in(q_in, v ∩ Ifree)) ∧ (v ∩ Oknown = λ_out(q_out, v ∩ (I ∪ Ofree)))`;
a missing `δ` or `λ` value is an inconsistent letter.

**Check:** `OutputLabeledTransducer::lambda` restricts the relation to `v`'s `Σ0` slice and returns the
cube over `Σ1`; `emits` tests `v ∧ cube ≠ ⊥`, which for a full-letter `v` is `v ∩ Σ1 = λ(q, v ∩ Σ0)` provided `λ` is functional and assigns every
`Σ1` variable. Otherwise `lambda` returns a set (`exist_Σ0(restrict(out, obs))`,
`src/output_labeled_transducer.cpp:62-66`) and `v ∧ cube ≠ ⊥` tests membership, not the paper's equality
(`include/ltlf_ek/transducer.hpp:61` says as much). The paper makes `λ` a function through
`λ: Q × Σ0 → Σ1` (main.tex:112, `\cref{def:probDefTransducer}`), so verdict 1 is `faithful` under that
hypothesis; `random_tin` builds one full `Iknown` cube per `Ifree` cube, so the generated corpus meets it.
Likewise `λ_out` reads `v ∩ (I ∪ Ofree)` only if `Tout` was built with `Σ0 = I ∪ Ofree`; nothing enforces
that, and the corpus's trivial `Tout` does not exercise it. An undefined `λ` yields `nullopt`, hence `false`. `consistent` is the conjunction of the two. This is the
paper's predicate, with `Σ0`/`Σ1` read from the transducer rather than hard-coded. The generated `Tin`
has `Oknown = ∅` and a trivial `Tout`, so the second conjunct is vacuously true there.

## 2. Committed totality of the generated `Tin` — underspecified

**Relied on by:** `random_tin` (`tests/ltlfsynt_oracle_test.cpp:2161`): `δ` has one edge per `Ifree` cube,
so it is deterministic and total; `λ` is a total function `Ifree → 2^Iknown`. Its doc comment calls this
"the committed Case-A regime". Empty `Iknown` takes `trivial_transducer`, also total.

**Paper:** "In the methods below we dont assume totality of the transducers", with undefined `δ`/`λ`
equal to an inconsistent letter. The `\cl` note after it says partiality is "a modelling commitment
rather than a notational convenience, and which of the two the methods implement should be said
explicitly"; the `\cl` witness after `\cref{def:probDefTransducer}` says the monolithic conjecture
`ψ_in → (φ ∧ ψ_out)` "needs either a totality hypothesis or an explicit ruling" on a run leaving a
transducer's domain.

**Check:** the generator's choice is the safe one — on a total `Tin` the partial and totalized readings
coincide, so neither reading of the open question changes any verdict the corpus grades. But the paper
states no hypothesis that licenses the oracle, so "total ⇒ the reduction applies" rests on the generator's
own commitment, not on a statement in `main.tex`. The repo's encoding of the paper's witness
(`PartialTinDivergesFromLtlfsyntUnderXBangTtWitness`, `tests/ltlfsynt_oracle_test.cpp:1570`; main.tex cites
only the file) is what a partial `Tin` would break.

**Coverage fact, not a verdict:** `GeneratedCorpusDifferential` grades only `V = ∅` cases (trivial `Tin`),
so the random total `Tin` is graded by `MetamorphicRoundTrip` and the cross-method agreement only, never
by `ltlfsynt`. The totality commitment therefore does not yet bear weight in the monolithic comparison.

## 3. Controller postcondition — underspecified

**Relied on by:** `verify_controller` (`src/verify_controller.cpp:147`), via `compute_bad`
(`src/verify_controller.cpp:84`): `Bad = νY. {s : ¬Acc(s) ∧ (hasDeadEnd(s) ∨ ∃ Ifree whose agreeing
successor ∈ Y)}`. `MetamorphicRoundTrip` (`tests/ltlfsynt_oracle_test.cpp:2810`) asserts every controller
a method returns passes it. `solve_dfa` (`src/solve_dfa.cpp`) solves reachability-as-Büchi, the same reading.

**Paper:** `\cref{def:probDefTransducer}`: every trace that agrees with `Tin`, `Tout` and `T_C` satisfies `φ`.
The `\na` note after `\cref{def:probDef}` says the definition "does not work out-of-the-box" because the
controller does not decide when the trace ends.

**Check:** the verifier makes the system the one that may stop: a trace counts when it reaches an
accepting state, and a run with no agreeing letter that is not yet accepting is a failure. That is the
mainstream system-controlled-termination reading. The differential does not contradict it on any graded case, though `ltlfsynt` is only weak
evidence of the reading (glossary). The code is internally consistent (`solve_dfa` and the verifier share it),
but the paper does not say who ends the trace, so no code-vs-paper mismatch can be called a bug either way.

## 4. Mealy observed slice — faithful

**Relied on by:** `solve_dfa` (`src/solve_dfa.cpp:49-66`) and `verify_controller`'s bucketing by `Ifree`
combination; the differential passes `--semantics=Mealy` to `ltlfsynt` with `--ins` the free inputs.

**Paper:** `S_C : (2^{I∪O})* × 2^I → 2^{Ofree}`, so `Σ0 = I` for `S_C`, in the turn order `Ifree`, then
`Sin` produces `Iknown`, then `S_C` produces `Ofree` having seen the same step's `I` (Mealy).

**Check:** the arena projects the pinned `Iknown`/`Oknown` out of every guard (`bdd_exist`, line 49) and
plays `Ifree` (environment first) against `Ofree`; the strategy's edges mention only `Ifree`/`Ofree`.
This loses nothing against `Σ0 = I` because `Iknown` is a function of the history and the same step's
`Ifree` (`λ_in` is functional, which `random_tin` guarantees), so seeing `Ifree` is seeing `I`. The
existential projection of `Iknown` is likewise exact for a functional `Tin`: exactly one `Iknown`
completion agrees per `Ifree` letter. Projecting out `Oknown` (`solve_dfa.cpp:49`) is exact for a different
reason: `Oknown = λ_out(q_out, v ∩ (I ∪ Ofree))` is fixed by `I` (which `Ifree` fixes) and the system's own
`Ofree`, provided `λ_out` is a function (`λ: Q × Σ0 → Σ1`, main.tex:112). If `λ_out` were non-functional,
the existential projection would let the system settle the leftover choice in its own favour. In the corpus
`Tout` is trivial, so this is never exercised; `random_tin` covers only the `Tin` half. Verdict 4 is
`faithful` for functional `λ_in` and `λ_out`. Moore is not attempted, as the paper's `\na` says its signatures commit to Mealy.
The `V = ∅` differential has `Σ0 = Ifree = I` outright.

## Filed

The `underspecified` verdicts (2 and 3) are filed together as one suggestion discovery (#49). There is no
`doc-bug`.
