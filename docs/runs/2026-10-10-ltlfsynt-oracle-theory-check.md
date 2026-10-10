# Run 2026-10-10 — `ltlfsynt` oracle: theory check of the assumption reduction

**Ticket:** #42. **PRD:** `docs/prd/ltlfsynt-oracle.md`. **Code under check:**
`tests/ltlfsynt_oracle_test.cpp` and the production predicate it exercises
(`include/ltlf_ek/consistency.hpp`, `src/consistency.cpp`, `include/ltlf_ek/role.hpp`).
**Reference:** `latex/main.tex`, read-only, at the committed submodule pointer.

The oracle compares `ltlf-ek-synth --dfa-product` with `ltlfsynt`, Mealy, in three regimes:
`(phi, T_in)` against `psi_in -> phi` with `Iknown` moved to `--ins` (Tables A–D);
`(phi, T_out)` against `(phi) & (psi_out)` with `Oknown` on `--outs` (Tables F–J and the
divergence control J-bad); and both together against `psi_in -> ((phi) & (psi_out))` (Tables M1–M2).
It rests on the paper in three places. Each gets one verdict, covering the `T_in` and the `T_out` side.

| # | Place the oracle relies on the paper | Verdict |
|---|---|---|
| 1 | Known-input semantics of the problem definition | faithful |
| 2 | The enabled predicate of `\cref{def:consistency}` | faithful |
| 3 | The totality that justifies the reduction | `underspecified` |

## 1. Known-input semantics (`\cref{definition}`, `\cref{def:probDefTransducer}`)

**Paper.** Turn order is environment (`Ifree`), then `S_in` (`Iknown`), then `S_C` (`Ofree`),
then `S_out`. `S_in : (2^{I∪O})^* × 2^{Ifree} → 2^{Iknown}`, so `Σ0 = Ifree`, `Σ1 = Iknown`.
`S_C` sees `Σ0 = I`, which is Mealy. A trace agrees with `S` when `v_t ∩ Σ1 = S(v_0⋯v_{t-1}, v_t ∩ Σ0)`
at every step.
The `T_out` side is `Σ0 = I∪Ofree`, `Σ1 = Oknown`: `S_out` sees what the controller just chose.
The two roles are asymmetric in the reduction. `psi_in` is an assumption, so it is the antecedent:
a trace outside it is not the system's to win. `psi_out` is a guarantee the system must meet, so it
is a conjunct with `phi`.

**Code that relies on it.**
- `sigma_slices(partition, Role::t_in)` (`include/ltlf_ek/role.hpp`, `src/role.cpp`) gives
  `Σ0 = Ifree`, `Σ1 = Iknown`, `Role::t_c` gives `Σ0 = I`, `Σ1 = Ofree`, and `Role::t_out` gives
  `Σ0 = I∪Ofree`, `Σ1 = Oknown`.
- `run_transducer` and `single_bit_sigma1_mutations` (`tests/ltlfsynt_oracle_test.cpp`) apply
  `lambda(q, σ0)` and then `delta(q, σ0 ∪ σ1)`, in the paper's order.
- Every `ltlfsynt` call pins `--semantics=Mealy`. `Iknown` goes in `--ins` and `Oknown` in `--outs`
  alongside `Ofree` (`--ins=a --outs=o,x` for Tables F–J; `--ins=a,k --outs=o,x` for M1–M2).
  Putting `Oknown` on `--outs` is what turns `psi_out` into a guarantee, and the formulas place it
  that way: `(phi) & (psi_out)` and `psi_in -> ((phi) & (psi_out))`.
- Table G (`T_out` copying from `Ofree`) is the case that fails if `T_out` sees only `I`.

**Verdict: faithful.** The slices on both sides, the order, the Mealy reading and the antecedent/conjunct asymmetry
match the paper.
The `\na` note after `\cref{def:probDef}` says the definition does not let the controller end the
trace. The oracle does not depend on it: both sides use the standard LTLf synthesis reading.
The one place that does depend on who ends the trace is item 3.

## 2. The enabled predicate (`\cref{def:consistency}`)

**Paper.** `cons(q_in, q_out, v)` holds when `v ∩ Iknown = λ_in(q_in, v ∩ Ifree)` and
`v ∩ Oknown = λ_out(q_out, v ∩ (I∪Ofree))`. A missing `δ` or `λ` value makes the letter
inconsistent. The paper has no definition named *enabled*: `\label{def:enabled}` was removed, and
*enabled* is this project's name for `cons` plus the partiality clause (`docs/GLOSSARY.md`,
"Partial transducers", terminology note).

