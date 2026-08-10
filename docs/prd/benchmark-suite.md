# PRD: Parametric benchmark suite

**Status:** Phase 1 implemented — this commit (`SizeMetric`/`size_metric_name`/`record_size_metric`/`BenchSizeMetric`/`BenchReport::metrics` in `include/ltlf_ek/bench.hpp` + `src/bench.cpp`; the B2 charge table wired into `src/dfa_product.cpp`, `src/nfa_product.cpp`, `src/mtdfa_product.cpp`, `src/mtnfa_product.cpp`, `src/otf_mtdfa_product.cpp`), branch `worktree-agent-a4f183dc855ffa8b4`; **repair round** on branch `bench-phase1` added `bench_scope_active()` and guarded the four call sites that evaluated an expensive argument before `record_size_metric`'s own no-op check (see *Developer comments* below). Phases 2–3 not started.
**Interface:** extends the existing benchmarking infrastructure (`docs/prd/benchmarking.md`) with a **metric sink** in `include/ltlf_ek/bench.hpp` (`SizeMetric` / `size_metric_name` / `record_size_metric`), a **benchmark registry** (`BenchFamily` / `BenchSubject` / `BenchCase`, `include/ltlf_ek/bench_suite.hpp`), and a new `ltlf-ek-bench` binary. **Does not change the `Synthesis` contract.**
**Recommended workflow:** **concurrent for Phase 1** (the metric sink mirrors the existing `Stage` registry one-for-one — high freeze confidence), **sequential for Phases 2–3** (the registry and the runner are genuinely being invented here; the test-writer should bind to the real signatures).
**main.tex ref:** none — this is infrastructure, like `docs/prd/benchmarking.md` and `docs/prd/cli-wrapper.md`. The measured quantities trace to the algorithm blocks they size: `alg:dfa_product` (§`fulldfa`), `alg:nfa_product` (§`nfa`), `alg:otfdfa_product` (§`otf`). The **comparability tier** rests on the LTLf $\equiv$ star-free correspondence, which `main.tex` does **not** currently state — see *Open theory questions touched*.

**Gates:**
- [x] glossary        — *Canonical size metric* (`SizeMetric`) and *Comparability tier* (`ComparabilityTier`) added 2026-08-09, pre-`/developer`; the shipped *Canonical benchmarking stage* entry's "Do not call it" line was amended to point at the new size axis
- [ ] tests           — unit + oracle coverage
- [ ] code-review     — domain (/code-reviewer) + generic (/code-review)
- [ ] theory-review   — code ↔ math faithfulness vs main.tex

**Unattended-ready:** **yes** (2026-08-09). Both domain terms this PRD introduces are now in `docs/GLOSSARY.md` — *Canonical size metric* (`SizeMetric`) and *Comparability tier* (`ComparabilityTier`); interfaces are frozen below; each phase has a machine-checkable checkpoint; and the one open theory question is on the Stop-list rather than in the implementation path. All three phases are launchable.

## Stop-list

An unattended run must **stop and record**, never guess, on any of these.

1. **The aperiodicity claim behind tier T3.** The parity-toggle $\Tin$ is asserted non-aperiodic (hence no $\psi$ exists at any size) on the standard LTLf $\equiv$ star-free correspondence, which is **not stated in `main.tex` and not otherwise verified** (`docs/handoffs/2026-08-08-benchmark-suite.md` S4). The family ships with the tier **declared as data**; a run must not attempt to prove, disprove, or "check" the claim, and must not promote a T3 row into a comparison table.
2. **A family whose structural numbers do not discriminate.** If `mirror-small`'s Goal NFA is not $O(n)$, or `cons-prunes`'s product is not $O(n)$ while its Goal is $2^n$, the family has degenerated exactly as `docs/prd/mtnfa-product.md:620` warns. Record the measured counts and stop — **choosing a replacement $\varphi_n$ is a user decision**, not a repair.
3. **Any need to change the `Synthesis` interface** to record a metric. The metric sink is deliberately out-of-band for this reason; if a method's count is unreachable from inside `synthesize`, record the hole and stop.
4. **A verdict disagreement in the Phase 2 `ltlfsynt` race.** The T1 race compares wall-clock, but it also incidentally compares verdicts against the monolithic reduction — a conjecture with a **known divergence witness** as of 2026-08-09 (O5, `docs/runs/2026-08-09-acceptance-mark-edgeless.md`). A disagreement is an O5-class theory finding, not a benchmark bug.
5. **A historical result that fails to reproduce** (Phase 2). Record the measured ratio beside the historical one and stop; do not tune the family, the timeout, or the repetition count to recover the old number.
   - **Standing exception for the 2026-08-11 run only** (the sweep feeding the 2026-08-12 presentation): **record and continue.** Finish the sweep, write the workbook, and flag the non-reproduction at the top of the run report. The protection this item exists for is intact — continuing is not tuning, and no family, timeout or repetition count may be adjusted to chase the old number. What the exception refuses is trading an entire deliverable for a caveat that would have been read the same evening anyway.

