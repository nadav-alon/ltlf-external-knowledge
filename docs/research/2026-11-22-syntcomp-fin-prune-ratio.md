# SYNTCOMP-fin, re-mined for prune ratio

Research for #77 (subticket of #76).
Every number here was measured this session, on the SYNTCOMP `tlsf-fin` corpus at `SYNTCOMP/benchmarks` commit `13336b3` (the HEAD of a fresh sparse clone, not the `2d53c6b` the 2026-08-18 sweep pinned: see §5), Release build, Spot 2.15.1, **Mealy** turn order (the corpus declares Moore).
Raw rows: `docs/research/data/2026-11-22-prune-ratio-rows.jsonl` (one line per instance, all 1742).
Regenerate with `scripts/prune_ratio_probe.cpp` (CMake target `prune-ratio-probe`) and `scripts/prune_ratio_sweep.py`.

## Answer

The corpus has **two** cases with prune ratio below 0.5, and **three more within 6 % of it**.
All five come from `Random/Lydia`; none from `Patterns` or `Two-player-Game`.
**All five are unrealizable**, so none is a race candidate; they show the knowledge cutting the product, not a speed-up on a game anyone needs to solve.

| case | `n_ap` | known outputs | K | U | prune ratio |
|---|---|---|---|---|---|
| `Random/Lydia/case_06_50/06` | 12 | 2 of 6 | 258 | 2562 | **0.101** |
| `Random/Lydia/case_06_50/38` | 12 | 1 of 6 | 834 | 3330 | **0.251** |
| `Random/Lydia/case_07_50/18` | 14 | 1 of 7 | 1026 | 2050 | 0.5005 |
| `Random/Lydia/case_05_50/09` | 10 | 1 of 5 | 258 | 514 | 0.502 |
| `Random/Lydia/case_03_50/04` | 6 | 1 of 3 | 18 | 34 | 0.529 |

The other four measured cases (`case_04_50/11`, and three tier-2 files `case_10_50/01`, `case_07_50/20`, `case_09_50/27`) have K = U = 3 (ratio 1.0): the extracted output is known but the Goal DFA is already 3 states, so there is nothing to cut.
Ratio is **never above 1** in this corpus.

"Candidates ≥ 3 with measured ratio" is met only if the 0.5 threshold is read loosely: strictly below 0.5 there are two.
The ticket's threshold is therefore **not** a clean cut here — three cases sit at 0.50 to 0.53, and a developer who wants them in or out has to decide (§5).

## 1. What was measured

`K` and `U` are `product_states` of `DfaProduct` (the reachable explicit product), on the **same** formula φ:

- **K**: the partition and `T_out` that `dependent_outputs` extracts from φ (`ltlf-ek-deps --direction out --emit-part --transducer`), `T_in` trivial.
  The probe round-trips `T_out` through the transducer file format, so K is what `ltlf-ek-synth --known-output-transducer` would see.
- **U**: the knowledge-free partition (everything free) with trivial transducers — the `dfa-product-unpruned` baseline.

Prune ratio = K/U.
The partition is the TLSF input/output split, both known sets empty (the 2026-08-18 setup); extraction is the only source of knowledge.
Inputs were not re-extracted: that sweep found 0/405 input dependencies, and with `input_known` empty the input side of K and U coincide.

## 2. Population and outcomes

1742 files, none dropped; each in its own process under `ulimit -v` and `timeout`.
Tier 1 is `n_ap` ≤ 16 (60 s, 2 GiB; the 13 tier-1 MEMOUTs re-run at 120 s, 8 GiB), tier 2 is `n_ap` > 16 (20 s, 2 GiB).
The 8 tier-2 MEMOUTs were retried at 60 s / 8 GiB: 4 became no-known-output, 4 became TIMEOUT.
Of the 13 tier-1 MEMOUTs, 7 became no-known-output, 4 TIMEOUT, 2 stayed MEMOUT (`case_04_50/04`, `/43`).
The 2 GiB first-pass MEMOUT rows are kept in `docs/research/data/2026-11-22-prune-ratio-memout-2gib.jsonl`.

