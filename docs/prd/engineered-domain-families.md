# PRD: Engineered domain families (slippery-world)

**Status:** **Phase 1 complete** (2026-08-20, branch `edf-phase1`) — the two
enumerated families `slippery-binary` / `slippery-onehot` and the five
`<method>-nk` subjects are registered in `src/bench_suite.cpp`, at the phase's
green checkpoint; see `docs/runs/2026-08-20-edf-phase1.md`. Phases 2–4 not
started. `BenchCase` untouched, as D4 promised.
**Interface:** adds `BenchFamily` / `BenchSubject` registrations to the landed
Phase 2 registry (`include/ltlf_ek/bench_suite.hpp`), plus **one** new library
API — *Produced-trace equivalence* (`produced_trace_equivalent`). **Does not
change the `Synthesis` contract** and does not modify `BenchCase`.
**Recommended workflow:** **concurrent for Phases 1 and 3** (both register
against the already-frozen `BenchFamily`/`BenchSubject` interfaces — high freeze
confidence), **sequential for Phases 2 and 4** (Phase 2 invents a public API;
Phase 4 adds a runner column).
**main.tex ref:** `\cref{def:indep}` (the Moore condition on $\lambda_{in}$),
`\cref{lem:indep-transducer}` (equirealizability of the extracted transducer),
`\cref{def:probDefTransducer}` (the postcondition *Controller verifier* decides).
The **domain framing** — one $\Tin$ reused across several tasks $\gamma$ — is
**not** covered by `\cref{lem:indep-transducer}` as written; see *Open theory
questions touched*.

