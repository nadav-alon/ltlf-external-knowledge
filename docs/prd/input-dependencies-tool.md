# PRD: input-dependency extraction (`ltlf-ek-deps --direction in`)

**Status:** implemented — **both phases landed 2026-08-03.** Phase 1 (shared `detail::` core + `dependent_inputs`) on branch `input-dependencies-tool`; Phase 2 (`--direction` CLI flag, commit `18a63d6`, plus the O1-in/O3-in/O4-in oracles, commit `d42fcbe`, and the fixture repair `5e2f215`) integrated on `worktree-indeps-phase2` — CMake needed no changes for the flag (the target and both source files were already wired by Phase 1); the two new test files are registered. Suite **572/572 green**. **All four gates closed** — `code-review` by `/code-reviewer` (domain) plus `/review 6` (generic), neither raising a must-fix. Status is `implemented`; every DoD bullet is met, including the measured $\Xdep\neq\emptyset$ rate.
**Interface:** new library entry `dependent_inputs` + a `--direction in|out` flag on the existing `ltlf-ek-deps`; does **not** implement `Synthesis`
**Recommended workflow:** concurrent — the *Interfaces & types* freeze is **high**: `dependent_inputs` mirrors the landed `dependent_outputs` shape term for term, and the only genuinely new logic is one `bdd_exist` plus a `spot::formula::Not`.
**main.tex ref:** `\cref{indep}` — `\cref{def:indep}`, `\cref{lem:indep-diagonal}`, `\cref{lem:indep-transducer}` (all written this session, both lemmas **unproved**)

**Gates:**
- [x] glossary        — new terms in docs/GLOSSARY.md C++ column
      _Closed 2026-08-03 (`/glossary`, user-attended). All three new terms
      written — *Dependent input set*, *Violation automaton*, *Projected
      live-letter region* — plus the new *Input-dependency extraction* sibling
      and all three amendments. Decisions taken with the user: $\Aneg$ is the
      **Violation automaton** (the lemma states it generically as "a
      deterministic automaton", so the DFA commitment lives in the C++ column,
      not the name); and *Dependency set* stays **one** entry carrying both
      instantiations ($(\mathcal{I}\cup\mathcal{O})\setminus\Xdep$ for outputs,
      $\mathcal{I}\setminus\Xdep$ for inputs), following the *Observed / produced
      slice* precedent — which also keeps the "conflating them is the Moore bug"
      warning stated as a relation between the two, where the danger is._
- [x] tests           — unit + oracle coverage
      _Closed 2026-08-03 by the unattended Phase 2 run (`/test-writer`, commit
      `d42fcbe`; fixture repair `5e2f215`). All four Phase 2 oracles are written
      and executing: **O1-in** (`tests/ltlf_ek_deps_input_test.cpp`,
      `LtlfEkDepsInputOracleTest.GeneratedCorpusEquirealizableAgainstBaselineAndLtlfsynt`
      + the I6 totality witness), **O3-in** (`LtlfEkDepsInputPartFile.*`,
      including I12's commutation), **O4-in**
      (`tests/dependent_inputs_controller_test.cpp`, four hand fixtures + an I6
      companion + a fixed-seed 6000-case generated-corpus sweep), alongside
      Phase 1's U1-in–U6-in and O5-in. Suite **572/572, 0 failed**;
      `tests/dependent_outputs_test.cpp` and `tests/ltlf_ek_deps_test.cpp` still
      pass with their pre-existing content unedited, so the extraction's
      regression bar holds at Phase 2 too. **Measured non-empty-$\Xdep$ rate
      (DoD): 7/150 = 4.7%**, from O1-in's corpus pre-filter — below the output
      tool's 12/150 (8%) and only just clearing the 5%-style floor (integer
      `150/20 = 7`), i.e. it passes with **thin margin**; see disagreement 6.
      One coverage gap is recorded as disagreement 5 rather than papered over:
      I12's transducer-equality clause exercises $\Tout$ but not $\Tin$._
      _Superseded gate notes from earlier passes:_
      _Phase 1 done, gate deliberately left OPEN: `/test-writer` landed
      `tests/dependent_inputs_test.cpp` (U1-in–U6-in, O5-in and the six
      library-level edge cases) and the suite is 556/556 green with
      `tests/dependent_outputs_test.cpp` and `tests/ltlf_ek_deps_test.cpp`
      **unedited** — the extraction's regression bar is met. The gate closes only
      when Phase 2's O1-in, O3-in and O4-in are also written._
      _Phase 2 (`/developer`, commit `18a63d6`) added only small CLI-level
      unit tests for the `--direction` flag's own plumbing (default-is-out,
      an invalid value, the "inputs" noun on stdout, no-transducer-on-empty-
      Xdep) to `tests/ltlf_ek_deps_test.cpp` — suite now 560/560, still with
      `tests/dependent_outputs_test.cpp` and `tests/dependent_inputs_test.cpp`
      **unedited**. O1-in, O3-in and O4-in (the semantic oracles this gate is
      actually waiting on) are `/test-writer`'s in-flight concurrent branch,
      not written here._
- [x] code-review     — domain (/code-reviewer) + generic (/review on the PR)
      _Closed 2026-08-03 by the unattended Phase 2 run, **both halves actually
      run**. Domain: `/code-reviewer` on `3eadb89..worktree-indeps-phase2` —
      **no must-fix**; no new public identifier names a domain concept
      (`direction`/`is_in`/`noun` are anonymous-namespace CLI locals), both
      transducer parses in the commutation test share one `spot::bdd_dict` so
      the asserted BDD equality is meaningful, no Spot machinery reinvented,
      `--direction out` byte-identical by construction. `theory-reviewer` was
      deliberately not spawned: the phase changed CLI orchestration and tests
      only, `detail::dependency_core` is untouched, and Phase 1 closed that
      gate. Generic: `/review 6` on PR #6 (`master...input-dependencies-tool`,
      +2494/−290, 17 files) — **no must-fix**; two "consider" items recorded as
      disagreements 7 and 8. `latex/` was never touched, so no `main.tex:NNN`
      citation drift._