| tier | no known output | measured | EMPTYLANG | TIMEOUT | MEMOUT | needs real TLSF |
|---|---|---|---|---|---|---|
| 1 (455) | 382 | 6 | 36 | 4 | 2 | 25 |
| 2 (1287) | 319 | 3 | 118 | 847 | 0 | 0 |

- "no known output" is `dependent_outputs` returning the empty set: K = U by construction, ratio 1.
- `EMPTYLANG`: φ unsatisfiable, the extractor exits before any product (exit 3 in `ltlf-ek-deps`).
- Every TIMEOUT happened before a K product was printed, i.e. **in extraction or earlier**, not in the products.
  The 847 tier-2 timeouts are `Two-player-Game` (393) and `Random` (447), and are a census of the wall, not a rate; the 2026-08-18 note found the same wall.
- The 25 real-TLSF files (`Scutella`, `chomp_game`) are out, as on 2026-08-18.
- Tier 1 "analysed" = 388 outputs-direction rows (32 + 343 + 13), the same 388 as the dependency sweep; 6 of them carry a known output, as there.
  The ratio is measured on those 6 and on 3 of tier 2.

The ticket's 8 GiB container cap did bite: `case_04_50/04` and `/43` are MEMOUT (`std::bad_alloc`) even at the 8 GiB re-run (above).
Every file was attempted, but the 20 s / 8 GiB caps left 847 TIMEOUT (tier 2) + 4 TIMEOUT and 2 MEMOUT (tier 1) unmeasured, so the time and memory caps in effect chose the measured subset.
The box has 19 GiB; the first pass ran 6 workers under a 2 GiB virtual cap each, the re-run 3 workers under 8 GiB.

## 3. Why these cases (inferred, not measured)

In all five, the Goal DFA is the whole reachable product for U (`U` equals the Goal's state count, e.g. 514), and K is about the Goal's states **consistent with the extracted `T_out`**: one known output roughly halves it (0.50, 0.50, 0.53), and 0.251 and 0.101 are steeper where the known output is also tied to more of the Goal.
I did not look at the formulas to confirm this; the numbers only say that `cons` removes letters here (K is below the Goal's state count), unlike the counter families of the 2026-11-21 note, where the ratio was above 1.
Of the formulas, I looked only at `case_05_50/09`: its dependent output `p159` appears in a conjunct of the form `G(p159 → X[!] p159 ∧ …)`, i.e. a latch; the others were not read.

## 4. Encoding sketch

Nothing is hand-encoded: the knowledge is extracted from the spec, so the encoding is the TLSF adapter plus the two CLIs.

1. `scripts/tlsf_adapter.py FILE.tlsf --emit-part P` gives φ and a part file with both known sets empty.
2. `ltlf-ek-deps --formula φ --part-file P --direction out --emit-part Q --transducer T` extracts `Oknown` and `T_out` (δ = the complete Goal DFA, λ = the live-letter region).
3. `ltlf-ek-synth --formula φ --part-file Q --known-output-transducer T --dfa-product` solves with the knowledge; the same call with `P` and no transducer is the baseline.
   The CLI prints only the verdict, so the state counts come from `scripts/prune_ratio_probe.cpp`, which wraps the same library calls under a `BenchScope`.

A check on `case_05_50/09`: the CLI gives UNREALIZABLE both with and without the transducer, as the probe's `realizable: false` for K and U.

## 5. Open points

- **The threshold.** Two cases are < 0.5, three more are 0.50 to 0.53.
  A strict reading gives two candidates, so the acceptance criterion's "≥ 3 candidates, or a stated reason there are none" is only met under the loose reading of the table above; I did not choose between them for the developer.
- **Every candidate is unrealizable** under Mealy, and the extractor's T_out is the whole Goal DFA, so the case for external knowledge here is a state-count one only.
  Whether the cut survives where the controller exists is untested: the corpus has none to test.
- **Corpus pin.** The clone is at `13336b3`, not `2d53c6b`; the 388 analysed / 6 known-output counts match the 2026-08-18 sweep, which suggests the tier-1 files did not change, but I did not diff the trees.
- **Moore/Mealy.** All numbers are Mealy; the corpus is Moore.
- **Tier 2 is a wall.** 847 of 1287 timed out before K could be computed, at 20 s; a longer budget would measure more `Random` cases and could add candidates.
