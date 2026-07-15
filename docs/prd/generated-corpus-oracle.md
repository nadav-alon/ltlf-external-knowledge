# PRD: fixed-seed generated-corpus differential + metamorphic test harness

**Status:** implemented — Phase 1 (corpus scaffold + `ltlf_to_dfa` structural
free-rider), Phase 2 (random $\Tin$ + metamorphic round-trip), and Phase 3
(differential + timeout plumbing) all landed. `tests/ltlfsynt_oracle_test.cpp`
`GeneratedCorpus.LtlfToDfaStructural` and `GeneratedCorpus.MetamorphicRoundTrip`
are green over 256 cases with no `ltlfsynt` dependency;
`LtlfsyntOracleTest.GeneratedCorpusDifferential` is green over the
$\mathcal{V}=\emptyset$, width-$\le3$ subset with `ltlfsynt` present (189/189
`ctest` overall).
**Interface:** extends the existing GoogleTest suite
`tests/ltlfsynt_oracle_test.cpp` (no production C++). Adds a seeded random-formula
/ random-partition / random-$\Tin$ generator and two new test bodies that grade a
generated corpus with the suite's **self-labeling** oracles (the oracle *is* the
expected value): the `ltlfsynt` **differential** and the
`synthesize`$\to$`verify_controller` **metamorphic round-trip**. Not a `Synthesis`
method; no new algorithm.
**main.tex ref:** Method 2 (`alg:dfa_product`); the *enabled* predicate
`\cref{def:consistency}` (§107–116) + committed Case-A totality (the generated $\Tin$
must satisfy these); the controller postcondition `\cref{def:probDefTransducer}`
(§129–131, decided by `verify_controller`); the Mealy observed slice
$\Sigma_0=\mathcal{I}$ of $S_C$ (`main.tex` §86).

**Gates:**
- [x] glossary        — no new domain identifier; testing-methodology prose note
      added to docs/GLOSSARY.md "Testing & oracles" (generated corpus + differential
      / metamorphic round-trip), no C++ column domain entry (per PRD Glossary note)
- [ ] tests           — unit + oracle coverage
- [x] code-review     — domain (/code-reviewer) + generic (/code-review) clean on
      working tree (Phase 1 `010cca9` + uncommitted P2/P3); only non-blocking
      cleanups (3× corpus rebuild, dead `WIFSIGNALED` arm, comment hygiene)
- [ ] theory-review   — code ↔ math faithfulness vs main.tex

## Goal
The suite's oracle **architecture** is strong — the external `ltlfsynt`
differential (`docs/prd/ltlfsynt-oracle.md`), the `verify_controller` metamorphic
round-trip (`docs/prd/controller-verifier.md`), the Moore monolithic baseline
(`tests/dfa_product_test.cpp:160`), and the faithfulness guard + meta-oracle
(`docs/prd/oracle-faithfulness-guard.md`) — but every one is fed a **thin,
hand-picked** corpus: formulas cap at ~3 operators / ≤3 APs / nesting depth ≤2,
`ltlf_to_dfa` determinism/completeness is asserted on exactly one formula
(`a U b`), and the whole external-knowledge thesis rests on a single genuine
knowledge-flip (`X[!] 1 & (o <-> X i)`). This PRD adds a **fixed-seed** generator
whose cases are graded by the two self-labeling oracles, so we get breadth at
near-zero authoring cost per case — no hand-labeled expected value, because the
oracle *is* the label. It **complements, does not supersede**, any existing PRD;
those curated corpora keep their archival hand-verified rows.

Scope pinned in the grill:

- **Two oracles driven.** (a) The `ltlfsynt` differential — verdict-only,
  empty-knowledge ($\mathcal{V}=\emptyset$, $\psiin=\top$) — and (b) the
  `synthesize`$\to$`verify_controller` metamorphic round-trip — library-only,
  including **generated known-input** $\Tin$. Plus a free-rider structural check:
  `ltlf_to_dfa($\varphi$)` determinism + completeness on every generated
  $\varphi$.
- **Generated known $\Tin$ is in v1** (metamorphic path only — it needs no
  $\psiin$). Generated $\Tout$ / $\Oknown$ and a generated $\psiin$ for a
  known-knowledge *differential* are **deferred** (see *Open theory questions* /
  `docs/BACKLOG.md`).
- **`X[!]` (strong next) is generated**, since it is the operator that stresses
  the system-controlled-termination / weak-X-at-final-position region (memory
  `ltlf-weak-x-and-termination-semantics`); Spot's `randltl` emits weak `X` by
  default, so the harness strengthens a fraction of `X` nodes to `X[!]`.