## Goal

Give the project **one committed, reproducible benchmark suite**, so that a claim about a method's cost can be re-run instead of quoted. The item exists because the flagship empirical result — `OtfMtdfaProduct`'s **5488x** win over `MtdfaProduct` — survives only as a transcribed markdown table: `docs/prd/otf-mtdfa-product.md:702` says in as many words that its harness and raw JSON are *"throwaway (not committed)"*, and the same is true of `MtnfaProduct`'s **16x-slower** negative result (`docs/prd/mtnfa-product.md:17`), which came from a different ad-hoc probe. Evening ranking decisions are already being made on those vanished numbers.

This PRD **extends and does not supersede** `docs/prd/benchmarking.md` (implemented 2026-07-13), which deliberately shipped observability only — a span collector plus the canonical `Stage` registry — and recorded that its container was designed to also hold size metrics, deferred to the backlog. This is that deferral, plus everything above it: families, a runner, a committed results store, and a cross-method table. Full grill state and the evidence trail: `docs/handoffs/2026-08-08-benchmark-suite.md`.

## Ubiquitous-language terms used

Existing, used as spelled in `docs/GLOSSARY.md`:

- **The five methods** — `DfaProduct`, `NfaProduct`, `MtdfaProduct`, `MtnfaProduct`, `OtfMtdfaProduct`. All five participate, **losers included** — a progression table that omits them is not a progression.
- **Canonical benchmarking stage** — `Stage` / `stage_name`, `include/ltlf_ek/bench.hpp`.
- **Goal automaton**, **Product**, **Controller**, **Knowledge transducer** ($\Tin$, $\Tout$), **Variable partition** ($\Ifree$, $\Iknown$, $\Ofree$, $\Oknown$), **Consistency filter** ($\cons$).
- **Generated corpus** — referenced only to keep `random_tin` **out of scope**.

**Added to `docs/GLOSSARY.md` 2026-08-09 by `/glossary`, before `/developer`:**

