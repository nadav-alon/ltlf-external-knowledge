# FOND survey widened: blocksworld and faults as verdict-flip candidates

Research for #363 (follow-up to `docs/research/2026-11-21-fond-pddl-ltlf-goals.md`, #78).
Corpus: [whitemech/FOND4LTLf](https://github.com/whitemech/FOND4LTLf) at `011d9d9a5bfd6406d2c358faf8f63167f6c839bb`, `tests/data/pddl-domains/blocksworld-ipc08` and `faults-ipc08` **[fetched]**.
The earlier note's claim that these two families are not in FOND4LTLf is wrong: they are, and this note encodes them.
Encoder: `scripts/fond_encode.py` with T_in as the domain, as in the earlier note, with one change: `:constants` count as objects, since faults declares its objects there and its problems have no `:objects`.
Verdicts are under Mealy turn order; the library has no Moore mode (#364, closed).
The parenthesised sizes in the `T_in states (Moore)` column are the *T_in Moore quotient* (`docs/GLOSSARY.md`), a state quotient, not a turn order.

## Answer

**Faults yields a verdict-flip member only under an authored goal; its source goal `F(made)` is unrealizable with and without T_in, so it is not a source-goal flip. Blocksworld was skipped for cost, not shown to be impossible.**

## faults-ipc08

Each `dNN.pddl` is a domain and `pNN.pddl` its problem (`pNN.pddl` has no `:objects`); NN operations, one fault type.
The domain is the same with one more `operation` constant per step, so `d01`/`p01` is the smallest non-trivial problem (it already has a fault branch and a repair); `d02`/`p02` is reported as well because it is the only problem where the two-operation goal `F(completed_o1 & completed_o2)` can be posed, and it gives a second flip member.
Reproduce: `python3 -I scripts/fond_encode.py measure build/ltlf-ek-synth faults-ipc08/dNN.pddl faults-ipc08/pNN.pddl GOAL`, `TMPDIR` set inside the repo.

| problem | goal φ | goal source | T_in states (Moore) | verdict with T_in | verdict without |
|---|---|---|---|---|---|
| p01 (1 op, 3 actions) | `F(made)` | the problem's own `(:goal (made))`, wrapped in `F` | 7 (4) | unreal. | unreal. |
| p01 | `F(completed_o1)` | **authored** | 7 (3) | **real.** | unreal. |
| p02 (2 ops, 5 actions) | `F(made)` | the problem's own, wrapped in `F` | 14 (9) | unreal. | unreal. |
| p02 | `F(completed_o1)` | **authored** | 14 (6) | **real.** | unreal. |
| p02 | `F(completed_o1 & completed_o2)` | **authored** | 14 (8) | unreal. | unreal. |

- The source goal `made` needs `finish`, whose precondition is `not last_fault`; every `perform_operation` may fault again forever, so it is unrealizable with T_in, the strong-versus-strong-cyclic gap of finding 1 in the earlier note.
  `p03` and `p04` give the same verdict on `F(made)` (T_in 52 and 30 states, Moore 18 and 21).
- `F(completed_o1)` is in the deterministic fragment (`completed` is set whichever `oneof` branch is picked), so the knowledge flips the verdict: two members, p01 and p02.
- `F(completed_o1 & completed_o2)` is unrealizable: `perform_operation_k_fault` needs `(not_fault f1)`, so once an operation faults no other operation can run until `repair_fault_1`, and the repair sets `not_completed ?o`, clearing `completed ?o`. The environment wins by faulting every operation, so the two `completed` atoms are never true together.
- Without T_in the fluents are free inputs, so every goal is unrealizable.

## blocksworld-ipc08

**Skipped for cost; not shown to be impossible.** No encoding was attempted.
`p00`–`p10` have 5 blocks, `p11`–`p20` 10, `p21`–`p30` 15; there is no smaller problem.
`p00` (the smallest by objects, 335 ground actions) has 103121 reachable PDDL states, counted with `MAX_STATES` raised to 300000 (44 s); the encoder's default cap of 20000 is a constant the encoder chose, not a limit, and it raises there.
The count is an **upper bound**: `Model._eval` has no case for PDDL `=`, so `(not (= ?b1 ?b2))` guards never block (#370) and states the domain forbids may be counted.
An explicit HOA T_in of that size would be about 10^7 edges with up to 335 actions each branching on `ev` (an estimate), which is why it was skipped; HOA size, synthesis time and memory were not measured.
What multiplies the states was not examined, so no cause is named; the missing `=` support is one candidate.
The T_in Moore quotient cannot be taken before enumerating the states, so quotienting does not rescue an explicit encoder.
No verdict is given for either arm.

Goal naming, for whoever retries with a smarter encoder: `p00` uses `ontable`, the domain's spelling; `p01` and others use `on-table`, which does not parse against the domain, like the zenotravel naming bug in the earlier note.
Fixing the predicate name to the domain's would count as encoding, not authoring.
The source goals are conjunctions of fluents (a PDDL reachability goal), so they would be encoded as `F(…)` of the conjunction.