## Ubiquitous-language terms used
All already in `docs/GLOSSARY.md`; the generator drives existing artifacts, it
introduces no new domain concept with a public C++ identifier.

- **Goal formula** $\varphi$ → `phi` (`spot::formula`) — here randomly generated.
- **Inputs / Outputs** and the four-way split
  $\Ifree,\Iknown,\Ofree,\Oknown$ → `VariablePartition` — here randomly split.
- **External knowledge strategy** ($\Tin$) → `Transducer` / `OutputLabeledTransducer`
  — here randomly generated (in-memory, not from a file).
- **Observed / produced slice** $\Sigma_0,\Sigma_1$ → for the generated $\Tin$,
  $\Sigma_0=\Ifree,\Sigma_1=\Iknown$ (glossary "Role" `t_in`).
- **Consistency** $\cons$ → `consistent` — enforced by `DfaProduct` and reused by
  `verify_controller`.
- **DFA product** (Method 2) → `DfaProduct`; **Goal DFA construction** →
  `ltlf_to_dfa` (its determinism/completeness is the structural free-rider check).
- **Controller verifier** → `verify_controller` — the metamorphic post-condition.
- **Controller (system strategy)** → `Controller` returned by
  `DfaProduct::synthesize`.

**Glossary note (flag for `/glossary`, do not block):** "generated corpus",
"differential oracle", "metamorphic round-trip" are **testing** concepts realised
by test-local helpers (anonymous namespace in `tests/ltlfsynt_oracle_test.cpp`),
not library APIs — like the *Faithfulness guard*, they likely warrant at most a
prose note, no C++ column entry. Confirm with `/glossary`.

## Behaviour / semantics (from main.tex)
The harness asserts **only** properties already established elsewhere; it adds no
synthesis semantics, it just feeds them a generated corpus.

### Placement and structure
Everything lives in `tests/ltlfsynt_oracle_test.cpp` (grill decision), reusing its
existing anonymous-namespace harness: `RunEkSynth` / `RunSubprocess`,
`ScopedTempFile`, `ShellQuote`, `ParseEkSynthVerdict` / `ParseLtlfsyntVerdict` /
`Verdict`, and the `LtlfsyntOracleTest` fixture (which `GTEST_SKIP()`s when
`ltlfsynt` is absent). **Three** new test bodies (two library, one differential)
grade one shared, seeded case list — realized as **separate `TEST`s** (grill:
additive phasing, one body per implementation phase, see *Implementation
phases*), each a single loop over the corpus:

1. **Structural test (library, never gated — Phase 1).** A `TEST(...)` (not under
   `LtlfsyntOracleTest`) that iterates every generated case and builds
   `ltlf_to_dfa($\varphi$)`, asserting it is **deterministic** and **complete**
   (see *ltlf_to_dfa structural check*). Needs no $\Tin$ — this is P1's
   self-labeling checkpoint.

2. **Metamorphic test (library, never gated — Phase 2).** A separate `TEST(...)`
   (not under `LtlfsyntOracleTest`) that iterates every generated case and runs
   `DfaProduct::synthesize($\varphi$, vars, t_in, t_out)`; if it returns a
   `Controller`, asserts `verify_controller($\varphi$, vars, t_in, t_out,
   *controller).ok` is **true** (the metamorphic round-trip). `t_out` is always
   the `trivial_transducer(vars, Role::t_out, dict)`; `t_in` is the generated
   $\Tin$ (trivial when $\Iknown=\emptyset$, else the random table transducer).
   - Both library bodies must **not** be skipped when `ltlfsynt` is missing — that
     is why they are separate `TEST`s from the differential, not `GTEST_SKIP`ing
     bodies.

3. **Differential generated test (gated on `ltlfsynt` — Phase 3).** A `TEST_F` under
   `LtlfsyntOracleTest` that iterates the **$\mathcal{V}=\emptyset$ subset** of the
   generated cases (all-free partition, so $\psiin=\top$) whose width is
   $\lvert\mathcal{I}\cup\mathcal{O}\rvert\le 3$ (see *Partition generation*), and
   asserts `ltlf-ek-synth` and `ltlfsynt --semantics=Mealy` return the **same**
   REALIZABLE/UNREALIZABLE verdict on the bare $\varphi$ (no assumption to reduce,
   so no load-bearing guard — that concept applies only when $\psiin\neq\top$).