- **Canonical size metric** (`SizeMetric` / `size_metric_name` / `record_size_metric`) — the size analogue of *Canonical benchmarking stage*: a closed registry of comparable **size** axes, carrying the same comparability caveat (two methods' same-named metric is comparable only when they charge the **same structure** to it).
- **Comparability tier** (`ComparabilityTier`, values `t1`/`t2`/`t3`) — the declared expressibility class of a family's $\Tin$, governing whether an external `ltlfsynt` comparison is a legitimate claim for that family.

Following the `bench.hpp` and `cli.hpp` precedent, the surrounding plumbing (`BenchFamily`, `BenchSubject`, `BenchCase`, `BenchParams`, the runner, the baseline reader/writer) is **infrastructure, not a domain concept**, and gets **no** glossary entry.

## Behaviour / semantics

### B1. Two output layers, different lifecycles

- **Layer 1 — structural metrics: deterministic, committed, asserted in `ctest`.** State counts are machine-independent and reproducible to the integer, which makes the family definitions self-validating and turns the suite into a **regression test**: a refactor that silently doubles a product size fails the suite. Timings can never do that.
- **Layer 2 — timings: nondeterministic, generated, snapshotted.** Written to a gitignored output directory; one report committed per sweep under `docs/runs/`, carrying provenance.

The structural layer is **not a supplement — it is the validity check on the family.** The repo learned this twice: *"State counts (the decisive number; timing alone cannot separate 'slower' from 'builds more')"* and *"Anyone re-running this must check the NFA size first — the intuitive family silently degenerates"* (`docs/prd/mtnfa-product.md:624`, `:620`), and Method 3.1 then ran the check first by design (`docs/prd/otf-mtdfa-product.md:726`).

### B2. The metric registry, and what each method charges

`SizeMetric` is a **closed canonical registry**, exactly like `Stage`: adding a value is deliberate (enum value + name row + glossary line), and a non-canonical quantity gets a free-form label instead. Recording is out-of-band — a method calls `record_size_metric` in the same place it opens a `BenchTimer`, and it is a near-zero-cost no-op when no `BenchScope` is active, so a run's verdict and controller stay byte-identical whether measurement is on or off.

| `SizeMetric` | Meaning | Unit |
|---|---|---|
| `goal_dfa_states` | states of the deterministic Goal automaton for $\varphi$ | states |
| `goal_nfa_states` | states of the nondeterministic Goal automaton for $\varphi$ | states |
| `nfa_product_states` | states of the **pre-determinization** product | states |
| `product_states` | states of the structure **handed to the game solver** | states |
| `product_bdd_nodes` | internal decision nodes of that structure | BDD nodes |
| `controller_states` | states of the returned `Controller`'s strategy | states |

**The charge table is the contract** — the number a method reports must come from the structure it actually built:

| Method | `goal_dfa_states` | `goal_nfa_states` | `nfa_product_states` | `product_states` | `product_bdd_nodes` | `controller_states` |
|---|---|---|---|---|---|---|
| `DfaProduct` | `goal->num_states()` | — | — | materialized product `num_states()` | — | ✓ |
| `NfaProduct` | — | Goal NFA `num_states()` | explicit NFA product `num_states()` | determinized product `num_states()` | — | ✓ |
| `MtdfaProduct` | `mtdfa->num_roots()` | — | — | product mtdfa `num_roots()` | `get_stats(nodes)` | ✓ |
| `MtnfaProduct` | — | `Mtnfa::states.size()` | — (product and determinization are fused) | product mtdfa `num_roots()` | `get_stats(nodes)` | ✓ |
| `OtfMtdfaProduct` | — (builds no Goal automaton) | — | — | explored product mtdfa `num_roots()` | `get_stats(nodes)` | ✓ |

Two rules make the holes safe:

1. **An absent metric is absent, never zero.** The baseline schema distinguishes them, and a method that starts emitting a metric it previously omitted is a baseline diff, not a silent change.
2. **`product_states` is defined by *role*, not by representation** — "the structure handed to the game solver" — which is why the mtdfa cells share the column with the explicit cells. `spot::mtdfa::num_roots()` counts states (the `states` array), the same unit as `twa_graph::num_states()`; this is already how `docs/prd/otf-mtdfa-product.md:718`'s table compared them. Where the units genuinely differ — decision-diagram size — there is a **separate** value, `product_bdd_nodes`, which no explicit cell emits.

### B3. Tiers are declared, never sniffed

A family declares its `ComparabilityTier`. Nothing inspects a transducer at run time to decide one.

| Tier | Condition | `ltlfsynt` cell |
|---|---|---|
| **T1** | $\psi$ exists and is $O(\lvert\tau\rvert)$ | Honest wall-clock race. The only tier where an external comparison is a legitimate claim. |
| **T2** | $\tau$ aperiodic so $\psi$ exists, but DFA→LTLf is non-elementary in general | Reportable **only** with $\lvert\psi\rvert$ as a column; never merged into the T1 table — racing here measures the *encoding*, not the method. |
| **T3** | $\tau$ non-aperiodic; no $\psi$ exists at any size | **n/a by expressibility** — a capability separation, not a timing result. |

If T2 could silently drift into the T1 table, the headline external comparison would become dishonest by accident. A T1 family therefore also **declares its $\psiin$ string** as data (the four hand-authored encodings in `tests/ltlfsynt_oracle_test.cpp:293-371` are the precedent); nothing derives $\psi$ from a transducer.

### B4. The families

Five families, all with a single sweep axis $n$ and a `realizable` flag (both prior harnesses measured realizable **and** unrealizable instances). All $\varphi_n$ below are the ones the repo has already measured — none are invented here.

| Family | $\varphi_n$ | Knowledge | Tier | What it is for |
|---|---|---|---|---|
| `cons-prunes` | $F(k \wedge X[!]^n k)$ | $k \in \Iknown$; one-state $\Tin$ whose $\lambda$ pins $k$ true every step | T1 | 3.1's Family A — Goal is $2^n$, product is $n+1$. The 5488x instance. |
| `cons-inert` | $F(k \wedge X[!]^n k)$ | $k \in \Ofree$; trivial transducers | T1 | 3.1's Family B — the control, where $\cons$ prunes nothing. A *functional* $\lambda$ over a non-empty $\Sigma_1$ always prunes something, which is why B moves $k$ rather than weakening $\lambda$. |
| `mirror-small` | $F(v \wedge X[!]^n(\neg X[!]\,\mathbf{1}))$ | trivial | T1 | `MtnfaProduct`'s corrected family — reverse language is deterministic in $O(n)$, so the Goal NFA is $n{+}3$ while the mtdfa is $2^n$. |
| `mirror-degenerate` | $F(v \wedge X[!]^n v)$ | trivial | T1 | **The documented trap.** `ltlf_to_nfa` is mirror-based, so this family's NFA is ~1027 states at $n=10$ — the same order as its DFA — and it never tested the hypothesis. Committed so nobody re-derives it. |
| `parity-t3` | $F(k \wedge X[!]^n k)$ | $k \in \Iknown$; the hand-built 2-state parity $\Tin$ (below) | **T3** | The capability separation: same $\varphi_n$, same game, swap the transducer and the competing tool leaves the table. |

`cons-prunes` / `cons-inert` and `mirror-small` / `mirror-degenerate` are **matched pairs** — same $\varphi_n$, one thing varied — which is the pattern that made the 3.1 result legible rather than anecdotal. `parity-t3` is matched to `cons-prunes` on $\varphi_n$ and differs only in $\Tin$.

**The T3 witness, fixed and hand-built.** Two states; $\delta$ toggles on input $a$ and stays put otherwise; $\lambda$ pins $k$ true in the even state and false in the odd one — *"$k$ holds iff an even number of $a$'s have occurred so far"*. Parity of a count is the canonical non-star-free property. A near-miss deliberately avoided: *"$k$ simply alternates T,F,T,F"* is star-free (`k & G(k -> X !k) & G(!k -> X k)`) and is **not** a witness — the toggle must be **input-triggered**. `random_tin` is **not** touched; it feeds the generated-corpus oracle, whose totality and determinism assumptions are that PRD's scope.

### B5. Determinism

Nothing in the suite draws random numbers: every $\varphi_n$ is a formula template, every transducer is hand-built, every sweep is a fixed range. This is what makes exact-integer assertions legitimate, and it is a property `/test-writer` should pin.

## Interfaces & types

**Freeze confidence: high for Phase 1, tentative for Phases 2–3.** Phase 1 mirrors the shipped `Stage` registry one-for-one, so the signatures fall straight out of the existing header. Phases 2–3 invent the registry and the runner; the shapes below are the contract to build against, but implementation may sharpen them.

### Phase 1 — the metric sink (`include/ltlf_ek/bench.hpp`)

```cpp
// Canonical comparable size axes --- the same soft registry as Stage.
enum class SizeMetric {
  goal_dfa_states,
  goal_nfa_states,
  nfa_product_states,
  product_states,
  product_bdd_nodes,
  controller_states,
};

std::string_view size_metric_name(SizeMetric m);

// Record one measurement into the active BenchScope's collector.
// No-op (near-zero cost) if no BenchScope is active --- the BenchTimer rule.
void record_size_metric(SizeMetric m, std::uint64_t value);
void record_size_metric(std::string label, std::uint64_t value);  // free-form tier

// True iff a BenchScope is active on this thread --- lets a call site guard
// an expensive-to-compute argument (e.g. a BDD-node traversal) before
// evaluating it, since record_size_metric's own no-op check only runs after
// its argument is already evaluated by the caller.
bool bench_scope_active();

// One recorded measurement.
struct BenchSizeMetric {
  std::string label;      // size_metric_name(SizeMetric) or a free string
  bool canonical;         // true => label is a registry key
  std::uint64_t value;
};
```

`BenchReport` gains one member, `std::vector<BenchSizeMetric> metrics;` — **flat, not nested**: a metric belongs to the run, not to a span. `BenchReport::to_json` always emits a `"metrics"` array, empty when nothing was recorded; that is an additive but **visible** schema change, so `tests/bench_test.cpp`'s JSON-schema case is a **knowingly-changed test**.

### Phase 2 — the registry (`include/ltlf_ek/bench_suite.hpp`)

```cpp
enum class ComparabilityTier { t1, t2, t3 };
std::string_view comparability_tier_name(ComparabilityTier t);

// Ordered so a row key is stable across runs.
using BenchParams = std::map<std::string, std::int64_t>;

struct BenchCase {
  std::string family;
  BenchParams params;                  // e.g. {{"n", 6}, {"realizable", 1}}
  spot::formula phi;
  VariablePartition vars;
  Transducer t_in;
  Transducer t_out;
  ComparabilityTier tier;              // DECLARED by the family (B3)
  std::optional<std::string> psi_in;   // required iff tier == ComparabilityTier::t1
  bool expected_realizable;
};

class BenchFamily {
 public:
  virtual ~BenchFamily() = default;
  virtual std::string name() const = 0;
  virtual std::vector<BenchParams> sweep(std::int64_t n_min,
                                         std::int64_t n_max) const = 0;
  virtual BenchCase instantiate(const BenchParams& params) const = 0;
};

// What is being measured. "Run the five synthesis methods" is one
// implementation; a future "time ltlf-ek-deps" or "time LtlfToDfa alone"
// is another, added without touching the runner.
class BenchSubject {
 public:
  virtual ~BenchSubject() = default;
  virtual std::string name() const = 0;        // e.g. "dfa-product"
  virtual void run(const BenchCase& c) const = 0;  // called under a live BenchScope
};

const std::vector<std::unique_ptr<BenchFamily>>& bench_families();
const std::vector<std::unique_ptr<BenchSubject>>& bench_subjects();

// One measured row. Timings and metrics share the key shape.
struct BenchRow {
  std::string family;
  BenchParams params;
  std::string subject;
  std::string key;          // size_metric_name(...) or stage_name(...)
  std::uint64_t value;      // count, or nanoseconds
};

// Run one case under one subject and harvest its report into rows.
std::vector<BenchRow> run_bench_case(const BenchCase& c, const BenchSubject& s);
```

The **row key is generic** — `(family, params, subject, key)` — deliberately, so a differently-shaped benchmark with different metrics can be added without a schema migration of the committed baseline. Fixed columns are the thing that would block it.

**Baseline store** (built in **Phase 3**, specified here because it keys off the registry above): `tests/fixtures/bench_structural_baseline.tsv`, one row per `(family, params, subject, metric, value)`, sorted deterministically by that key so a diff is readable. Absent means absent (B2 rule 1) — a missing row is not a zero.

**Regenerate path** (Phase 3, no binary needed):

```sh
LTLF_EK_BENCH_REGEN=1 ctest --test-dir build -R BenchStructural   # rewrites the .tsv
git diff tests/fixtures/bench_structural_baseline.tsv             # review every changed cell
```

The env-var switch follows the `LTLF_EK_SOAK` precedent. Without it, the test **asserts** every cell exactly.

### Phase 2 (cont.) — the timing runner (`ltlf-ek-bench`)

```
ltlf-ek-bench --families=<csv|all> --subjects=<csv|all> --n-min=N --n-max=N
              --repeat=K --timeout=SECONDS --out=FILE [--ltlfsynt=PATH]
```

Reports the **minimum** of `K` repetitions (both prior harnesses did; `--repeat=3` default, `--timeout=20` default). Writes JSON to a gitignored directory **inside the repository** — `build/benchout/` is the default, and `--out` must resolve there or under `docs/runs/`. **Never `/tmp`, never `$TMPDIR`:** verified 2026-08-09, the sandbox mounts `/tmp` (and the background-job tmp dir) **read-only**, so a run that measures for two hours and then writes to `/tmp` loses everything at the last step. Writes inside the repo succeed. The committed snapshot under `docs/runs/` carries provenance: machine, **`ldd`-resolved Spot version** (not `pkg-config` — several installs shadow each other via `LD_LIBRARY_PATH`), repo commit, timeout, repetition count, and the structural numbers observed during the sweep (unasserted at these larger $n$).

**If implementation proves this contract wrong:** that is a PRD-change event — update this section and propagate to any in-flight test branch; the developer does not silently re-shape the interface on its own branch.

## Implementation phases

- **Phase 1 — metric sink + instrumentation.** `SizeMetric`, `size_metric_name`, `record_size_metric`, `BenchSizeMetric`, the `BenchReport::metrics` member and its JSON emission; then the charge table of B2 wired into all five methods at the sites that already open a `BenchTimer`. **Green checkpoint:** suite green with `tests/bench_test.cpp`'s schema case knowingly updated, plus new cases asserting that each method emits exactly its charge-table row set, and a **zero-perturbation** case (verdict and controller identical with and without an active `BenchScope`) mirroring the one `docs/prd/benchmarking.md` already ships. No families exist yet. *This is the only phase that edits the five method `.cpp` files.*

- **Phase 2 — registry, families, timing sweep, workbook, `ltlfsynt` race.** `bench_suite.hpp` per above; the five families of B4; the five-method `BenchSubject`; the `ltlf-ek-bench` binary with repetition / timeout / provenance; the gitignored output dir and the `docs/runs/` snapshot; the **T1 race** against `ltlfsynt`; the cross-method table; and the **spreadsheet export** (B6). **Green checkpoint:** the binary builds and runs a bounded sweep end to end; a snapshot lands in `docs/runs/` with full provenance; `docs/runs/<date>-benchmarks.xlsx` exists and opens; the re-derived 5488x and 16x-slower ratios are reported **beside** their historical values with an explicit delta. No test asserts a timing ratio.

- **Phase 3 — committed structural baseline + `ctest` assertions.** The baseline `.tsv`; the `BenchStructural` ctest case asserting every cell exactly; the regenerate path. Sweep $n = 2..8$, sized to add **≤ ~30 s** to `ctest`. **That budget is an estimate, not a measurement** — 5 families × 2 polarities × 7 values of $n$ × 5 methods is ~350 runs, two of whose methods spawn `mona` per run. Measure it; if it busts the budget, **lower `n_max` and regenerate the baseline** (the assertion's sensitivity does not depend on reaching large $n$ — B1). Do not instead move the case behind an opt-in label: a label the day-run does not execute protects nothing. **Green checkpoint:** `BenchStructural` passes against the committed baseline; the regenerate path reproduces it byte-for-byte; the discrimination assertions of *Test oracles* O2 pass.

**Phases 2 and 3 were swapped on 2026-08-09** against a Wednesday 2026-08-12 progress presentation. The original order put the committed baseline before the sweep; the baseline is **regression protection with no presentation value**, while the sweep is the deliverable, and only the sweep is on the critical path. Nothing else moved: Phase 3 still consumes only what Phase 2 defines, so the dependency direction is unchanged and each phase still lands independently.

Each phase leaves the tree compiling and independently testable.

### B6. Spreadsheet export (Phase 2)

`ltlf-ek-bench --xlsx=FILE` writes a workbook via a **Python helper invoked by the binary** (or a standalone script the runner shells out to) using **`openpyxl`** — installed and **verified 2026-08-09**: version 3.1.5 at `~/.local/lib/python3.10/site-packages`, importable from the sandboxed `/usr/bin/python3`, and confirmed to round-trip a 3-sheet workbook through `pandas.ExcelWriter(engine="openpyxl")` preserving numeric cell types and non-ASCII text. `pandas` alone cannot write `.xlsx`. If `openpyxl` is **absent**, the runner must **fall back to CSV and say so loudly in its exit message** rather than failing the sweep: the measurements are the expensive part and must never be lost to a formatting dependency.

Workbook shape — one sheet per concern, because a single flat table is what made the previous results unreadable:

| Sheet | Contents |
|---|---|
| `summary` | one row per (family, method): best time at the largest $n$ that completed, the speedup vs `MtdfaProduct`, and the declared *Comparability tier*. |
| `timings` | the full sweep: family, $n$, realizable, method, per-*stage* nanoseconds and the wall total; timeouts marked as such, never blank. |
| `structural` | the *Canonical size metric* rows for the same cases — the honesty column, so a reader can tell "slower" from "builds more". |
| `ltlfsynt` | the T1 race only. T2/T3 rows appear with an explicit `n/a — by expressibility` string, **never** an empty cell that reads as a missing measurement. |
| `provenance` | machine, `ldd`-resolved Spot version, repo commit, timeout, repetition count, date. |

**Sweep budget.** Bound the whole run to a stated wall-clock ceiling (default **2 h**) and per-family $n$ ranges rather than one global range — `cons-inert` at $n = 20$ costs seconds per run per method while `cons-prunes` is flat. On exceeding the ceiling the runner **stops cleanly and writes what it has**; a partial workbook beats no workbook.

**`ltlfsynt` invocation.** `ltlfsynt` is a **shell alias** on this machine (`~/opt/spot-2.15.1/bin/ltlfsynt`), so it is **not on a spawned subprocess's `PATH`**. Always pass `--ltlfsynt=<absolute path>`; if the flag is absent and the name does not resolve, skip the race and record it as skipped — never report an unraced tier as a loss.

## Edge cases

- **MONA absent.** `NfaProduct` and `MtnfaProduct` shell out to mona. Their subjects **skip** (the existing `MONA_FOUND` gate is the precedent), and the baseline assertion must treat their rows as *skipped*, never as failures or zeros — otherwise the suite goes red on a machine without mona.
- **`OtfMtdfaProduct` has no Goal automaton.** It emits no `goal_*` metric at all; the table shows a hole. It also has no `automaton_construction` span, which is why `docs/prd/otf-mtdfa-product.md` reports `automaton_construction + product_construction` **summed** — the Phase 2 table must do the same or it flatters `MtdfaProduct` by exactly the `spot::ltlf_to_mtdfa` cost under test.
- **$n = 0$ and $n = 1$.** $X[!]^0$ degenerates to $\varphi$ itself; the sweep starts at $n = 2$, and a family must reject a parameter below its own floor rather than emit a malformed formula.
- **Unrealizable instances.** Every family instantiates both polarities; `synthesize` returning `nullopt` is a normal outcome, and `controller_states` is then **absent**, not zero.
- **Timeout (Phase 2).** A case exceeding `--timeout` records a timeout marker with the elapsed bound; it is not an error, and it must not abort the remaining sweep. Structural rows already gathered for that case stay valid.
- **Empty $\Sigma_1$.** `cons-inert` deliberately relies on the fact that a *functional* $\lambda$ over a non-empty $\Sigma_1$ always prunes something; a family must not "simplify" itself into an empty $\Sigma_1$.
- **Partial transducers are out of scope.** Every family's $\Tin$/$\Tout$ is total. Given the O5 witness (a partial $\Tin$ changes the verdict, `docs/runs/2026-08-09-acceptance-mark-edgeless.md`), a partial-transducer family would entangle measurement with an open theory question.
- **Nested `BenchScope`.** Already forbidden (asserts) by the shipped infrastructure; the runner must open exactly one per case.

## Test oracles (for /test-writer)

1. **Size-metric emission oracle (Phase 1).** For each of the five methods, the set of canonical metrics emitted under a `BenchScope` equals **exactly** its row in the B2 charge table — no extras, no omissions. This is what makes a silently-dropped metric visible.
2. **Discrimination oracle (Phase 2, asserted in Phase 3) — the family validity check.** Assert the structural *shape* each family claims, not just its committed integers: `cons-prunes` has `goal_dfa_states` exponential in $n$ while `product_states` is linear; `cons-inert` has `product_states` ≥ `goal_dfa_states`; `mirror-small` has `goal_nfa_states` linear in $n$ while the mtdfa route is exponential; `mirror-degenerate` has `goal_nfa_states` of the **same order as the DFA** — its degeneracy is asserted deliberately, since that is the property being preserved as a warning.
3. **Exact-baseline oracle (Phase 3).** Every committed cell matches, and the regenerate path is idempotent (regenerating an unchanged tree produces a byte-identical file).
4. **Zero-perturbation oracle (Phase 1).** A run's verdict and controller are identical with and without an active `BenchScope` — the invariant `docs/prd/benchmarking.md` already established for spans, extended to metrics.
5. **Cross-method agreement on the families (Phase 2).** All five methods agree on realizability for every case, and each agrees with the family's declared `expected_realizable`. This reuses the existing cross-method oracle machinery and turns the benchmark corpus into an additional differential corpus for free.
6. **Determinism oracle (Phase 2).** Running the same case twice in one process yields identical structural rows — cheap, and it pins B5.
7. **Tier-declaration oracle (Phase 2).** Every `ComparabilityTier::t1` family carries a non-empty `psi_in`; no other tier does. This is the mechanical form of "the tier is declared, never sniffed".

## Open theory questions touched

- **The T3 aperiodicity claim.** That the parity-toggle $\Tin$ admits **no** $\psiin$ at any size rests on LTLf $\equiv$ FO[$<$] $\equiv$ star-free, a correspondence `main.tex` does not state and which has not been checked against a source here. It is a paper-level claim; the suite only needs the tier as *data*. Stop-list item 1. A `\cl` note stating the correspondence would be the natural home for it — for `/theory-review`, not for a day-run.
- **The monolithic reduction.** The Phase 2 `ltlfsynt` race hands the tool $\psiin \rightarrow (\varphi \wedge \psiout)$, which is a **conjecture, not a theorem**, and as of 2026-08-09 has a divergence witness on partial transducers (O5). All families here are total, where no divergence is known — but the paper's external comparison carries the caveat until it is resolved. Stop-list item 4.
- **No `\na` in `main.tex` is touched.** This PRD adds no math.

## Definition of done

- All three phases landed; the tree compiles and `ctest` is green.
- `docs/GLOSSARY.md` carries *Canonical benchmark metric* and *Comparability tier* with their C++ column filled.
- `tests/fixtures/bench_structural_baseline.tsv` is committed, asserted cell-exact by `BenchStructural`, and regenerable by the documented one-line command.
- All five methods emit exactly their charge-table metrics; the holes (`OtfMtdfaProduct` goal, `NfaProduct`/`MtnfaProduct` under absent mona) are explicit rather than zero.
- One timing snapshot is committed under `docs/runs/` with full provenance, reporting the re-derived 5488x and 16x-slower ratios beside their historical values.
- A spreadsheet (`docs/runs/<date>-benchmarks.xlsx`, five sheets per B6) is produced and opens; if `openpyxl` was unavailable the runner fell back to CSV and said so, rather than losing the sweep.
- `docs/prd/otf-mtdfa-product.md` and `docs/prd/mtnfa-product.md` each gain a one-line pointer from their "harness not committed" note to the suite that now owns those numbers.

## Developer comments / PRD disagreements

- **2026-08-10 — `bench_scope_active()` added to the Phase 1 interface.** B2 and `bench.hpp:57` both promise `record_size_metric` is a "near-zero cost no-op if no `BenchScope` is active", but that promise was never reachable from a call site: `record_size_metric(m, expr)` evaluates `expr` (a normal function argument) **before** the function's own `g_active_collector == nullptr` check runs, so an expensive `expr` pays its full cost regardless of whether a scope is active. At `product_bdd_nodes`'s three call sites (`src/{mtdfa,mtnfa,otf_mtdfa}_product.cpp`), `expr` is `product->get_stats(/*nodes=*/true, .../*paths=*/false).nodes` — a full BDD-node traversal, linear in the largest structure in the run, on the hot path of every production `synthesize()` call of all three mtdfa-family methods. Added `bool bench_scope_active()` (declared in `bench.hpp`, defined in `bench.cpp` as `g_active_collector != nullptr`) so a call site can guard the expensive argument itself; guarded the three `get_stats()` sites and the smaller `BenchTimer(std::string("determinize"))` construction in `src/nfa_product.cpp`. This is additive to the frozen Phase 1 block, not a re-shaping: `record_size_metric`'s own signatures and no-op behaviour are unchanged, the new predicate is infrastructure (no `docs/GLOSSARY.md` entry, same as the rest of `bench.hpp`'s plumbing).
- The four gates in the header are ticked by the skills that perform them.
