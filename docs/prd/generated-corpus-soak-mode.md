# PRD: generated-corpus soak mode (wall-clock-budgeted escalating runner)

**Status:** implemented — Phase 1 + Phase 2 landed, plus a soak-driver
OOM/deadline-overrun bug fix (2026-07-12, uncommitted; branch `master`; see
"Developer comments / PRD disagreements")
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
      --test-dir build`); branch `master`, uncommitted.
      **Phase 2 (2026-07-12, /test-writer):** three deliverables landed, all
      in `tests/ltlfsynt_oracle_test.cpp`.
      (1) `Ladder.MonotoneAndClampedAcrossLevels` (pure function, levels
      0..40, asserts non-decreasing ramped fields, unramped fields pinned to
      `base`'s value, and the three ceilings both never-exceeded and
      actually-reached by level 40) plus
      `Ladder.DrawnPartitionRespectsWidthCeilingsAtHighLevels` (calls
      `random_partition` — no formula generation, so no Spot RNG hazard — at
      levels 0/1/5/20/100, asserts the drawn `|I|`/`|I∪O|` stay within the
      ceilings). Both run unconditionally on the default `ctest` path (no
      env, no synthesis, sub-millisecond).
      (2) `GeneratedCorpusSoak.DISABLED_LtlfToDfaStructuralReachesAtLeastOneLevel`
      — `DISABLED_`-prefixed so GoogleTest skips it under a plain `ctest`
      run (verified: 215/215 non-disabled tests pass, this one reports
      "Disabled" and does not execute); sets `LTLF_EK_SOAK=1` via
      `ScopedEnvVar` scoped to the test body, drives the
      `LtlfToDfaStructural` assertion through `run_corpus`, and asserts
      `levels_reached >= 1` plus a generous elapsed-time ceiling. Manually
      verified enabled (`--gtest_also_run_disabled_tests
      --gtest_filter=GeneratedCorpusSoak.*`): `levels_reached=8,
      cases_run=1792, elapsed_ms=1717` for the 1 s budget — no OOM, no
      hang.
      (3) `DumpTinForReplay` closes the metamorphic `t_in` replay gap:
      renders the reachable (state, Ifree-letter) → (lambda, delta-dst)
      table for a generated case's random `Tin` (Ifree alone determines
      every pair, since `random_tin`'s guards/lambda read only Sigma0), via
      a throwaway registrar `twa_graph` on `t_in`'s own dict (idempotent
      `register_ap`, no new accessor needed on `OutputLabeledTransducer`).
      Wired into `MetamorphicRoundTrip`'s `EXPECT_TRUE(...).ok` `<<` stream,
      so GoogleTest only evaluates/prints it on an actual failure (verified
      via a temporary forced-failure scratch test, output confirmed
      readable and reverted before landing) — zero cost on passing cases.
      No same-seed-twice-in-process reproducibility test was written (the
      hard constraint from the Phase-2 developer note above; per-case
      replay is `(phi, partition)` from `SCOPED_TRACE` + this `t_in` dump,
      not corpus regeneration).
      Full suite green, 215/215 (`ctest --test-dir build`), 1 `DISABLED_`
      soak test correctly excluded from the count; branch `master`,
      uncommitted.
- [x] code-review     — domain (/code-reviewer) + generic (/code-review) both
      clean on the Phase 1 diff (2026-07-12). No correctness findings (the one
      candidate — the seed golden false-failing via Spot's process-global RNG —
      was empirically refuted: goldens are stable across isolated / shared-process
      runs). Four low-severity cleanups applied: portable FNV-1a checksum (was
      std::hash, implementation-defined); `EnvInt` → `std::optional` (removed the
      9-block read/cast boilerplate); `DescribeCorpusConfig` now prints all knobs
      (tree_min/cases/timeout) for full replay; dropped duplicate
      `kGoldenDefaultCorpusCaseCount`; single-computed seed checksum. Suite green
      213/213 after fixes. **Phase 2 diff re-reviewed (2026-07-13)** —
      `/code-reviewer` (domain) + `/code-review` (generic, high) both clean on the
      `env_soak_secs`/`ladder`/`run_corpus`/`GenerateOneCase`/joint-clamp/
      `DumpTinForReplay` diff. `DumpTinForReplay`'s Spot usage (t_in's own dict,
      idempotent AP registration, Ifree-only enumeration) confirmed idiomatic; no
      domain-invariant or synthesis-semantics code touched. Three fixes applied:
      `static_assert(kCorpusWidthCeiling < kCorpusUnionCeiling)` + `std::max(0,…)`
      guard on the joint clamp (latent negative-range UB); widened `ladder`'s
      per-level additions to `long long` before the min-clamp (signed-overflow UB
      on a pathological env max); trimmed the ceiling-constant deviation narration
      to a PRD pointer. One cosmetic finding left (`levels_reached` may overcount a
      zero-case level by 1 — diagnostic only). Suite green 215/215 after fixes.
- [ ] theory-review   — code ↔ math faithfulness vs main.tex (n/a for the Phase 1
      test-harness diff; the Spot RNG quirk is a mechanical Spot-API item captured
      as a Phase-2 must-resolve, not a theory question). Phase 2 is likewise a pure
      test-harness diff, no new synthesis semantics — n/a expected, but unconfirmed.

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

- **2026-07-12 (Phase 2 landing).** Implemented `env_soak_secs()`, the three
  ceiling constants, the joint width clamp in `random_partition`, `ladder(base,
  L)`, `run_corpus(base, per_case)` (returning a `RunCorpusStats{levels_reached,
  cases_run}` the PRD didn't name — a small addition so each body's
  `RecordProperty` calls have something to read; the PRD only said "the shared
  level/deadline driver", not its return shape), and rewrote all three bodies to
  call it. Green checkpoint reached: `LTLF_EK_SOAK` unset stays 213/213
  byte-identical to Phase 1 (`ctest --test-dir build`); a manual
  `LTLF_EK_SOAK=5` smoke on each of the three bodies (via `--gtest_filter`)
  reached `levels_reached` 4–10 within a few seconds of the 5 s budget, no OOM,
  `differential_skipped` present and 0 on this box.
  - **`ladder`'s `output_max` field does not itself subtract the drawn `|I|`.**
    The PRD's ladder pseudocode writes `output_max = min(B.output_max + L,
    kCorpusUnionCeiling - drawn |I|)`, but "drawn `|I|`" is a **per-case**
    quantity, not knowable when `ladder(base, L)` builds one `CorpusConfig` for
    the whole level. Implemented as written elsewhere in the same PRD section
    ("Joint width clamp (pinned draw order)"): `ladder` sets
    `cfg.output_max = min(B.output_max + L, kCorpusUnionCeiling)` (no `|I|`
    term), and the `- drawn |I|` reduction happens where `|I|` actually exists —
    inside `random_partition`, after drawing that case's own `|I|`. Net effect on
    the drawn ranges is identical to the PRD's intent; only *where* the
    `|I|`-dependent half of the clamp is applied differs from a literal reading
    of the ladder line.
  - **⚠ Load-bearing checkpoint finding, deeper than the PRD's "Fresh seed per
    level" theory — the Spot non-determinism is *not* (only) the process-global
    RNG.** Implemented exactly as directed: `spot::srand(cfg.seed)` immediately
    before each per-level `BuildGeneratedCorpus` in `run_corpus`'s level loop
    (`#include <spot/misc/random.hh>` added). Per the PRD's own instruction, I
    then did the "quick manual check" — build the *same* `cfg.seed` twice in one
    process (with the fix applied both times) and diff the corpora. **It still
    diverges**: case 0 of an 8-case corpus matched between the two builds, but
    case 1 did not (`"p5 & ((p3 & Fp6) | (!p3 & G!p6))"` vs `"p6 & ((p3 & Fp4) |
    (!p3 & G!p4))"` — a same-shape formula with different atomic-prop names).
    I isolated this to `generate_random_formula` itself: two **back-to-back**
    calls with an *identical* `VariablePartition`, `CorpusConfig`, and
    `std::mt19937(999)` seed, with an *explicit* `spot::srand(777)` reset
    immediately before **each** call, still diverged ("Xp6" vs "Xp5") — proving
    the divergence is **not** a leftover-RNG-state issue (an explicit,
    identical `srand` reset right before construction has zero effect on the
    result; confirmed by also forcing `spot::srand(0)` inside
    `generate_random_formula` right before constructing `randltlgenerator`,
    which changed nothing). The actual mechanism (traced into Spot's own
    source, `spot/tl/formula.cc` `fnode::ap` and `spot/tl/randomltl.cc`
    `next()`'s `mrand(rl->ap()->size())`): `spot::formula` atomic props are
    **reference-counted and hash-consed by name** (`fnode::ap`'s `m.name2ap`
    cache); when the last reference to an atomic prop goes out of scope its
    numeric id is placed on a **free list** (`m.free_apid`) and **recycled** for
    the next new atomic prop. `randltlgenerator::next()` selects which AP
    becomes a given literal by indexing (`mrand` + `std::advance`) into an
    **id-sorted** `atomic_prop_set` — so if a name's id gets recycled between two
    calls (because a *previous* call's returned formula didn't happen to
    reference that name, so its temporary `atomic_prop_set` reference was the
    last one and got dropped), the *same* `mrand()` draw lands on a *different*
    name. This is a property-hash-consing/GC effect, orthogonal to (and not
    fixed by) resetting the RNG. **Consequence:** `run_corpus`'s in-process,
    multi-corpus-per-process construction is *not* guaranteed to make a level's
    corpus a bit-perfect pure function of `cfg.seed` even with the PRD's
    directed fix applied — the fix is still correct and worth keeping (it *does*
    reset the one mechanism the PRD identified, and does not hurt), but it is
    **necessary, not sufficient**. **What is unaffected:** the documented
    *per-case replay recipe* (fresh process, `LTLF_EK_SOAK` unset, pin the
    printed seed + knobs via `LTLF_EK_CORPUS_*`, run the single body) — a fresh
    process starts with an empty atomic-prop table, so a *single*
    `BuildGeneratedCorpus` call in a fresh process is unaffected by this
    recycling effect (this is exactly the mechanism Phase 1's own golden tests
    already rely on and have proven stable, 213/213, repeatedly). **Flagged for
    `/test-writer`:** do **not** write an in-process "same `cfg.seed` built
    twice ⇒ byte-identical corpus" unit test for `run_corpus`'s level loop — it
    will be flaky/false-failing for the reason above, not a real bug when it
    fails. The ladder monotonicity/clamp test (pure function of `(base, L)`,
    no corpus construction) is unaffected and safe to write as specified.
    Also flagged for `/theory-review` in spirit (it is a Spot-library mechanical
    quirk, not a `main.tex` question, so likely a no-op there) and for a
    `docs/BACKLOG.md` note if a future soak wants true in-process
    level-to-level reproducibility (would require keeping every previously-used
    atomic prop alive for the process's lifetime, e.g. a held `atomic_prop_set`
    accumulator — out of scope here, not requested by this PRD).

- **2026-07-12 (soak-driver OOM/deadline-overrun bug fix, post-Phase-2).** A
  user-reproduced defect in the landed Phase 2 driver: `LTLF_EK_SOAK=40` on
  `GeneratedCorpusDifferential` ran 128s (well past the 40s budget), then threw
  `std::bad_alloc`, and the next test body hung. Root cause: `run_corpus`'s
  `soak>0` branch called `BuildGeneratedCorpus(cfg)` to materialise the
  **entire** `cfg.case_count`-case corpus for a level **before** any per-case
  deadline check ran — so at a high level (wide `Ifree`, growing `Tin` state
  count) the corpus-build step itself was unbounded and could exhaust memory
  before the deadline check inside the old per-corpus loop ever got a chance to
  fire. The width ceilings did not help because they bound a single case's
  size, not the *unbounded build step* that sat outside any check. Three fixes,
  all test-local (`tests/ltlfsynt_oracle_test.cpp`), no production C++:
  1. **Lazy, one-case-at-a-time generation in the soak branch.** Extracted the
     per-case body of `BuildGeneratedCorpus`'s loop (`random_partition` →
     `generate_random_formula` → `strengthen_next` → `random_tin`) into a new
     helper `GeneratedCase GenerateOneCase(std::mt19937&, const
     CorpusConfig&)`. `BuildGeneratedCorpus` itself is otherwise **unchanged**
     (still builds the full vector up front, still the exact call the
     `soak==0` fast path and the golden tests use) — it just calls
     `GenerateOneCase` once per case instead of inlining the body, which does
     not alter the `std::mt19937` draw order. `run_corpus`'s `soak>0` level
     loop no longer calls `BuildGeneratedCorpus` at all; it owns its own
     `std::mt19937 rng(cfg.seed)` and calls `GenerateOneCase(rng, cfg)`
     per case, with the deadline checked **immediately before each
     generation** (not just between cases/levels as before).
  2. **Lowered ceilings**, since a single case can still be large enough to OOM
     even when generated one at a time: `kCorpusWidthCeiling` 12 → 10,
     `kCorpusUnionCeiling` 16 → 12 (deviates from this PRD's pinned 12/16 —
     the PRD did not anticipate a single case, at its own pinned ceiling,
     being large enough to OOM before the *next* deadline check). The default
     level-0 ranges (`input_max`/`output_max` = 5/5) stay far below both new
     ceilings, so the byte-identical default-corpus golden is unaffected.
  3. **Per-case `std::bad_alloc` catch in the soak loop.** `GenerateOneCase`'s
     result is now wrapped in a `try`/`catch (const std::bad_alloc&)`; a catch
     increments a new `RunCorpusStats::cases_skipped` counter (a PRD-unnamed
     addition, same rationale as `levels_reached`/`cases_run`) and the loop
     moves on to the next case index — never aborting the body. All three
     bodies now also `RecordProperty("cases_skipped", ...)`. This makes an
     over-large draw degrade to a skip even if a future ceiling still proves
     too generous on some machine, rather than crashing the process (which is
     also what caused the *next* test body to hang — a crashed process mid-body
     leaves GoogleTest's harness in a bad state for whatever runs next).

  **Verification.** `cmake --build build -j` clean;
  `ctest --test-dir build --output-on-failure` **215/215**, including
  `GeneratedCorpus.DefaultCorpusIsByteIdenticalToGolden`, all five
  `CorpusConfigFromEnv.*` env-plumbing tests, and both `Ladder.*` tests
  (unaffected — they exercise `ladder`/`random_partition` directly, not
  `run_corpus`). Manual soak runs (`--gtest_filter`, this box):
  `LTLF_EK_SOAK=20` on `GeneratedCorpusDifferential` → 28.5s wall (8.5s over
  budget — one in-flight `ltlfsynt`/`ek-synth` subprocess pair finishing, each
  bounded by `subprocess_timeout_secs`), `levels_reached=13`,
  `cases_run=3114`, `cases_skipped=0`, `differential_skipped=0`, peak RSS
  ≈1.8 GB, no OOM. `LTLF_EK_SOAK=20` on `GeneratedCorpus.LtlfToDfaStructural`
  → 30.6s wall, `levels_reached=12`, `cases_run=2859`, `cases_skipped=0`, no
  OOM. Neither the original 128s-then-crash symptom nor a hang reproduced.
  - **Follow-up finding, not fixed here (out of this bug's scope).**
    `LTLF_EK_SOAK=20` on `GeneratedCorpus.MetamorphicRoundTrip` took **105s**
    (`levels_reached=5`, `cases_run=1229`, `cases_skipped=0`, no OOM) — it
    passed and did not crash or hang, but overran the budget by far more than
    the other two bodies. Cause: unlike the differential (bounded per
    subprocess by `subprocess_timeout_secs`) and the structural check (fast
    `ltlf_to_dfa` only), `MetamorphicRoundTrip`'s per-case work is
    `DfaProduct::synthesize` + `verify_controller`, which has **no time bound
    at all** — the PRD's own "Deadline mechanics" already pins this as
    accepted ("the case in flight when it passes runs to completion... No
    mid-case interruption"), but did not anticipate how large that in-flight
    cost could get at level 5 (`tree_size_max` = 25, wide alphabet): a single
    `synthesize` call on a large generated case can dominate the wall clock
    for tens of seconds with no way to bound it short of adding a
    synthesize-level timeout (a real feature, not a bug-fix line item, and
    arguably production-adjacent since `DfaProduct::synthesize` itself has no
    timeout parameter). Not addressed by this fix — flagged for
    `docs/BACKLOG.md` if a soaker wants `MetamorphicRoundTrip`'s overrun
    tightened (e.g. a thread-based watchdog around `synthesize`, or a note in
    the PRD's "Deadline mechanics" section documenting that library-body
    overrun can be large, not just "a few seconds").

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