Each body is a single loop with `SCOPED_TRACE` printing the offending
`$\varphi$` + partition + case index (grill: no 256 parameterized CTest entries;
the seed + index reproduces any failure).

### Determinism / seeding (bespoke — pinned)
- One `constexpr unsigned kCorpusSeed` (fixed, e.g. `20260706`, project style
  mirrors `kGuardSampleSeed = 20260705`). One `constexpr std::size_t
  kCorpusCaseCount` (~256, tunable). One `constexpr unsigned
  kCorpusSubprocessTimeoutSecs = 10`.
- `BuildGeneratedCorpus()` seeds **one** `std::mt19937 rng(kCorpusSeed)` and a
  Spot `randltlgenerator` from a seed derived deterministically from it, then emits
  exactly `kCorpusCaseCount` cases into a `std::vector`. All test bodies iterate
  that same vector, so within one landed tree a failure reproduces identically
  across runs and machines. (Across the *phased* rollout the single stream shifts
  when P2 inserts its $\Tin$ draw — an accepted trade, see *Implementation
  phases* → cross-phase seed note; reproducibility is a property of the final
  tree, not of the intermediate P1-only / P2-only states.)

### Formula generation (bespoke — pinned)
- Reuse **Spot's `randltlgenerator`** (`<spot/tl/randomltl.hh>`, the class backing
  the `randltl` binary; do not hand-roll). Operator palette: the LTLf-safe set
  `U / R / W / F / G / X / ! / & / | / -> / <->`. Tree-size cap **≤ ~8–10** nodes
  (so `ltlf_to_dfa` / `ltlfsynt` stay tractable) while nesting depth ≥3 is
  reachable.
- **`X[!]` injection.** `randltl` emits weak `X` (`op::X`). After generation,
  recursively rewrite the formula tree, replacing each `op::X` node with
  `op::strong_X` (`X[!]`) with probability ~**0.30**, drawn from `rng` (so it is
  seeded/reproducible). Both `ltlf-ek-synth` and `ltlfsynt` accept weak `X` and
  `X[!]` (the curated corpus already mixes both), so no encoding hazard — this
  only shifts the distribution toward the strong-next hardness region.
  **Spot-version caveat (pinned so `/developer` need not discover it):**
  `op::strong_X` is hidden from Spot's public operator enum before 2.13. Use the
  version-safe opt-in — `#define SPOT_USES_STRONG_X 1` **before** including
  `<spot/tl/formula.hh>` (documented Option 1 in that header, works for Spot ≥2.9)
  — then build the node with `spot::formula::unop(spot::op::strong_X, child)`.
  Do **not** switch to string round-tripping.
