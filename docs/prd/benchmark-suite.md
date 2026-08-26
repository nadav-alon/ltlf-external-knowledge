# PRD: Parametric benchmark suite

**Status:** **Phase 1 complete** (glossary / tests / code-review closed; theory-review N/A for this phase — see the gates). Implemented (`SizeMetric`/`size_metric_name`/`record_size_metric`/`BenchSizeMetric`/`BenchReport::metrics` in `include/ltlf_ek/bench.hpp` + `src/bench.cpp`; the B2 charge table wired into `src/dfa_product.cpp`, `src/nfa_product.cpp`, `src/mtdfa_product.cpp`, `src/mtnfa_product.cpp`, `src/otf_mtdfa_product.cpp`), branch `worktree-agent-a4f183dc855ffa8b4`; **repair round** on branch `bench-phase1` added `bench_scope_active()` and guarded the four call sites that evaluated an expensive argument before `record_size_metric`'s own no-op check (see *Developer comments* below). **Phase 2, part A (the registry only) implemented** on branch `worktree-bench-phase2`: `include/ltlf_ek/bench_suite.hpp` + `src/bench_suite.cpp` — `ComparabilityTier`/`comparability_tier_name`, `BenchParams`/`BenchCase`/`BenchFamily`/`BenchSubject`, the five B4 families (`cons-prunes`, `cons-inert`, `mirror-small`, `mirror-degenerate`, `parity-t3`), the five-method `BenchSubject`s (`dfa-product`, `nfa-product`, `mtdfa-product`, `mtnfa-product`, `otf-mtdfa-product`), `bench_families()`/`bench_subjects()`, `run_bench_case`; wired into the `ltlf_ek` library target in `CMakeLists.txt`. **Not built in this pass** (a second pass, per the launching instruction): the `ltlf-ek-bench` binary, the xlsx export (B6), and the `ltlfsynt` T1 race. See *Developer comments* below for the interface sharpening (`BenchCase::t_in`/`t_out` are the concrete `OutputLabeledTransducer`, not the abstract `Transducer`) and the runtime (not `MONA_FOUND`-compile-time) mona gate this library-target file needed.
**Phase 2, part B implemented 2026-08-11** on this same worktree branch (`worktree-bench-phase2`, uncommitted, per the launching instruction — the launcher owns the commit): `src/ltlf_ek_bench.cpp` → the `ltlf-ek-bench` binary (CLI exactly per "Phase 2 (cont.)" plus a `--budget=SECONDS` flag this developer named, default 7200 = 2h, for the "stated wall-clock ceiling... overridable by a flag" B6 asks for without pinning a flag spelling); `scripts/bench_xlsx_export.py`, the Python/openpyxl helper the binary shells out to for B6's 5-sheet workbook (with a stdlib-`csv` fallback, loud on stderr and exit code 2, when `openpyxl` is not importable); the `ltlfsynt` T1 race; wired into `CMakeLists.txt` (`add_executable(ltlf-ek-bench ...)`, plus the compile-time `LTLF_EK_REPO_ROOT` / `LTLF_EK_BENCH_XLSX_SCRIPT` / `LTLF_EK_BUILD_TYPE` / `LTLFSYNT_BINARY` definitions it needs). Verified end-to-end with a bounded smoke sweep (`--families=cons-prunes,parity-t3 --subjects=dfa-product,mtdfa-product --n-min=2 --n-max=3 --repeat=1`): JSON lands with all 5 logical sections, the xlsx opens with the 5 named sheets and correct headers/values, the T1 race agrees with `ltlfsynt` on every row (`verdict_mismatch_count: 0`), the T3 family correctly shows `"n/a -- by expressibility"` with no `ltlfsynt` contact attempted, `--budget=0` stops the sweep immediately and still writes a valid (empty-but-well-formed) report, and `--out=/tmp/...` / `--out=$TMPDIR/...` / an outside-repo `--out` are all rejected with a usage error (exit 2) before any measurement work starts. See *Developer comments* below for five further sharpenings (the per-case timeout mechanism, the `psi_in -> phi` reduction dropping `psi_out`, the `expected_realizable`-as-ek-verdict choice, the summary sheet's realizable-only polarity, and the `automaton_construction + product_construction` "construction_ns" column).
**Phase 2 landed on `master` 2026-08-18** (backlog `#1`, easy half). `worktree-bench-phase2` was merged as `f83177f`, and the two `knowledge-chain` families were cherry-picked from `9929078` as `c542802` — code and tests only; that commit's `docs/presentation/*` and 2026-08-11 workbook edits were deliberately left behind (their base commit `3abeedd` is not in this history, and Friday's aggregation supersedes that workbook with a fresh Release sweep). **Phase 2 is therefore no longer branch-only: a run reading this PRD from `master` now sees the real state.** The family set is seven: the five B4 families plus `knowledge-chain` and `knowledge-chain-inert`, the first two that make $\lvert\Tin\rvert = n$ rather than one state. Post-merge suite: **621/622**, the single failure being oracle 5 on `parity-t3`, below.

**Phase 2 (cont. II) — per-case process isolation — implemented 2026-08-23** on branch `worktree-bench-runner-isolation`, uncommitted (this run's own launcher owns the commit). `RunCaseWithTimeout` (`src/ltlf_ek_bench.cpp`) replaces its detached-`std::thread` timeout strategy with a forked child per case, closing the SIGSEGV-before-`--out` failure mode this phase exists to repair. Verified against this phase's own green checkpoint: `--timeout=10` over a sweep containing the known-slow `nfa-product`/`slippery-onehot` $n=3$ cell exits 0 with a complete JSON and that cell marked `"TIMEOUT"`; the same sweep at `--timeout=3000` exits 0 in ~13.5 min with real numbers for that cell; `structural` (count) rows are byte-identical between the two runs and, independently, between the pre-change and post-fix binaries on cases that do not need the timeout path — the rewrite changes nothing about the computed metrics. `ctest`: **693/694**, same pre-existing `parity-t3` failure, no regression. See *Developer comments* below for the full design as built.

**Known-open, not a regression:** `BenchSuiteCrossMethodAgreement.AllFiveMethodsAgreeWithEachOtherAndTheDeclaredVerdict` fails on `parity-t3` at $n = 2, 3$. All five methods **agree with each other** (`false`); only the family's declared `expected_realizable = true` dissents, so this is a wrong **declaration**, not a method divergence. It predates the merge (`9929078`'s own message records it as one of three pre-existing failures) and is untouched here per Stop-list 4 — a verdict disagreement is an O5-class theory finding, not a benchmark bug. **Phase 3 must not bake this cell into the structural baseline while it stands.**

