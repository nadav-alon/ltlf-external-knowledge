# PRD: generated-corpus soak mode (wall-clock-budgeted escalating runner)

**Status:** implemented — Phase 1 only (Phase 2 not started)
**Interface:** extends the GoogleTest suite `tests/ltlfsynt_oracle_test.cpp`
(no production C++); parameterizes the existing `BuildGeneratedCorpus` +
three corpus bodies on a config struct, adds per-knob env overrides, and adds
a wall-clock-budgeted escalating driver. Not a `Synthesis` method; no new
algorithm.
**main.tex ref:** unchanged from v1 — Method 2 (`alg:dfa_product`); the
*enabled* predicate `\cref{def:enabled}` (§107–116) + committed Case-A totality
(the generated $\Tin$ must satisfy these); the controller postcondition
`\cref{def:probDefTransducer}` (§129–131, decided by `verify_controller`); the
Mealy observed slice $\Sigma_0=\mathcal{I}$ of $S_C$ (`main.tex` §86). This PRD
asserts **no new** synthesis semantics — it only feeds the existing oracles a
larger, escalating corpus.

**Gates:**
- [x] glossary        — Phase 1 introduces no new domain identifier (CorpusConfig
      / corpus_config_from_env / the extracted kCorpus* constants are test-harness
      plumbing, per the PRD's own "Ubiquitous-language terms used" note); no
      docs/GLOSSARY.md edit needed for Phase 1.
- [x] tests           — Phase 1 harness tests added to `tests/ltlfsynt_oracle_test.cpp`
      (anonymous namespace, same file so they see `CorpusConfig`/
      `corpus_config_from_env`/`BuildGeneratedCorpus`): `ScopedEnvVar` RAII
      env guard + `ChecksumGeneratedCorpus` helper, then
      `GeneratedCorpus.DefaultCorpusIsByteIdenticalToGolden` (the critical
      guard — no env set, `corpus_config_from_env()` matches every
      `CorpusConfig{}` default, and the built 256-case corpus's case-0
      `phi`/partition plus a full-stream checksum match a golden captured
      from the just-landed default corpus) and five
      `CorpusConfigFromEnv.*` env-plumbing tests
      (`CasesOverrideChangesCorpusSize`,
      `SeedOverrideChangesCorpusDeterministically` — against its own
      captured golden, not an in-process double-build (see code comment: a
      same-seed `spot::randltlgenerator` re-construction in the *same*
      process is not reproducible — a Spot RNG quirk unrelated to
      `CorpusConfig`, flagged for `/theory-review` / Phase 2's `run_corpus`
      level loop), `MalformedSeedThrows`, `ZeroCasesThrows`,
      `TreeMinGreaterThanMaxThrows`). Full suite green, 213/213 (`ctest
      --test-dir build`); branch `master`, uncommitted. Out of scope (Phase
      2 code doesn't exist yet): ladder monotonicity/clamp test, soak-smoke
      test.
- [x] code-review     — domain (/code-reviewer) + generic (/code-review) both
      clean on the Phase 1 diff (2026-07-12). No correctness findings (the one
      candidate — the seed golden false-failing via Spot's process-global RNG —
      was empirically refuted: goldens are stable across isolated / shared-process
      runs). Four low-severity cleanups applied: portable FNV-1a checksum (was
      std::hash, implementation-defined); `EnvInt` → `std::optional` (removed the
      9-block read/cast boilerplate); `DescribeCorpusConfig` now prints all knobs
      (tree_min/cases/timeout) for full replay; dropped duplicate
      `kGoldenDefaultCorpusCaseCount`; single-computed seed checksum. Suite green
      213/213 after fixes.
- [ ] theory-review   — code ↔ math faithfulness vs main.tex (n/a for the Phase 1
      test-harness diff; the Spot RNG quirk is a mechanical Spot-API item captured
      as a Phase-2 must-resolve, not a theory question)

## Goal
The fixed-seed generated corpus (`docs/prd/generated-corpus-oracle.md`, v1 fully
implemented, all three phases landed) is deliberately tuned for the **fast
`ctest` gate**: one `kCorpusSeed`, 256 cases, tree-size $\le 10$, partitions
$\lvert\mathcal{I}\rvert\in[1,5]/\lvert\mathcal{O}\rvert\in[0,5]$, differential
width $\le 3$ — it runs in well under a second, so the cases it draws are small
and easy to solve. This PRD adds an **opt-in "soak" mode**: a wall-clock-budgeted
runner that starts near today's parameters and **escalates complexity** round
after round (bigger $\Ifree$, deeper formulas, wider alphabets, more transducer
states), running as many generated cases as it can until a caller-set deadline
(e.g. `LTLF_EK_SOAK=120` for ~2 minutes), then stops. It is meant to run
occasionally (nightly / pre-release), **never** on the default build.

It **complements, does not supersede** the v1 PRD (which stays
`Status: implemented`): soak reuses v1's three self-labeling bodies verbatim —
the `ltlf_to_dfa` structural free-rider, the `synthesize`$\to$`verify_controller`
metamorphic round-trip, and the `ltlfsynt` differential — and only changes *what
corpus* they iterate and *for how long*.

Scope pinned in the grill (2026-07-12):

- **The default path stays byte-for-byte unchanged.** With `LTLF_EK_SOAK` unset
  (or `0`) and no per-knob env var set, `BuildGeneratedCorpus` draws the exact
  same 256-case corpus from `kCorpusSeed` it does today, and each body runs it
  once — the fast gate and the landed tree's reproducibility are untouched.
- **Two orthogonal layers.** (1) A **per-knob override layer** (Phase 1): every
  tunable becomes env-overridable, defaulting to its current constant. This is
  also the **deterministic replay layer** — a soak failure is reproduced by
  pinning these knobs + the seed and running the ordinary single pass.
  (2) An **escalating budget driver** (Phase 2): when `LTLF_EK_SOAK>0`, each body
  loops escalation *levels* until the deadline, each level a fresh-seed corpus at
  bumped parameters.
- **All three bodies escalate** under the budget; the `ltlfsynt` differential
  keeps a low width clamp ($\le 5$) plus its existing per-subprocess timeout+skip
  so the external tool can never dominate the run.
- **Collect-all, not fail-fast.** A soak keeps running to the deadline and records
  every failing case (via `EXPECT_*` + `SCOPED_TRACE`), so one run surfaces every
  reproducer it found rather than stopping at the first.
- **No new synthesis behaviour and no new domain concept.** Escalation ladder /
  soak / budget are *operational test-harness* terms, like v1's "generated corpus"
  — at most a prose note for `/glossary`, no C++ column domain entry.

## Ubiquitous-language terms used
All already in `docs/GLOSSARY.md`; soak drives the exact same artifacts v1 does,
just at escalating scale. No new domain identifier.

- **Goal formula** $\varphi$ → `phi` (`spot::formula`) — generated, now at
  escalating tree size.
- **Inputs / Outputs** and the four-way split
  $\Ifree,\Iknown,\Ofree,\Oknown$ → `VariablePartition` — generated, now at
  escalating (clamped) width.
- **External knowledge strategy** ($\Tin$) → `Transducer` /
  `OutputLabeledTransducer` — the in-memory `random_tin`, now with an escalating
  state-count ceiling; $\Sigma_0=\Ifree,\Sigma_1=\Iknown$ (glossary "Role"
  `t_in`).
- **DFA product** (Method 2) → `DfaProduct`; **Goal DFA construction** →
  `ltlf_to_dfa`; **Controller verifier** → `verify_controller`; **Consistency**
  $\cons$ → `consistent` — all unchanged, exercised on larger cases.
- **Letter alphabet** $\Sigma=2^{\mathcal{I}\cup\mathcal{O}}$ → `LetterAlphabet` /
  `build_product`'s per-letter driver — the $2^{\lvert\mathcal{I}\cup\mathcal{O}\rvert}$
  cost is exactly what the width ceiling (below) bounds.

**Glossary note (flag for `/glossary`, do not block):** "soak mode", "escalation
ladder", "escalation level", "time budget" are **operational testing** terms
realised by test-local helpers (anonymous namespace in
`tests/ltlfsynt_oracle_test.cpp`), not library APIs — like v1's *generated
corpus* prose note, they likely warrant at most a one-line addition to the
existing "Testing & oracles" note, no C++ column entry. Confirm with `/glossary`.

## Behaviour / semantics (from main.tex)
The harness asserts **only** the three properties v1 already established; it adds
no synthesis semantics. Everything below is the *driver / configuration* around
those unchanged assertions.

### Configuration struct (bespoke — pinned)
Introduce a test-local `struct CorpusConfig` holding every tunable, with a
factory that reads env overrides and falls back to the current `constexpr`
constants when a var is unset:

```
struct CorpusConfig {
  unsigned    seed          = kCorpusSeed;              // 20260706
  std::size_t case_count     = kCorpusCaseCount;         // 256
  int         tree_size_min  = kCorpusTreeSizeMin;       // 1
  int         tree_size_max  = kCorpusTreeSizeMax;       // 10
  int         input_max      = kCorpusInputMax;          // 5   (|I| upper bound)
  int         output_max     = kCorpusOutputMax;         // 5   (|O| upper bound)
  int         tin_states_max = kCorpusTinStatesMax;      // 3
  std::size_t diff_width_cap = kCorpusDiffWidthCap;      // 3
  unsigned    subprocess_timeout_secs = kCorpusSubprocessTimeoutSecs; // 10
};
CorpusConfig corpus_config_from_env();   // per-knob env overrides, else defaults
```

- The three existing width-range literals become named constants so the config
  has a default to fall back to: `kCorpusInputMax = 5`, `kCorpusOutputMax = 5`
  (from `random_partition`'s `input_count(1,5)`/`output_count(0,5)`),
  `kCorpusTinStatesMax = 3` (from `random_tin`'s `state_count(1,3)`), and
  `kCorpusDiffWidthCap = 3` (from the hard-coded `if (width > 3) continue;` at
  `ltlfsynt_oracle_test.cpp:1216`). `kCorpusIknownProbability` (0.5) and
  `kCorpusStrongXProbability` (0.30) are **not** exposed (not part of the ladder;
  see *Out of scope*).
- **Env var names** (prefix `LTLF_EK_CORPUS_`, one per knob):
  `LTLF_EK_CORPUS_SEED`, `_CASES`, `_TREE_MIN`, `_TREE_MAX`, `_INPUT_MAX`,
  `_OUTPUT_MAX`, `_STATES_MAX`, `_DIFF_WIDTH`, `_TIMEOUT`. Plus the soak switch
  `LTLF_EK_SOAK` (below). These names are a `/developer` convenience, not domain
  vocabulary; keep them exactly so the replay recipe in *Reproducibility* is
  stable.
- **Parse failure is loud, never silent.** A present-but-malformed value
  (non-integer, negative, or — for counts/maxes — zero) must **throw**
  `std::runtime_error` naming the offending var, failing the test. A malformed
  soak knob must not silently run the default corpus and masquerade as a pass.
  (`seed` accepts any `unsigned`; `tree_size_min ≤ tree_size_max` is validated,
  else throw.)

### The default (non-soak) path is byte-identical (pinned invariant)
With `LTLF_EK_SOAK` unset/`0` **and** no `LTLF_EK_CORPUS_*` set,
`corpus_config_from_env()` returns exactly the defaults above, and each body runs
`BuildGeneratedCorpus(config)` **once** with `config.seed = kCorpusSeed`. The
refactor from free `constexpr`s to a config struct **must preserve the
`std::mt19937` draw order** (`random_partition` → `generate_random_formula` →
`strengthen_next` → `random_tin`, per case), so the emitted 256-case corpus is
**bit-identical** to today's. This is the single most important correctness
constraint of Phase 1 — it is what keeps the fast gate and existing failures
unchanged. `/test-writer` should assert this (e.g. a spot-check that a
fixed-seed default corpus's first case's `phi`/partition match a golden string),
or `/developer` verifies the suite stays green with no case-set drift.

### Soak switch and budget (bespoke — pinned)
- `LTLF_EK_SOAK` is **both the switch and the wall-clock budget in seconds**.
  Unset or `0` ⇒ soak off (single fast pass, above). `LTLF_EK_SOAK=120` ⇒ escalate
  for ~120 s. Malformed (non-integer, negative) ⇒ throw (loud, as above).
- **The budget is per test body.** Each of the three `TEST`/`TEST_F` bodies
  escalates independently for `LTLF_EK_SOAK` seconds, because GoogleTest bodies
  run sequentially and are commonly filtered to one via `--gtest_filter`. Running
  all three under `ctest` therefore takes ≈ 3× the budget. This is **documented
  loudly** in a comment at the switch and in the *Definition of done*; a soaker
  who wants a single body's worth of time filters to it. (Rationale: splitting one
  wall budget across three independently-schedulable bodies is awkward and breaks
  under `--gtest_filter`; per-body is the honest, predictable reading of "one var
  = budget seconds".)
- **Deadline mechanics.** `deadline = std::chrono::steady_clock::now() +
  seconds(LTLF_EK_SOAK)`, captured once at body entry. The **level loop** checks
  `now() < deadline` before starting each level; the **inner case loop** checks it
  before each case and `break`s out mid-level when passed. The deadline is
  **soft**: the case in flight when it passes runs to completion (a single wide
  case or one `ltlfsynt` subprocess — the latter already bounded by
  `subprocess_timeout`). No mid-case interruption.

### Escalation ladder (bespoke — pinned)
When soak is on, level `L = 0, 1, 2, …` derives a per-level `CorpusConfig` from
the env-base config `B` (`corpus_config_from_env()` — so a soaker can still shift
the *starting* point or seed), by the monotone schedule:

```
level L config, given base B:
  tree_size_max  = B.tree_size_max + 3*L                       // 10, 13, 16, ...
  input_max      = min(B.input_max  + L, kCorpusWidthCeiling)  // 5, 6, ...,  <=12
  output_max     = min(B.output_max + L, kCorpusUnionCeiling - drawn |I|)  // joint clamp, below
  tin_states_max = B.tin_states_max + L                        // 3, 4, 5, ...
  diff_width_cap = min(B.diff_width_cap + L, kCorpusDiffWidthSoakCap)  // 3,4,5, cap 5
  tree_size_min, subprocess_timeout, case_count = B.*  (unchanged per level)
```

- **Fresh seed per level.** A single `std::mt19937 seed_rng(B.seed)` is seeded
  once per body; level `L`'s corpus uses `seed = seed_rng()` (the `L`-th
  successive draw). So the whole soak run is **reproducible from `B.seed`**: level
  `L`'s seed and thus its 256 cases are fully determined. (Successive `mt19937`
  outputs, not `B.seed + L`, to avoid low-bit correlation between adjacent levels.)
  - **⚠ Phase-2-must-resolve (found in Phase-1 test-writing, 2026-07-12).**
    `generate_random_formula` sets `spot::randltlgenerator`'s `"seed"` **option**,
    which draws from Spot's **process-global** RNG — constructing the generator
    twice with the same explicit seed in the **same process** does **not** reset
    it, so a *second* `BuildGeneratedCorpus` call in one process is **not** a pure
    function of its seed for the formula part. Phase 1 is unaffected (each `ctest`
    body is a fresh process, one build). But `run_corpus` builds one corpus **per
    level in one process**, so the "reproducible from `B.seed`" guarantee above
    **does not hold as written**. The Phase-2 developer must make each level's
    corpus deterministic given its seed — most likely call `spot::srand(cfg.seed)`
    (Spot's global-RNG seeder) **immediately before** each per-level
    `BuildGeneratedCorpus`, and confirm two same-`cfg.seed` builds in one process
    now match (a Phase-2 unit test). This is a mechanical Spot-API fix, not a
    `/theory-review` item. The per-case replay recipe (fresh process, one build) is
    unaffected either way.
- **Cases per level** = `case_count` (256 by default). A level need **not**
  complete — the deadline commonly cuts it mid-way; that is intended ("run as many
  as it can").
- **Width ceilings guard against OOM, not just time.** Escalating $\Ifree$ is
  exponential twice: `random_tin` enumerates $2^{\lvert\Ifree\rvert}$ cubes and
  `build_product` materialises $2^{\lvert\mathcal{I}\cup\mathcal{O}\rvert}$ letters,
  so an unbounded width climb **crashes on memory before the timer fires**.
  Constants `kCorpusWidthCeiling = 12` (max $\lvert\mathcal{I}\rvert$, hence
  $\lvert\Ifree\rvert\le 12$) and `kCorpusUnionCeiling = 16` (max
  $\lvert\mathcal{I}\cup\mathcal{O}\rvert$) clamp this. Once a level's ranges are
  saturated at the ceiling, further levels keep the same (max) width and just draw
  **fresh seeds** — new formula/partition/$\Tin$ shapes at max width until the
  deadline. Tree size keeps growing past the point where width is pinned.
- **Joint width clamp (pinned draw order).** In `random_partition`, draw
  $\lvert\mathcal{I}\rvert\in[1,\ \min(\text{input\_max},\ 12)]$ **first**, then
  $\lvert\mathcal{O}\rvert\in[0,\ \min(\text{output\_max},\ 16-\lvert\mathcal{I}\rvert)]$,
  so $\lvert\mathcal{I}\cup\mathcal{O}\rvert\le 16$ and
  $\lvert\Ifree\rvert\le 12$ always hold. (For the default level-0 ranges 5/5 the
  clamp is inert, so the non-soak corpus is unchanged.)

### The three bodies under soak (unchanged assertions)
A shared driver factors the level/deadline loop so it is not triplicated. Sketch:

```
template <class PerCase>
void run_corpus(const CorpusConfig& base, PerCase&& per_case) {
  unsigned soak = env_soak_secs();                 // 0 => off
  if (soak == 0) {                                  // fast path: one corpus
    auto corpus = BuildGeneratedCorpus(base);
    for (i) per_case(corpus[i], base, /*level=*/0, i);
    return;
  }
  auto deadline = steady_clock::now() + seconds(soak);
  std::mt19937 seed_rng(base.seed);
  for (unsigned L = 0; steady_clock::now() < deadline; ++L) {
    CorpusConfig cfg = ladder(base, L);             // clamped, above
    cfg.seed = seed_rng();
    auto corpus = BuildGeneratedCorpus(cfg);
    for (i) { if (steady_clock::now() >= deadline) break;
              per_case(corpus[i], cfg, L, i); }
  }
}
```

Each existing body supplies its `per_case` lambda with its **current, unchanged**
assertions:

1. **`TEST(GeneratedCorpus, LtlfToDfaStructural)`** — `ltlf_to_dfa(phi)` is
   `is_deterministic` + `is_complete`. Never gated on `ltlfsynt`.
2. **`TEST(GeneratedCorpus, MetamorphicRoundTrip)`** — if
   `DfaProduct::synthesize` returns a `Controller`, `verify_controller(...).ok`;
   unrealizable ⇒ assert nothing further (one-directional, per v1). Never gated.
3. **`TEST_F(LtlfsyntOracleTest, GeneratedCorpusDifferential)`** — over the
   $\mathcal{V}=\emptyset$ subset with $\lvert\mathcal{I}\cup\mathcal{O}\rvert\le
   \text{diff\_width\_cap}$ (was the literal `3`, now the clamped config value,
   $\le 5$ under soak), `ltlf-ek-synth` and `ltlfsynt --semantics=Mealy` agree on
   the verdict; `subprocess_timeout` timeout ⇒ `continue` + skip counter. Gated on
   `ltlfsynt` (GTEST_SKIP when absent — soak or not).

Because collect-all uses `EXPECT_*` (already the case in bodies 1 and 3; body 2
becomes `EXPECT_*` too if it isn't), GoogleTest records **every** failure across
every level and reports them all at the end — the desired "collect until
deadline" behaviour with no extra machinery.

### Reproducibility of a soak run (pinned)
Since *which* cases run depends on machine speed, per-run reproducibility is
impossible — but **per-case** reproducibility is preserved and is what matters:

- Each body's `SCOPED_TRACE` is extended to print, in addition to today's
  `case i: phi=… partition=…`, the **level seed** and the **level's config**
  (`seed=<n> level=<L> tree_max=<..> input_max=<..> output_max=<..>
  states_max=<..> diff_width=<..>`). (Today's trace prints neither seed nor
  params — see `ltlfsynt_oracle_test.cpp:1137,1163,1221`.)
- **Replay recipe** (documented in a comment): to reproduce a soak failure, set
  `LTLF_EK_CORPUS_SEED=<seed>`, `LTLF_EK_CORPUS_TREE_MAX=<tree_max>`,
  `_INPUT_MAX`, `_OUTPUT_MAX`, `_STATES_MAX`, `_DIFF_WIDTH` to the printed values,
  leave `LTLF_EK_SOAK` **unset**, and run the single body via `--gtest_filter`.
  `BuildGeneratedCorpus` regenerates the identical 256-case vector; the failing
  case is at the printed index. (Full determinism holds because a level's corpus
  is a pure function of `(config, seed)`.)
- **Skip/level accounting.** Each body ends with `RecordProperty` for visibility:
  `levels_reached`, `cases_run`, and (differential only) `differential_skipped`
  (already present). Failures surface through the standard `EXPECT_*` +
  `SCOPED_TRACE` channel; no separate summary structure is needed.

## Interfaces & types
No production C++. All additions are test-local (anonymous namespace in
`tests/ltlfsynt_oracle_test.cpp`):

- `struct CorpusConfig { … }` and `CorpusConfig corpus_config_from_env();` — the
  config + env reader above (loud on malformed values).
- New named constants beside the existing block
  (`ltlfsynt_oracle_test.cpp:908–934`): `kCorpusInputMax = 5`,
  `kCorpusOutputMax = 5`, `kCorpusTinStatesMax = 3`, `kCorpusDiffWidthCap = 3`
  (extracted from today's literals), plus `kCorpusWidthCeiling = 12`,
  `kCorpusUnionCeiling = 16`, `kCorpusDiffWidthSoakCap = 5`. Existing
  `kCorpusSeed`, `kCorpusCaseCount`, `kCorpusTreeSizeMin/Max`,
  `kCorpusSubprocessTimeoutSecs`, `kCorpusIknownProbability`,
  `kCorpusStrongXProbability` stay as the config's fallback defaults.
- `BuildGeneratedCorpus(const CorpusConfig&)` — the existing signature
  `std::vector<GeneratedCase> BuildGeneratedCorpus()`
  (`ltlfsynt_oracle_test.cpp:1092`) gains a `CorpusConfig` parameter (default
  argument = `corpus_config_from_env()` **not** used — callers pass explicitly so
  the ladder can inject per-level configs). Its body reads `seed`, `case_count`,
  the tree/width/state maxes from the config instead of the constants.
- `random_partition(std::mt19937&, const CorpusConfig&)` and
  `random_tin(partition, rng, dict, const CorpusConfig&)` — take the config so
  their range literals (`input_count`, `output_count`, `state_count`) and the
  joint width clamp read from it. `generate_random_formula` takes
  `tree_size_min/max` from the config. `strengthen_next` unchanged (probability
  not exposed).
- `run_corpus(const CorpusConfig& base, PerCase&&)` — the shared level/deadline
  driver above, plus a tiny `unsigned env_soak_secs()` reader (0 = off, loud on
  malformed).
- Env readers: one small `getenv`-and-parse helper (loud on malformed) reused by
  `corpus_config_from_env` and `env_soak_secs`.

Reused as-is: `DfaProduct::synthesize`, `verify_controller`, `ltlf_to_dfa`,
`trivial_transducer`, `OutputLabeledTransducer`, `VariablePartition::split`,
`all_letters_over`, the whole subprocess/verdict harness (`RunEkSynth`,
`RunLtlfsynt`, `RunSubprocess` with its existing `timeout_secs`/`timed_out`
plumbing from v1 Phase 3), `ParseEkSynthVerdict`/`ParseLtlfsyntVerdict`,
`JoinCsv`, `DescribeGeneratedPartition`.

## Implementation phases
Two independently-landable phases, each a **separate `/developer` session**, each
leaving `tests/ltlfsynt_oracle_test.cpp` compiling and `ctest` green. Phase 1 is
purely a **deterministic refactor + env layer** with the default path proven
byte-identical; Phase 2 adds the escalating driver on top without touching the
per-case assertions.

- **Phase 1 — Config struct + per-knob env overrides + seed-in-trace (deterministic
  foundation & replay layer).**
  Lands: `struct CorpusConfig` + `corpus_config_from_env()` (loud on malformed);
  the extracted named constants (`kCorpusInputMax`, `kCorpusOutputMax`,
  `kCorpusTinStatesMax`, `kCorpusDiffWidthCap`); `BuildGeneratedCorpus(const
  CorpusConfig&)` and the config-threaded `random_partition` /
  `generate_random_formula` / `random_tin`; the differential's `if (width > 3)`
  becomes `if (width > cfg.diff_width_cap)`. Each body calls
  `BuildGeneratedCorpus(corpus_config_from_env())` **once** (no level loop yet)
  and its `SCOPED_TRACE` prints `seed=` + the config maxes alongside today's
  `phi`/partition/index. **No `LTLF_EK_SOAK`, no ladder, no ceilings, no
  `run_corpus` driver.**
  **Green checkpoint:** with **no** env vars set, `ctest` is green and the corpus
  is **byte-identical to today** (same 256 cases, all three bodies pass exactly as
  before — assert/spot-check the no-drift invariant). Setting a single knob (e.g.
  `LTLF_EK_CORPUS_CASES=16`) visibly and deterministically changes the run;
  malformed values throw.
  Stubbed for later: no escalation, no budget.

- **Phase 2 — `LTLF_EK_SOAK` escalating budget driver.**
  Lands: `LTLF_EK_SOAK` switch + `env_soak_secs()`; the ceiling constants
  (`kCorpusWidthCeiling = 12`, `kCorpusUnionCeiling = 16`,
  `kCorpusDiffWidthSoakCap = 5`); the joint width clamp in `random_partition`; the
  `run_corpus(base, per_case)` shared driver (fast single pass when off; level +
  fresh-seed + deadline loop when on); the `ladder(base, L)` schedule; and the
  rewrite of the three bodies to call `run_corpus` with their unchanged per-case
  assertions (body 2 switched to `EXPECT_*` if needed for collect-all). Traces
  additionally print `level=L`; bodies `RecordProperty("levels_reached", …)` /
  `("cases_run", …)`.
  **Green checkpoint:** with `LTLF_EK_SOAK` unset, `ctest` green and **still
  byte-identical** to Phase 1's default (the `soak==0` branch of `run_corpus` must
  reproduce Phase 1's single-pass exactly — same seed, same clamp-inert ranges).
  With `LTLF_EK_SOAK=5`, a single body (via `--gtest_filter`) runs several
  escalating levels and exits within a few seconds of the budget without OOM;
  `levels_reached` > 1. The differential stays width-clamped ≤5 and any `ltlfsynt`
  slowness is skipped, not failed.

## Edge cases
- **Soak off, no env (the gate)** — single fast pass, byte-identical corpus; the
  only path CI ever takes. The hard invariant (must not regress).
- **Malformed / negative / zero env value** — throw `std::runtime_error` naming
  the var; never silent-fallback (a broken soak knob must fail, not fake a pass).
- **`tree_size_min > tree_size_max`** (via env) — validated in
  `corpus_config_from_env`, throws.
- **Width ceiling reached** — further levels hold width at
  $\lvert\mathcal{I}\rvert\le 12$, $\lvert\mathcal{I}\cup\mathcal{O}\rvert\le 16$
  and only escalate tree size + draw fresh seeds; no OOM. (The differential's own
  $\le 5$ clamp means it thins to almost nothing at high levels — most cases fail
  its $\mathcal{V}=\emptyset\ \wedge\ \text{width}\le 5$ filter — which is the
  intended "sparingly".)
- **Deadline passes mid-level** — inner loop `break`s; the in-flight case
  finishes (soft deadline). A single `ltlfsynt` subprocess is additionally bounded
  by `subprocess_timeout`.
- **`LTLF_EK_SOAK` set but very small (e.g. `1`)** — at least level 0 runs to
  completion or until the deadline; `levels_reached ≥ 1`. No special-casing.
- **`ltlfsynt` absent under soak** — the differential body `GTEST_SKIP`s (via
  `LtlfsyntOracleTest`) exactly as v1; the two library bodies still soak.
- **Empty $\Ofree$ / empty $\Iknown$ / empty $\Ifree$** — unchanged from v1;
  `random_partition` / `random_tin` handle them at every width (the
  `trivial_transducer` empty-$\Iknown$ branch still fires).
- **Per-run non-reproducibility** — **expected and accepted**: which cases run
  depends on machine speed. Per-case reproducibility is preserved via the printed
  `(seed, level-config, index)`; do **not** try to make the run count
  machine-independent (that was the rejected "fixed levels" option).
- **A soak-surfaced mismatch** — same rule as v1: "investigate, don't adjust". A
  differential disagreement or a failed metamorphic round-trip on a large case is
  a candidate `DfaProduct`/semantics bug, a Spot drift, or a real divergence —
  flag to `/theory-review`; never silence by editing the assertion, dropping the
  case, or lowering a ceiling to dodge it.

## Test oracles (for /test-writer)
The corpus stays **self-labeling** — no hand-authored expected values. Soak
changes the corpus's size/shape and adds a driver; the three graders are
unchanged and remain the labels:

1. **Metamorphic round-trip (library, always on).** Unchanged: realizable ⇒
   `verify_controller(...).ok`.
2. **`ltlf_to_dfa` structural (library, always on).** Unchanged: determinism +
   completeness on every generated $\varphi$.
3. **Differential (subprocess, gated, $\mathcal{V}=\emptyset$, width ≤
   `diff_width_cap`).** Unchanged: verdict agreement.

New test-harness properties `/test-writer` should cover (these test the *driver*,
not new domain behaviour):

- **Byte-identical default (the critical guard).** With no env set,
  `BuildGeneratedCorpus(corpus_config_from_env())` equals today's corpus — e.g.
  assert the first case's `phi` (streamed) and partition against a captured golden
  string, or assert `case_count == 256` and a stable checksum over the streamed
  cases. This is what proves the refactor didn't shift the `mt19937` stream.
- **Env override plumbing.** `LTLF_EK_CORPUS_CASES=8` ⇒ corpus has 8 cases;
  `LTLF_EK_CORPUS_SEED=1` ⇒ a *different* (but internally deterministic) corpus;
  a malformed value ⇒ throws. (Set/unset env within the test via `setenv`/`unsetenv`
  and restore; keep these off the default `ctest` path — they configure, they
  don't soak.)
- **Ladder monotonicity + clamps.** `ladder(base, L)` is non-decreasing in `L` for
  every ramped field, and never exceeds the ceilings
  (`input_max ≤ 12`, a drawn partition's `|I∪O| ≤ 16`, `diff_width_cap ≤ 5`). A
  cheap pure-function unit test — no synthesis needed.
- **Soak smoke (short budget).** `LTLF_EK_SOAK=1` (or a couple seconds) on one
  library body reaches `levels_reached ≥ 1`, terminates within a small margin of
  the budget, and does not OOM. Keep this **out** of the default `ctest` run
  (guard with the env var, or a manual/`DISABLED_` body a soaker enables), so the
  fast gate never pays the budget.

`/test-writer`'s job is the **mechanical** translation: the config reader + its
loud-on-malformed parsing, the byte-identical golden guard, the ladder unit test,
and the extended `SCOPED_TRACE`. **Do not** invent per-case expected verdicts,
**do not** auto-derive a $\psiin$ (there is none by design), and **do not** put a
real soak budget on the default gate.

## Open theory questions touched
None new — soak inherits v1's exactly, now exercised on larger cases (which is
the point: more evidence, no new resolution):

- **One-directional metamorphic on generated known-knowledge** (v1 open item) —
  unchanged; soak widens $\Tin$ shapes but still cannot catch a
  *wrongly-unrealizable* known-knowledge verdict. The co-generated
  $(\Tin,\psiin)$ family (`docs/BACKLOG.md`) remains the route; deferred.
- **Trace-termination / Mealy semantics** (glossary "Open theory questions") — the
  `X[!]` bias plus escalating tree depth deliberately stresses the
  system-controlled-termination reading shared by `solve_dfa` and
  `verify_controller`. Soak **gathers more evidence** on it; a mismatch it surfaces
  is a `/theory-review` item, not an arena edit.
- **Method-2 arena input partition ($\Ifree$ vs full $\mathcal{I}$)** (deferred in
  `docs/prd/dfa-product.md`) — wider soak differentials give broader independent
  evidence, but do not resolve it.
- **No `\na`/stub in `main.tex` is modified.**

## Definition of done
- `tests/ltlfsynt_oracle_test.cpp` gains: `struct CorpusConfig` +
  `corpus_config_from_env()` (loud on malformed); the extracted named constants +
  the three ceiling constants; `BuildGeneratedCorpus(const CorpusConfig&)` and the
  config-threaded generators; the `LTLF_EK_SOAK` switch + `env_soak_secs()`; the
  `run_corpus` level/deadline driver + `ladder(base, L)`; the joint width clamp;
  and extended `SCOPED_TRACE`s (seed + level + config) — landed across the two
  phases above.
- **The default `ctest` gate is byte-for-byte unchanged**: `LTLF_EK_SOAK` unset
  and no `LTLF_EK_CORPUS_*` set ⇒ the same 256-case corpus from `kCorpusSeed`, all
  three bodies green exactly as before, proven by the no-drift guard.
- With `LTLF_EK_SOAK>0`: each body escalates for that many seconds (documented as
  **per body**; all three under `ctest` ≈ 3× — filter to soak one), reaching
  `levels_reached > 1`, staying under the width ceilings without OOM, and
  collecting every failure to the deadline with a `(seed, level-config, index)`
  reproducer replayable via the per-knob env vars.
- The differential under soak stays width-clamped ≤ 5 and skips (never fails) on
  `ltlfsynt` timeout; absent `ltlfsynt`, it `GTEST_SKIP`s and the library bodies
  still soak.
- Malformed env values throw loudly; ladder is monotone and clamped (unit-tested).
- `cmake --build build -j && ctest --test-dir build --output-on-failure` green on
  the **default** path (or failures reported with output, per `/test-writer`).
- Glossary: confirm with `/glossary` whether "soak mode" / "escalation ladder"
  warrant a one-line addition to the existing "Testing & oracles" note (expected:
  no C++ column entry).
- `docs/BACKLOG.md`: move the "Intense soak mode for the generated corpus" item to
  Done with the landing commit, once both phases are in.

## Developer comments / PRD disagreements

- **2026-07-12 (Phase 1 landing).** No disagreements with the PRD's pinned
  shapes/names; two small implementation choices the PRD left open, recorded
  for the record:
  - The env-reading helper is a template `EnvInt<T>(name, min, max, out,
    has_value)` (parses via `std::stoll`, throws `std::runtime_error` naming
    the var on non-integer/trailing-garbage/out-of-range) rather than one
    untemplated `getenv`-and-parse function — the PRD's "Env readers" bullet
    only asked for "one small helper reused by `corpus_config_from_env` and
    `env_soak_secs()`"; the template covers both `int`- and
    `std::size_t`/`unsigned`-typed knobs without duplicating the parse/throw
    logic. `env_soak_secs()` itself is Phase 2 scope and not added yet.
  - `case_count`/`diff_width_cap`/etc. env floors: the PRD says "for
    counts/maxes — zero" must throw; implemented as "env override must be
    >= 1" (so `LTLF_EK_CORPUS_CASES=0`, `_OUTPUT_MAX=0`, etc. all throw),
    consistent with that line. `output_max`'s *default* (5) still lower-bounds
    `random_partition`'s drawn `|O|` at 0 as today (the knob's floor and the
    distribution's floor are different things — only the former is
    env-validated).

## Out of scope (pinned, do not implement here)
- **Distribution-probability knobs.** `kCorpusIknownProbability` (0.5) and
  `kCorpusStrongXProbability` (0.30) stay fixed constants — they are not part of
  the complexity ladder and were not requested. If a future soak wants to sweep
  the distribution, add them as further `LTLF_EK_CORPUS_*` knobs then.
- **Formula shrinking on failure** — a separate `docs/BACKLOG.md` item; soak
  prints the un-shrunk reproducer, as v1 does.
- **Generated $\Tout$ / $\Oknown$ and the co-generated $(\Tin,\psiin)$
  known-knowledge differential** — separate backlog items; soak keeps
  $\Oknown=\emptyset$, $\Tout$ trivial, exactly as v1.
- **Splitting one wall budget across the three bodies** — rejected; the budget is
  per body (see *Soak switch and budget*).
```