- **APs come from the partition, not from `randltl`'s default `p0…`.** Generate the
  partition **first** (below), take its exact AP-name set, and pass that set to
  `randltlgenerator`. Consequence: every generated $\varphi$'s APs are a **subset**
  of $\mathcal{I}\cup\mathcal{O}$ **by construction** — the AP-naming blind spot
  (a typo'd AP silently becoming a fresh free input to `ltlfsynt`) cannot occur, so
  no separate AP-scope guard is needed for generated cases. `randltl` may leave
  some partition APs unused; that is a legal formula over a subset (extra free
  variables simply exist).

### Partition generation (bespoke — pinned)
Per case, draw from `rng`:
- **Metamorphic (library, fast) width:** $\lvert\mathcal{I}\rvert\in[1,5]$,
  $\lvert\mathcal{O}\rvert\in[0,5]$ ($0$ hits the empty-$\Ofree$ edge case). A
  random subset of the inputs is marked $\Iknown$ (**may be empty** = degenerate
  empty-knowledge). $\Oknown=\emptyset$ always in v1 ($\Tout$ stays trivial). AP
  names are fresh `p0, p1, …` up to $\lvert\mathcal{I}\cup\mathcal{O}\rvert$.
- **Differential (subprocess, slow) width cap:** the differential body only runs
  cases with $\mathcal{V}=\emptyset$ **and**
  $\lvert\mathcal{I}\cup\mathcal{O}\rvert\le 3$ (grill: "differential caps
  narrower"). Rationale: `ltlfsynt` blows up combinatorially on wide alphabets, so
  the deep/wide stress is routed to the fast in-process metamorphic oracle while
  the external check stays *effective* (not mostly-skipped) on tractable sizes.
  Every generated case still carries its full width for the metamorphic body; the
  differential body simply filters.
- Build the `VariablePartition` via `VariablePartition::split(inputs, outputs,
  governed)` with `governed = ` the chosen $\Iknown$ set.

### Random $\Tin$ generation (bespoke — pinned to code)
Build an `OutputLabeledTransducer` **in memory** (no file I/O — like
`tests/dfa_product_test.cpp`'s `TinAlwaysI`), on the same shared `bdd_dict` as the
rest of the case. Role `t_in` ⇒ $\Sigma_0=\Ifree$, $\Sigma_1=\Iknown$. Validity
(deterministic + total, the committed **Case-A** regime, `\cref{def:consistency}`) is
guaranteed **by construction**:

```
n = rng in [1, 3]            # number of states
g = make_twa_graph(dict); register every I∪O AP; g.new_states(n); init 0
for q in 0..n-1:
  lambda_q = bddfalse
  for each valuation ifree over Ifree (all 2^{|Ifree|} of them):
    dst = rng in [0, n-1]
    g.new_edge(q, dst, cube(ifree))          # delta: one edge per Ifree-cube
    lambda_q |= cube(ifree) & rand_iknown_cube(rng)  # a full Iknown assignment
  lambda[q] = lambda_q
OutputLabeledTransducer(g, lambda, sigma0_cube=cube(Ifree), sigma1_cube=cube(Iknown))
```

- **Determinism + totality of $\delta$:** the per-state edges are guarded on
  **mutually exclusive, exhaustive** $\Ifree$-cubes (one per valuation), so for any
  full letter exactly one edge matches — deterministic and total over $\Ifree$
  (the transducer observes only $\Sigma_0=\Ifree$; it ignores the $\Iknown$/$\Ofree$
  bits of the letter, as `\cref{def:consistency}` allows).
- **Functionality + totality of $\lambda$:** for each $\Ifree$-valuation exactly one
  $\Iknown$ cube is OR'd in (`rand_iknown_cube` assigns **every** $\Iknown$ var a
  truth value), so $\lambda(q,\cdot)$ is a total function $\Ifree\to 2^{\Iknown}$ —
  the exact shape `OutputLabeledTransducer` expects.
- **Empty $\Ifree$** (all inputs known): the "for each valuation" loop runs once
  (the empty cube = `bddtrue`), giving a constant-knowledge transducer — valid.
- **Empty $\Iknown$:** the case is generated as `trivial_transducer(vars,
  Role::t_in, dict)` instead (degenerate empty-knowledge); no random table needed.
- **No $\psiin$ is produced.** The metamorphic oracle checks the generated $\Tin$
  against `verify_controller`, which consumes the transducer directly — so there is
  nothing to hand-encode and the *faithfulness guard* (which cross-checks a
  $(\Tin,\psiin)$ pair) **does not apply** to generated cases.

### `ltlf_to_dfa` structural check (free rider)
For every generated $\varphi$, assert the returned `spot::twa_graph_ptr` is
**deterministic** (`spot::is_deterministic`, `<spot/twaalgos/isdet.hh>`, or: no
state has two outgoing edges whose guards overlap) and **complete**
(`spot::is_complete`, `<spot/twaalgos/complete.hh>`, or: every state's outgoing
guards sum to `bddtrue`) — `ltlf_to_dfa` calls `spot::complete_here`, so
completeness must hold. This kills the "`ltlf_to_dfa` asserted on one formula"
blind spot at zero oracle cost (a pure library property, no external tool, no
expected value). `/test-writer` picks the exact predicate spelling.

### Subprocess timeout (bespoke — pinned)
- Wrap each differential subprocess in a wall-clock **timeout** of
  `kCorpusSubprocessTimeoutSecs` (prefix the shell command with `timeout
  <N>s …`, coreutils `timeout`, present in this environment). Thread a
  `timed_out` signal back through `RunSubprocess` (e.g. exit code **124** from
  `timeout`, or `WIFSIGNALED`).
- **On timeout: skip that case** — `continue` the loop, increment a skip counter,
  and (optionally) `RecordProperty("differential_skipped", n)` for visibility. A
  slow `ltlfsynt` is **not** a test failure. With the ≤3 differential width cap
  timeouts should be rare; the counter surfaces regressions if they are not.
- On any **other** unexpected exit code / missing verdict word, fail loudly with
  captured stderr (unchanged from the existing `ParseEkSynthVerdict` /
  `ParseLtlfsyntVerdict` contract) — a parse/usage error must never masquerade as
  a verdict.

### Mealy guard (pinned, not a fork)
The differential drives `ltlfsynt --semantics=Mealy` **exclusively** — matching
our Mealy controller ($S_C$ observes $\Sigma_0=\mathcal{I}$, `main.tex` §86). The
in-process **Moore** baseline (`ltlf_to_mtdfa_for_synthesis`,
`tests/dfa_product_test.cpp`) is **not** used on the generated corpus: it would
throw spurious failures on Mealy-sensitive formulas (`o <-> i`). Both tools share
Spot's LTLf family, so the weak-X-at-final-position convention is consistent across
the differential's two sides.

## Interfaces & types
No production C++. All additions are test-local (anonymous namespace in
`tests/ltlfsynt_oracle_test.cpp`):

- `struct GeneratedCase { spot::formula phi; VariablePartition partition;
  /* enough to rebuild t_in deterministically, e.g. the rng-derived transducer
  spec, or the built OutputLabeledTransducer on a per-case dict */ };` — exact
  shape is `/developer`'s call, but it must let **both** test bodies replay the
  identical case (the metamorphic body needs `t_in`; the differential body needs
  only `phi` + partition).
- `std::vector<GeneratedCase> BuildGeneratedCorpus();` — seeded from `kCorpusSeed`,
  emits `kCorpusCaseCount` cases.
- Formula/partition/$\Tin$ generator helpers as pinned above (`randltlgenerator`
  wrapper, `strengthen_next(formula, rng)`, `random_partition(rng)`,
  `random_tin(partition, rng, dict)`).
- Timeout plumbing: extend `RunSubprocess` (or add a variant) to prefix `timeout`
  and report `timed_out`.

Reused as-is: `DfaProduct::synthesize`, `verify_controller`, `ltlf_to_dfa`,
`trivial_transducer`, `OutputLabeledTransducer`, `VariablePartition::split`,
`consistent` (indirectly), and the whole existing subprocess/verdict harness.

## Implementation phases
Three independently-landable phases, each a **separate `/developer` session**,
ordered by dependency (formula → $\Tin$ → external differential). Each leaves
`tests/ltlfsynt_oracle_test.cpp` compiling and `ctest` green. The three checks
are **three separate test bodies** (grill decision), so every phase is purely
**additive** — a later phase grows the shared `GeneratedCase` /
`BuildGeneratedCorpus` (and, in P3, `RunSubprocess`) but **never rewrites a prior
phase's assertions**.

**Cross-phase seed note (grill decision — "accept corpus shift").** The corpus
uses **one** `std::mt19937(kCorpusSeed)` with **no reserved draw slots**. When P2
inserts its per-case $\Tin$ draw, the single stream shifts, so P1-only and
P2-only intermediate trees run on *different* 256-case corpora. This is
**accepted**: the oracles are self-labeling, so any corpus is valid, and once P3
lands the draw order is final and reproducible-by-index from then on. Only the
throwaway intermediate reproducibility is sacrificed — it has no archival value.
Do **not** add per-index sub-RNGs or reservation to "fix" this; it is a
deliberate choice, not an oversight.

- **Phase 1 — Corpus scaffold + `ltlf_to_dfa` structural free-rider.**
  Lands: `kCorpusSeed` / `kCorpusCaseCount` constants; the `GeneratedCase` struct
  in its P1 shape (`spot::formula phi; VariablePartition partition;` — **no**
  `t_in` field yet); the `randltlgenerator` wrapper + `X[!]` strengthener
  (`strengthen_next`, with the pre-2.13 `op::strong_X` opt-in pinned in *Formula
  generation*); `random_partition`; and `BuildGeneratedCorpus()` emitting
  `kCorpusCaseCount` cases **partition-first** (so every $\varphi$'s APs are a
  subset of $\mathcal{I}\cup\mathcal{O}$ by construction). One library body
  `TEST(GeneratedCorpus, LtlfToDfaStructural)` asserts `ltlf_to_dfa($\varphi$)` is
  **deterministic + complete** on every case, with `SCOPED_TRACE(phi, index)`.
  **Green checkpoint:** `ctest` green with the structural TEST passing over all
  cases — needs **neither** a generated $\Tin$ **nor** `ltlfsynt`.
  Stubbed for later: no metamorphic body, no differential body.

- **Phase 2 — Random $\Tin$ + metamorphic round-trip.**
  Lands: the in-memory `random_tin(partition, rng, dict)` builder (the BDD-cube
  block in *Random $\Tin$ generation* — deterministic + total Case-A by
  construction; `trivial_transducer` when $\Iknown=\emptyset$); extends
  `GeneratedCase` to carry the built $\Tin$ (or the data to rebuild it) and
  extends `BuildGeneratedCorpus` to draw it (**this is where the accepted corpus
  shift occurs**). A second library body `TEST(GeneratedCorpus,
  MetamorphicRoundTrip)` runs `DfaProduct::synthesize($\varphi$, vars, t_in,
  trivial t_out)` and, when a `Controller` is returned, asserts
  `verify_controller(...).ok`; unrealizable cases check nothing further
  (one-directional, per *Edge cases*).
  **Green checkpoint:** `ctest` green with **both** library TESTs passing — still
  **no** `ltlfsynt` dependency.
  Stubbed for later: `RunSubprocess` unchanged; no differential body.

- **Phase 3 — Differential + timeout plumbing.**
  Lands: `kCorpusSubprocessTimeoutSecs`; the `RunSubprocess` **optional-timeout**
  extension (trailing `std::optional<unsigned> timeout_secs = std::nullopt` +
  `bool* timed_out = nullptr`; when set it prefixes `timeout <N>s` and maps exit
  `124`/`WIFSIGNALED` → `*timed_out`) — **existing callers untouched** by the
  default. The differential body `TEST_F(LtlfsyntOracleTest,
  GeneratedCorpusDifferential)` iterates the $\mathcal{V}=\emptyset$, width-$\le3$
  subset, asserts `ltlf-ek-synth` and `ltlfsynt --semantics=Mealy` agree on the
  verdict, and on timeout `continue`s + increments a skip counter
  (`RecordProperty`).
  **Green checkpoint:** with `ltlfsynt` present, the differential agrees on every
  non-timed-out case; with it absent, the body `GTEST_SKIP`s and P1+P2 stay green.
  Existing oracle/differential tests are unaffected by the `RunSubprocess` change.

## Edge cases
- **`ltlfsynt` absent** — the differential body `GTEST_SKIP`s (via
  `LtlfsyntOracleTest`); the **library body still runs** (this split is the whole
  reason for two bodies).
- **Empty $\Ofree$** — generated when $\lvert\mathcal{O}\rvert=0$; the controller
  controls nothing. Both oracles must handle it (the curated suite already does).
- **Empty $\Iknown$** — $\Tin$ is `trivial_transducer`, not the random table;
  metamorphic degenerates to empty-knowledge.
- **Empty $\Ifree$** (all inputs known) — random $\Tin$'s valuation loop runs once
  on the empty cube (constant knowledge); valid.
- **Unrealizable generated case** — `synthesize` returns `nullopt`; the metamorphic
  body checks **nothing further** (one-directional: no controller to verify). This
  is an accepted v1 limitation — the *differential* covers the
  wrongly-`unrealizable` direction, but only for $\mathcal{V}=\emptyset$. Record it
  in *Open theory questions*.
- **Subprocess timeout** — skip the case, do not fail (above).
- **Degenerate formula** (`randltl` yields `0` / `1` / a pure boolean) — legal;
  both oracles already handle `0`/`1` (Table E). No special-casing.
- **`ltlf_to_dfa` not complete/deterministic** — would be a real `ltlf_to_dfa`
  bug surfaced by the free-rider check; report, do not adjust the assertion.

## Test oracles (for /test-writer)
The corpus is **self-labeling** — there are **no hand-authored expected values** to
encode. The two graders *are* the labels:

1. **Metamorphic round-trip (library, always on).** For every generated case, if
   `DfaProduct::synthesize` returns a `Controller`, `verify_controller` on the same
   $(\varphi, \Tin, \Tout, T_C)$ must return `ok` (this is the standing
   "every `solve_dfa` controller verifies" invariant, `docs/prd/controller-verifier.md`,
   now exercised on generated $\varphi$ **and** generated $\Tin$).
2. **Differential (subprocess, gated, $\mathcal{V}=\emptyset$, width ≤3).**
   `ltlf-ek-synth --dfa-product --realizable --inputs … --outputs … --formula φ`
   vs `ltlfsynt --ins=… --outs=… --semantics=Mealy --realizability -f φ` must agree
   on the printed verdict word.
3. **`ltlf_to_dfa` structural (library, always on).** Determinism + completeness on
   every generated $\varphi$.

`/test-writer`'s job is the **mechanical** translation into GoogleTest: the seeded
generator, the two loop bodies, `SCOPED_TRACE` on `(phi, partition, index)`, the
timeout→skip plumbing, and the skip counter. **Do not** invent per-case expected
verdicts, **do not** auto-derive a $\psiin$ for generated $\Tin$ (there is none by
design), and **do not** run the tools by hand before writing the assertions — the
oracle establishes truth at runtime. A generated **mismatch** is
"investigate, don't adjust": it is a candidate `DfaProduct` bug, a Spot version
drift, or a real semantics divergence — flag to `/theory-review`, never silence by
editing the assertion or dropping the case.

## Open theory questions touched
- **One-directional metamorphic on generated known-knowledge.** For a
  known-input generated case the metamorphic oracle can only catch a
  *wrongly-realizable* controller (it verifies the controller `synthesize`
  returned); a *wrongly-unrealizable* verdict has no controller to check and no
  $\psiin$ to feed `ltlfsynt`. Closing it needs a $(\Tin,\psiin)$ pair denoting
  one language — **not** obtainable by inverting either arrow of a free-form case
  (transducer→$\psiin$ fails on non-star-free $\delta$; random-$\psiin$→transducer
  fails because a random formula is rarely a total functional strategy). The
  working route — **co-generating both from one bounded-memory source**
  (`const/copy/delay/window-boolean`), which is simultaneously a deterministic
  total transducer and a direct $\text{LTL}_f$ formula — is **deferred to v2**,
  spec'd shovel-ready in `docs/BACKLOG.md` ("Co-generated $(\Tin,\psiin)$ family →
  known-knowledge differential"). It stays complementary to v1's free-form table.
- **Trace-termination / Mealy semantics.** The generator's `X[!]` bias
  deliberately stresses the system-controlled-termination reachability reading
  shared by `solve_dfa` and `verify_controller` (glossary "Open theory questions →
  Trace-termination semantics"). This harness gathers **evidence** on that shared
  reading; it does **not** resolve it. A differential Mealy mismatch it surfaces is
  a `/theory-review` item, not an arena edit.
- **Method-2 arena input partition ($\Ifree$ vs full $\mathcal{I}$).** Deferred in
  `docs/prd/dfa-product.md`; the generated differential gives *broader* independent
  external evidence on whichever choice is live but does not resolve it.
- **No `\na`/stub in `main.tex` is modified.** `FP`/aggregation stubs are untouched
  (those methods are not wired in the CLI and not exercised here).

## Definition of done
- `tests/ltlfsynt_oracle_test.cpp` gains: `kCorpusSeed` / `kCorpusCaseCount` /
  `kCorpusSubprocessTimeoutSecs` constants, `BuildGeneratedCorpus()` and its
  generator helpers (`randltlgenerator` wrapper + `X[!]` strengthener, partition
  generator, in-memory random-$\Tin$ builder), the timeout plumbing, and the
  **three** new test bodies (structural + metamorphic library-always, plus the
  differential-gated) — landed across the three phases in *Implementation phases*.
- The library body runs (and is green) even where `ltlfsynt` is absent; the
  differential body `GTEST_SKIP`s cleanly there and, where present, agrees on every
  non-timed-out $\mathcal{V}=\emptyset$ width-≤3 case.
- Every generated $\varphi$ passes the `ltlf_to_dfa` determinism + completeness
  check; every realizable generated case's controller passes `verify_controller`.
- The corpus is reproducible **in the final landed tree**: the fixed seed makes
  any failure print a re-runnable `(phi, partition, index)`; no shrinking in v1.
  (Intermediate phase trees run on their own corpus — an accepted phased-rollout
  trade, see *Implementation phases*.)
- Generated APs are in-partition by construction (partition-first generation); no
  AP-scope guard needed for generated cases.
- `cmake --build build -j && ctest --test-dir build --output-on-failure` green (or
  failures reported with output, per `/test-writer`).
- Glossary: confirm with `/glossary` whether "generated corpus" / "metamorphic
  round-trip" warrant a prose note (expected: no C++ column entry).
- Deferred follow-ups logged in `docs/BACKLOG.md`: the co-generated
  $(\Tin,\psiin)$ family for a known-knowledge differential (v2, spec'd there);
  generated $\Tout$ / $\Oknown$; formula shrinking on failure.

## Developer comments / PRD disagreements

**2026-07-06 (Phase 1 landing).** No disagreements; two implementation
choices the PRD left to `/developer`'s call, recorded here for traceability:

- **`kCorpusIknownProbability = 0.5`** for `random_partition`'s per-input
  "mark Iknown" coin flip. The PRD pins the *shape* ("a random subset of the
  inputs is marked Iknown, may be empty") but not the exact probability; 0.5
  is a plain uniform-subset choice.
- **Operator-palette exclusions encoded as `randltlgenerator` priorities**
  `"xor=0,M=0"` to match the PRD's pinned LTLf-safe palette (`U / R / W / F
  / G / X / ! / & / | / -> / <->`), since Spot's default `random_ltl`
  priorities also include `xor` and `M` (strong release). `strongX` is left
  at its library default of 0 (unused) since `strengthen_next` injects
  `X[!]` as a separate post-processing pass, per the PRD.

Both are tunable constants/strings, not domain semantics, so neither
warranted a PRD amendment. `#define SPOT_USES_STRONG_X 1` is a no-op on the
installed Spot version (unconditionally >= 2.13, `strong_X` already public)
but was still added, verbatim as pinned, for portability.

**2026-07-06 (Phase 2 landing).** No disagreements; one implementation
choice the PRD left to `/developer`'s call ("exact shape is `/developer`'s
call"), recorded here for traceability:

- **`GeneratedCase` grew a `t_in` field holding the built
  `OutputLabeledTransducer` directly** (not a separate rng-spec struct), each
  built on its own private `spot::bdd_dict` at corpus-build time — mirrors
  the structural test's existing per-case-dict idiom, and lets the
  metamorphic body replay `synthesize`/`verify_controller` with zero rebuild
  logic (`t_out` is derived at test time via `trivial_transducer(partition,
  Role::t_out, c.t_in.dict())`, sharing that same dict as
  `DfaProduct::synthesize` requires).
- `random_tin` reuses the file's existing `all_letters_over` helper (already
  defined above, for the faithfulness guard) to enumerate $\Ifree$ letters,
  rather than duplicating an enumerator — a plain reuse, not a semantic
  choice.

**Green checkpoint reached:** `ctest` green, 188/188, including both
`GeneratedCorpus.LtlfToDfaStructural` and
`GeneratedCorpus.MetamorphicRoundTrip` (256 cases each), no `ltlfsynt`
dependency. Phase 3 (differential + timeout plumbing) not yet started.

**2026-07-06 (Phase 3 landing).** No disagreements; the PRD's pinned CLI
forms, timeout/skip semantics, and verdict contract were encoded as
specified. Two mechanical points worth recording:

- **`RunSubprocess` extension is two trailing defaulted parameters on the
  existing function** (`std::optional<unsigned> timeout_secs = std::nullopt,
  bool* timed_out = nullptr`), not a separate overload — this is what makes
  "existing callers untouched by the default" hold trivially (same function,
  same signature prefix, no call site edited). `RunEkSynth` and
  `LtlfsyntOracleTest::RunLtlfsynt` grew the identical two trailing defaulted
  parameters (forwarding to `RunSubprocess`) so the differential body could
  thread the timeout through the two thin per-binary wrappers rather than
  bypassing them to call `RunSubprocess` directly — the PRD's "Extend
  `RunSubprocess`" bullet implied this forwarding but didn't spell it out; a
  plain mechanical consequence, not a semantic choice.
- **`JoinCsv` helper** added for the differential's `--inputs`/`--outputs`
  (ek-synth) and `--ins=`/`--outs=` (ltlfsynt) arguments, joining a
  `VariablePartition` set with commas (`src/ltlf_ek_synth.cpp`'s `SplitCsv`
  counterpart). When `output_free` is empty the `--outputs`/`--outs=` flag is
  omitted entirely (mirrors the existing `EmptyOutputsAcceptedByBothTools*`
  tests) rather than passed as an empty string; `input_free` is never empty
  for the $\mathcal{V}=\emptyset$ subset (input_known empty + $|\mathcal{I}|
  \ge 1$ by `random_partition`'s draw range forces at least one free input),
  so no analogous omission is needed there.

No new domain identifier was introduced (`kCorpusSubprocessTimeoutSecs`, the
`RunSubprocess`/`RunEkSynth`/`RunLtlfsynt` timeout parameters, `JoinCsv`, and
`GeneratedCorpusDifferential` are all test-local plumbing, per this PRD's own
glossary note), so the glossary gate is left for `/glossary` to confirm per
the Definition of done, not ticked here.

**Green checkpoint reached:** `cmake --build build -j` clean;
`ctest --test-dir build` green, 189/189, including
`LtlfsyntOracleTest.GeneratedCorpusDifferential` (real `ltlfsynt` present at
build-configure time, `LTLFSYNT_EXECUTABLE` resolved) agreeing with
`ltlf-ek-synth` on every non-timed-out case in the generated
$\mathcal{V}=\emptyset$, width-$\le3$ subset; both Phase 1/2 library bodies
unaffected. All three phases of this PRD are now landed.