- [x] theory-review   — code ↔ math faithfulness vs main.tex
      _Closed 2026-08-03 (`/theory-review`, faithfulness mode, unattended, on
      `master...worktree-indeps-phase1`). **No `code-bug`.** I1–I11 confirmed in
      code with anchors; `dependent_outputs` behaviourally unchanged by the
      extraction. Two `underspecified` notes drafted into `docs/BACKLOG.md`
      (`latex/` is an uninitialized submodule in a worktree, so they are not in
      `main.tex` yet). The reviewer additionally **resolved** the open question
      "does $\lambda_{in}$ stay valid when $\Tout$ is later replaced?"
      affirmatively — see *Findings deferred* in
      `docs/runs/2026-08-03-input-dependencies-phase1.md`._

## Goal

Extract the **environment**'s forced moves from $\varphi$ and materialise them as
a $\Tin$, the way `docs/prd/output-dependencies-tool.md` already extracts the
**system**'s forced moves as a $\Tout$. Where that tool answers "which outputs
must the system play, or lose?", this one answers "which inputs must the
environment play, or lose?" — and the answer is external knowledge inherent to
$\varphi$ in exactly the sense of `main.tex:59-62`, so `ltlf-ek-synth` can be fed
a derived $\Tin$ instead of a hand-authored fixture. Every method's benchmarks
and oracles currently run against a trivial or hand-written $\Tin$; this closes
that gap on the input side.

Supersedes nothing. It **answers** the *Open theory questions* item "Input
dependencies need a different notion (I8)" of
`docs/prd/output-dependencies-tool.md`, which correctly said the output notion
does not transfer, but located the difference only in $\Ydep$. There is a second
difference, and it is the load-bearing one: the analysed language is
$L(\lnot\varphi)$, not $L(\varphi)$. Adjacent:
`docs/prd/output-dependencies-tool.md` (the tool being extended, and the source
of every piece of plumbing reused here), `docs/prd/transducer-file-format.md`
(the format emitted), `docs/prd/cli-wrapper.md` (`--known-input-transducer`),
`docs/prd/ltlfsynt-oracle.md` (the external oracle reused by O1-in).

## Ubiquitous-language terms used

Existing glossary terms: *Goal formula* ($\varphi$), *Inputs / Outputs*, *Free
inputs / Known inputs / Free outputs / Known outputs*
($\Ifree,\Iknown,\Ofree,\Oknown$), *Turn order*, *Governed variables (V)*,
*Closed universe of APs*, *External knowledge strategy* ($\Tin$), *Transducer*,
*Output-labeled transducer*, *Role* (`t_in`), *Observed / produced slice*
($\Sigma_0=\Ifree$, $\Sigma_1=\Iknown$), *Transducer file format (%%LAMBDA
block)*, *Parse a transducer*, *Print a transducer*, *Output function (lambda)*,
*Transition function (delta)*, *Letter*, *Cube*, *Goal DFA construction*
(`ltlf_to_dfa`), *NFA / DFA for the Goal*, *Consistency (cons)* and its
partiality clause, *Determinacy witness* (`undetermined_variable`, reused
verbatim), *Live-letter region* ($\liveset{s}$), *Dependent output set*,
*Dependency set*, *Output-dependency extraction*, *Controller verifier*,
*Generated corpus*, *Faithfulness guard*.

**Glossary gaps — DONE 2026-08-03, do not re-open.** All six landed; the launch
gate's naming requirement is satisfied and `/developer` will not stop on it. Kept
below as the record of what was written and why. Three new concepts and three
amendments to existing entries:

1. **Dependent input set** (new) — $\Xdep\subseteq\mathcal{I}$, `\cref{def:indep}`.
   C++: the `dependent` member of `DependentInputs`. Do not call it: the
   dependent variables (bare — ambiguous now that both directions exist), the
   known inputs (that is $\Iknown$, a *partition* notion), input dependencies
   (bare).
2. **Violation automaton** (new) — $\Aneg$, `\cref{lem:indep-diagonal}`: the
   deterministic automaton with $L(\Aneg)=L(\lnot\varphi)$, on which *live*
   means the **environment** can still win. C++: `ltlf_to_dfa(spot::formula::Not(phi), dict)`
   — no dedicated constructor. Do not call it: the negated DFA (that names the
   construction, not the concept), the complement automaton (it is built by
   translation, not by complementing $A$ — see I2), the environment automaton.
3. **Projected live-letter region** (new) — $\liveproj{s}$,
   `\cref{lem:indep-diagonal}`: $\liveset{s}$ with $\mathcal{O}$ existentially
   projected away, i.e. the letters-over-$\mathcal{I}$ the environment may still
   play. C++: file-local to the shared core. Do not call it: $\liveset{s}$ /
   `live_region` (that is the unprojected region over $\mathcal{I}\cup\mathcal{O}$
   — conflating them is exactly the Moore bug), the input region.
4. **Amend *Dependency set*** — its current text says the $\Ydep=\Sigma_0$
   coincidence is "emphatically *not* a coincidence for `t_in`". That was right
   about *this* $\Ydep$ and is now misleading: `\cref{def:indep}` defines a
   **different** $\Ydep$ ($\mathcal{I}\setminus\Xdep$) which *does* equal
   $\Sigma_0$ for `t_in`, exactly. The entry should carry both.
5. **Amend *Dependent output set*** — its "Do not call it" bans "dependent
   variables (bare — the term is **output**-specific; the input notion is
   strictly stronger and unbuilt)". The input notion is now built, and it is
   **not** a strengthening: it is a different language plus a projection (I2, I3).
6. **Amend *Output-dependency extraction*** — it should gain a sibling
   *Input-dependency extraction* entry and a cross-reference, and its "a future
   input-dependency tool composes on disjoint keys" sentence becomes a statement
   about a tool that exists (I12).

