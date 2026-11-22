# FOND/PDDL planning domains with LTLf goals: do they prune?

Research for #78 (subticket of #76).
Everything below was run this session; the encoder is `scripts/fond_encode.py`, tested by `scripts/test_fond_encode.py`.
Corpus: [whitemech/FOND4LTLf](https://github.com/whitemech/FOND4LTLf) at `011d9d9a5bfd6406d2c358faf8f63167f6c839bb`, `tests/data/pddl-domains/` **[fetched]**.
That repository ships fifteen-odd problems each of `tireworld` and `zenotravel` plus `robot-coffee`, and its LTLf/PLTLf goals live in test strings, not in files.
Blocksworld and faults are in it (corrected in `docs/research/2026-11-22-fond-blocksworld-faults.md`); other classic FOND families (acrobatics, first-responders) are not, and the standard FOND suites (PRP, MyND, IPC-2008 FOND) were **not looked for**, so they were **not surveyed**.

## Answer

**Three candidates encode as a T_in/T_out pair, and every one of them has a prune ratio far above 1 against the goal alone: 82 to 8670.**
Under the repo's own definition (`docs/GLOSSARY.md`, *Prune ratio*) that is the "world model lives in T_in" regime of the slippery arms: the knowledge costs states, it does not save them.
What the knowledge does buy is the verdict.
Without it the fluents are free inputs the environment controls, so no goal that needs the world to cooperate is realizable; with it, the goals in the deterministic fragment (no `oneof` branch has to come out right) become realizable.
So the corpus yields members for the class of *verdict-flipping* knowledge, not of state-pruning knowledge.

## Encoding sketch

A FOND domain is an agent choosing an action and nature choosing the outcome of a `oneof`.
That is already a two-player game, so the domain is the knowledge and the goal is φ.

| role | variables | source |
|---|---|---|
| free input | `ev`, one bit | nature's pick among a `oneof`'s two branches |
| known inputs | the fluents the goal mentions | emitted by T_in each step |
| free outputs | `a0…a{k-1}`, k = ⌈log₂(ground actions + 1)⌉ | the action, binary-coded; code 0 and any code past the last action are the no-op |

- One T_in state per reachable PDDL state (breadth-first from `:init`, a queue).
  T_in's δ reads the action outputs `a*` as well as `ev`; only λ, the valuation of the observed fluents, reads nothing of the current letter.
  That is the Moore condition of `\cref{def:indep}`, so δ reading O is sound, as in `scripts/slippery_world.py`.
- An action whose precondition fails is a no-op.
  This is the slippery-world convention ("a wall bumps").
  The alternative, a T_out that forbids the action, is not used: nothing here restricts an output, so **T_out is trivial in all three encodings**.
- A deterministic action ignores `ev`.
- LTLf goals are over the fluents' AP names (`vehicle_at_n1`, `has_coffee`, …); `F` is the only temporal operator used.
- The `_moore` columns come from `encode(..., moore_quotient=True)`, which `measure` always runs beside the plain encoding; the `encode` subcommand never quotients.
  The *T_in Moore quotient* (`docs/GLOSSARY.md`) merges T_in states by the goal's fluents: the smallest T_in with the same observable behaviour, the fair measure of what the world model needs.

## Measured

`dfa-product` on the case's own goal; "without" is the same goal with every known input demoted to free and no transducer.
It is measured by running `--dfa-product` with `--inputs/--outputs` directly, a substitute for the bench's `dfa-product-unpruned` subject that was not run through it, so the ratio is the glossary's by definition, not by the same harness.
`product_states` is the reachable explicit product.
Ratio = with / without.
Reproduce a row with `python3 -I scripts/fond_encode.py measure build/ltlf-ek-synth DOMAIN PROBLEM GOAL`, `TMPDIR` set inside the repo.
The inputs are in the corpus checkout above: `tests/data/pddl-domains/tireworld/{domain,p01}.pddl`, the `robot-coffee` directory's domain and `coffee2` problem, and `tests/data/pddl-domains/zenotravel/domain.pddl` with the vendored `docs/research/data/zenotravel.mini.pddl`.

| domain, problem | goal φ | T_in states (Moore) | product with (Moore) | without | prune ratio (Moore) | verdict with / without |
|---|---|---|---|---|---|---|
| tireworld p01 (17 locations, 62 actions) | `F(vehicle_at_n0)` | 8670 (6944) | 17340 (13888) | 2 | 8670 (6944) | unreal. / unreal. |
| tireworld p01 | `F(vehicle_at_n1)` | 8670 (6944) | 8673 (6947) | 2 | 4336.5 (3473.5) | **real.** / unreal. |
| tireworld p01 | `F(vehicle_at_n1 & F(vehicle_at_n3))` | 8670 (6945) | 8679 (6954) | 3 | 2893 (2318) | unreal. / unreal. |
| robot-coffee coffee2 (5 rooms, 15 actions) | `F(coffeeat_o1)` | 160 (11) | 168 (12) | 2 | 84 (6) | unreal. / unreal. |
| robot-coffee coffee2 | `F(has_coffee)` | 160 (10) | 166 (16) | 2 | 83 (8) | **real.** / unreal. |
| robot-coffee coffee2 | `F(has_coffee & F(robot_at_lab))` | 160 (10) | 246 (26) | 3 | 82 (8.67) | **real.** / unreal. |
| zenotravel mini (1 aircraft, 1 person, 3 cities, 3 fuel levels, 56 actions) | `F(atperson_p0_c2)` | 126 (126) | 228 (228) | 2 | 114 (114) | unreal. / unreal. |
| zenotravel mini | `F(flying_a0_c1)` | 126 (126) | 252 (252) | 2 | 126 (126) | **real.** / unreal. |

- The three domains are the candidates; the rows are goals on them.
- The knowledge product is at most T_in's states times the goal's, because every T_in state is reachable under a free `ev` and a free action; it is smaller when the goal is accepted early (the product stops growing at the accepting sink, as in tireworld `F(vehicle_at_n1)`, 8673, and coffee rows 166 and 246).
  The knowledge therefore prunes nothing here: `cons` has nothing to cut, since T_in is total.
- "without" is 2 or 3 for a reason: with the fluents free the goal's own DFA is the whole product.
  Any goal-only denominator is tiny for the same reason, so the ratio reads as the size of the world model, not as work saved.
- The Moore-quotient column is the lower bound on that cost.
  It stays in the thousands for tireworld and at 126 for zenotravel, where nothing merges (fuel and the boarding flags all matter to the future); only coffee collapses, to 10–11 states.
- `zenotravel/p01` has more than 20000 reachable PDDL states, so the case above is a hand-reduced problem (`docs/research/data/zenotravel.mini.pddl`).
  The domain file is the corpus's, unmodified; the mini problem is new.
  Tireworld p01 and `coffee2` are the corpus's own problems, unmodified.

## Findings that matter beyond the table

1. **LTLf realizability is strong planning; the benchmarks are meant for strong-cyclic.**
   Every goal that needs a `oneof` branch to come out right (`coffeeat_o1`, `atperson_p0_c2`, reaching `n0` through a flat tire) is unrealizable in all three domains, because the environment may pick the failing branch forever.
   The realizable rows are exactly the goals in the deterministic fragment.
   A fairness assumption on `ev` would be the natural second piece of knowledge, and it is not a T_in/T_out pair: it is a ψ_in over the free input, outside this note's scope.
2. **The ratio is the wrong class criterion for these.**
   The glossary calls the prune ratio the benchmark's class criterion; a FOND domain cannot meet it with the world model in T_in.
   A better fit is the verdict flip (a knowledge-free `unrealizable` against a knowledge `realizable`) plus the T_in Moore-quotient size.
   That is a choice for the developer.
3. **The corpus has a naming bug.** `zenotravel/domain.pddl` uses `atperson`; `p01` matches, but `p02`–`p15` use `at-person`, so they do not parse against that domain.
   The encoder reads only the domain's spelling, hence p01.
4. **The corpus's own goals are not in files.**
   `robot-coffee/coffee2.pddl` carries `(:goal (and (connected o3 c)))`, a static fact that is true in `:init`, with the intended `until (true) (coffee_at …)` goals commented out; its `coffee_at` is `coffeeat` in the domain.
   Every goal above is therefore authored here.
5. **Not covered.**
   Nested or non-binary `oneof`, `:conditional-effects` and numeric fluents are rejected by the encoder.
   The three problems fit because each `oneof` has two branches and sits at the top of the effect.

## Candidate ranking for a second run

1. **robot-coffee**: small enough to read, the only one whose T_in Moore-quotients (160 → 10), and a verdict flip with a goal that is an LTLf formula rather than a reachability atom.
2. **tireworld p01**: the largest world model (8670 states), the strongest "knowledge is cost" data point, and the corpus has fifteen problems of increasing size to scale along.
3. **zenotravel mini**: nothing merges under the quotient, so it is the cleanest *negative* for any shrinking story.
