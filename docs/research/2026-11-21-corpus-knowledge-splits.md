# Which benchmark families of Lydia, Syft and the 2025 MTDFA paper have a natural knowledge split?

Research for #79 (subticket of #76).
Sources are tagged **[fetched]** (read this session) or **[snippet]** (search summary only).
Numbers in §3 were measured this session with `ltlf-ek-synth --dfa-product` (Release, Spot 2.15.1), except `U`, which was a hand-demoted `--dfa-product` run (`ltlf-ek-synth` cannot run the `dfa-product-unpruned` bench subject).
The generator was a scratch script and is not kept, so **the numbers cannot be regenerated from this note**: §2 states the encoding rules for Single-Counter only; the Double-Counters `t_out` for the environment counter and residual Goal, and the four Nim clock conjuncts, were not written down.

## Answer

Three families have a natural split, and I encoded all three and measured the prune ratio: **Single-Counter**, **Double-Counters** and **Nim**.
Every prune ratio is **at least 1** (1.0 to 20): the knowledge costs product states on these families, it does not save them, except `nim 1 heap, size 1` (ratio 1.00), where it is inert.
**Assumption awaiting the developer's decision:** #79 asks for "environment assumptions as T_in", and I read "natural knowledge split" more broadly, as including `t_out` (the parent #76 speaks of T_in/T_out).
Under that reading the split is in every case an *output* knowledge strategy `t_out`.
Under the strict `t_in` reading the answer is **none**: **no environment assumption in the corpus determines an input**, so these corpora have nothing to encode (§4).
The one corpus whose assumptions are separated from the guarantee (the GR(1) families) states only constraints on free inputs, which cannot be a `t_in` (§4).

## 1. What the three papers benchmark on

- **MTDFA paper**: Duret-Lutz, Zhu, Piterman, De Giacomo, Vardi, *Engineering an LTLf Synthesis Tool* (CIAA 2025), <https://arxiv.org/abs/2507.02491> **[fetched, HTML appendix 0.I]**.
  It reports on the SyntComp LTLf benchmarks, in three groups:
  - *game*: single counter, double counters, nim (Tabajara and Vardi, IJCAI 2019);
  - *pattern*: nested `U`, and conjunctions of `G(v_i)` or `F(v_j)` (Xiao et al., AAAI 2021);
  - *random*: random conjunctions of LTLf specifications (Zhu et al., IJCAI 2017).
  The paper says the patterns use each variable once, so preprocessing or one-step (un)realizability reduces them to true or false; the games are "the most challenging".
