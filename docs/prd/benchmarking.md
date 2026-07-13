# PRD: Benchmarking / stage timing

**Status:** implemented — branch agent-a033ae3d24cc7caeb
**Interface:** new benchmarking infrastructure (`BenchScope` / `BenchTimer` / `BenchReport` / `Stage`, `include/ltlf_ek/bench.hpp`); `DfaProduct` and the CLI are instrumented. **Does not change the `Synthesis` contract.**
**Recommended workflow:** concurrent — the span mechanism is a thin, standard RAII collector that falls straight out of the types (high freeze confidence); the test-writer binds to `BenchScope`/`BenchTimer`/`BenchReport`/`Stage` + the frozen JSON schema.
**main.tex ref:** no benchmarking algorithm exists in `main.tex` (this is infrastructure, like `docs/prd/cli-wrapper.md` and the oracle PRDs). The *canonical stages* trace to the algorithm blocks they time: `LtlfToDfa` / `Product` / `SolveDfa` in `alg:dfa_product` (§fulldfa), and `Aggregation` (Methods 3.2/3.3).

**Gates:**
- [x] glossary        — *Canonical benchmarking stage* (`Stage`) added, this session (pre-`/developer`)
- [ ] tests           — unit + oracle coverage
- [ ] code-review     — domain (/code-reviewer) + generic (/code-review)
- [ ] theory-review   — code ↔ math faithfulness vs main.tex

## Goal

Give the repo **one** benchmarking system that measures how long each stage of a
synthesis run takes, so the methods can be evaluated and compared — both against
each other (e.g. automaton-construction time for the DFA path vs the future NFA
path) and, via an end-to-end wall total, against external tools such as
`ltlfsynt`. This is the driver deferred wholesale from
`docs/prd/symbolic-dfa-product.md` ("No benchmarking in this PRD") so that all
benchmarking shares one uniform design. This PRD **sets the repo-wide contract**
and wires **`DfaProduct` only**; the other four methods adopt it later by adding
span guards (no infra change), and it is designed so the metric container can
also hold size metrics (deferred — see BACKLOG).

The design resolves the stated tension — *dynamic* stage timing (a new phase must
need no infra addition) vs *comparable* parts (DFA-construction time must line up
with NFA-construction time) — with a **two-tier** model: a small **closed,
soft registry** of canonical comparable stages, plus **free-form nested
sub-spans** underneath for method-specific detail. This is the standard shape of
tracing systems (open-ended spans + a stable subset of "semantic conventions" for
the names you aggregate on).

## Ubiquitous-language terms used

- **Goal DFA construction (`ltlf_to_dfa`)**, **Product (`build_product_symbolic`
  / `materialize_product`)**, **Game solving (`solve_dfa`)**, **Aggregation
  (`aggregate`)** — the algorithmic spine the canonical stages name; all already
  in `docs/GLOSSARY.md`.
- **The five methods** / `Synthesis` — unchanged; the interface is not touched.
- **NEW — Canonical benchmarking stage (`Stage`)**: the closed enum naming the
  cross-method-comparable timing axes, each value an alias of an existing spine
  algorithm term. **Not yet in `docs/GLOSSARY.md`** — run `/glossary` (see *Open
  glossary work*). The plumbing types (`BenchScope`, `BenchTimer`, `BenchSpan`,
  `BenchReport`) are **infra, not domain concepts**, so — following the
  `include/ltlf_ek/cli.hpp` precedent — they get **no** glossary entry.

## Behaviour / semantics

Benchmarking is **observability only**: it reads a clock and appends records. It
**must not** alter any synthesis logic, so a run's controller / verdict is
**byte-identical** whether timing is on or off (this preserves the byte-identical
stdout the *Generated corpus* differential oracle depends on).

**Two-tier stage model.**
- A **canonical stage** is one of a small closed registry (`enum class Stage`).
  It is the unit of cross-method comparison: when two methods both emit
  `Stage::automaton_construction`, those durations are defined to be comparable.
  A canonical stage that only *some* methods emit (e.g. `aggregation`, emitted by
  Methods 3.2/3.3 only) is fine — comparability means "when both emit it, they
  compare."
- A **free-form sub-span** carries an arbitrary string label and needs **no infra
  change** to introduce (the "new phase needs no infra" requirement). It nests
  under whatever span is open.