Also new but **infrastructure, not domain** (no glossary entry, per the
`bench.hpp` / `cli.hpp` precedent): the `detail::` dependency core, the
`--direction` argument plumbing, and the shared `dependency_types.hpp` header.

## Behaviour / semantics (from main.tex)

All three anchors were written into `latex/main.tex` this session (§`indep`),
with the prose under `\cl` notes and both lemmas **explicitly unproved**, in the
same style §`outdep` already uses.

**I1 — the dependency criterion (`\cref{lem:indep-diagonal}`).** With $\Aneg$ the
*Violation automaton*, $\Xdep\subseteq\mathcal{I}$ is input-dependent **iff** for
every **reachable live** state $s$ of $\Aneg$, no two $u,u'\in\liveproj{s}$ have
$u\cap\Ydep = u'\cap\Ydep$ and $u\cap\Xdep \neq u'\cap\Xdep$ — equivalently,
$\liveproj{s}$ read as a relation $\Ydep\to\Xdep$ is **functional**. Determinism
collapses the compatible pairs to the diagonal exactly as in
`\cref{lem:outdep-diagonal}`, so the check is per-state and linear in $|\Aneg|$
with no pair search.

**I2 — the analysed automaton is $\Aneg$, not $A_\varphi$, and that is the whole
polarity of the tool.** *Live* on $\Aneg$ means an accepting state of $\Aneg$ is
reachable, i.e. the **environment** can still force a violation of $\varphi$; a
letter outside $\liveset{s}$ is one the **environment** loses by playing. This is
the precise dual of the output tool, whose $\liveset{s}$ on $A_\varphi$ collects
the letters the **system** does not lose by playing. Build it by translating the
negation — `ltlf_to_dfa(spot::formula::Not(phi), dict)` — **not** by flipping
acceptance on $A_\varphi$: `ltlf_to_dfa` returns a complete DFA whose acceptance
also encodes the empty/length-0 convention (`main.tex` has no $\text{LTL}_f$
preliminaries, see *Open theory questions*), so an acceptance flip is an
untested equivalence, not a free complement. Everything downstream of the build
is formula-agnostic and shared.

**I3 — the projection is the Moore restriction, and it is $\exists$ not
$\forall$.** $\Sin$ moves **before** the controller (`main.tex:88`), so
$\Sigma_0=\Ifree$ and $\lambda_{in}$ may not observe the current step's
$\mathcal{O}$ — unlike $\Sout$, which moves last and observes everything, which
is why the output tool needs no projection at all. So the functionality test runs
on $\liveproj{s} = \exists\mathcal{O}.\liveset{s}$, a relation over $\mathcal{I}$
alone. Symbolically one operation:
`R_proj = bdd_exist(live_region_s, output_cube)`, after which
`undetermined_variable(R_proj, Xdep, Xdep_cube, aut)` applies **verbatim** — its
documented precondition is a relation over (observed ∪ produced) only, and
$R_{\rm proj}$ ranges over $\Ifree\cup\Xdep = \mathcal{I}$ exactly.

The quantifier is **not** interchangeable. Under $\forall$, a value of $\Xdep$
would count as available only when *every* output completion keeps the
environment alive; at a state whose live letters are
$(a\wedge b\wedge x)\vee(a\wedge\lnot b\wedge\lnot x)$ (with $a,b\in\mathcal{I}$,
$x\in\mathcal{O}$) the environment has two genuine options for $b$, neither
survives both values of $x$, so $\forall$ yields an **empty** admissible set,
passes the at-most-one test vacuously, and reports $\{b\}$ dependent. The
emitted $\lambda_{in}$ would then default there and forbid the environment a move
plain synthesis grants it. Required test fixture: a state with this region shape
must report $\{b\}$ **not** dependent.

**I4 — liveness is the same backward BFS, reused not reimplemented.** `live(s)` =
some accepting state of $\Aneg$ is reachable from $s$, **including $s$ itself**;
computed by backward BFS from `state_is_accepting` over reversed edges, **skipping
`bddfalse` edges** (an unsatisfiable guard can never be taken, so it must not
propagate liveness — the bug fixed by `9f8d295`). `spot::purge_dead_states` must
**not** be used, for the same Büchi-vs-finite reason the output tool documents.
Reflexivity is load-bearing and has the same consequence: $\liveproj{s}$ may be
**empty at a live $s$** (a terminal accepting state of $\Aneg$), which is legal
and carries no constraint. The strongest true invariant remains the disjunction
*live **non-accepting** ⇒ has a live successor*. This is not re-derived here — it
is the shared `detail::` core, called by both directions.

**I5 — analysis on the pruned view, emission on the complete $\Aneg$
(`\cref{lem:indep-transducer}`).** Liveness prunes only the *analysis*. The
emitted $\Tin$ uses $\delta_{in} = \delta_{\Aneg}$ of the **complete** automaton
(free — `ltlf_to_dfa` already calls `spot::complete_here`; do **not** purge
before emitting), and $\lambda_{in}(s) = \liveproj{s}$ **totalised**: on
$\Ifree$-assignments $\liveproj{s}$ does not cover, commit a fixed default
$\Xdep$-cube.

**I6 — totality is a soundness requirement, and it fails in the *opposite*
direction from the output tool.** This is the invariant most likely to be got
wrong by analogy. For $\Tout$, a partial $\lambda$ deleted letters and thereby
constrained the **environment**, taking away moves it was *winning* with. Here,
an $\Ifree$-observation uncovered by $\liveproj{s}$ is one on which the
environment has **already lost** — so a partial $\lambda_{in}$ deletes exactly
the environment's *losing* moves and hands it a strictly stronger position than
plain synthesis grants it, turning a **realizable $\varphi$ into an apparently
unrealizable one**. The sharpest case is a **non-live** $s$, where
$\liveproj{s}=\emptyset$ and *every* observation is uncovered: a partial $\Tin$
would leave no consistent letter at all and stop the trace at the exact moment
the environment loses, instead of letting the system finish winning. Since every
$\varphi$ that is realizable at all has reachable non-live states in $\Aneg$,
this is not a corner case — it is the common path. Required test fixture: U4-in.

