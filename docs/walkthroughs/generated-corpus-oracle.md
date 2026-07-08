# As-built: generated-corpus oracle

Code guide for `docs/prd/generated-corpus-oracle.md` (all three phases).
Implementing commits: `010cca9` (Phase 1), `715d6b6` (Phase 2+3).

**No production C++.** Everything lives in the anonymous namespace of
`tests/ltlfsynt_oracle_test.cpp`, reusing that file's existing subprocess /
verdict harness (`RunSubprocess`, `ParseEkSynthVerdict` / `ParseLtlfsyntVerdict`,
the `LtlfsyntOracleTest` fixture, `all_letters_over`). A seeded generator feeds
three self-labeling oracles — the oracle *is* the expected value.

Dependency order: `constants + GeneratedCase` → `random_partition` →
`generate_random_formula`/`strengthen_next` → `random_tin` →
`BuildGeneratedCorpus` → the three test bodies.

## Major components

- **`GeneratedCase`** — the case shape — `tests/ltlfsynt_oracle_test.cpp:941`.
  `{spot::formula phi; VariablePartition partition; OutputLabeledTransducer t_in;}`.
  Holds the *built* `t_in` (not an rng-spec), each on its **own private
  `bdd_dict`** so the metamorphic body replays with zero rebuild.

- **Seed constants** — `:908`. One `std::mt19937(kCorpusSeed=20260706)`,
  `kCorpusCaseCount=256`, **no reserved draw slots** (deliberate cross-phase
  shift; do not "fix" — `:1084`).

- **`random_partition`** — the four-way split `VariablePartition` — `:968`.
  `|I|∈[1,5]`, `|O|∈[0,5]` (0 = empty-Ofree edge), per-input `Iknown` coin at
  `kCorpusIknownProbability=0.5`; `Oknown=∅` always in v1. Builds via
  `VariablePartition::split(inputs, outputs, governed)`.

- **`generate_random_formula`** — the Goal formula `phi` — `:999`. Thin wrapper
  over `spot::randltlgenerator`. **Partition-first AP naming** (`:1001`): APs are
  the partition's exact I∪O set, so every `phi`'s APs are in-partition *by
  construction* — no AP-scope guard needed. Palette restricted via priorities
  `"xor=0,M=0"` (mutable `char*` buffer — `parse_options` strtok()s in place).

- **`strengthen_next`** — the `X[!]` injector — `:954`. Bottom-up `formula::map`;
  rewrites each `op::X` → `op::strong_X` at prob `kCorpusStrongXProbability=0.30`,
  seeded from `rng`. Requires `#define SPOT_USES_STRONG_X 1` **before** the first
  `<spot/tl/formula.hh>` include (`:13-19`) — pre-2.13 opt-in, no-op on ≥2.13.
  Stresses the weak-X-at-final-position region (memory
  `ltlf-weak-x-and-termination-semantics`).

- **`random_tin`** — the external-knowledge strategy `t_in` — `:1034`. In-memory
  `OutputLabeledTransducer`, **valid (Case-A deterministic + total) by
  construction**, no post-check. Empty `Iknown` short-circuits to
  `trivial_transducer` (`:1037`). Core loop (`:1061`): δ = one edge per
  `all_letters_over(ifree)` cube (mutually exclusive/exhaustive ⇒ det + total);
  λ = one full random `Iknown` cube OR'd per Ifree-cube (total function
  `Ifree→2^Iknown`). Registers *all* I∪O APs on the case dict (`:1044`) so the
  goal DFA later shares numbering. `Role::t_in` ⇒ Σ0=Ifree, Σ1=Iknown.

- **`BuildGeneratedCorpus`** — the seeded stream — `:1092`. Draw order
  **partition → formula → t_in** off one rng; the `random_tin` call is the
  Phase-2 insertion point of the accepted corpus shift. One fresh `bdd_dict` per
  case. Every test body rebuilds it independently (3× rebuild — non-blocking
  cleanup, PRD §29). `DescribeGeneratedPartition` (`:1109`) feeds `SCOPED_TRACE`.

- **`RunSubprocess` timeout extension** — `:125`. Two trailing defaulted params
  (`std::optional<unsigned> timeout_secs`, `bool* timed_out`) — same function, so
  existing call sites untouched. Prefixes coreutils `timeout <N>s`, maps exit
  `124`/signalled → `*timed_out`. Forwarded through `RunEkSynth` (`:155`) and
  `RunLtlfsynt` (`:235`).

## Tests

Three additive bodies, one shared corpus; the feature *is* tests.

- **P1 `TEST(GeneratedCorpus, LtlfToDfaStructural)`** — `:1131`. Structural
  free-rider: `ltlf_to_dfa(phi)` is `is_deterministic` + `is_complete` on every
  case. Pure library property, never gated. Kills the "asserted on one formula"
  blind spot.

- **P2 `TEST(GeneratedCorpus, MetamorphicRoundTrip)`** — `:1157`. **Domain
  oracle (metamorphic).** `synthesize → verify_controller.ok` on generated `phi`
  + generated `t_in` (`t_out` = trivial on `c.t_in.dict()`). **One-directional /
  incomplete-but-never-wrong**: unrealizable (`nullopt`) `continue`s unchecked.
  Never gated — the *only* body that exercises a non-trivial generated `t_in`.

- **P3 `TEST_F(LtlfsyntOracleTest, GeneratedCorpusDifferential)`** — `:1204`.
  **Domain oracle (differential).** `ltlf-ek-synth` vs `ltlfsynt
  --semantics=Mealy` agree on the bare-`phi` verdict, over the **V=∅, width-≤3**
  subset (Mealy-only, no Moore baseline; no load-bearing guard since ψ_in=⊤).
  Reads only `phi`/`partition`, never `t_in`. Timeout → skip + `RecordProperty
  ("differential_skipped", …)`, never fail. Gated: `GTEST_SKIP` when `ltlfsynt`
  absent.

**Gaps (by design; logged in `docs/BACKLOG.md`):**
- Known-knowledge *wrongly-unrealizable* — uncovered (metamorphic is
  one-directional; differential is empty-knowledge only). → co-generated
  `(Tin, ψ_in)` family (v2).
- Generated `Tout` / `Oknown` — never exercised (`Tout` always trivial). → v2.
- No shrinking — failure reports the raw ≤10-node formula. → v2.
- Single seed at fast-gate sizes — → the "intense soak mode" backlog item
  (env-overridable knobs + seed sweep).
