# Tools and capabilities

This document is the non-benchmark half of the 2026-08-12 progress
presentation. It explains what this project's shipped tooling does, in terms
of what goes in and what comes out — not in terms of `main.tex` algorithms —
and backs every claim with a real, re-runnable command and its literal
captured output. It assumes no familiarity with the paper.

All commands below are run from the repository root
(`/home/cowclaw/ltlf-external-knowledge`, or an equivalent checkout) with the
binaries invoked by absolute path:

```
/home/cowclaw/ltlf-external-knowledge/build/ltlf-ek-synth
/home/cowclaw/ltlf-external-knowledge/build/ltlf-ek-deps
/home/cowclaw/opt/spot-2.15.1/bin/ltlfsynt
```

Input files (part files, transducer files, one formula file) live under
`docs/presentation/inputs/`, so every command in this document can be re-run
exactly as written. Every example's literal output is stored beside it under
`docs/presentation/transcripts/`.

## 1. What the project is, in one page

The project synthesizes a **controller** for an LTLf **goal formula**
$\varphi$ — the usual reactive-synthesis question, "does a strategy exist
that makes $\varphi$ true no matter what the environment does?" — but adds a
second source of information: **external knowledge**, supplied as
**knowledge transducers** ($\Tin$, $\Tout$) rather than folded into $\varphi$
itself.

Concretely, at every step the environment picks its free inputs, then an
external-knowledge strategy for the input side ($\Tin$) computes a set of
*known* inputs from the history so far **and the current free inputs**, then the controller picks its free
outputs, then an external-knowledge strategy for the output side ($\Tout$)
computes the *known* outputs. The controller only has to win against a
**restricted** environment/output side — one that is committed to behaving
according to $\Tin$/$\Tout$ — rather than an arbitrary one. That restriction
is what "external knowledge" buys: a formula that is unrealizable in the
open, monolithic game can become realizable once the environment (or part of
the system) is known to follow a fixed, regular strategy.

Why is this an interesting move, rather than just a convenience? Because
knowledge captured as a transducer is **arbitrary regular**, and LTLf
formulas are **not** — there is a two-state transducer that no LTLf formula
expresses, at any size. This rests on the standard LTLf $\equiv$ star-free
correspondence, which `main.tex` does not currently state and which is not
independently verified in this repository; it is asserted here as a
well-known result, not proved by this project. (The same caveat governs
`docs/prd/benchmark-suite.md`'s Comparability tiers, which classify a
benchmark family's $\Tin$ by whether an equivalent LTLf formula exists at
all.)