**Gates:**
- [x] glossary        — *Produced-trace equivalence* (`produced_trace_equivalent`
  / `EquivalenceResult`) added and ratified by `/glossary` 2026-08-19,
  **pre-`/developer`**, under *Testing & oracles* (the *Controller verifier*
  precedent: a library-API oracle belongs there). The ratification pass also
  found the phrase *"language-equivalence oracle"* already taken by
  `tests/emits_dfa_test.cpp:405` for a **bounded** check and put it on the
  do-not-call-it line by name, and added the missing **domain-framing lemma**
  item to *Open theory questions* (this PRD's Stop-list 6)
- [ ] tests           — unit + oracle coverage. **Phase 1 closed 2026-08-20**:
  `tests/slippery_world_test.cpp` covers T2, T3, T4, T5, T7 plus the $n<2$ floor
  and D7's two demotion/skip units (10 tests, all green). T1/T6 await Phase 2's
  API, T8/T9 await Phase 3's compact arm — so the box stays open PRD-wide.
- [ ] code-review     — domain (/code-reviewer) + generic (/code-review).
  **Domain half clean on the Phase 1 diff, 2026-08-20** (no must-fix; four
  `consider` notes recorded in *Developer comments* below). The **generic half
  is owed**: `/code-review` is not agent-invocable and the day-run does not
  push, so no PR exists to run it against. Gate stays open until it does.
- [ ] theory-review   — code ↔ math faithfulness vs main.tex. **Not run on
  Phase 1, deliberately**: the diff is registry/benchmark-*data* (family
  generators + subjects), not semantic algorithm code — it touches no method,
  no `cons`, no progression, no product construction, and neither the
  `Synthesis` nor the `Transducer` contract. The one theory question these
  families do raise is the **domain-framing lemma**, which Stop-list 6 forbids a
  run from touching.

**Unattended-ready:** **yes — unconditionally, as of 2026-08-19.** The one
condition (the *Produced-trace equivalence* glossary entry) was closed the same
evening: `/glossary` ratified it, so `/developer` cannot stall on a missing term.
Every other decision is closed below: the sweep axis, the three arms, the phase slicing and
their checkpoints, the sweep parameters, and the structural assertions. The two
verdict declarations Phase 1 asserts were **measured tonight**, not assumed
(`REALIZABLE` / `UNREALIZABLE` at $N = 4$ and $N = 8$, `ltlfsynt`
`--semantics=Mealy --realizability`).

## Stop-list

An unattended run must **stop and record**, never guess, on any of these.

1. **The certificate fails on the compact arm** — `produced_trace_equivalent`
   reports the compact $A_N$ and $\Tin$ are not equivalent on non-empty words.
   Do **not** repair by weakening $A_N$, by relaxing the check to containment, or
   by excluding the witness. The certificate is the *only* thing standing between
   a ripple-carry bug and a wrong headline; a run that edits either side of an
   equivalence to make it pass has destroyed the guarantee it was built for.
2. **No separation by $n = 6$** — the compact arm's $\lvert\mathrm{DFA}(A_N)\rvert$
   does not grow superpolynomially in $\lvert A_N\rvert$, i.e. binary-compact
   tracks binary-enumerated the way the probe's two encodings tracked each other.
   Record the measured counts and **stop**: choosing a replacement $A_N$ is a user
   decision, exactly as `docs/prd/benchmark-suite.md` Stop-list 2 reserves. Do
   **not** scale $n$ past 6 chasing it.
3. **A verdict disagreement** — among the five methods, between a method and
   `ltlfsynt`, or between the EK and no-knowledge columns. That is an O5-class
   theory finding, not a benchmark bug (`docs/prd/benchmark-suite.md` Stop-list 4).
4. **The five methods agree with each other but dissent from the family's
   declared `expected_realizable`.** That is a wrong *declaration*, and it is the
   live `parity-t3` situation. Record it; do **not** flip the declaration
   unattended.
5. **The empty-word line is not a failure.** `emits_dfa`'s initial state is final
   by construction while the repo's $\text{LTL}_f$ convention rejects the empty
   word, so the two *always* disagree there. It is reported on its own line and
   must never be "fixed", folded into the verdict, or treated as a red test.
6. **The domain-framing lemma** — that a $\Tin$ built from $A$ alone is sound for
   *every* task $\gamma$, not merely for the formula it was extracted from — is
   **not in `main.tex`**. A run must not attempt to prove, disprove, or "check"
   it (see *Open theory questions touched*).
7. **Turn order.** These families are Mealy. A Moore reading yields a **wrong
   verdict with no crash** (`include/ltlf_ek/turn_order.hpp`). Any suspicion that
   the ordering is wrong stops the run — it is not a slow result, it is a false one.
8. **One-hot dying at large $n$** is a reportable data point, not a bug to tune
   around. At $n = 6$ the one-hot arm carries $2 \cdot 2^{6} = 128$ position APs;
   if it times out, record the row as `TIMEOUT` and continue the other arms.

## Goal

Give idea 1 — *external knowledge as an engineered domain* — a committed,
reproducible family set, so that the claim "EK avoids an **exponential**
translation" can be re-run instead of asserted.

The motivation is a measured null. Tuesday's corpus sweep
(`docs/runs/2026-08-18-syntcomp-fin-sweep.md`) found **1.5 %** output-dependency
hit rate and **0.0 %** input-dependency hit rate on tier 1 of `SYNTCOMP-fin`, and
the stronger crossed finding **hits ∩ raceable band = ∅**: where the dependencies
are, the problem is already trivial; where the problem is hard, there are no
dependencies. External knowledge therefore cannot be recovered *from $\varphi$*
and must come from outside it — which is what `main.tex`'s Introduction already
asserts about PDDL domains and what nobody has measured. This PRD builds the
measurement.

Wednesday's feasibility probe (`docs/runs/2026-08-19-slippery-world-probe.md`)
confirmed the four premises the idea rests on — $\Tin$ is expressible, the
certificate computes in microseconds, verdicts agree 12/12, and a gap opens —
but it also produced the finding this PRD is shaped around: the probe's $A_N$
**enumerates cells per coordinate**, so $\lvert\mathrm{DFA}(A_N)\rvert = N^2 + 2$
in *both* encodings and the EK win is a **constant factor** (1.7–4.0×) with no
encoding separation. A constant factor is not the claim. The claim needs an $A_N$
written **compactly over the position bits**, which is what Phase 3 builds.

This PRD **cites and does not supersede** `docs/prd/benchmark-suite.md`: that one
owns the *harness* (metric sink, registry, `ltlf-ek-bench`, xlsx export, the
`ltlfsynt` T1 race), all of which landed on `master` 2026-08-18. This one owns
*families*. The corpus arms are deliberately **out of scope**: arm (a) is a
finished measurement living in `docs/runs/`, and arm (b) (invented $\Tout$
sensitivity) is a later PRD.

## Ubiquitous-language terms used

Existing, used as spelled in `docs/GLOSSARY.md`:

- **Knowledge transducer** ($\Tin$, $\Tout$), **Output-labeled transducer**,
  **Transition function** ($\delta$), **Output function** ($\lambda$).
- **Variable partition** ($\Ifree$, $\Iknown$, $\Ofree$, $\Oknown$), **Turn order**.
- **Produced-trace language** ($\psi$, $\psiin$, $\psiout$) — the concept the new
  API decides equality of.
- **Observed / produced slice** ($\Sigma_0$ / $\Sigma_1$), **Role**.
- **Goal formula** ($\gamma$), **Goal DFA construction** (`LtlfToDfa`),
  **Output agreement** (`emits`, `emits_dfa`), **Consistency** ($\cons$).
- **Comparability tier** (`ComparabilityTier`) — all families here are **T1**.
- **Canonical size metric**, **Canonical benchmarking stage**.
- **Controller verifier** (`verify_controller`), **Faithfulness guard** — the
  mutation-based cross-check the new API is the *complete* counterpart of.
- **The five methods** — all five participate, plus their five no-knowledge twins.

**New — must be in `docs/GLOSSARY.md` before `/developer` runs:**

- **Produced-trace equivalence** (`produced_trace_equivalent` /
  `EquivalenceResult`) — the complete decision that a transducer's
  *produced-trace language* and a declared $\psi$ denote the same language over
  **non-empty** words. Distinct from the *Faithfulness guard*, which samples
  single-bit mutations (sound, not complete); this decides.

Per the `bench_suite.hpp` precedent, the family and subject registrations
(`SlipperyFamily`, the `-nk` subjects) are **infrastructure, not domain
concepts**, and get **no** glossary entry.

## Behaviour / semantics

### D1. The domain, pinned

$N \times N$ grid. `slip` $\in \Ifree$; the position APs $\in \Iknown$;
`mvl/mvr/mvu/mvd` $\in \Ofree$; $\Oknown = \emptyset$. The start cell is
$(0,0)$.

$\delta$ is **total** over all 16 `mv` valuations via the fixed priority
$L > R > U > D$ (no direction = stay), so the five priority classes partition
$2^{\{`mv`\}}$. A move changes **one** coordinate: $x$ moves iff the class is
$L$ or $R$, $y$ iff it is $U$ or $D$. The step is **1 cell, or 2 when `slip`
holds**.

**Walls saturate — they do not block.** The transition is
$x' = \min(x + d,\, N-1)$ and $x' = \max(x - d,\, 0)$, i.e. **clamping**: an
overshoot lands *on* the boundary cell. This is what
`scripts/slippery_world.py:47` implements and what the committed fixtures and the
passing certificate encode. Note that the probe script's own docstring says
"walls are no-ops (bumping = stay)" — **that prose is wrong**; the code clamps,
and clamping is normative here.

Both choices (total $\delta$, saturating walls) exist so that
$A_{\text{rest}} = \top$: neither restricts an *output*, so the whole assumption
lives in $\Tin$ and the EK side is handed $\gamma_N$ alone with no residual
conjunct.

**The Moore condition holds by construction, not by argument**
(`\cref{def:indep}`, input-dependencies PRD *I3*): $\lambda$ emits the current
state's position literals and does **not** read $\Sigma_0 = \{`slip`\}$, so
position at $t$ is a function of the run's state, i.e. of history strictly before
$t$.

### D2. The sweep axis is the bit-width

**$n$ is bits per coordinate and $N = 2^n$.** The floor is $n = 2$ ($N = 4$);
`instantiate()` must reject $n < 2$ rather than emit a malformed formula.

$n = 1$ ($N = 2$) is excluded because it is degenerate: $c = \lfloor (N-1)/2
\rfloor = 0$, so the centre **is** the start cell and the goal is satisfied at
$t = 0$ — the probe measured `REALIZABLE` there and it carries no information.

Powers of two are load-bearing, not cosmetic. With $N = 2^n$ **every** code over
$2n$ bits denotes a real cell, so there are no unrepresentable positions for
$A_N$ to exclude. That keeps *Produced-trace equivalence* a clean equality (no
range-invariant conjunct on either side) and turns saturation into a bit test
(all-ones / all-zeros) instead of a comparison against an arbitrary constant —
which is what keeps the compact $A_N$ at $O(n^2)$.

### D3. Three arms — a factorial design, not a pair

| arm | family name | position encoding | $A_N$ style | $\lvert A_N\rvert$ (top-level conjuncts) |
|---|---|---|---|---|
| 1 | `slippery-binary` | binary, $2n$ APs | enumerated | $14N + 1$ |
| 2 | `slippery-onehot` | one-hot, $2N$ APs | enumerated | $14N + 1$ |
| 3 | `slippery-binary-compact` | binary, $2n$ APs | compact (D5) | **15**, independent of $N$ |

Arms 1 vs 2 isolate **encoding** with the formula style held fixed — and the
probe already measured this contrast as **null**, so it ships as a *confirmed
negative control*, not an untested assumption. Arms 1 vs 3 isolate
**compactness** with the encoding held fixed; this is where the separation must
appear.

**Arms 1 and 3 share a byte-identical $\Tin$.** Only $\psiin$ differs. That is
what makes the compactness contrast clean, and it is an asserted test (T8).

Each arm is **one** `BenchFamily` sweeping $n$ with the landed
`realizable` flag selecting the goal — matching the registry's existing "single
sweep axis $n$ plus a realizable flag" shape:

- `realizable = 1` → **corner**, $\gamma_N = \mathsf{F}(\text{pos} = (N{-}1, N{-}1))$
- `realizable = 0` → **centre**, $\gamma_N = \mathsf{F}(\text{pos} = (c,c))$,
  $c = \lfloor (N-1)/2 \rfloor = 2^{n-1} - 1$

Under $N = 2^n$ the centre code is exactly *all bits set except the most
significant*, so both goals stay $O(n)$.

### D4. $\psiin$ **is** $A_N$ — and the certificate is what makes that honest

These are T1 families, and `BenchCase::psi_in` is required for T1. For these
families the declared $\psiin$ and the externalized assumption $A_N$ are the
**same object**: $A$ is deleted from the formula and lives only in the
transducer (grill Q3), and the thing that proves the deletion lossless is
$L(\Tin) \equiv L(A_N)$ — which is precisely the T1 declaration.

Two consequences the implementation depends on:

- **No new `BenchCase` field is needed.** The no-knowledge subjects read
  `psi_in`; they are not inventing a formula, they are reading the family's own
  declared assumption.
- The certificate **retroactively hardens** the five landed families, whose
  $\psiin$ is currently hand-declared with nothing checking it
  (`docs/prd/benchmark-suite.md` B3). Phase 2 closes that hole.

### D5. The compact $A_N$ — pinned to the bit

This is a **bespoke mechanism**, not lifted from `main.tex`, so it is specified
here past sketch level. Everything below is exact; nothing is left for
`/developer` to discover by running it.

Bits per axis: $b^x_0 \ldots b^x_{n-1}$ and $b^y_0 \ldots b^y_{n-1}$, LSB first,
named `bx0..bx{n-1}` / `by0..by{n-1}` (the probe's spelling). Value
$= \sum_i b_i 2^i \in [0, N-1]$.

**Predicates** over one axis's bit vector $b$:

- $\mathrm{Max}(b) \equiv \bigwedge_{i<n} b_i$ — value $N-1$
- $\mathrm{Min}(b) \equiv \bigwedge_{i<n} \lnot b_i$ — value $0$
- $\mathrm{Max}_1(b) \equiv \lnot b_0 \wedge \bigwedge_{1 \le i<n} b_i$ — value $N-2$
- $\mathrm{Min}_1(b) \equiv b_0 \wedge \bigwedge_{1 \le i<n} \lnot b_i$ — value $1$

**Update relations**, all using **weak `X`** (see D6):

- $\mathrm{Keep}(b) \equiv \bigwedge_{i<n} (\mathsf{X} b_i \leftrightarrow b_i)$
- $\mathrm{Inc}(b) \equiv (\mathsf{X} b_0 \leftrightarrow \lnot b_0) \wedge
  \bigwedge_{1 \le i<n} \bigl(\mathsf{X} b_i \leftrightarrow (b_i \oplus \bigwedge_{j<i} b_j)\bigr)$
- $\mathrm{Inc}_2(b) \equiv (\mathsf{X} b_0 \leftrightarrow b_0) \wedge
  (\mathsf{X} b_1 \leftrightarrow \lnot b_1) \wedge
  \bigwedge_{2 \le i<n} \bigl(\mathsf{X} b_i \leftrightarrow (b_i \oplus \bigwedge_{1 \le j<i} b_j)\bigr)$
- $\mathrm{Dec}(b) \equiv (\mathsf{X} b_0 \leftrightarrow \lnot b_0) \wedge
  \bigwedge_{1 \le i<n} \bigl(\mathsf{X} b_i \leftrightarrow (b_i \oplus \bigwedge_{j<i} \lnot b_j)\bigr)$
- $\mathrm{Dec}_2(b) \equiv (\mathsf{X} b_0 \leftrightarrow b_0) \wedge
  (\mathsf{X} b_1 \leftrightarrow \lnot b_1) \wedge
  \bigwedge_{2 \le i<n} \bigl(\mathsf{X} b_i \leftrightarrow (b_i \oplus \bigwedge_{1 \le j<i} \lnot b_j)\bigr)$
- $\mathrm{SetMax}(b) \equiv \bigwedge_{i<n} \mathsf{X} b_i$
- $\mathrm{SetMin}(b) \equiv \bigwedge_{i<n} \lnot \mathsf{X} b_i$

$\mathrm{Inc}_2$ is "add 1 starting at bit 1", which is add-2; its guard
guarantees value $\le N-3$ so the carry never leaves bit $n-1$. $\mathrm{Dec}_2$
is the borrow dual under value $\ge 2$.

**The seven rules per axis** (written as a case-split conjunction, so each is
total). For the $x$ axis, with $G_L, G_R, G_U, G_D, G_S$ the five priority-class
literal guards:

| class | slip | rule on $b^x$ |
|---|---|---|
| $R$ | $\lnot$`slip` | $(\mathrm{Max} \to \mathrm{Keep}) \wedge (\lnot\mathrm{Max} \to \mathrm{Inc})$ |
| $R$ | `slip` | $((\mathrm{Max} \vee \mathrm{Max}_1) \to \mathrm{SetMax}) \wedge (\lnot(\mathrm{Max} \vee \mathrm{Max}_1) \to \mathrm{Inc}_2)$ |
| $L$ | $\lnot$`slip` | $(\mathrm{Min} \to \mathrm{Keep}) \wedge (\lnot\mathrm{Min} \to \mathrm{Dec})$ |
| $L$ | `slip` | $((\mathrm{Min} \vee \mathrm{Min}_1) \to \mathrm{SetMin}) \wedge (\lnot(\mathrm{Min} \vee \mathrm{Min}_1) \to \mathrm{Dec}_2)$ |
| $U$ | — | $\mathrm{Keep}$ |
| $D$ | — | $\mathrm{Keep}$ |
| $S$ | — | $\mathrm{Keep}$ |

The $y$ axis is the same table with $U$ playing $L$'s role (decrement) and $D$
playing $R$'s (increment), and $L, R, S$ keeping.

**The whole formula:**

$$A_N \;\equiv\; \mathrm{Min}(b^x) \wedge \mathrm{Min}(b^y) \;\wedge\;
\bigwedge_{\text{axis}} \; \bigwedge_{(\text{class},\,\text{slip})}
\mathsf{G}\bigl( (G_{\text{class}} \wedge \text{slipLit}) \to \text{rule} \bigr)$$

That is $1$ init conjunct $+ 2 \times 7 = 14$ implications $= \mathbf{15}$
top-level conjuncts for every $n$, each of size $O(n^2)$ — against the
enumerated arm's $14N + 1$. Both counts are **exact and derived from the
construction**, so both are asserted (T5), not measured.

### D6. Weak `X` is mandatory

$A_N$ uses **weak `X`** ($\mathsf{X}$, not $\mathsf{X[!]}$). With $\mathsf{X[!]}$
the rule is violated at the last position of every trace — the guards are total,
so some class always fires — and $A_N$ collapses to `false`. This is not
stylistic: weak `X` is what makes $A_N$ agree with "$\delta$ is undefined past
the end" on the transducer side. A satisfiability test guards it (T7), because
the collapse is silent: `false` $\to \gamma$ is valid, so every verdict would
come back `REALIZABLE` and look fine.

### D7. The no-knowledge column

Five new `BenchSubject`s named `<method>-nk` (`dfa-product-nk`,
`nfa-product-nk`, `mtdfa-product-nk`, `mtnfa-product-nk`,
`otf-mtdfa-product-nk`). Each:

1. parses `BenchCase::psi_in` (required — a case without it is **skipped
   cleanly**, recording nothing, exactly as a MONA-absent subject does, so the
   five landed families are unaffected);
2. builds $\psiin \to \varphi$;
3. builds a *Variable partition* with $\Iknown$ demoted into $\Ifree$ and
   $\Oknown$ into $\Ofree$;
4. runs the method with $\Tin, \Tout$ from
   `trivial_transducer(partition, role, dict)`
   (`include/ltlf_ek/output_labeled_transducer.hpp`).

**Order matters in step 3→4 and it is enforced by a throw, not by convention:**
`trivial_transducer` is valid only when the role's *produced* slice $\Sigma_1$ is
empty and throws `std::invalid_argument` otherwise. The demotion in step 3 is
what makes $\Iknown = \emptyset$, so building the transducer against the
*domain* partition would throw. Demote first, then construct.

Pairing with the EK row is by identical `(family, params)`, which is already the
row key — no new pairing convention.

### D8. Structural layer — exact where we build it, bounded where Spot decides

Committed, deterministic, asserted in `ctest` (`docs/prd/benchmark-suite.md` B1
layer 1). **ours-no-knowledge is the structural baseline**; `ltlfsynt` stays
**timing-only** (it reports no state counts).

**Exact** — constructed by the generator, so a deviation is our bug:

- $\lvert\Tin\rvert = N^2 = 4^n$, all three arms.
- $\lvert A_N \rvert$ top-level conjuncts: $14N+1$ (arms 1, 2), $15$ (arm 3).
- EK `goal_mtdfa_roots` $= 1$ — the EK side translates $\gamma_N$ alone.

**Bounded** — decided by Spot's translation and minimization, so a Spot upgrade
that shifts a minimized count by a few states must not turn the suite red:

- $\lvert\mathrm{DFA}(A_N)\rvert \ge N^2$ for every arm (it must track position).
- Arms 1, 2: $\lvert\mathrm{DFA}(A_N)\rvert \le (14N+1)^2$ — **polynomial in the
  formula size**.
- Arm 3: $\lvert\mathrm{DFA}(A_N)\rvert \ge 4^n$ against a **constant** conjunct
  count — **superpolynomial in the formula size**.
- no-knowledge `goal_mtdfa_roots` $\ge N^2$ for arms 1 and 2 (the probe measured
  exactly $N^2$).

The last two bullets together **are** the separation claim, stated as assertions
rather than as a reading of a chart.

**Reported, not asserted:** `product_bdd_nodes`, including the counterintuitive
fact that **EK's is larger** (289 vs 225 at $N=4$ binary in the probe). It is the
same finding from the other direction — EK does not win by carrying a smaller
object, it wins by not doing a translation — but it is a BuDDy-decided number and
locking it would be brittle.

### D9. Amortization — the extractor column

Per $n$, run the existing input-dependency extraction with a timeout and report
its wall-clock beside the race: the *"if you had to derive this knowledge rather
than author it"* upper bound, which is what Friday's break-even $k$ needs (Q1
says transducer-construction cost is outside the race but **reported** as its own
column).

**Pinned, because two of these are error conditions rather than choices:**

- **Entry point:** `dependent_inputs(phi, partition, dict)`
  (`include/ltlf_ek/dependent_inputs.hpp`), the same core the `ltlf-ek-deps`
  binary drives — not a re-implementation.
- **Formula:** $A_N \to \gamma_N$, the **whole monolithic task**, not $A_N$
  alone. That is what a user actually holds before knowing the problem is
  separable, and it is what arm (a) ran on the corpus — so this column is
  directly comparable to Tuesday's sweep instead of being a private measurement.
- **Partition: the no-knowledge one** ($\Iknown = \emptyset$, position demoted
  into $\Ifree$) — **mandatory, not stylistic**. `dependent_inputs` throws
  `std::invalid_argument` when `partition.input_known` is non-empty (*I9*), so
  passing the domain partition is not a worse measurement, it is an exception.
  This is also the semantically right frame: extraction is what you do *before*
  you have the knowledge.
- **Both outcomes are results, neither is a failure:** a completion gives Friday
  a real break-even $k$; a timeout or blowup is the same wall Tuesday's corpus
  sweep hit, which makes the null and the engineered-domain route one story
  instead of two. Report the outcome class (`OK` / `TIMEOUT` / `MEMOUT`)
  **separately** from the elapsed time, as Monday's pre-registration required.

Both outcomes are publishable and neither is a failure: a number gives a real
break-even, and a timeout/blowup is the same wall Tuesday's corpus sweep hit,
which makes the null and the engineered-domain route one story instead of two.

## Interfaces & types

**Freeze confidence: high** for the family/subject registrations (they implement
already-frozen abstract bases and add no new shapes) and for `BenchCase` (it is
**not touched**). **Tentative** for the one new API below — it is genuinely being
invented here, which is why Phase 2 runs sequential.

### New library API — *Produced-trace equivalence*

New header `include/ltlf_ek/produced_trace_equivalence.hpp`:

```cpp
namespace ltlf_ek {

// The complete counterpart of the Faithfulness guard: decides whether a
// Knowledge transducer's Produced-trace language and a declared psi denote the
// same language over NON-EMPTY words.
struct EquivalenceResult {
  bool equivalent_on_nonempty;
  bool empty_word_agrees;                        // reported, NEVER folded in
  std::optional<std::vector<bdd>> counterexample; // shortest non-empty witness
  unsigned tau_dfa_states;                        // |emits_dfa(tau)|
  unsigned psi_dfa_states;                        // |ltlf_to_dfa(psi)|
  unsigned product_states;                        // reachable pairs explored
};

EquivalenceResult produced_trace_equivalent(const Transducer& tau,
                                            spot::formula psi,
                                            const VariablePartition& vars,
                                            Role role);

}  // namespace ltlf_ek
```

Pinned behaviour — every one of these is a decision, not an implementation
detail:

1. **Construction.** Build `emits_dfa(tau, ...)` and `ltlf_to_dfa(psi)` on **one
   shared `bdd_dict`**, then walk a synchronous product. A missing edge on either
   side is an **implicit rejecting sink**, not a skipped letter.
2. **Verdict.** `equivalent_on_nonempty` is false iff some **reachable**
   state pair other than the initial pair has differing finality.
3. **Witness.** `counterexample` is the **shortest** such word, found by BFS over
   product states with letters visited in `bdd_dict` variable order — so it is
   **deterministic and reproducible**, not "some" witness. `std::nullopt` iff
   `equivalent_on_nonempty`.
4. **The empty word is excluded from the verdict and reported separately.**
   `empty_word_agrees` = (`emits_dfa`'s initial state is final) == (`ltlf_to_dfa`'s
   initial state is final). In practice it is **always false** — `emits_dfa`'s
   initial state is final by construction (a run of length 0 vacuously agrees
   with $\lambda$) while the repo's $\text{LTL}_f$ convention rejects the empty
   word. That is a mismatch between two encodings of "language", not a fact about
   any domain. See Stop-list 5.
5. **`role` is not defaulted**, for the same reason the *Faithfulness guard*'s is
   not: a defaulted `Role` is exactly how a $\Tout$ pair silently gets checked
   under $\Tin$ slices and passes vacuously. Slices come from `sigma_slices`.
6. **Termination.** The product is finite (both sides are DFAs); the walk is a
   plain BFS to fixpoint with no iteration bound to choose.
7. **Degenerate inputs.** `psi` = `tt` / `ff` are legal and decided normally. A
   `tau` whose $\delta$ is nowhere defined yields `tau_dfa_states >= 1` with an
   empty non-empty language; the comparison is still exact.

### Registry additions (no new abstractions)

Three `BenchFamily` subclasses — `slippery-binary`, `slippery-onehot`,
`slippery-binary-compact` — each with `sweep(n_min, n_max)` over $n \ge 2$ ×
`realizable` ∈ {0,1}, and `instantiate()` filling the landed `BenchCase` fields:
`phi` = $\gamma_N$, `vars`, `t_in` (the enumerated $N^2$-state
*Output-labeled transducer*), `t_out` = trivial total, `tier` =
`ComparabilityTier::t1`, `psi_in` = $A_N$ as text, `expected_realizable` = the
`realizable` param.

Five `BenchSubject` subclasses per D7.

**If implementation proves this contract wrong:** that is a PRD-change event —
update this section and propagate to any in-flight test branch; the developer
does not silently re-shape the interface on its own branch. In particular,
**adding a field to `BenchCase` is a PRD-change event on
`docs/prd/benchmark-suite.md` as well**, and D4 exists to explain why none is
needed.

## Implementation phases

Four phases, each a separate `/developer` session, each leaving the tree
compiling and independently testable.

**Phase 1 — the enumerated families and the no-knowledge subjects.**
*Workflow: concurrent.* Registers `slippery-binary` and `slippery-onehot` (D1–D4,
enumerated $A_N$) and the five `-nk` subjects (D7). May stub nothing; the
compact arm does not exist yet.
**Green checkpoint:** compiles; both families instantiate at $n = 2, 3$; the five
methods **agree with each other and with `expected_realizable`** on every case
(corner `true`, centre `false` — measured at $N=4$ and $N=8$, see below); the
`ltlfsynt` T1 race reports `verdict_mismatch_count: 0`; the existing suite is
unchanged at 621/622 (the known-open `parity-t3` cell).

**Phase 2 — *Produced-trace equivalence* as a library API.**
*Workflow: sequential.* Implements the API above with its own unit tests, and
**retrofits it to the five landed T1 families**, closing the B3 hole.
**Green checkpoint:** compiles; the API's unit tests pass including the
**negative control** (T6); the certificate is green on `slippery-binary` and
`slippery-onehot` at $n = 2, 3$ and on every landed T1 family's $\psiin$.

**Phase 3 — the compact arithmetic $A_N$.**
*Workflow: concurrent.* Registers `slippery-binary-compact` (D5) reusing Phase
1's $\Tin$ verbatim.
**Green checkpoint:** compiles; **the Phase 2 certificate proves
$L(\Tin) \equiv L(A_N^{\text{compact}})$** at $n = 2, 3, 4$; T8 (identical $\Tin$)
passes; the structural bounds of D8 hold.
*Phase 2 must precede Phase 3 — the certificate is the ripple-carry's
correctness test, and it is the only reason a bespoke arithmetic encoding is safe
to attempt unattended.*

**Phase 4 — the sweep, the extractor column, and the committed report.**
*Workflow: sequential.* Adds the D9 extractor-cost column to `ltlf-ek-bench` and
runs the sweep.
**Green checkpoint:** a committed report under `docs/runs/` with the three arms,
both goals, EK and no-knowledge columns, the `ltlfsynt` timing column, the
extractor column, and the structural table — plus an explicit statement of
whether the separation appeared (Stop-list 2).

**Sweep parameters** (all overridable by the shipped `ltlf-ek-bench` flags):
$n = 2 \ldots 6$ (so $N = 4, 8, 16, 32, 64$ and $\lvert\mathrm{DFA}\rvert = 4^n =
16 \ldots 4096$), `--repeat=5` reported as the **median** (matching the probe's
protocol so its numbers stay comparable), per-case timeout **60 s**,
`--budget=7200` (the runner's shipped default).

## Edge cases

- **$n < 2$** — `instantiate()` rejects rather than emitting a malformed formula.
  $n = 1$ is the degenerate centre-is-start case (D2).
- **The empty word** — always disagrees; reported on its own line, never folded
  into the verdict (D5 of the API, Stop-list 5).
- **`X[!]` instead of weak `X`** — collapses $A_N$ to `false` *silently*, and
  `false` $\to \gamma$ is valid so every verdict returns `REALIZABLE`. Guarded by
  a satisfiability test (T7).
- **Unrealizable cases (centre)** — no controller, so `controller_states` and the
  `goal_*` metrics are **absent, never zero** (B2 rule 1).
- **One-hot at large $n$** — $2 \cdot 2^n$ position APs; $n = 6$ is 128 APs and
  may time out. Record the row, continue the other arms (Stop-list 8).
- **A `BenchCase` without `psi_in`** — the `-nk` subjects skip cleanly, recording
  nothing (not a zero row), so the five landed families are unaffected.
- **MONA absent** — existing skip semantics, unchanged.
- **Trivial $\Tout$** — total and single-state throughout; $\Oknown = \emptyset$,
  so no family here exercises the $\Tout$ side. The certificate's `role`
  generality is nonetheless implemented and tested, because it is what closes B3
  for the landed families.

## Test oracles (for `/test-writer`)

- **T1 — certificate green.** `produced_trace_equivalent(t_in, psi_in, vars,
  Role::t_in)` reports `equivalent_on_nonempty` for all three arms, both goals,
  $n = 2, 3, 4$.
- **T2 — cross-method agreement.** The five methods agree with each other and
  with `expected_realizable` on every case at $n = 2, 3$. Backed by measurement,
  not assumption: `ltlfsynt --semantics=Mealy --realizability` on
  $A_N \to \gamma_N$ gives **corner `REALIZABLE` / centre `UNREALIZABLE` at both
  $N = 4$ and $N = 8$** (run 2026-08-19, binary arm).
- **T3 — `ltlfsynt` T1 race agreement.** Existing machinery;
  `verdict_mismatch_count: 0`.
- **T4 — metamorphic round-trip.** `verify_controller` accepts the controller
  synthesized for every realizable (corner) case.
- **T5 — structural, exact.** $\lvert\Tin\rvert = 4^n$; conjunct counts $14N+1$ /
  $15$; EK `goal_mtdfa_roots` $= 1$.
- **T6 — negative control for the certificate.** A deliberately-wrong $A_N$ must
  be reported **not** equivalent, with a witness. Use two mutants, both
  *satisfiable* (this is the weakness the *Faithfulness guard*'s $\Tout$ side has
  and this API must not inherit): (a) drop the `slip` case from one rule, so
  $A_N$ is too **weak**; (b) replace $\mathrm{Keep}$ at a wall with
  $\mathrm{Inc}$, so $A_N$ is too **strong**. Without T6 the certificate proves
  nothing.
- **T7 — $A_N$ satisfiable.** Guards the silent `X[!]` collapse (D6).
- **T8 — shared $\Tin$.** `slippery-binary` and `slippery-binary-compact` produce
  the **identical** transducer at every $n$ (compare printed form). This is what
  makes the compactness contrast a controlled one.
- **T9 — structural, bounded.** The D8 inequalities, in particular arm 3's
  $\lvert\mathrm{DFA}(A_N)\rvert \ge 4^n$ against a constant conjunct count.

## Open theory questions touched

- **The domain-framing lemma is missing, and this PRD depends on it.**
  `\cref{lem:indep-transducer}` proves equirealizability for the formula the
  transducer was *extracted from*. This PRD reuses **one** $\Tin$ across **two**
  tasks ($\gamma_{\text{corner}}$, $\gamma_{\text{centre}}$) and asserts the
  reduction is sound for both — i.e. the version where the transducer is
  extracted from $A$ alone and reused across arbitrary $\gamma$. That
  generalization is **not written**. It is scheduled for Thursday night in
  `docs/plans/2026-08-17-week.md`; until then it is Stop-list 6 and no run may
  attempt it. (Note what does *not* rescue it: the certificate proves
  $L(\Tin) \equiv L(A_N)$, a **language** fact, not the equirealizability of the
  two synthesis problems.)
- **`main.tex` states no $\text{LTL}_f$ preliminaries at all**, so the weak-`X`
  reading D6 depends on is unwritten paper-wide (already tracked under *Open
  theory questions* in `docs/GLOSSARY.md`; a `\cl` note is written into
  `latex/main.tex`, unpushed).
- **The aperiodicity / star-free correspondence** behind tier T3 is untouched
  here — every family in this PRD is T1 — but the certificate is the machinery
  that would let a future run *check* a T1 declaration rather than trust it.

## Definition of done

- All four phases landed, each at its green checkpoint.
- `produced_trace_equivalent` in `include/ltlf_ek/`, with T6's negative control
  passing on **satisfiable** mutants.
- The three families and ten subjects registered; the five landed families
  unaffected (621/622 unchanged apart from the certificate now guarding their
  $\psiin$).
- *Produced-trace equivalence* in `docs/GLOSSARY.md` with its 3-column mapping and
  its `Do not call it` line.
- A committed sweep report under `docs/runs/` that states, in one sentence,
  whether the compact arm separated — including if it did not (Stop-list 2).
- The four gates ticked by the skills that perform them.

## Developer comments / PRD disagreements

### Phase 1, 2026-08-20 — three deviations, all in tests

Full write-up in `docs/runs/2026-08-20-edf-phase1.md`; the short form:

1. **D3/D8's "$14N+1$ top-level conjuncts" needs a flatten-aware reading.** The
   generator emits exactly 1 initial-cell conjunct + $14N$ rules as D3
   specifies, but `spot::op::And` is n-ary and flattens, so the initial-cell
   conjunct's own $2n$ (binary) / $2N$ (one-hot) literals become children of the
   same top-level `And` and `formula::size()` reads $14N + 2n$ resp. $14N + 2N$.
   T5 now asserts the count as constructed — exactly $14N$ `G`-rooted rules plus
   a literals-only remainder — which is also the form that stays comparable to
   Phase 3's 14 `G`-rules. **The PRD text is not wrong, only the naive way to
   check it is**; D3's table is left as written.

2. **One measured-infeasible cell.** `NfaProduct` on `slippery-onehot` at
   $n = 3$ takes ~7 min per goal (407 s / 433 s, of which ~200 s is
   determinization) against < 1 s for the other nineteen (method, arm, $n$)
   cells. It terminates; it is not stuck. Excluded by construction from the two
   cross-method tests under Stop-list 8, with the measurement written at the
   exclusion site. **Stop-list 8 arrives at $n = 3$, not $n = 6$** — worth
   knowing before Phase 4 sets its budget.

3. **The pre-existing tier test enumerates the T1 set exactly**, so it grew by
   the two new arms and was renamed
   `ExactlyTheFourTrivialKnowledgeFamilies…` → `ExactlyTheDeclaredT1Families…`.
   Free finding from the same change: the `-nk` subjects now also run against
   the four landed T1 families and **agree with `expected_realizable`
   everywhere** — D7's demotion path validated on families it was not written
   for.

### Phase 4 is blocked by a runner bug this phase exposed

`RunCaseWithTimeout` (`src/ltlf_ek_bench.cpp:283`) bounds a case by **detaching**
its worker thread on a deadline miss. The detached worker is still inside
Spot/MONA at process exit, so **any** timing-out case ends the run in a SIGSEGV
*before* `--out` is written — reproduced deterministically at `--timeout=10` on
the cell above (exit 139, no JSON at all; the same run at `--timeout=3000` exits
0 with a complete report).

That directly contradicts Stop-list 8, which requires a timing-out row to be
recorded and the other arms continued. Phase 4 sweeps $n = 2\ldots6$ at a 60 s
budget and *expects* one-hot to time out, so it cannot run until this is fixed.
**Not fixed here:** the runner belongs to `docs/prd/benchmark-suite.md`, and the
only real fix (a forked child per case, since a blocking Spot call cannot be
cancelled in-thread) is a change to a settled part of the harness — a user's
call, not a day-run's.

### `/code-reviewer` (domain), Phase 1 diff, 2026-08-20 — clean

No must-fix. Checked and clean: the fresh-`bdd_dict` + `register_turn_order_aps`
idiom matches the landed families exactly; `build_nk_case` correctly builds a
*new* dict rather than reusing the domain one (the demoted partition's turn
order differs and `register_ap` is idempotent, so reuse could not repair an
already-fixed level); $\lambda$ commits only position literals and never reads
`slip`, so D1's Moore condition holds by construction; every new identifier is
inside the file's anonymous namespace, so no glossary row is owed. Four
`consider` notes, none acted on:

- The comment at the `slippery_step` block litigates the spec ("that prose is
  wrong" about `scripts/slippery_world.py`'s docstring). That argument belongs
  here in the PRD (D1 already makes it); the source should just cite it.
- `slippery_step` takes `const std::string& cls` but every caller passes a
  `const char*`, so each call builds a temporary; an enum would be both cheaper
  and non-stringly-typed.
- The five `-nk` subjects guard in two different orders — the MONA pair tests
  `c.psi_in` before `build_nk_case`, the other three after. Harmless, uneven.
- The Stop-list 8 exclusion is duplicated across `tests/slippery_world_test.cpp`
  and `tests/bench_suite_test.cpp` (this file's one-suite-per-file norm, but it
  is now a fact stated twice).

**One observation for Phase 2, not a defect.** For the *enumerated* arms both
sides of the certificate — $\Tin$ and $A_N$ — are generated from the same
`slippery_step`, so $L(\Tin) \equiv L(A_N)$ there can only catch encoding and
quantifier bugs, never a wrong step function. That is not a weakness of the
certificate, it is why Phase 3 is the phase that needs it: the compact $A_N$'s
ripple-carry is written independently, and the certificate is its correctness
test. T6's negative control remains the thing that proves the check has teeth.