**The stage registry is soft/revisable — this is a design property, not a hazard.**
Adding, renaming, or re-mapping a canonical stage touches **only** the enum, its
name table, and one glossary line — never the collector or the emitter (both are
generic over the span tree). **This is explicitly NOT a PRD-change event** and
does not lower the freeze confidence of the *mechanism*. In particular, the
Method-1 detail that **NFA determinization runs *after* the product** — so the
eventual mapping might be "`ltlf_to_dfa` vs `ltlf_to_nfa` under
`automaton_construction`, with determinization folded into
`product_construction`" — is a **deferred convention question for when Method 1
lands** (see *Open theory / convention questions*); this PRD does not lock it.

**What `DfaProduct` emits (this PRD).** Only the three stages it actually has:

| Stage (canonical) | Instrumented at |
|---|---|
| `automaton_construction` | `ltlf_to_dfa(phi, dict)` |
| `product_construction`   | `build_product_symbolic` + `materialize_product` (one span covering both) |
| `game_solving`           | `solve_dfa(product, vars)` |

`aggregation` is a **reserved** registry value that nothing emits yet.

**Whole-run scope + total.** The CLI installs one `BenchScope` around the whole
run (parse → build transducers → synthesize). Parsing/setup appear as a
**free-form** top-level `input_parsing` span; the three canonical stages are
emitted inside `synthesize()` and become top-level spans of the report.
`BenchReport::total` is the `BenchScope`'s own wall lifetime — the end-to-end
number comparable to an external tool's wall-time — and is independent of which
stages were instrumented, so `total − Σ roots` honestly exposes uninstrumented /
overhead time.

**Emission.** A `--benchmark=FILE` CLI flag writes the report as **structured
nested JSON** to `FILE`. **stdout and stderr are unchanged** (the controller HOA /
verdict still owns stdout, unpolluted for existing pipes and the differential
oracle). The file is written on both **realizable and unrealizable** completions
(both are full runs). On a thrown internal error the file is **best-effort** (may
be absent or partial). A benchmark-file **write failure** prints a warning to
stderr and **does not change the exit code** — a benchmarking I/O problem must not
mask the synthesis verdict. `--benchmark` combined with `--model-check` simply
yields `total` + `input_parsing` and no canonical synthesis stages (no
special-casing — the scope just times whatever `main()` does).

**Gating.** Always compiled; runtime-gated. When no `BenchScope` is active every
`BenchTimer` is a near-zero-cost no-op (a thread-local null check). No build
flags.

## Interfaces & types

**Freeze confidence: high.** The mechanism is a thin, standard RAII span collector
— the types fall straight out and are unlikely to churn. (The *stage vocabulary*
is deliberately soft, but that is a documented design property, not interface
churn; it does not lower this confidence.)

New header **`include/ltlf_ek/bench.hpp`**, impl **`src/bench.cpp`**, namespace
`ltlf_ek`:

```cpp
#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace ltlf_ek {

// Canonical comparable stages — the soft registry (see PRD "Behaviour").
// Deliberate to add (enum value + name row + glossary line); that is the ONLY
// infra a new *comparable axis* needs. A non-canonical sub-phase needs none.
enum class Stage {
  automaton_construction,   // LtlfToDfa / LtlfToNfa (Goal automaton build)
  product_construction,     // Product (build_product_symbolic + materialize)
  game_solving,             // SolveDfa
  aggregation,              // Methods 3.2/3.3 aggregate (reserved; unused here)
};

// Stable key string for a canonical stage (the JSON "label").
std::string_view stage_name(Stage s);

// One recorded span: a tree node. Parent wall-duration contains its children.
struct BenchSpan {
  std::string label;                    // stage_name(Stage) or a free string
  bool canonical;                       // true => label is a registry key
  std::chrono::nanoseconds duration;
  std::vector<BenchSpan> children;
};

// The report for one BenchScope lifetime.
struct BenchReport {
  std::chrono::nanoseconds total;       // the BenchScope's own wall lifetime
  std::vector<BenchSpan> roots;         // spans with no open parent
  // Structured nested JSON (schema in PRD "Test oracles"); integer nanoseconds.
  void to_json(std::ostream& os) const;
};

// RAII: installs a thread-local collector for the dynamic scope of this object,
// and measures its own wall lifetime as report().total. Nested install is
// forbidden (asserts) — there is at most one active collector per thread.
class BenchScope {
 public:
  BenchScope();
  ~BenchScope();
  BenchReport report() const;           // callable while alive or from dtor path
  BenchScope(const BenchScope&) = delete;
  BenchScope& operator=(const BenchScope&) = delete;
};

// RAII span guard. Opens a span in the active collector on construction, closes
// it on destruction. No-op (near-zero cost) if no BenchScope is active.
class BenchTimer {
 public:
  explicit BenchTimer(Stage s);         // canonical span   (canonical == true)
  explicit BenchTimer(std::string label);  // free-form sub-span (canonical == false)
  ~BenchTimer();
  BenchTimer(const BenchTimer&) = delete;
  BenchTimer& operator=(const BenchTimer&) = delete;
};

}  // namespace ltlf_ek
```