Synthesis under external knowledge is implemented by **five methods**
(`DfaProduct`, `NfaProduct`, `MtdfaProduct`, `MtnfaProduct`,
`OtfMtdfaProduct`) — different algorithms, and in two cases different
**Representations** (the data structure an automaton is held in — explicit
`spot::twa_graph` vs. Spot's `mtdfa`) of the same method — all implementing
one shared interface, so a caller can swap the method without changing
anything else about the problem. Example 3 below runs the identical instance
under two of them and shows they agree.

## 2. The tools

Two binaries ship today.

### `ltlf-ek-synth` — synthesis

Given a goal formula, a **variable partition** (the four-way split of
propositions into $\Ifree,\Iknown,\Ofree,\Oknown$ — free vs. known, input vs.
output), and (optionally) one knowledge transducer per known set, it
dispatches to a `Synthesis` method and reports either a **realizability
verdict** or the synthesized **controller**.

Flags that matter (from the binary's own argument parser; there is no
`--help` — an unrecognised flag or missing required one reports a usage
error to stderr instead):

| Flag | What it does |
|---|---|
| `--dfa-product` / `--mtdfa-product` / `--nfa-product` / `--mtnfa-product` / `--otf-mtdfa-product` | select a method×Representation cell — one of the PRD's **eight method×Representation flags** over the five methods; these five are wired, and three more (`--otf-dfa-product`, `--otf-agg-product`, `--otf-dyn-agg-product`) are recognised but report "not yet implemented" |
| `--formula=STRING` | the goal formula $\varphi$ |
| `--part-file PATH` | the variable partition, from a part file (below); mutually exclusive with... |
| `--inputs a,b,...` / `--outputs x,y,...` | ...the empty-knowledge shorthand: everything free, nothing known |
| `--known-input-transducer PATH` | $\Tin$, required iff $\Iknown\neq\emptyset$ |
| `--known-output-transducer PATH` | $\Tout$, required iff $\Oknown\neq\emptyset$ |
| `--realizable` | print only the verdict (`REALIZABLE`/`UNREALIZABLE` to stdout) instead of the controller |
| `--model-check` | run the **controller verifier** instead of (or after) synthesizing — see example 5 |
| `--controller PATH` | a transducer file to model-check directly, instead of synthesizing one first |
| `--benchmark PATH` | write a per-stage timing report to `PATH` as JSON (see "Stage timing" below) |
| `--minimize-mtdfa` | `MtdfaProduct`-only knob, ignored elsewhere |

Output, when a controller is printed (default mode, realizable): the
controller as a Spot HOA automaton on stdout, exit 0. Unrealizable: the word
`UNREALIZABLE` (stderr by default, stdout under `--realizable`), exit 20.
Usage error: exit 2. Internal / not-yet-implemented: exit 1.

### `ltlf-ek-deps` — dependency extraction

A **separate** binary, producing the external knowledge the five methods
*consume* rather than consuming it itself. Given a goal formula and a
partition, it searches for a **dependent output set** (which outputs are
forced by the history and every other variable) or, under `--direction in`,
a **dependent input set** (the symmetric question for inputs, run against
the formula's negation — the environment's forced moves). Either way it can
emit a ready-to-use knowledge transducer plus an updated part file.

| Flag | What it does |
|---|---|
| `--formula=STRING` | the goal formula |
| `--part-file` / `--inputs`/`--outputs` | same partition sources as `ltlf-ek-synth` |
| `--direction in\|out` | which dependency to compute (default `out`) |
| `--transducer PATH` | write the resulting $\Tin$/$\Tout$ here |
| `--emit-part PATH` | write the updated part file (with the newly-known set filled in) here |
| `--verbose` | narrate the greedy search on stderr |

Output: one line to stdout, `dependent inputs: ... (of ...)` or `dependent
outputs: ... (of ...)`, plus exit 0 on success, exit 3 if the analysed
language is empty ($\varphi$ unsatisfiable for `out`, valid for `in`), exit 2
on a usage error.

### The two file formats, shown as real files

**Part file** (`docs/presentation/inputs/part-a-k-o.txt`, used by every
example below):

```
input_free:   a
input_known:  k
output_free:  o
output_known:
```

Four keys, one per set of the variable partition; a missing or empty value is
the empty set.

**Transducer file format** (`docs/presentation/inputs/tin-eventual-k.hoa`,
$\Tin$ for example 1): a Spot HOA automaton for $\delta$ (acceptance marks
are ignored — a `Transducer` has none), then, after HOA's `--END--`, a
`%%LAMBDA` block giving one boolean formula per state for $\lambda$ (`0` =
undefined there):

```
HOA: v1
States: 2
Start: 0
AP: 2 "a" "k"
acc-name: all
Acceptance: 0 t
--BODY--
State: 0
  [t] 1
State: 1
  [t] 1
--END--
%%LAMBDA
state 0: !k
state 1: k
```

This one says: start in state 0 (`k` false), move unconditionally to state 1
on the first step, and stay there forever (`k` true from the second step on,
regardless of the free input `a`). Note $\Sigma_0/\Sigma_1$ (which variables
this transducer observes vs. produces) are **not** stored in the file — they
are derived from the transducer's **Role** (`t_in`, `t_out`, or `t_c`) and
the partition it is loaded against.

## 3. Capabilities, as a table

| Capability | Tool / flag | Demonstrated by |
|---|---|---|
| Realizable verdict | `ltlf-ek-synth --realizable` | `realizable-known-input` (§4a) |
| Unrealizable verdict | `ltlf-ek-synth` (default or `--realizable`) | `unrealizable-same-formula` (§4b) |
| Controller emission (HOA) | `ltlf-ek-synth` (default mode) | `realizable-known-input` (§4a) |
| Controller verification | `ltlf-ek-synth --model-check` | `controller-verification` (§4e) |
| Dependent-output extraction | `ltlf-ek-deps --direction out` | `dependency-extraction-directions` (§4d) |
| Dependent-input extraction | `ltlf-ek-deps --direction in` | `dependency-extraction-directions` (§4d) |
| The five methods as interchangeable implementations of one interface | any `--<method>-product` flag | `cross-method-agreement` (§4c) |
| Stage timing | `ltlf-ek-synth --benchmark` | `stage-timing-shape` (§4g) |

## 4. Example runs

Examples (a), (b), (c), (e), and (f) share the partition
`docs/presentation/inputs/part-a-k-o.txt` above: a free input `a`, a known
input `k`, and a free output `o` (no known output, so `--known-output-transducer`
is never needed — the trivial output transducer is substituted automatically).
Example (d) is the exception: it runs `ltlf-ek-deps`, a different tool, on a
different variable set entirely (`--inputs a,b --outputs o`, no `k` and no
knowledge) chosen to demonstrate dependency extraction rather than external
knowledge.

### (a) Realizable, with a non-trivial $\Tin$ — `realizable-known-input`

Formula: `X[!] k` — "the trace continues for at least one more step, and at
that next step `k` holds." $\Tin$ is `tin-eventual-k.hoa` above: a genuinely
stateful strategy (not a constant), whose whole point is that `k` is false
at the first step and forced true from the second step onward, independent
of what the free input `a` does. Because that forcing does **not** depend on
the environment's future moves, the controller can rely on it, and the
instance is realizable regardless of what the (irrelevant, here) output `o`
does.

```
$ /home/cowclaw/ltlf-external-knowledge/build/ltlf-ek-synth --dfa-product \
    --formula="X[!] k" \
    --part-file docs/presentation/inputs/part-a-k-o.txt \
    --known-input-transducer docs/presentation/inputs/tin-eventual-k.hoa
```

Output (`docs/presentation/transcripts/realizable-known-input.txt`):

```
HOA: v1
States: 2
Start: 0
AP: 2 "a" "o"
acc-name: all
Acceptance: 0 t
properties: trans-labels explicit-labels state-acc complete
properties: deterministic
spot-state-player: 0 1
controllable-AP: 1
--BODY--
State: 0
[t] 1
State: 1
[t] 0
--END--
```

Exit code 0. This is the synthesized controller as a Spot Mealy machine — two
states, `o` unconstrained (the formula never mentions it, so the game is won
independent of `o`).

### (b) The same $\varphi$, made unrealizable by changing only the knowledge — `unrealizable-same-formula`

Same formula, same partition — only $\Tin$ changes, to `tin-delay-k.hoa`: a
different 2-state strategy where `k` at step $t$ is simply the free input `a`
one step earlier ($k_0=\bot$, $k_t=a_{t-1}$). Now whether `k` holds at
position 1 is entirely up to the environment's choice of `a` at position 0,
which the controller cannot influence — so `X[!] k` is no longer
guaranteed.

```
$ /home/cowclaw/ltlf-external-knowledge/build/ltlf-ek-synth --dfa-product \
    --formula="X[!] k" \
    --part-file docs/presentation/inputs/part-a-k-o.txt \
    --known-input-transducer docs/presentation/inputs/tin-delay-k.hoa
```

Output (`docs/presentation/transcripts/unrealizable-same-formula.txt`):
stdout empty, stderr `UNREALIZABLE`, exit code 20.

This is the cleanest demonstration that the external knowledge is
load-bearing: nothing about $\varphi$, $\mathcal{I}$, or $\mathcal{O}$
changed — only which regular strategy the environment's known input is
committed to.

### (c) The same instance under two different methods — `cross-method-agreement`

Re-running example (a)'s instance (`X[!] k`, `tin-eventual-k.hoa`) once under
`DfaProduct` (Method 2, the explicit DFA product) and once under `NfaProduct`
(Method 1, the NFA product — internally shells out to MONA for the reversed
past-DFA construction):

```
$ /home/cowclaw/ltlf-external-knowledge/build/ltlf-ek-synth --dfa-product \
    --formula="X[!] k" --part-file docs/presentation/inputs/part-a-k-o.txt \
    --known-input-transducer docs/presentation/inputs/tin-eventual-k.hoa --realizable

$ /home/cowclaw/ltlf-external-knowledge/build/ltlf-ek-synth --nfa-product \
    --formula="X[!] k" --part-file docs/presentation/inputs/part-a-k-o.txt \
    --known-input-transducer docs/presentation/inputs/tin-eventual-k.hoa --realizable
```

Both print `REALIZABLE`, exit code 0
(`docs/presentation/transcripts/cross-method-agreement.txt`). Two
independently-implemented algorithms — one building an explicit determinized
product, the other building and reversing an NFA via an external DFA
minimizer — agree on the same instance, which is exactly the point of having
one shared `Synthesis` interface with interchangeable implementations.

### (d) Dependency extraction, both directions — `dependency-extraction-directions`

A different tool, and a different formula: `F(a ^ b)` ("eventually `a` and
`b` differ") over free inputs `a`, `b` and a free output `o` that the
formula never mentions.

```
$ /home/cowclaw/ltlf-external-knowledge/build/ltlf-ek-deps --formula="F(a ^ b)" \
    --inputs a,b --outputs o --direction in

$ /home/cowclaw/ltlf-external-knowledge/build/ltlf-ek-deps --formula="F(a ^ b)" \
    --inputs a,b --outputs o --direction out
```

Output (`docs/presentation/transcripts/dependency-extraction-directions.txt`):

```
dependent inputs: a   (of a, b)
```
```
dependent outputs: none
```

`--direction in` finds a genuine forced pattern: whichever value `b` takes,
`a` is eventually forced to differ from it (this is the textbook witness for
"dependence does not decompose": each of `{a}`, `{b}` is dependent alone,
`{a,b}` jointly is not). `--direction out` correctly reports nothing — the
formula never constrains `o` at all, so there is no output dependency to
extract. Both are real answers, not failures: an empty dependent set is a
legitimate result, not an error (exit code 0 both times).

### (e) A controller verification pass — `controller-verification`

Reusing example (a)'s realizable instance, but with `--model-check` instead
of default/`--realizable` mode: the CLI synthesizes a controller (since no
`--controller` file is given) and then runs the **controller verifier**
against it — an independent check, built directly on strategy agreement, of
whether *every* trace consistent with $\Tin$, the (trivial) $\Tout$, and the
synthesized controller satisfies $\varphi$.

```
$ /home/cowclaw/ltlf-external-knowledge/build/ltlf-ek-synth --dfa-product \
    --formula="X[!] k" --part-file docs/presentation/inputs/part-a-k-o.txt \
    --known-input-transducer docs/presentation/inputs/tin-eventual-k.hoa --model-check
```

Output (`docs/presentation/transcripts/controller-verification.txt`):
`SAFE`, exit code 0.

### (f) The O5 boundary — a known, deliberately-pinned divergence — `o5-boundary-divergence`

**This is not a benchmark and not a claim about which tool is "right."** The
project's five methods synthesize directly against $\varphi$, $\Tin$,
$\Tout$; a second, much older idea (never proved in this project, and as of
2026-08-09 known to have a counterexample) is that the same problem could
instead be solved by folding the knowledge into $\varphi$ as an assumption
and handing the result to a plain, off-the-shelf LTLf synthesizer — the
**monolithic reduction**, $\psi_{in}\rightarrow\varphi$. `docs/runs/2026-08-09-acceptance-mark-edgeless.md`
records the project's first known divergence witness for that conjecture,
and this example reproduces it live.

$\varphi = $ `X[!] 1` (the trace must have length $\geq 2$). $\Tin$
(`docs/presentation/inputs/tin-o5-partial-delta-dead.hoa`) is
**$\delta$-partial**: from its single live state it commits `k := a` and
then transitions to a *second* state with **no outgoing edges at all** — the
transducer's own run simply stops after one step. Its produced-trace
language is $\psi_{in} = (k \leftrightarrow a) \wedge \lnot(X[!]\,1)$ — i.e.
$\Tin$ itself never produces a trace of length $\geq 2$.

```
$ /home/cowclaw/ltlf-external-knowledge/build/ltlf-ek-synth --dfa-product \
    --formula="X[!] 1" --part-file docs/presentation/inputs/part-a-k-o.txt \
    --known-input-transducer docs/presentation/inputs/tin-o5-partial-delta-dead.hoa --realizable
```
stdout `UNREALIZABLE`, exit code 20 — the product of $\varphi$'s DFA with a
$\delta$-dead $\Tin$ has nowhere to go after one step, so the game is stuck.

```
$ /home/cowclaw/opt/spot-2.15.1/bin/ltlfsynt --ins=a,k --outs=o --semantics=Mealy \
    --realizability -F docs/presentation/inputs/o5-reduced-formula.ltlf
```
where the file contains the monolithic reduction, verbatim:
`((k <-> a) & !(X[!] 1)) -> (X[!] 1)`. stdout `REALIZABLE`, exit code 0.

Full transcript: `docs/presentation/transcripts/o5-boundary-divergence.txt`.
**Why they differ, in one paragraph:** `main.tex`'s consistency filter turns
a missing $\delta$ or $\lambda$ value into a **deleted letter** for **every**
party at once. A partial and a totalized $\Tin$ therefore produce the same
traces — but they need not yield the same *synthesis verdict*, because
deleting a letter removes moves from the system just as much as from the
environment. Here the system loses the ability to reach
its goal at all once $\Tin$ runs out of transitions, where the monolithic
encoding's implication $\psi_{in}\rightarrow\varphi$ is instead satisfied
**vacuously** the moment $\psi_{in}$ itself becomes false (which the system
can force by continuing the trace past $\Tin$'s one live step). This is an
**open theory question** — the equirealizability conjecture needs either a
totality hypothesis on $\Tin$/$\Tout$, or an explicit ruling on runs that
leave a transducer's domain (the open trace-termination question after
`main.tex`'s `def:probDef`) — not a bug in either tool.

These six are hand-picked to each make one point clearly; they are not the
project's only correctness evidence. `ctest` also runs a **Generated
corpus** — a fixed-seed list of many more $(\varphi, \text{partition},
\Tin)$ cases, produced by a random generator and graded automatically rather
than against a hand-labeled expected value: the **differential** mode
cross-checks `ltlf-ek-synth` against `ltlfsynt` (inheriting the same O5
caveat above wherever it applies), and the **metamorphic round-trip** mode
checks that every synthesized controller passes its own controller
verifier.

### (g) "Stage timing" — the report's shape, not a benchmark — `stage-timing-shape`

`--benchmark PATH` on `ltlf-ek-synth` writes a JSON report to `PATH` with one
object per span it emitted, each carrying a `duration_ns` field. Most spans
are **canonical benchmarking stages** (`canonical: true` — `automaton_construction`,
`product_construction`, `game_solving`, and, Methods 3.2/3.3 only,
`aggregation`); `input_parsing` is a free-form sub-phase carried alongside
them with `canonical: false`, not itself a canonical benchmarking stage:

```
$ /home/cowclaw/ltlf-external-knowledge/build/ltlf-ek-synth --dfa-product \
    --formula="X[!] k" --part-file docs/presentation/inputs/part-a-k-o.txt \
    --known-input-transducer docs/presentation/inputs/tin-eventual-k.hoa \
    --realizable --benchmark /tmp/report.json
```

Full transcript, including the literal JSON written to `/tmp/report.json`:
`docs/presentation/transcripts/stage-timing-shape.txt`. It is reproduced only
to show the report's **shape**, not as a timing claim (Stop-list 4, this
PRD): the instance above is a two-state toy, and its microsecond-scale
numbers are process-startup noise, not data. `docs/prd/benchmark-suite.md`
(landing 2026-08-11, §5 below) is where real, comparable measurements
belong — this example is not one of the six the presentation is built
around, it exists only so the capability table's "stage timing" row has a
demonstrating example that is a real, literal, re-runnable command like
every other row's.

## 5. What is measured, and where

This document shows *that* the tooling works and *what* it looks like when
run — never how fast. Wall-clock and size measurements across the five
methods, on committed benchmark families with declared expressibility tiers,
are `docs/prd/benchmark-suite.md`'s job (Phase 2, landing 2026-08-11). That
PRD also carries the caveat this document repeats in **§4f**: any comparison
against `ltlfsynt` on the monolithic reduction inherits the O5 **caveat**
above, and a verdict disagreement there is a theory finding, not a benchmark
bug. See that PRD and its resulting run report for the actual numbers.