**Interface:** extends the existing benchmarking infrastructure (`docs/prd/benchmarking.md`) with a **metric sink** in `include/ltlf_ek/bench.hpp` (`SizeMetric` / `size_metric_name` / `record_size_metric`), a **benchmark registry** (`BenchFamily` / `BenchSubject` / `BenchCase`, `include/ltlf_ek/bench_suite.hpp`), and a new `ltlf-ek-bench` binary. **Does not change the `Synthesis` contract.**
**Recommended workflow:** **concurrent for Phase 1** (the metric sink mirrors the existing `Stage` registry one-for-one — high freeze confidence), **sequential for Phases 2–3** (the registry and the runner are genuinely being invented here; the test-writer should bind to the real signatures).
**main.tex ref:** none — this is infrastructure, like `docs/prd/benchmarking.md` and `docs/prd/cli-wrapper.md`. The measured quantities trace to the algorithm blocks they size: `alg:dfa_product` (§`fulldfa`), `alg:nfa_product` (§`nfa`), `alg:otfdfa_product` (§`otf`). The **comparability tier** rests on the LTLf $\equiv$ star-free correspondence, which `main.tex` does **not** currently state — see *Open theory questions touched*.

**Gates:**
- [x] glossary        — *Canonical size metric* (`SizeMetric`) and *Comparability tier* (`ComparabilityTier`) added 2026-08-09, pre-`/developer`; the shipped *Canonical benchmarking stage* entry's "Do not call it" line was amended to point at the new size axis
- [x] tests           — **Phase 1 only.** `tests/bench_size_metric_test.cpp` (17 cases) + 2 schema cases in `tests/bench_test.cpp`; verified 2026-08-10 against the Phase 1 green checkpoint: a per-method *exactly its charge-table row set* case for all five methods, the two absent-never-zero cases (`OtfMtdfaProduct` emits no `goal_*`; unrealizable emits no `controller_states`), and a `BenchScopeZeroPerturbation` case for all five. Suite green: 603/603 on `bench-phase1`. **Phase 2 (cont. II) covered 2026-08-23** by `tests/bench_runner_isolation_test.cpp` (6 cases, `/test-writer` over two rounds): the phase’s own green checkpoint (a timing-out cell still exits 0 and writes a complete, schema-valid JSON; that cell is marked and every other cell carries real measurements), the `kFailed` paths driven through the `LTLF_EK_BENCH_FAULT_INJECT` hook (`FAILED_signal_11`, `FAILED_exit_1`, `PARTIAL_FAILED_1_of_3`, each asserting B2 rule 1 — metrics **absent**, never zeros — while the other cells stay real), and `NoOrphanedRunningDescendantSurvivesADeadlineMissKill`, which samples `/proc` *while the parent is alive* and was verified to fail when the kill-and-reap path is removed. Suite: **699/700** (sole red = the known-open `parity-t3` cell below). Phase 3 and the rest of Phase 2 remain uncovered.
- [x] code-review     — **both halves closed 2026-08-10.** Domain (`/code-reviewer`) clean. Generic (`/code-review` on PR #12) returned 4 findings, all verified and all acted on: the two MEDIUM ones (the `num_roots()`/`num_states()` unit mismatch) resolved by the user's choice of option (a) — see *Resolved: the `goal_*` / `product_states` unit mismatch* below, which also rewrote B2 note 2 and the glossary entry it rested on; the two LOW ones fixed (`docs/prd/benchmarking.md`'s stale `to_json` schema, and a `SUCCEED()`-only no-op test that could not observe its own claim). Three `consider`-level notes recorded, not acted on.
- [ ] theory-review   — **no theory surface in Phase 1.** The diff adds no semantics: `cons`, progression, sink placement, final-state classification and the `Synthesis`/`Transducer` contracts are untouched, and every method edit is a `record_size_metric` call plus a mechanical `return f(x)` → `auto c = f(x); …; return c;` refactor. Per this PRD's own header (*main.tex ref: none — this is infrastructure*) the theory-reviewer was **not** spawned. The gate stays open for Phases 2–3, where the *comparability tier* lands and with it the unstated LTLf $\equiv$ star-free claim (Stop-list 1).

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

| `SizeMetric` | Meaning | Representation | Unit |
|---|---|---|---|
| `goal_dfa_states` | states of the deterministic Goal automaton for $\varphi$ | explicit (`twa_graph::num_states`) | states |
| `goal_nfa_states` | states of the nondeterministic Goal automaton for $\varphi$ | either — **measured comparable** | states |
| `goal_mtdfa_roots` | roots of the symbolic Goal mtdfa for $\varphi$ | symbolic (`mtdfa::num_roots`) | roots |
| `nfa_product_states` | states of the **pre-determinization** product | explicit | states |
| `product_states` | states of the explicit structure handed to the game solver | explicit (`twa_graph::num_states`) | states |
| `product_mtdfa_roots` | roots of the symbolic structure handed to the game solver | symbolic (`mtdfa::num_roots`) | roots |
| `product_bdd_nodes` | internal decision nodes of that structure | symbolic | BDD nodes |
| `controller_states` | states of the returned `Controller`'s strategy | explicit — **all five** | states |

**The charge table is the contract** — the number a method reports must come from the structure it actually built:

| Method | `goal_dfa_states` | `goal_nfa_states` | `goal_mtdfa_roots` | `nfa_product_states` | `product_states` | `product_mtdfa_roots` | `product_bdd_nodes` | `controller_states` |
|---|---|---|---|---|---|---|---|---|
| `DfaProduct` | `goal->num_states()` | — | — | — | materialized product `num_states()` | — | — | ✓ |
| `NfaProduct` | — | Goal NFA `num_states()` | — | explicit NFA product `num_states()` | determinized product `num_states()` | — | — | ✓ |
| `MtdfaProduct` | — | — | `mtdfa->num_roots()` | — | — | product mtdfa `num_roots()` | `get_stats(nodes)` | ✓ |
| `MtnfaProduct` | — | `Mtnfa::states.size()` | — | — (product and determinization are fused) | — | product mtdfa `num_roots()` | `get_stats(nodes)` | ✓ |
| `OtfMtdfaProduct` | — (builds no Goal automaton) | — | — | — | — | explored product mtdfa `num_roots()` | `get_stats(nodes)` | ✓ |

Two rules make the holes safe:

1. **An absent metric is absent, never zero.** The baseline schema distinguishes them, and a method that starts emitting a metric it previously omitted is a baseline diff, not a silent change.
2. **A value names a count *and the representation it was counted on*; a shared column is a measured claim.** *(Rewritten 2026-08-10 — see the resolved blocker below. This rule previously said the opposite: that `product_states` was defined by **role**, so the mtdfa cells could share the explicit column because `num_roots()` and `num_states()` were "the same unit". That was false and produced wrong cross-method ratios.)* An explicit value and a symbolic value never share a column, which is why `goal_mtdfa_roots` and `product_mtdfa_roots` exist. Two columns **are** shared, and each was measured rather than assumed: `goal_nfa_states` (`NfaProduct` vs `MtnfaProduct` — equal on all five probe formulas, because `Mtnfa::states` is one MTBDD per NFA state with set-valued terminals) and `controller_states` (all five return the same `spot::twa_graph_ptr`). Where the unit genuinely differs — decision-diagram size — there is a **separate** value, `product_bdd_nodes`, which no explicit cell emits.

3. **Cross-representation comparison is a deliberate act.** The workbook may still put `product_states` beside `product_mtdfa_roots`, but it must say what each counts; an equal column name must never imply it silently. Note `docs/prd/otf-mtdfa-product.md:718`'s existing table compared `num_roots()` against `num_states()` directly — that comparison inherits this defect and should be re-checked when Phase 2 re-derives those numbers.

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

- **Phase 2 (cont. II) — per-case process isolation in `ltlf-ek-bench`.** *Added 2026-08-23; authorized by the user the same day.* `RunCaseWithTimeout` (`src/ltlf_ek_bench.cpp:283`) currently bounds a case by **detaching** its worker thread on a deadline miss. The detached worker is still inside Spot/MONA at process exit, so **any** timing-out case kills the run with SIGSEGV *before* `--out` is written — reproduced deterministically at `--timeout=10` (exit 139, **no JSON at all**), while the same sweep at `--timeout=3000` exits 0 with a complete report. That directly contradicts this PRD's own Stop-list-8 contract that a timing-out row is **recorded** and the other families continue, and it is why the failure mode is not a degraded report but *no report*. **The fix is a forked child per case** — a blocking Spot call cannot be cancelled in-thread, so the timeout has to be enforced on something the OS can kill: `fork()`, run the case in the child, return the measurement over a pipe, `waitpid` with the deadline, `SIGKILL` and reap on a miss. A child that dies by signal, times out, or OOMs yields a row marked as such; the parent never inherits its Spot/BDD state. **Green checkpoint:** `ltlf-ek-bench --timeout=10` over a sweep containing at least one known-slow cell exits **0**, writes a complete, schema-valid JSON, marks that cell as timed-out, and carries a real measurement for every other cell; the same sweep at a generous timeout still reproduces the pre-change numbers; `ctest` green apart from the known-open `parity-t3` cell. *Numbered as a Phase 2 continuation, not Phase 4, because it repairs the Phase 2 runner rather than adding a deliverable — the existing "Phase 2 (cont.)" heading sets that convention.*
  **Why it is being done now:** `docs/prd/engineered-domain-families.md` Phase 4 cannot run at all until this lands (its own *"Phase 4 is blocked by a runner bug this phase exposed"* section), and this PRD's Phase 3 baseline sweep has the same exposure at a wider $n$ range. It is the single blocking dependency for both.

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

## Resolved: the `goal_*` / `product_states` unit mismatch

**Opened 2026-08-10 by the generic `/code-review` on PR #12; resolved the same
day by the user, who chose option (a) — split the axis.** Kept here in full
because the reasoning is the justification for the B2 rewrite above, and because
the *shape* of the mistake is the reusable lesson: a conformance check against a
spec cannot catch a false claim inside the spec.

B2 note 2 and *Canonical size metric* consequence (1) both assert that
`spot::mtdfa::num_roots()` and `twa_graph::num_states()` are "the same unit".
**They are not**, and Spot's own header says so: `num_roots()` is "the size of
the `states` array — it does not account for any bddfalse or bddtrue state",
while a sibling `mtdfa::num_states()` exists precisely as "the size that the
transition-based output of `as_twa()` would have"
(`spot/twaalgos/ltlf2dfa.hh:146-160`).

This is not academic. `ltlf_to_dfa` (`src/ltlf_to_dfa.cpp:14-21`) is
`ltlf_to_mtdfa` → `as_twa(state_based=true)` → `complete_here`, so `DfaProduct`
and `MtdfaProduct` start from **the same mtdfa** and then charge the same axis
two different counts. Measured directly against Spot 2.15.1:

| $\varphi$ | `MtdfaProduct` charges (`num_roots`) | `mtdfa::num_states()` | `DfaProduct` charges |
|---|---|---|---|
| `G(i -> o)` | 1 | 1 | 3 |
| `G(i) \| F(o)` | 2 | 3 | 4 |
| `(i U o) & G(!i \| o)` | 2 | 2 | 3 |
| `X[!]X[!]a` | 3 | 4 | 5 |
| `F(a & X[!]b)` | 2 | 3 | 3 |

Two consequences decide the fix:

1. **The gap is not a constant**, so it cannot be corrected downstream in the
   workbook.
2. **Swapping to `num_states()` does not close it either** — the deltas become
   2, 1, 1, 1, 0. The remaining gap is `as_twa(state_based=true)`'s state
   splitting plus `complete_here`'s rejecting sink. There is **no accessor**
   that makes the two numbers commensurable, because the underlying objects are
   not the same object: `DfaProduct`'s Goal is a materialized, state-split,
   completed `twa_graph`; `MtdfaProduct`'s is a symbolic mtdfa.

The same mismatch applies to `product_states` at `src/mtdfa_product.cpp:77`,
`src/mtnfa_product.cpp:270`, `src/otf_mtdfa_product.cpp:242` — and that is the
cell the glossary **explicitly blessed**, on the premise now shown false.

**Why it matters now:** the `DfaProduct`-vs-`MtdfaProduct` goal-size ratio is
one of the headline numbers this suite exists to produce for the 2026-08-12
presentation. Phase 2's cross-method table would present these side by side.

**Chosen: (a), split the axis.** `goal_mtdfa_roots` and `product_mtdfa_roots`
were added to the registry; `MtdfaProduct` moved off `goal_dfa_states`, and all
three mtdfa-family methods moved off `product_states`. The two rejected options,
recorded because the reasoning still applies if this is ever revisited:

- **(b) Charge both from the same materialized artifact.** Comparable, but the
  mtdfa family would have to build the explicit automaton *just to measure it* —
  perturbing the very cost being measured and defeating the point of the
  symbolic methods.
- **(c) Keep one column, declare the axis non-comparable** via the tier
  machinery, and never rank on it. Rejected: a column that exists but must not
  be compared is a trap for the next reader.

**What (a) costs, stated plainly:** there is no longer a single number comparing
`DfaProduct`'s Goal to `MtdfaProduct`'s. That comparison was never sound, so
nothing true was lost — but the deck cannot show one, and Phase 2's cross-method
table gains two columns that are mostly holes.

**What the split does NOT cover.** Only the DFA/product axes split. Two columns
stay shared, and both were **measured** rather than assumed during the fix:

- `goal_nfa_states` — `NfaProduct`'s explicit Goal NFA vs `MtnfaProduct`'s
  `Mtnfa::states.size()`: **equal on all five probe formulas** (3/3, 5/5, 4/4,
  10/10, 5/5). `Mtnfa::states` holds one MTBDD per NFA state with set-valued
  terminals interpreted by the pool, so there is no `bddtrue`/`bddfalse` state
  to inflate or deflate the count. The column is sound.
- `controller_states` — all five methods return `Controller{spot::twa_graph_ptr}`,
  the same type counted the same way.

This is the rule the rewrite of B2 note 2 encodes: **a shared column is a
measured claim, not a naming convention.**

## Developer comments / PRD disagreements

- **2026-08-10 — `bench_scope_active()` added to the Phase 1 interface.** B2 and `bench.hpp:57` both promise `record_size_metric` is a "near-zero cost no-op if no `BenchScope` is active", but that promise was never reachable from a call site: `record_size_metric(m, expr)` evaluates `expr` (a normal function argument) **before** the function's own `g_active_collector == nullptr` check runs, so an expensive `expr` pays its full cost regardless of whether a scope is active. At `product_bdd_nodes`'s three call sites (`src/{mtdfa,mtnfa,otf_mtdfa}_product.cpp`), `expr` is `product->get_stats(/*nodes=*/true, .../*paths=*/false).nodes` — a full BDD-node traversal, linear in the largest structure in the run, on the hot path of every production `synthesize()` call of all three mtdfa-family methods. Added `bool bench_scope_active()` (declared in `bench.hpp`, defined in `bench.cpp` as `g_active_collector != nullptr`) so a call site can guard the expensive argument itself; guarded the three `get_stats()` sites and the smaller `BenchTimer(std::string("determinize"))` construction in `src/nfa_product.cpp`. This is additive to the frozen Phase 1 block, not a re-shaping: `record_size_metric`'s own signatures and no-op behaviour are unchanged, the new predicate is infrastructure (no `docs/GLOSSARY.md` entry, same as the rest of `bench.hpp`'s plumbing).
- **2026-08-10 — `/code-reviewer` (domain), `master..bench-phase1`: clean, no must-fix.** Verified cell-for-cell that all five methods charge exactly their B2 row and nothing else; that `product_states` is taken by *role* (post-`minimize_mtdfa` in `MtdfaProduct`, post-determinization in `NfaProduct`), per the glossary entry's consequence (1); and that `bench_scope_active()` is covered by that entry's explicit infra exemption, so no glossary row is owed. Three `consider`-level notes, none acted on:
  1. `src/nfa_product.cpp:65` — the `std::optional<BenchTimer> sub` guard is the one guard of the four that buys nothing: `"determinize"` is 11 chars, inside libstdc++'s 15-char SSO buffer, so the unguarded ctor never allocated. It trades that for an `optional` and a wider destruction scope. Behaviour is identical either way — `sub` is constructed after the `product_construction` timer, so it still pops first and the sub-span still nests. Reverting that one site to the plain nested block `{ BenchTimer sub("determinize"); D = nfa_to_dfa(P); }` would read better; the other three guards are load-bearing and must stay.
  2. The three `get_stats(nodes)` guards are verbatim triplicates (3-line comment + 3-line guard) in `src/{mtdfa,mtnfa,otf_mtdfa}_product.cpp`. The obvious dedup — a helper in `bench.hpp` — would drag `spot::mtdfa_ptr` into an infra header whose includes are currently pure-`std`, which is the worse trade, so the duplication is **deliberate**. Noted so Phase 2 does not add a fourth copy by reflex.
  3. `src/nfa_product.cpp:53` `spot::complete_here(nfa)` mutates `nfa` **in place**, so recording `goal_nfa_states` at line 37 *before* the product block is order-critical, not stylistic — after completion `nfa->num_states()` counts the sink. It cannot silently regress (the charge-table case asserts the exact value), but a reader moving that line for tidiness would change the number.