**I7 — the default cube is pinned.** `default_X` = the **all-negative** cube over
$\Xdep$, matching the output tool so the two directions are reproducible under
one rule. Symbolically:
`lambda_s = R_proj | (!bdd_exist(R_proj, X_cube) & default_X)`.
Any fixed choice is sound (`\cref{lem:indep-transducer}`: the defaulted
observations are ones on which no continuation violates $\varphi$), but it must
be *fixed*, or the emitted file is not reproducible and the round-trip oracle
becomes flaky.

**I8 — greedy accumulation, lexicographic order, over $\mathcal{I}$.**
$\Xdep\gets\emptyset$; for each $z\in\mathcal{I}$ in `std::set<std::string>`
order, if $\Xdep\cup\{z\}$ is input-dependent then $\Xdep\gets\Xdep\cup\{z\}$.
The test **must** use the accumulated $\Xdep$; singleton-union is unsound.
Witness, and a required test fixture: $\varphi = F(a\oplus b)$,
$\mathcal{I}=\{a,b\}$, $\mathcal{O}=\{x\}$ — the violating language is that of
$G(a\leftrightarrow b)$, so each of $\{a\}$ and $\{b\}$ is input-dependent but
$\{a,b\}$ is **not** (both $a=b=\top$ and $a=b=\bot$ keep the environment alive).
Result is subset-**maximal**, not maximum; lexicographic order therefore picks
*which* maximal set is returned, and **changing the order later is a PRD-change
event**. As in the output tool, $\Aneg$, the live set and every $\liveproj{s}$ are
**independent of $\Xdep$** and are computed **once**, before the greedy loop.

**I9 — part-file co-management, dual ownership.** With `--direction in` the tool
**owns** `input_free` / `input_known` (it repartitions $\mathcal{I}$ into
$\Ifree\uplus\Xdep$) and **passes `output_free` / `output_known` through
verbatim**. It **refuses** a non-empty `input_known` on input
(`std::invalid_argument` / usage error), for the dual of the output tool's
reason: `main.tex:126` has exactly one $\lambda_{in}$ producing all of $\Iknown$, so
there is no "compose two $\Tin$s" notion.

**I10 — a $\Tout$ present on input does not refine the analysis.** If
`output_known` is non-empty the analysis still runs on $\Aneg$ alone, ignoring any
$\Tout$. **Sound but incomplete**, the dual of the output tool's I10: a $\Tout$
restricts the system, shrinking the trace set, so *more* inputs could be
dependent; and a $\lambda_{in}$ correct on $L(\lnot\varphi)$ stays correct on any
subset of it, because restricting the system only makes violation *harder* for
the environment, so every env-dead state stays dead. That last clause is an
argument, not a proof — **flagged for `/theory-review`**.

**I11 — the degenerate case is $\varphi$ **valid**, not $\varphi$
unsatisfiable.** The output tool exits 3 when $L(\varphi)=\emptyset$, because
then every candidate is vacuously dependent. The mirror here is
$L(\lnot\varphi)=\emptyset$, i.e. $\varphi$ is a **tautology**: the environment
can never win, every state of $\Aneg$ is dead, and the greedy loop would
confidently return $\Xdep=\mathcal{I}$. The shared core's check is one and the
same — *the initial state of the automaton it analyses is not live* — so
`UnsatisfiableFormula` is literally accurate in both directions ($\lnot\varphi$
really is unsatisfiable) and exit 3 needs no new dispatch; only the CLI message
names the direction. Symmetrically, each direction passes the **other**
degeneracy through harmlessly: an unsatisfiable $\varphi$ makes every state of
$\Aneg$ live, so `--direction in` simply reports nothing dependent, and is **not**
an error.