**Collector mechanism (pinned — do not leave to discovery).**
- The active collector is a **thread-local** owned by the live `BenchScope`
  (`thread_local` pointer to the collector; `nullptr` when none active).
- The collector holds a **stack of open spans**. `BenchTimer` ctor: if no active
  collector, mark self inactive and return (the no-op path); else record
  `steady_clock::now()` and push a frame. `BenchTimer` dtor: if inactive, return;
  else pop, set `duration = now() - start`, and **append the finished `BenchSpan`
  to its parent's `children`** — the parent being the frame now on top of the
  stack, or `roots` if the stack is empty.
- `BenchScope` ctor: **assert** no collector is already active (nested install
  forbidden), install one, record its start. `BenchScope` dtor: uninstall,
  compute `total`. `report()` returns the accumulated roots + total.
- **Clock:** `std::chrono::steady_clock` (monotonic), stored as nanoseconds,
  wall-time (single-threaded, so wall ≈ work).
- **Exception safety:** RAII closes open spans as the stack unwinds; a report is
  therefore always well-formed (possibly partial). Zero-duration spans and an
  empty `roots` are valid.
- **Determinism:** no RNG. Durations are non-deterministic wall values — hence
  the report goes to a **file**, never the byte-compared stdout.

CLI (`include/ltlf_ek/cli.hpp` / `src/ltlf_ek_synth.cpp`): add a value-taking
`--benchmark=FILE` flag (`CliArgs::benchmark_file`, an `std::optional<std::string>`).
When set, `main()` constructs a `BenchScope`, emits a free-form `input_parsing`
span around formula + transducer parsing, and writes `report().to_json` to `FILE`
on the completion paths. **Recommended** implementation for covering every return
path (realizable / unrealizable / throw) uniformly: an RAII emit-guard whose
destructor writes the file and swallows I/O errors to a stderr warning; an
explicit write before each return is an acceptable alternative — the **observable
behaviour above is the contract**, the mechanism is the developer's call.

**If implementation proves this contract wrong:** that is a PRD-change event —
update this section and propagate to any in-flight test branch; the developer does
not silently re-shape the interface on its own branch. (Reminder: editing the
`Stage` *registry* is **not** such an event — it is a routine soft-registry edit.)

## Edge cases

- **No `BenchScope` active** — every `BenchTimer` is a no-op; nothing recorded, no
  crash. (This is the default `ctest`/library path.)
- **Nested `BenchScope`** — forbidden; the second install asserts.
- **Unrealizable run** — the stages still ran; the report is produced and the file
  written (exit code 20 unchanged).
- **Exception mid-stage** — RAII closes open spans; report may be partial; file
  best-effort. Synthesis exit code is authoritative.
- **A canonical stage emitted zero times** (e.g. the future on-the-fly method with
  a combined automaton+product span) — simply absent from the report; aggregation
  tooling tolerates a missing key.
- **A stage emitted more than once** (a future loop) — recorded as multiple
  sibling nodes; downstream aggregation sums them. Allowed.
- **Free-form label equal to a canonical name string** — permitted; the
  `canonical` flag (not the string) is authoritative, so comparison tooling still
  distinguishes them.
- **`--benchmark` with `--model-check`** — `total` + `input_parsing` only, no
  canonical synthesis stages; not an error.
- **Benchmark-file write failure** (bad path, permissions) — stderr warning; exit
  code unchanged (the synthesis verdict wins).
- **Empty report** (scope opened, no timers fire) — `roots` empty, `total` = scope
  lifetime; valid JSON.

## Test oracles (for /test-writer)