**Code that relies on it.**
- `emits` and `consistent` (`src/consistency.cpp`): `λ` undefined gives false. Otherwise the
  letter must lie in the cube `λ` returns. `consistent` is the conjunction over `T_in` and `T_out`.
  They pass `λ` the full letter. The narrowing to `Σ0` happens in `OutputLabeledTransducer::lambda`
  (`src/output_labeled_transducer.cpp:51–66`, `bdd_exist(v, sigma1_cube_)`): for `T_in` it keeps
  `v ∩ Ifree`, for `T_out` it keeps `v ∩ (I∪Ofree)`. Table G exists to catch a `T_out` that sees only `I`.
- The `--dfa-product` path the oracle drives uses `emits_region` (`include/ltlf_ek/transducer.hpp:55–64`)
  instead of per-letter `emits`. Their equivalence rests on the variable-scope invariant documented
  there: the stored relation ranges over `Σ0∪Σ1` only.
- `δ`-definedness is left to the caller, which reads it off the successor in the product loop.
  Together the two halves are the paper's predicate.
- The oracle reaches it through `ltlf-ek-synth --dfa-product`, and mirrors it in `run_transducer`,
  where an undefined `λ` or `δ` yields no trace.

**Verdict: faithful.** The per-letter check uses the full letter, which the paper's `v ∩ Σ0`
projection makes equivalent, since a transducer's `λ` reads only its own `Σ0`. A missing value
removes the letter for every party, as the partiality clause says.
`python3 scripts/gen-md-macros.py --check` and `python3 scripts/check-main-tex-refs.py --check` both
pass, so the macros and the `main.tex:NNN` citations in the repo's markdown are still true.

## 3. Totality (`\cref{definition}`, the reduction `\na` after `\cref{def:probDefTransducer}`)

**Paper.** The `\na` after `\cref{def:probDefTransducer}` states the reduction
(`T_in` ⇔ `psi_in -> (phi ∧ psi_out)`) as *to be proven*. The text says "we dont assume totality
of the transducers", and that a sink totalization "does not impact its behaviour in allowed
traces". The two `\cl` notes in `latex/main.tex`, on the totalization sentence and the
2026-08-09 divergence witness, record that this fails for the synthesis verdict: with a
`δ`-dead `T_in` and `phi = X[!]tt`, the methods say unrealizable and `psi_in -> phi` says
realizable. The paper states no hypothesis under which the reduction holds.

**Code that relies on it.**
- The known-input corpus (Tables A–D) and the faithfulness guard use total, deterministic `T_in`
  fixtures. `run_transducer` and `single_bit_sigma1_mutations` say they rely on determinism and
  totality.
- The `T_out` side relies on it equally: Tables F–J, M1–M2 and `KnownOutputFaithfulnessGuardTest`
  (`tests/ltlfsynt_oracle_test.cpp`) use total `T_out` fixtures. The paper's witness note says the
  divergence appears once `Tin` or `Tout` is partial, and the `\cl` notes on the partiality clause
  say a partial `T_out` deletes environment letters. No test measures the partial `T_out` case, and
  `(phi) & (psi_out)` would be the reduction under which it diverges.
- `PartialTinDivergesFromLtlfsyntUnderXBangTtWitness` measures the partial case: ek reports
  UNREALIZABLE, `ltlfsynt` REALIZABLE.
- The reduction argument in `docs/prd/ltlfsynt-oracle.md` ("Why the reduction is equirealizable")
  says "deterministic and total in the committed **Case-A** regime". The glossary's "Partial
  transducers" entry ("language-equivalent") is sharpened by the 2026-08-09 `X[!]tt` witness above
  (GLOSSARY: O5), not retracted.

**Verdict: `underspecified`.** The paper does not say which of two things is meant: whether the
system may leave a transducer's domain to end the trace there, or whether such a run is deleted.
The oracle is sound on the fragment it covers (total `T_in` and `T_out`), and the divergence it cannot cover
is measured and asserted separately. Nothing in the code contradicts the paper, so there is no
`code-bug`.

## Result

- No `code-bug`: no production change and no new test.
- No `doc-bug`.
- One `underspecified`, item 3. It is also open as a note in `main.tex` (the two `\cl` notes), and
  it is filed as #56.
- Nothing under `latex/` changed.