**I12 — the two directions commute.** Running `ltlf-ek-deps` twice, in either
order, yields the same final part file and the same pair of transducers. The keys
are disjoint (I9 and the output tool's I9), and neither analysis can see the
other's result: `--direction in` projects **all** of $\mathcal{O}$ away, so a
non-empty `output_known` cannot affect it; and `--direction out`'s
$\Ydep=\mathcal{I}\cup\Ofree$ legally **includes** $\Iknown$, since $\Sout$ moves
after $\Sin$ in the turn order, so a non-empty `input_known` changes nothing it is
allowed to observe. This is the composition the output tool's I9 was designed for,
now exercised. Required test fixture: O3-in.

## Interfaces & types

**Freeze confidence: high.** `dependent_inputs` mirrors the landed, gated
`dependent_outputs` term for term; `DependentInputs` is `DependentOutputs` with
one member renamed; the shared core is an extraction of code that already exists
and is covered by 480 passing tests. The only new logic is one `bdd_exist` (I3)
and one `spot::formula::Not` (I2).

### Phase 1 — shared core + the analysis

The body of `src/dependent_outputs.cpp` moves into an internal core. **The
landed public surface does not change**: `dependent_outputs`, `DependentOutputs`,
`CandidateObserver` and `UnsatisfiableFormula` keep their exact signatures,
member names and header, so no existing call site or test is touched and
`docs/prd/output-dependencies-tool.md`'s frozen block needs no amendment.

```cpp
// include/ltlf_ek/dependency_types.hpp  (new; the two public types both
// directions share, lifted verbatim out of dependent_outputs.hpp, which now
// includes this header so its own surface is unchanged)

// Thrown when the language the analysis runs on is empty --- L(phi) for the
// output direction, L(!phi) (i.e. phi valid) for the input direction.  IS-A
// std::invalid_argument, so callers catching the base are unaffected.
struct UnsatisfiableFormula : std::invalid_argument {
  using std::invalid_argument::invalid_argument;
};

// Optional narration hook for the greedy loop: called once per candidate in
// lexicographic order, with the determinacy witness when rejected and nullopt
// when accepted.  Purely observational.
using CandidateObserver = std::function<void(
    const std::string& z, bool accepted,
    const std::optional<std::string>& undetermined)>;
```

```cpp
// include/ltlf_ek/detail/dependency_core.hpp  (new, internal --- no glossary
// entry, per the bench.hpp precedent)

namespace ltlf_ek::detail {

// The direction-neutral result; the public structs are thin renames of it.
struct DependencyResult {
  std::set<std::string> dependent;
  VariablePartition partition;
  std::optional<OutputLabeledTransducer> transducer;
};

// The whole of \cref{lem:outdep-diagonal} / \cref{lem:indep-diagonal} and their
// transducer lemmas, once.  `role` alone selects all four direction-dependent
// axes, and they are NOT independent knobs --- a struct of four booleans would
// make invalid combinations representable, so the core derives them:
//
//   role      analysed formula   scanned set        projected   emitted Role
//   t_out     phi                partition.output_free   no     t_out
//   t_in      Not(phi)           partition.input_free    yes     t_in
//
// Throws std::invalid_argument if the role's own known-set is non-empty on
// input (I9) or an AP of `phi` lies outside partition.universe(); throws
// UnsatisfiableFormula if the analysed automaton's initial state is not live
// (I11).  Role::t_c throws std::invalid_argument.
DependencyResult run_dependency_analysis(
    const spot::formula& phi, const VariablePartition& partition, Role role,
    const spot::bdd_dict_ptr& dict, const CandidateObserver& on_candidate);

}  // namespace ltlf_ek::detail
```

```cpp
// include/ltlf_ek/dependent_inputs.hpp  (new)

// Result of the maximally-input-dependent search (\cref{def:indep}).
struct DependentInputs {
  // Xdep: the maximally input-dependent set, lexicographic-greedy (I8).
  std::set<std::string> dependent;
  // `partition` with `dependent` moved input_free -> input_known, and
  // output_free / output_known passed through verbatim (I9).
  VariablePartition partition;
  // The Tin of \cref{lem:indep-transducer}: delta = the COMPLETE violation
  // automaton, lambda = the totalised projected live-letter region (I5, I7),
  // Role::t_in slices from `partition`.  nullopt iff `dependent` is empty ---
  // there is no Tin to build when Iknown is empty (see Edge cases).
  std::optional<OutputLabeledTransducer> t_in;
};

// Find a maximally input-dependent set of `phi` and materialise it as external
// knowledge.  Built on the shared `dict` (\cref{lem:indep-diagonal},
// \cref{lem:indep-transducer}; docs/prd/input-dependencies-tool.md).
//
// Throws std::invalid_argument if `partition.input_known` is non-empty (I9) or
// if an AP of `phi` lies outside `partition.universe()`, and
// UnsatisfiableFormula if `phi` is VALID (I11).
DependentInputs dependent_inputs(const spot::formula& phi,
                                 const VariablePartition& partition,
                                 const spot::bdd_dict_ptr& dict,
                                 const CandidateObserver& on_candidate = {});
```

### Phase 2 — the CLI flag

No new library entry point. One new flag on `ltlf-ek-deps`:

| flag | meaning |
|---|---|
| `--direction in\|out` | which dependency to extract; **default `out`**, so every existing invocation is unchanged |

Everything else is reused unmodified: `--formula`, `--part-file`, `--inputs` /
`--outputs`, `--emit-part`, `--transducer`, `--verbose`, the exit codes, the
three resolved-path guards, and `PendingArtifact` / `CommitArtifacts` /
`RemoveStaleTransducer` with the part file as keystone. `--direction` selects
which library entry runs, which two part-file keys are owned, and the `Role`
written into the emitted transducer.

stdout keeps its one-line contract, with the noun following the direction:
`dependent inputs: a   (of a, b)`, or `dependent inputs: none`. Exit codes are
unchanged and mean the same thing in both directions: `0` success (including
$\Xdep=\emptyset$), `2` usage error, `3` the analysed language is empty
($\varphi$ unsatisfiable for `out`, $\varphi$ valid for `in`), `1` internal/IO.

**If implementation proves this contract wrong:** that is a PRD-change event —
update this section and propagate to any in-flight test branch; the developer
does not silently re-shape the interface on its own branch.

## Implementation phases

- **Phase 1 — the shared core and `dependent_inputs`.** Extract
  `compute_live`, `compute_live_regions`, the greedy loop, the totalisation and
  the unsatisfiable check from `src/dependent_outputs.cpp` into
  `src/detail/dependency_core.cpp`; reduce `dependent_outputs` to a delegation
  that rebuilds its own struct; lift `CandidateObserver` / `UnsatisfiableFormula`
  into `dependency_types.hpp`; add `dependent_inputs` with I2's negation and I3's
  projection. **Green checkpoint:** compiles; the existing suite passes
  **unchanged and unedited** (this is the regression bar for the extraction —
  if a test needed editing, the public surface moved and that is a PRD-change
  event); U1-in–U5-in and O5-in pass. Stubs the CLI — library-only.
- **Phase 2 — the `--direction` flag.** Flag parsing, direction-dependent key
  ownership and stdout noun, the exit-3 message, CMake unchanged (no new
  target). **Green checkpoint:** compiles; O1-in passes against `ltlfsynt`, and
  O3-in's part-file pass-through, refusal and commutation cases pass.

Each phase leaves the tree compiling and independently testable.

## Edge cases

- **$\Xdep=\emptyset$** (nothing input-dependent): exit `0`, write the part file
  (`input_known:` empty), and **write no transducer file** —
  `src/ltlf_ek_synth.cpp:205` *rejects* `--known-input-transducer` when
  `input_known` is empty ("ambiguous"), so emitting one would produce a file that
  breaks the pipeline it feeds. `t_in` is `nullopt`; stdout says
  `dependent inputs: none`.
- **$\varphi$ valid** ($L(\lnot\varphi)=\emptyset$, the initial state of $\Aneg$
  is not live): every $\Xdep$ is vacuously input-dependent, so the greedy loop
  would return $\mathcal{I}$. Detect and exit `3` with a message naming the
  direction, writing **no** artifacts. `dependent_inputs` throws
  `UnsatisfiableFormula` (I11).
- **$\varphi$ unsatisfiable, under `--direction in`**: **not** an error. Every
  state of $\Aneg$ is live, so the analysis runs normally and typically reports
  nothing dependent. Worth an explicit test, since the reflex from the output
  tool is to expect exit 3 here.
- **$\mathcal{I}=\emptyset$**: the greedy loop is empty, $\Xdep=\emptyset$ — same
  as the first case, not an error.
- **$\mathcal{O}=\emptyset$**: legal, and the regime where I3's projection is a
  no-op — this is exactly O5-in's hypothesis, not a degenerate corner.
- **Non-empty `input_known` on input**: refuse (I9), exit `2`.
- **Non-empty `output_known` on input**: legal — passed through verbatim and
  ignored by the analysis (I10). Explicit test, so the pass-through does not
  silently regress.
- **`--emit-part` equal to `--part-file`, or any two file flags naming one
  resolved path**: refuse, exit `2` — the existing guard, which must cover the
  `--direction in` invocation too (the hazard is a property of the path, not of
  the flag that lands on it — the second-round review finding).
- **An AP of $\varphi$ outside `universe()`**: `std::invalid_argument`, exit `2`,
  per the closed-universe rule — and it must stay exit `2` even when an AP is
  *named* `unsatisfiable` (the `UnsatisfiableFormula` type already guarantees
  this; do not reintroduce message-substring dispatch).
- **A state whose $\liveproj{s}$ is empty** — legal, not impossible, for both the
  reflexivity reason of I4 (an accepting state of $\Aneg$ all of whose successors
  are dead) and the non-live reason of I6. It carries no constraint:
  `undetermined_variable(bddfalse, ...)` correctly reports functional, and I7
  defaults every observation.
- **Every observation defaulted at a non-live state**: dead states are still
  emitted (I5 keeps the complete automaton) and their $\lambda_{in}$ is wholly the
  default cube. Not an error — and by I6 it is the case that *must* be total.

## Test oracles (for /test-writer)

**Unit fixtures (Phase 1).** Each is a hand-verified $\varphi$ over a tiny
partition. Note that $\Aneg$ is the automaton to reason about in every one of
them — hand-derive $L(\lnot\varphi)$ first, then liveness on it.

- **U1-in — dependent, non-vacuous.** $\varphi = F(a\oplus b)$,
  $\mathcal{I}=\{a,b\}$, $\mathcal{O}=\{x\}$ ⇒ $\Xdep=\{a\}$ (lexicographic; see
  U5-in). $L(\lnot\varphi)$ is that of $G(a\leftrightarrow b)$: one live state
  $s$ with $\liveproj{s}=(a\leftrightarrow b)$, functional from $\{b\}$ to
  $\{a\}$; assert $\lambda_{in}(s,b)=a$ and $\lambda_{in}(s,\lnot b)=\lnot a$.
- **U2-in — not dependent.** $\varphi = G(a\rightarrow x)$, $\mathcal{I}=\{a\}$,
  $\mathcal{O}=\{x\}$ ⇒ $\Xdep=\emptyset$: the environment can always still play
  $a$ and hope for $\lnot x$, so both $a$ and $\lnot a$ stay in $\liveproj{s}$
  with an empty $\Ydep$ to separate them. Also asserts `t_in == nullopt`.
- **U3-in — the $\exists$/$\forall$ linchpin (I3).**
  $\varphi = F(\lnot a \vee (b \oplus x))$, $\mathcal{I}=\{a,b\}$,
  $\mathcal{O}=\{x\}$ ⇒ $\Xdep=\{a\}$. Hand-derivation: $\lnot\varphi$ is
  $G(a\wedge(b\leftrightarrow x))$, so $\Aneg$ has one live accepting state $s$
  with self-loop guard $\liveset{s} = a\wedge(b\leftrightarrow x)$ and a dead
  sink. Projecting, $\liveproj{s} = \exists x.\,a\wedge(b\leftrightarrow x) = a$,
  with $b$ free — so $\{a\}$ is functional from $\{b\}$ and accepted, while
  $\{a,b\}$ has $\Ydep=\emptyset$ and two points differing on $b$, and is
  rejected. A $\forall$-projecting implementation instead computes
  $\forall x.\,a\wedge(b\leftrightarrow x) = \mathtt{ff}$, which is vacuously
  functional for **every** candidate, so it accepts $\{a\}$ and then $\{a,b\}$
  and returns $\{a,b\}$. This is the **only** fixture that separates the two
  readings — U1-in's $\liveset{s}$ does not mention $x$, so $\exists$ and
  $\forall$ agree there — and it is therefore required, not optional.
- **U4-in — totality (I6).** Reuse U1-in and assert $\lambda_{in}$ is **defined**
  (the default cube, not `nullopt`) at the **dead sink** of $\Aneg$ — the state
  reached by any letter with $a\neq b$. A partial implementation returns
  `nullopt` and fails here. Assert on `t_in->lambda(s_dead, v)` for every letter.
- **U5-in — order determinism (I8).** U1-in's $\varphi$ has **two** distinct
  maximal input-dependent sets, `{a}` and `{b}`. Assert the lexicographic one,
  `{a}`, is returned, and that it is returned on repeated calls in one process.
- **U6-in — singleton-union is unsound (I8).** Same $\varphi$; assert $\Xdep$ is a
  **singleton**, not `{a,b}`. A singleton-union implementation returns `{a,b}`.

**Duality oracle (Phase 1) — O5-in.** When $\mathcal{O}=\emptyset$ the projection
of I3 is a no-op and the input notion collapses onto the output notion exactly.
For a family of $\varphi$ over inputs only, assert

```
dependent_inputs (phi,      inputs = I, outputs = {}).dependent
  ==
dependent_outputs(Not(phi), inputs = {}, outputs = I).dependent
```

and that the two emitted transducers agree on state count, initial state and the
$\lambda$ relation per state (BDD equality; the `Role` differs, so
$\Sigma_0/\Sigma_1$ differ, but the relation does not). State numbering is
comparable because both sides build the **identical** automaton from the
identical formula via one `ltlf_to_dfa` call; if that ever ceases to hold,
compare through `print_transducer`/`parse_transducer` instead of by index. Zero
new machinery — both entry points already exist —
and it pins the "input dependencies are output dependencies on $\Aneg$ with the
roles flipped" identity directly rather than by prose. Include at least one
$\varphi$ from U1-in/U2-in restricted to inputs.

**End-to-end equirealizability oracle (Phase 2) — O1-in, the linchpin.** From
`\cref{lem:indep-transducer}`: for a corpus of $\varphi$, the verdict of
`ltlf-ek-synth --formula φ --part-file <emitted> --known-input-transducer <emitted>`
must equal the verdict of `ltlf-ek-synth --formula φ --inputs … --outputs …` with
no external knowledge, **and** equal Spot's `ltlfsynt` on $\varphi$ (reusing
`docs/prd/ltlfsynt-oracle.md`'s harness). This is the test that catches I6 — with
a partial $\Tin$ a realizable $\varphi$ flips to unrealizable and the oracle
fails. Run it over the existing *Generated corpus* formulas restricted to cases
where $\Xdep\neq\emptyset$, and **assert a non-trivial fraction of the corpus does
yield a non-empty $\Xdep$**, else the oracle is vacuous. The output tool's
equivalent cleared 12 of 150 against a 5% floor; the input-side rate is unmeasured
and may differ, so measure it and record the number in the PRD rather than
assuming the same floor holds.

**Controller-verifier oracle (Phase 2) — O4-in.** Where the emitted $\Tin$ is
non-trivial and synthesis succeeds, `verify_controller(phi, vars, t_in,
trivial_t_out, t_c)` must accept — the internal linchpin, independent of the
method that produced $t_c$, and the first time the verifier runs against a $\Tin$
that was **derived** rather than hand-authored.

**Part-file co-management oracle (Phase 2) — O3-in.** Feed a part file with a
non-empty `output_known`; assert the emitted part file preserves `output_free`
and `output_known` equivalently (as sets) and repartitions only the two input
keys; assert a non-empty `input_known` input is refused with exit `2`; assert
`--emit-part` equal to `--part-file` is refused; and assert **I12's commutation**
— running `--direction out` then `--direction in` and running them in the
opposite order produce part files equal as sets and transducer files that parse
to equal transducers.

**Regression bar for the extraction (Phase 1).** The existing
`tests/dependent_outputs_test.cpp` and `tests/ltlf_ek_deps_test.cpp` must pass
**without edits**. Any edit needed is evidence the core extraction changed the
public surface, which is a PRD-change event on
`docs/prd/output-dependencies-tool.md`, not a test fix.

## Open theory questions touched

- **All three new statements are unproved.** `\cref{def:indep}` is a definition,
  but `\cref{lem:indep-diagonal}` (the diagonal collapse under projection) and
  `\cref{lem:indep-transducer}` (the construction and its equirealizability
  claim) are stated in `main.tex` §`indep` under `\cl` notes with no proofs. O1-in
  is empirical evidence for the second, not a proof. For `/theory-review`.
- **The equirealizability claim's direction of interest is new.** For $\Tout$ the
  worry was that forcing the system might cost it something; here it is that
  forcing the **environment** might. The sketch in the `\cl` note argues both
  directions, but the "any environment deviation enters a state from which no
  continuation violates $\varphi$" step leans directly on
  `\cref{def:probDef}`'s unsettled termination semantics — more heavily than the
  output side does, because "the environment loses" is only meaningful once that
  note is settled. For `/theory-review`.
- **Refining the analysis by a given $\Tout$ (I10)** — sound-but-incomplete today.
  Whether running the check on $\Aneg\times\Tout$ finds strictly more dependent
  inputs, and whether the resulting $\lambda_{in}$ stays valid when $\Tout$ is
  later replaced, is unexamined. Note the output tool's I10 has the *same* shape
  and its theory-review observed the analogous invariance held there.
- **The X-shift second formulation.** Rewriting $\varphi$ so each output atom $o$
  becomes $X\,o$ moves the outputs into the previous letter, which makes the Moore
  restriction structural instead of quantified, and the unprojected criterion then
  applies verbatim. It should report the same $\Xdep$. Deliberately **not** built
  here (it needs a formula rewriter, an un-shift register on the emitted
  transducer, and a decision on the trailing position under weak $X$) — captured
  in `docs/BACKLOG.md`.
- **The `main.tex:613` commented block is a different question and stays
  commented.** It proposes deciding input dependence over an external knowledge
  base $\Gamma$ by synthesizing $\Gamma$ with $D$ as outputs and counting
  strategies. That is a different input object ($\Gamma$, not $\varphi$) and a
  different decision procedure; nothing here resolves it, and the block is left
  as the author wrote it.
- **`main.tex` has no $\text{LTL}_f$ preliminaries** (tracked under the `FP`
  entry) — load-bearing twice here: `\cref{def:indep}` quantifies over
  $L(\lnot\varphi)$ as finite non-empty traces, and I2's refusal to obtain $\Aneg$
  by flipping acceptance on $A_\varphi$ is precisely because the empty/length-0
  convention is undefined in the paper.

## Developer comments / PRD disagreements

Recorded by the unattended Phase 1 run (2026-08-03). All are **"consider"** —
none was acted on, each is the user's call.

1. **I2's rationale is stale, though its conclusion is right** (`/theory-review`,
   `doc-bug`). I2 and the *Violation automaton* glossary entry both justify
   refusing to build $\Aneg$ by flipping acceptance on $A_\varphi$ with "an
   acceptance flip is an untested equivalence, not a free complement". The
   reviewer **tested it**: against an independent LTLf trace evaluator (no
   `ltlf_to_dfa` in the loop), 17 formulas × all traces to length 4, including
   the empty/length-0 cases `!X[!]1` and `a & !X[!]1` — 0 mismatches, 0
   acceptance-flip equivalence violations. The code's choice should stand (it is
   the glossary definition, and O5-in's state-index comparison depends on both
   sides calling the same `ltlf_to_dfa`), but the wording should read as a
   *design choice*, not a soundness requirement, or a future reader will treat a
   correct construction as forbidden. **Glossary wording is user-owned, so no
   edit was made.**