Timing is non-deterministic — **never assert absolute durations**; assert
structure, ordering, non-negativity, and containment only.

**Unit (`bench.cpp`):**
- **No-op when inactive** — a `BenchTimer` with no live `BenchScope` records
  nothing and does not crash.
- **Tree shape** — a `BenchScope` with hand-nested `BenchTimer`s yields the
  expected labels, `canonical` flags, and parent/child nesting; `BenchTimer(Stage)`
  ⇒ `label == stage_name(stage)` and `canonical == true`; `BenchTimer(std::string)`
  ⇒ `canonical == false`.
- **Monotonic containment** — every `duration >= 0`; a parent's `duration >=` each
  child's; `total >= Σ roots.duration` (allowing overhead).
- **Nested scope asserts** — installing a second `BenchScope` triggers the
  assertion (death test).
- **`to_json` schema** — output parses as one JSON object with `total_ns` (int),
  `roots` (array); each node has `label` (string), `canonical` (bool),
  `duration_ns` (int), `children` (array). Assert **keys/structure**, not values.

**Integration (`DfaProduct` under a scope):** run a small realizable case inside a
`BenchScope`; assert exactly the three canonical stages
`automaton_construction`, `product_construction`, `game_solving` are present, each
once, `canonical == true`, in that order; `total > 0`.

**Zero-perturbation oracle (the load-bearing one):** for a set of *Generated
corpus* cases, the synthesized controller / realizability verdict is
**byte-identical** with vs without an active `BenchScope`. This mechanically
guards the "observability only" invariant and ties into the existing
corpus differential.

**JSON schema (frozen for the concurrent test branch):**
```json
{
  "total_ns": 1234567,
  "roots": [
    { "label": "input_parsing", "canonical": false, "duration_ns": 4200, "children": [] },
    { "label": "automaton_construction", "canonical": true, "duration_ns": 880000,
      "children": [ { "label": "determinize", "canonical": false, "duration_ns": 210000, "children": [] } ] },
    { "label": "product_construction", "canonical": true, "duration_ns": 90000, "children": [] },
    { "label": "game_solving", "canonical": true, "duration_ns": 300000, "children": [] }
  ]
}
```
Integer nanoseconds; one object; UTF-8; no trailing content.

## Open theory / convention questions touched

- **NFA-method stage mapping (deferred to Method 1).** Determinization runs *after*
  the product in the NFA path, so the eventual canonical mapping is unsettled —
  candidates: (a) `automaton_construction` = `ltlf_to_dfa` vs `ltlf_to_nfa` only,
  with determinization folded into `product_construction`; or (b) a dedicated
  reserved stage. Resolve when Method 1 lands; it is a soft-registry edit, not a
  re-freeze. No `main.tex` `\na` is touched (benchmarking is infra with no math).

## Open glossary work

- **Done (this session).** `docs/GLOSSARY.md` gained a **Canonical benchmarking
  stage (`Stage`)** entry under a new *Benchmarking* group, mapping each enum
  value to its existing spine algorithm term and recording that the registry is
  soft; the free-form sub-span counterpart and the "Do not call it" line
  (phase/step/part; metric/measurement) are pinned. The plumbing types
  (`BenchScope`/`BenchTimer`/`BenchSpan`/`BenchReport`) deliberately get **no**
  entry (infra; `cli.hpp` precedent).

## Definition of done

- `include/ltlf_ek/bench.hpp` + `src/bench.cpp` implement `Stage`/`stage_name`,
  `BenchSpan`, `BenchReport::to_json`, `BenchScope`, `BenchTimer` exactly as
  frozen above; tree compiles green.
- `DfaProduct::synthesize` emits the three canonical stages; the CLI adds
  `--benchmark=FILE`, the whole-run `BenchScope`, the `input_parsing` span, and
  the file-write with the specified realizable/unrealizable/throw/write-failure
  behaviour.
- Unit + integration + zero-perturbation oracles pass; the full suite stays green
  and corpus verdicts remain byte-identical.
- `docs/GLOSSARY.md` gains the `Stage` entry (via `/glossary`).
- BACKLOG items logged for the two deferred pieces (size metrics; Chrome-trace
  exporter) and for wiring the remaining four methods.
- Gates run: `/code-reviewer` + `/code-review`, `/theory-review` (light — confirms
  the stage↔algorithm mapping is faithful and that benchmarking touches no math).
```