- **Syft / LydiaSyft** (<https://github.com/whitemech/LydiaSyft>, `examples/TLSF/` **[fetched]**) ships those same groups as `.tlsf` files (`Two-player-Game/{Single-Counter,Double-Counter,Nim}`, `Patterns/{GFand,Uright}`, `Random/`), plus two families with an explicit environment assumption: `FairnessStability/counter` and `GR1benchmarks/{finding_nemo,workstation_supply,tcp_handshake}`.
  The two sets come from Zhu et al. (IJCAI 2021), the GFSynth benchmarks, also mirrored at `SYNTCOMP/benchmarks` `tlsf/ltl_f` **[fetched]**.
- **Lydia** (<https://github.com/whitemech/lydia>): a formula-to-DFA tool, so its repository has no synthesis corpus of its own beyond `examples/counter_1.ltlf` **[fetched]**.

Not checked: the Syft and Lydia papers' own text (I read their repositories, not the papers), so "Lydia has no synthesis corpus" holds for the repository only.

## 2. Encodings

Every case is an LTLf formula from the corpus, run under **Mealy** (the repo's turn order), where the corpus is labelled Moore, system-first.
The monolithic instance is the published formula with the published input/output split; the knowledge instance moves the conjuncts that *define* a variable out of the formula and into a `t_out` file, and keeps the rest as the Goal.
That a variable's value is a function of the history is the only criterion for it to be known.

**Single-Counter (N bits, `counter_N.tlsf`).**
Inputs `init_counter_i`, `inc`; outputs `counter_i`, `carry_i`.
`counter_i` at step 1 is `init_counter_i` at step 0; for t ≥ 1, `carry_0(t+1) = inc(t)`, `carry_i(t+1) = counter_{i-1}(t) ∧ carry_{i-1}(t+1)`, `counter_i(t+1) = counter_i(t) ⊕ carry_i(t+1)`.
All 2N outputs are known (`output_free` is empty); the values at steps 0 and 1 the formula leaves free appear in no other conjunct, so `t_out` fixes them to false.
`t_out` has 2^(N+1)+1 states (5, 9, 17, 33 for N = 1..4); the Goal is `G(¬inc → X[!] inc) → X[!] F(⋀ ¬counter_i)`, 4 DFA states for every N.

**Double-Counters (N bits, `counters_N.tlsf`).**
The environment's counter (`counter_env_i`, `carry_env_i`, driven by input `inc_env`) is the Single-Counter register, and becomes `t_out` the same way.
The system's counter is driven by the free output `inc_sys` and starts at a value the formula leaves free at step 1, so it is not a function of the history; it stays in the Goal.

**Nim (`nim_h_k.tlsf`).**
The heap update is underdetermined (the mover picks a heap and an amount), so only the **turn clock** is known: `turn_sys` holds at even steps and `turn_env` at odd ones.
`t_out` has 2 states; the Goal is the published formula minus the four conjuncts that define the clock.

For every case the knowledge instance, the monolithic instance under `dfa-product` and `ltlfsynt --semantics=Mealy --realizability` **agree on the verdict** (13 of 13), so the encodings keep the problem.

## 3. Measured

`K` is `product_states` of `dfa-product` with `t_out` over the residual Goal.
`U` is `product_states` of `dfa-product` on the same Goal with the known variables demoted to free (the same quantity as the `dfa-product-unpruned` bench subject): the **prune ratio** is `K/U`.
`M` is `product_states` of `dfa-product` on the published monolithic formula, so `K/M` is a **monolithic ratio**, specific to this note (not the glossary's *nk-product ratio*, which needs a ψ_in and a `-nk` baseline; these cases have none).
"Goal" is the Goal DFA's state count.

| case | Goal | `t_out` states | K | U | prune ratio | M | monolithic ratio K/M | verdict (ltlfsynt / ours) |
|---|---|---|---|---|---|---|---|---|
| single N=1 | 4 | 5 | 10 | 4 | 2.50 | 15 | 0.67 | realizable / realizable |
| single N=2 | 4 | 9 | 20 | 4 | 5.00 | 27 | 0.74 | realizable / realizable |
| single N=3 | 4 | 17 | 40 | 4 | 10.00 | 51 | 0.78 | realizable / realizable |
| single N=4 | 4 | 33 | 80 | 4 | 20.00 | 99 | 0.81 | realizable / realizable |
| double N=1 | 15 | 5 | 29 | 15 | 1.93 | 21 | 1.38 | realizable / realizable |
| double N=2 | 23 | 9 | 89 | 23 | 3.87 | 69 | 1.29 | realizable / realizable |
| double N=3 | 39 | 17 | 305 | 39 | 7.82 | 261 | 1.17 | realizable / realizable |
| nim 1 heap, size 1 | 9 | 2 | 9 | 9 | 1.00 | 5 | 1.80 | unrealizable / unrealizable |
| nim 1 heap, size 2 | 12 | 2 | 18 | 12 | 1.50 | 13 | 1.38 | realizable / realizable |
| nim 1 heap, size 3 | 15 | 2 | 24 | 15 | 1.60 | 17 | 1.41 | realizable / realizable |
| nim 1 heap, size 4 | 18 | 2 | 30 | 18 | 1.67 | 22 | 1.36 | realizable / realizable |
| nim 2 heaps, size 1 | 28 | 2 | 33 | 28 | 1.18 | 23 | 1.43 | realizable / realizable |
| nim 2 heaps, size 2 | 49 | 2 | 60 | 49 | 1.22 | 41 | 1.46 | unrealizable / unrealizable |

Reading it:

- Prune ratio is at least 1 everywhere (exactly 1 only for the inert `nim 1 heap, size 1`), and for Single-Counter it grows with `t_out`'s 2^(N+1)+1 states: I infer that `cons` prunes little or nothing, since a deterministic register should make no letter inconsistent, so the product only gains the register's states (not measured: K counts reachable states, which cannot tell `cons` pruning from plain unreachability).
  The glossary's *Prune ratio* says "**above 1** where the world model lives in $\Tin$ (the slippery arms)"; this note extends that to `t_out`, since the denominator (`Iknown`/`Oknown` demoted to free) covers it too.
- Against the monolithic instance the knowledge product is smaller only on Single-Counter (0.67 to 0.81), and the gap closes as N grows; on Double-Counters and Nim it is larger (1.17 to 1.80).
  Single-Counter is also not a game at all once all outputs are known: nothing is left for the controller to choose, so it is a monitor with a counter blow-up.
- Whether this says anything about speed was not measured; only state counts were.

## 4. Families with no split, and why

- **`GR1benchmarks/finding_nemo`, `workstation_supply`, `tcp_handshake`** (assumption and guarantee shipped as separate `*_env_safety.ltlf` / `*_agn_safety.ltlf` files) **[fetched]**: the assumptions are safety constraints such as `G ¬syn → G ¬syn_ack` or `G(¬room_1 → ¬sense_nemo)`, over free inputs and the agent's own outputs.
  They restrict which input letters can occur; they never fix an input's value.
  The agent's moves (`room_i`, `hallway_0`, `station_0`) are nondeterministic choices under an adjacency rule, so they are not knowledge either.
  A `t_in` must produce a known input, and these have none.
  `ltlf-ek-synth` also rejects a `--known-input-transducer` when `input_known` is empty (`ValidateKnownTransducerFlags`), so even a transducer whose only job is a partial δ cannot be passed.
  Not measured.
- **`FairnessStability/counter`**: the assumption is a fairness or stability condition on free inputs (a CNF file), and the guarantee's adder relation is not a function (`G(¬add → ¬X c0)` is an implication one way only).
  Not encoded; I did not check whether a function can be carved out of it.
- **Patterns, Random**: no variable is determined by the history (patterns use each variable once; random conjuncts mostly share no variables).
  Not encoded.

## 5. Open points

- **Moore/Mealy.** The corpus is Moore, system-first; every number above is Mealy, so the *verdicts* need not match the corpus's published ones.
  I did not run LydiaSyft, so nothing here compares against its numbers.
- **The residual Goals are mine.** I checked the 13 verdicts, not language equality; the values I fixed at steps 0 and 1 are the ones the formula never mentions, so a different residual would only move them.
- **Sizes stop at N = 4 (single), 3 (double), and nim up to 2 heaps of size 2**: enough for the trend, not for a scaling claim.