2. **The output direction's exception messages changed prefix.** Errors now read
   `run_dependency_analysis: …` where they read `dependent_outputs: …`, and
   `src/ltlf_ek_deps.cpp:369,414` prints `e.what()` to stderr, so this is
   user-visible. No test asserts on it and exit codes are unaffected, but it is
   the one thing about the output direction that is not bit-for-bit identical
   after the extraction. Fix would be to have each delegating wrapper re-throw
   with its own prefix — a decision about the CLI's user-facing text.
3. **`scanned` reads `partition.output_free` where the old code read
   `partition.outputs()`** (`src/detail/dependency_core.cpp:220`). Equal only
   because the I9 guard forces the role's own known-set empty; the repartition
   loop below still iterates `partition.outputs()`. Harmless today, latent if I9
   is ever relaxed.

Recorded by the unattended Phase 2 run (2026-08-03). Also **"consider"** — not
acted on.

4. **Two Phase 2 oracle fixtures were wrong on first write, both the same trap,
   and neither was a code bug.** `F(a \oplus b)` has a non-empty $\Xdep$ but is
   *unrealizable* — no output occurs in it, so the environment simply always
   plays $a=b$. It was picked twice (once by `/test-writer` for O4-in, once for
   O1-in's I6 witness) as a "non-empty $\Xdep$" witness where a *realizable* one
   was needed. Both now use the U3-in shape `F(\lnot a \lor (b \oplus x))`, in
   which the output genuinely participates. Worth stating in the PRD's oracle
   section that an I6 witness must be realizable **and** input-dependent, since
   the two conditions pull in opposite directions and the trap is not obvious.
5. **I12's commutation oracle covers $\Tout$ but not $\Tin$**
   (`tests/ltlf_ek_deps_input_test.cpp`, `I12DirectionsCommute`). Its fixture
   $G(a \leftrightarrow x) \land F(b \oplus c)$ has $\Xdep^{out}=\{x\}$ but
   $\Xdep^{in}=\emptyset$, so the transducer-equality clause runs for $\Tout$
   while the $\Tin$ comparison is guarded away — the commutation content for the
   input direction reduces to "both orders agree the file is absent". The guard
   itself is correct (the Edge case says an empty $\Xdep$ writes no transducer),
   and the run is not vacuous, but the direction this PRD exists for is the
   under-covered one. **The obstruction looks structural, not a bad guess:**
   probing the built CLI, $F(\lnot b \lor (c \oplus y))$ alone gives
   $\Xdep^{in}=\{b\}$, and conjoining *any* out-dependent constraint onto it
   drives $\Xdep^{in}$ to $\emptyset$ — because the input side analyses
   $L(\lnot\varphi)$ and $\lnot(A \land B) = \lnot A \lor \lnot B$ opens live
   escape routes, so no input stays forced. That is I2's negation behaving
   exactly as specified, but it means a single $\varphi$ making **both**
   directions non-empty may not be constructible by conjunction at all. Whether
   to hunt for one by another route, or to accept the split coverage and say so
   in the oracle section, is a decision for the grill.
6. **The measured $\Xdep\neq\emptyset$ rate clears its floor by one case.**
   7/150 = 4.7%, versus the output tool's 12/150. The floor is written as integer
   `150/20 = 7`, so the assertion passes at exactly the boundary: one fewer
   dependent formula in a regenerated corpus and O1-in fails on vacuity rather
   than on a real defect. Either the floor or the corpus filter probably wants
   revisiting; the number is recorded honestly rather than tuned.

Recorded by `/review` on PR #6 (the generic half of the `code-review` gate),
2026-08-03. Also **"consider"**; no must-fix was raised.

7. **Two `--direction in` Edge cases have no CLI test.** Both were verified
   correct by hand against the built binary and neither is a defect — they are
   simply unpinned: (a) $\varphi$ **valid** exits `3` with a message that does
   name the direction (`phi is valid (…the environment can never force a
   violation)`); (b) $\varphi$ **unsatisfiable** under `--direction in` exits `0`
   and reports nothing dependent. The PRD's Edge cases call (b) out explicitly as
   "worth an explicit test, since the reflex from the output tool is to expect
   exit 3 here", and no test was written for it. As it stands, a future refactor
   of the exception → exit-code mapping can regress either silently.
8. **`--direction` is carried as a validated `std::string`, then re-tested by
   value.** `ParseArgs` rejects anything but `in`/`out`, but `CliArgs::direction`
   stays a string and `main()` re-derives `const bool is_in = (args.direction ==
   "in")`. `detail/dependency_core.hpp` argues in its own comment against making
   invalid combinations representable; parsing straight into an enum (or into
   `Role`) would apply that same argument one layer up. Style only.

## Definition of done

- Both phases landed; the tree compiles and `ctest` is green, with the
  pre-existing tests passing unedited.
- `--direction in` output feeds `ltlf-ek-synth --known-input-transducer`
  unmodified (O1-in exercises exactly that composition).
- U1-in–U6-in and O1-in, O3-in, O4-in, O5-in written and passing.
- `/glossary` run for the three new terms and the three amendments in
  *Ubiquitous-language terms used*.
- The measured non-empty-$\Xdep$ rate of O1-in's corpus recorded in this PRD.
- `/theory-review` run against `main.tex` §`indep` — both lemmas are unproved and
  the code is their only evidence.
- The four gates in the header ticked by the skills that perform them.