- The four gates in the header are ticked by the skills that perform them.
- **2026-08-11 — Phase 2 part A (`bench_suite.hpp`/`.cpp`), interface sharpening and design decisions.** The frozen block's `BenchCase::t_in`/`t_out` are typed `Transducer` (main.tex's abstract interface, `include/ltlf_ek/transducer.hpp`), which is pure-virtual and cannot be a struct member by value; `instantiate()` must return an owning `BenchCase`, so the only fix that does not dangle is to hold the concrete `OutputLabeledTransducer` (the project's sole `Transducer` subtype) by value. This is the sharpening the header itself invites ("implementation may sharpen them"), not a re-shaping of a *frozen* contract — every other field and every method signature is unchanged. Five further decisions, each an encoding of already-decided or already-precedented material, not a new invention:
  1. **Realizable/unrealizable via the historical conjunct, uniformly.** `docs/prd/otf-mtdfa-product.md` ("Benchmark results, 2026-07-29") records that the unrealizable variant of a family is `phi_n & X[!] i` with `i` a fresh `input_free` AP no transducer commits to (so the adversarial environment always falsifies it), and that this "reproduce[s] both tables within noise". B4 states only the realizable `phi_n` per family and leaves the polarity mechanism to this precedent; all five families use it identically (`i` added to `input_free` only on the unrealizable instance).
  2. **`psi_in` for the three trivial-knowledge families.** `cons-inert`/`mirror-small`/`mirror-degenerate` have empty `Iknown`/`Oknown`, so there is no external-knowledge language to encode; `psi_in = "1"` (vacuously true) is the honest encoding of "no constraint", not a placeholder — a T1 family still needs *some* non-empty `psi_in` string (PRD "The five methods"), and `"1" -> phi` is a legitimate (if degenerate) equirealizable race for Phase 2's later `ltlfsynt` cell.
  3. **`v`/`k` placed as `Ofree`, not `Ifree`, for the three trivial-knowledge families.** The Goal automaton's size (the discriminator B4 exists to measure) depends only on `phi`'s AP set, never on which `VariablePartition` slice an AP sits in, so this choice is free; `Ofree` was picked to match `cons-inert`'s literal "k in Ofree" so the realizable case is trivially true (the controller can just always assert it) without inventing a second mechanism.
  4. **The MONA gate is a runtime probe here, not the `MONA_FOUND` compile define.** `MONA_FOUND` is defined only on the `unit_tests` CMake target (`target_compile_definitions(unit_tests PRIVATE MONA_FOUND=1)`); `bench_suite.cpp` lives in the `ltlf_ek` library target, which has no compile-time knowledge of mona's presence. Added a private, cached `mona_available()` (`std::system("command -v mona ...")`) in `src/bench_suite.cpp`'s anonymous namespace — infra, not a domain concept, no glossary entry — so `NfaProductSubject`/`MtnfaProductSubject::run` can skip cleanly (return before doing anything, so `run_bench_case` harvests zero rows) exactly as the `MONA_FOUND` ctest gate does for tests.
  5. **Measured, not assumed, the families discriminate (Test oracle #2's claims) at n=2,4 on this machine (mona present, so all ten `(family, subject)` pairs ran):** `cons-prunes` goal_dfa_states 5→17 vs product_states 4→6 (exponential vs linear, matches "Goal is 2^n, product is n+1"); `cons-inert` product_states == goal_dfa_states exactly at every n (cons prunes nothing); `mirror-small` goal_nfa_states 5→7 (exactly n+3) vs goal_mtdfa_roots 4→16 (exactly 2^n); `mirror-degenerate` goal_nfa_states 7→19 vs goal_dfa_states 5→17 (same order, the documented degeneracy); `goal_nfa_states` equal between `nfa-product` and `mtnfa-product` on every case (B2 note 2's shared-column claim). No family degenerated (Stop-list 2 not triggered); no `Synthesis` interface change was needed (Stop-list 3 not triggered).
  - **Known, pre-existing, out-of-scope failure observed while running the full suite post-change:** `BenchScopeDeathTest.NestedBenchScopeInstallAsserts` (Phase 1, `tests/bench_test.cpp:418`) fails ("failed to die") on this worktree's configured build, because the tree is configured `Release` (`-DNDEBUG`), which compiles out the `assert()` the death test expects to fire. This is a pre-existing build-configuration gap, not something this diff touches (`bench_suite.cpp` neither opens nor tests `BenchScope` nesting) — flagged here rather than silently left for the next runner to rediscover.
- **2026-08-11 — Phase 2 part B (`ltlf-ek-bench`, B6 export, the `ltlfsynt` T1 race), sharpenings.** `run_bench_case` (frozen, `bench_suite.hpp`) is a single blocking call with no cancellation hook and no way to distinguish a `BenchRow` sourced from a metric vs. a span except by key membership in the canonical `Stage`/`SizeMetric` name sets — none of this is a contract change, but five choices were needed to turn the frozen registry into a bounded, resumable-on-partial-failure sweep:
  1. **Per-case timeout is enforced with a detached `std::thread`, not a subprocess.** `run_bench_case` cannot be preempted from inside; the only mechanism available without re-architecting the registry into a subprocess-per-case protocol (which would need a second CLI surface just to invoke one `(BenchCase, BenchSubject)` pair) is to run it on a worker thread and, on a deadline miss, `detach()` rather than `join()`. The detached thread is abandoned (not killed) and keeps running in the background; every method here provably terminates given enough wall time (finite automata, no genuine infinite loop), so this bounds the common "slow" case without claiming to bound a truly hung one. `BenchCase`/`BenchSubject` objects handed to a detached thread must outlive it, so every instantiated `BenchCase` lives in a `std::deque` that is never erased for the process's lifetime (`std::deque::push_back` never invalidates existing references), and `BenchSubject` pointers come from `bench_subjects()`'s own process-lifetime static vector.
  2. **The T1 race drops `psi_out` from "ψ_in → (φ ∧ ψ_out)" and reduces to "ψ_in → φ"** because every one of the five B4 families uses `trivial_transducer` for `t_out` (part A's `Developer comments` #2: `Iknown`/`Oknown` are empty in the three trivial-knowledge families, and `cons-prunes`/`parity-t3`'s `Oknown` is likewise empty), so `psi_out` is vacuously `"1"` and `phi & psi_out == phi` for every case this suite instantiates. This is exactly `tests/ltlfsynt_oracle_test.cpp`'s Table A–D reduction (`"(" + psi_in + ") -> (" + phi + ")"`), reused rather than re-derived. If a future family gives `t_out` a non-trivial `psi_out`, the reduction must be revisited — flagged here so it is not silently wrong for that family.
  3. **The T1 race's "ek verdict" is `BenchCase::expected_realizable`, not any one subject's output.** Test oracle #5 (Phase 2) already establishes that all five methods must agree with the family's declared verdict, so comparing against the declared ground truth is equivalent to comparing against any individual method's result and avoids picking a method arbitrarily (or requiring one to have actually run, given `--subjects` filtering and MONA absence).
  4. **The cross-method `summary` sheet compresses the `n`/`realizable` axes it does not name.** B6 says "one row per (family, method)"; `largest_n_completed` matches that literally, but `timings`/`structural` also carry a `realizable` axis the summary doesn't. Chosen: the summary reports the `realizable = true` polarity only (matching the historical 5488x/16x-slower headline numbers, both realizable instances), with the full realizable/unrealizable breakdown left in `timings`/`structural`. `speedup_vs_mtdfa_product` additionally requires `MtdfaProduct`'s own `largest_n_completed` to equal the row's — if the two methods didn't survive to the same `n` (e.g. one timed out earlier), the cell is `null` rather than comparing timings at different scales.
  5. **A `construction_ns` column (== `automaton_construction + product_construction`, 0 for an absent stage) sits beside `best_time_ns` (== the full stage-sum, "wall total") in `summary`.** This is the literal encoding of the "Edge cases" `OtfMtdfaProduct`-has-no-`automaton_construction` warning: since `OtfMtdfaProduct` never opens that span, summing (rather than reading `product_construction` alone as a stand-in) is what keeps the column from crediting `MtdfaProduct` with a smaller share of its real cost than it pays. `best_time_ns` is always the full stage-sum (never just `construction_ns`) so it can never itself become the flattering shortcut this rule exists to prevent.
  - **CLI surface note:** the PRD's literal flag block does not name a sweep-budget flag; this developer added `--budget=SECONDS` (default 7200 = 2h) per B6's prose ("a stated wall-clock ceiling (default 2 h)... overridable by a flag"). Not a contract change (the PRD explicitly asked for an overridable flag without pinning its spelling), recorded here so a future reader knows where the name came from.
- **2026-08-11 (launcher) — BLOCKER: `parity-t3`'s realizable polarity is not realizable, and the family definition is the user's to repair.** Test oracle #5's case (`tests/bench_suite_test.cpp:460`, `BenchSuiteCrossMethodAgreement.AllFiveMethodsAgreeWithEachOtherAndTheDeclaredVerdict`) fails on exactly two rows — `family=parity-t3 n=2 realizable=1` and `n=3 realizable=1` — and on nothing else: all four other families pass both polarities, and on the failing rows **all five methods agree with each other** and return **unrealizable** against a declared `expected_realizable = true`. The disagreement is therefore not between methods; it is between the methods and B4's declaration, which makes it a family-definition finding rather than a code defect.
  - **Why the declaration is the wrong half.** B4 gives `parity-t3` the *same* $\varphi_n = F(k \wedge X[!]^n k)$ as `cons-prunes` and varies only $\Tin$ — which is exactly the intended capability separation, but it also silently moves who controls $k$. In `cons-prunes`, $\lambda$ pins $k$ true at every step, so $\varphi_n$ is satisfied on every trace and the realizable polarity is trivially realizable. Under the parity witness, $k$ is a *function of the environment's* $a$, and $\varphi_n$ mentions no output at all, so no system strategy can influence its truth: the instance is realizable iff the environment cannot avoid $k_t \wedge k_{t+n}$. It always can. Let $k$ be true exactly on the $t$ with $\lfloor t/n \rfloor$ even (reachable, since the environment freely chooses when $a$ toggles the parity); then $t$ and $t+n$ always sit in adjacent blocks and are never both true. This holds for **every** $n$, which is why the failure is not an artefact of the test's small sweep — no larger $n$ rescues it.
  - **What is *not* claimed here.** This says nothing about the T3 aperiodicity claim (Stop-list 1). It is a statement about who controls $k$ under this $\varphi_n$, and it was deliberately *not* checked by attempting to construct a $\psiin$ or race `ltlfsynt` on a monolithic reduction, since building a $\psiin$ for this family is exactly what Stop-list 1 forbids.
  - **Why the run stopped rather than repaired it.** Every available repair is a choice about what the family is *for*: (a) declare both polarities unrealizable — cheap, but it collapses the `realizable` axis for this family and leaves the capability separation demonstrated on a trivially-lost game; (b) give `parity-t3` a $\varphi_n$ that mentions an output, so the system has a strategy to find — this is Stop-list 2's "choosing a replacement $\varphi_n$ is a user decision" in all but name, and it also breaks the "matched to `cons-prunes` on $\varphi_n$" property B4 explicitly wants; (c) change $\Tin$ — which forfeits the T3 witness. Nothing in the PRD settles which, so the launcher recorded it and stopped. **The rest of the suite is green (620/621), and the sweep and workbook were produced anyway**, since `parity-t3` is T3 and Stop-list 1 already bars it from every comparison table — the deliverable does not rest on it.
  - **Verified end-to-end**, not merely compiled: a bounded smoke sweep (2 families incl. the T3 witness, 2 subjects, n=2..3, repeat=1) produced a JSON report with all 5 sections populated correctly, an xlsx that opens with the 5 named sheets and matching headers/values via `openpyxl.load_workbook`, a CSV fallback exercised directly against the same report (forcing `ImportError` on `openpyxl`) that produced 5 well-formed CSVs and the documented exit code 2, `--budget=0` stopping the sweep immediately while still writing a valid report, and `--out` rejected for `/tmp`, `$TMPDIR`, and an outside-repo target. The T1 race agreed with `ltlfsynt` on every row of the smoke sweep (no Stop-list item 4 disagreement fired); Stop-list item 4's handling itself (loud stderr + `verdict_mismatch: true`, never "fixed") was written but not exercised by the smoke sweep, since it did not happen to hit a disagreement.
- **2026-08-23 — Phase 2 (cont. II) implemented on branch `worktree-bench-runner-isolation`, uncommitted.** `RunCaseWithTimeout` (`src/ltlf_ek_bench.cpp`) is rewritten from the detached-`std::thread` strategy (part B sharpening #1 above, now obsolete) to a **forked child per case**, exactly as this phase's entry specifies: `fork()`, the child runs `run_bench_case` and serializes just its `(key, value)` rows to a pipe (family/params/subject are already known to the parent from `case_ptr`/`subject_ptr`, so the wire format carries nothing else), `std::cout.flush(); std::fflush(nullptr);` before the fork so the child does not duplicate the parent's buffered progress output, and the child ends every path in `_exit()` (never `exit()`/`return`) — including on a caught exception (`_exit(1)`), so no atexit handler or Spot/BDD static destructor runs in the child. The parent closes the write end, `poll()`s the read end against the remaining deadline, reads to EOF, and always reaps: `waitpid` directly on a clean EOF, `kill(pid, SIGKILL)` then a blocking `waitpid` on a deadline miss — never a zombie either way. `TimedRunResult` gained a three-way `RunOutcome` (`kSuccess`/`kTimedOut`/`kFailed`) in place of the old bool; a child that dies by signal, exits nonzero, or writes a truncated payload is `kFailed` with a `failure_detail` string (`"signal_11"`, `"exit_1"`, `"truncated payload"`, `"pipe() failed"`, `"fork() failed"`, `"pipe read error"`) — none of these five/six error paths are a domain concept, so none needs a `docs/GLOSSARY.md` entry, same treatment as the removed thread-based mechanism had. `RunCaseRepeated`'s `RepeatedResult` gained a parallel `failures`/`last_failure_detail` counter beside the existing `timeouts`; the sweep loop's marker-row logic (~`ltlf_ek_bench.cpp` "the sweep") now emits a `"FAILED_<detail>"` row (all repeats failed, no timeouts) and/or a `"PARTIAL_FAILED_<k>_of_<K>"` row (some repeats failed, at least one succeeded) alongside the pre-existing `"TIMEOUT"`/`"PARTIAL_TIMEOUT_<k>_of_<K>"` rows — same shape (`TimingRow` with a marker key, `ns=0`, `timed_out=true`), so the JSON schema, `scripts/bench_xlsx_export.py`, and every other consumer are unchanged. The comment block above `RunCaseWithTimeout` is rewritten to describe the fork strategy; its one substantive correction is that `case_ptr`/`subject_ptr` no longer need to outlive a detached background worker (there is none), so the `std::deque<BenchCase>` process-lifetime requirement the old comment leaned on is no longer load-bearing for this function — callers still keep that deque (harmless), just not because this function needs it.
  - **No `main.tex`/`Synthesis` surface touched** — this is process-isolation plumbing around an already-frozen `run_bench_case`, same "no theory surface" classification Phase 1/2 parts A and B carry.
  - **Verified per this phase's green checkpoint, not merely compiled.** (1) `./build/ltlf-ek-bench --families=slippery-onehot --subjects=nfa-product,dfa-product --n-min=2 --n-max=3 --repeat=1 --timeout=10 --out=build/benchout/repro10.json` (the known-slow cell being `nfa-product` on `slippery-onehot` at $n=3$, ~7 min/goal per `docs/prd/engineered-domain-families.md`'s Phase 1 finding #2) exited **0**, wrote a complete JSON with both `n=3` `nfa-product` cells (`realizable` true and false) marked `"TIMEOUT"`/`timed_out:true`, and every other cell (both `dfa-product` cells at every $n$, both `nfa-product` cells at $n=2$) carrying a full real stage/metric row set — no crash, no truncated report. (2) The same sweep at `--timeout=3000` exited **0** in ~13m29s (matching the ~7 min/goal estimate for the two $n=3$ `nfa-product` goals) and produced real numbers for the cells that timed out in (1). Cross-checked three ways: the two runs' `dfa-product` timing rows differ only in `ns` (ordinary run-to-run wall-clock jitter, expected since these are wall-clock spans) with an identical stage-key set; every `structural` (count) row shared by both runs is **byte-identical** (0 mismatches over 12 shared cells); and, to isolate this diff from run-to-run noise entirely, the **pre-change binary** (`git stash` on just this file, rebuilt, `--n-max=2` so no timeout path is exercised either way) was run on the same cases and its `structural` rows match the post-fix binary's **exactly** (0 mismatches over 12 cells) — the fork/pipe rewrite changes nothing about the computed metrics, only how the deadline is enforced. (3) `ctest --test-dir build -j4`: **693/694**, the sole failure being the pre-existing, untouched `BenchSuiteCrossMethodAgreement.AllFiveMethodsAgreeWithEachOtherAndTheDeclaredVerdict` on `parity-t3` (Stop-list 4, above) — no regression, no new failure.
- **2026-08-23 — five findings from that day's code review fixed, same branch.** (1) `src/ltlf_ek_bench.cpp:407` (line numbers as of this fix): `pipe()` → `pipe2(pipefd, O_CLOEXEC)` — without `O_CLOEXEC` a MONA-backed subject's `sh -c ... mona ...` grandchild inherited the write end, so a grandchild that outlived the direct child kept the pipe open, the parent never saw EOF, and a case that actually *succeeded* was reported `TIMEOUT` with its rows discarded. (2) The deadline-miss path now sends `kill(pid, SIGKILL)` to the direct child unconditionally, alongside the existing `kill(-pid, SIGKILL)` on the group: both `setpgid` calls (child's `setpgid(0,0)`, parent's mirroring `setpgid(pid,pid)`) are unchecked, so if the group never formed the group kill would return `ESRCH` and signal nothing, hanging the immediately-following blocking `waitpid` — the direct-child kill closes that gap regardless of which `setpgid` fired. (3) The `poll()` timeout is now clamped to `INT_MAX` before the `static_cast<int>` — `ms` is a signed 64-bit millisecond count and `timeout_seconds` is unsigned, so an unclamped narrowing could turn a large `--timeout` negative, which `poll()` reads as "block forever". (4) Fixed a stale comment: the child's own comment said `setpgid(0, 0)` races the parent's `setpgid(pid, 0)`, but the parent call is actually `setpgid(pid, pid)` — corrected to match. (5) `src/bench_suite.cpp`'s `mona_available()` comment claimed the probe is "computed once and cached... a probe on every case would be wasteful", which stopped being true once Phase 2 (cont. II) made every case a fresh forked child (each child re-runs the function-local static once, since it starts from the parent's un-primed state) — comment corrected to describe per-case re-computation as the accepted cost of isolation; the caching mechanism itself is untouched. Also added a test-only fault-injection hook, `MaybeInjectFault` (`src/ltlf_ek_bench.cpp`, called in the child right after `setpgid(0,0)`), read via the env var **`LTLF_EK_BENCH_FAULT_INJECT`**: value `"segv"` forces a null-pointer write (`SIGSEGV`) in the child before it runs the case, value `"exit1"` forces `_exit(1)`; either may be suffixed `:<k>` (1-based) to scope the fault to just the k-th call to `RunCaseWithTimeout` in the process, e.g. `"exit1:2"` faults only the second case and leaves every other case to run normally. The scoping counter (`case_index`) is a `static` incremented once per call *before* `fork()`, so parent and child agree on it with no IPC — the child is a copy of the parent's address space at fork time. Absent or empty env var is a no-op; nothing in the production path can set it. The hook only; writing tests against it is the next agent's job. **Verified:** `cmake --build build -j` clean; `ctest --test-dir build -j8` **696/697**, sole red cell still the known-open `BenchSuiteCrossMethodAgreement...parity-t3` (Stop-list 4, above) — no regression, no new failure.
  - **Measurement-semantics note, separate from the five findings (reviewer flag, not a fix):** repeats are now cold processes — each of the `--repeat=K` runs behind a `(family, subject)` cell is a fresh `fork()`, so it starts with cold Spot/BuDDy caches. Before this phase, all `K` repeats ran inside one long-lived process and later repeats benefited from warm caches built up by earlier ones within the same process. The reported `min`-of-`K` is therefore now a **cold-start minimum** where it previously could be (partially) a warm-cache minimum — this is a systematic shift in what the timing numbers mean, not run-to-run jitter, and every prior timing snapshot under `docs/runs/` was warm in this sense. It does **not** touch the `structural` (count) rows, which this phase's own checkpoint verified are byte-identical pre/post-change (above) — counts don't depend on cache state. No action taken here; recorded so a future comparison against a pre-2026-08-23 timing snapshot accounts for the difference rather than reading it as noise or a regression.
- **2026-08-23 — the `code-review` gate's open list for Phase 2 (cont. II): recorded, not fixed.** Two review rounds ran on this phase (round 1 on the fork rewrite, round 2 on the fix); round 2's verdict is **clean**. Round 1's two must-fixes (the untested `kFailed` path, and a "no zombies" test that could not fail) were closed by the fix round, together with three items the launcher **promoted** from *consider* to must-fix because each silently corrupts the sweep this phase exists to make survivable: the missing `O_CLOEXEC` (a `mona` grandchild holding the pipe open turns a *successful* case into a `TIMEOUT` with its rows discarded), the unverified process group (`kill(-pid,…)` returning `ESRCH` leaves the blocking `waitpid` to hang the sweep unbounded), and the unclamped `poll` timeout (a large `--timeout` narrowed to a negative `int` and waited forever). What remains open, all *consider*-level, none blocking, none acted on:
  - **Signal handling for an interactive sweep.** `setpgid` moves the child out of the terminal's foreground group, so Ctrl-C on a multi-hour sweep kills only the parent; the child and its MONA grandchild survive and keep burning a core. The parent installs no `SIGINT`/`SIGTERM` handler to tear the live child group down. *Promote this if sweeps are ever run interactively rather than from a script.*
  - **`waitpid` is not `EINTR`-retried, and `status` is pre-initialised to 0** — which `WIFEXITED`/`WEXITSTATUS` read as a clean exit 0, so an interrupted wait would report a killed child as a *success*. Unreachable today (no handler is installed anywhere in the repo), but it is the one place in this diff where the failure mode is a silent wrong answer rather than a crash. Pairs with the item above: installing a handler makes it reachable.
  - **Marker-row detail strings are not identifier-shaped.** `failure_detail` carries spaces and parentheses, so a `stage` value can read `FAILED_pipe() failed`. The JSON schema and `scripts/bench_xlsx_export.py` pass them through unharmed (fixed-column projection, verified), but every other stage/marker value in the report is identifier-shaped. Snake-case them.
  - **`_exit()` discards the child's buffered `std::cout`.** Anything `run_bench_case` prints to stdout is now silently lost; `std::cerr` survives, being unit-buffered.
  - **`ParseInt` accepts negatives**, so `--timeout=-1` wraps to roughly 136 years. The clamp above removes the infinite-`poll` hang, but the deadline is still effectively unbounded — a CLI-validation fix, not a runner one.
  - **The fault-injection hook is silent when live.** A stale exported `LTLF_EK_BENCH_FAULT_INJECT` turns a real sweep into `FAILED_` markers indistinguishable from genuine crashes, and the JSON records neither the variable nor the fact of injection. A one-line stderr banner plus a report field would close it. Relatedly, a malformed scope (`"segv:"`, `"segv:x"`) parses to 0, matches no 1-based index, and silently disables injection; and `segv` is a `volatile` null store (UB) where `raise(SIGSEGV)` would be deterministic — a toolchain that lowers the store to `__builtin_trap` yields `SIGILL` and fails the `FAILED_signal_11` assertion.
  - **Two stale comments in the tests**, both cosmetic: `tests/bench_runner_isolation_test.cpp:474-477` still says the orphan property correlates *process-group ids*, contradicting the `NOTE` at `:447` that pgid is unusable here (this sandbox's `ps` reports `pgid=0` for every process, so the test walks the PPID chain instead); and `:413-417` uses bare `setenv`/`unsetenv` rather than an RAII guard, so a throwing `RunCli` would leak the variable into later tests in the same binary.
