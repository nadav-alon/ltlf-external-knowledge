# PRD: output-dependency extraction tool (`ltlf-ek-deps`)

**Status:** draft
**Interface:** new binary `ltlf-ek-deps` + library entry `dependent_outputs`; does **not** implement `Synthesis`
**Recommended workflow:** concurrent — the *Interfaces & types* freeze is **high**: every signature is a thin composition of existing glossary types (`spot::formula`, `VariablePartition`, `OutputLabeledTransducer`), and the one genuinely new predicate already exists in-tree as inline code at `src/transducer_io.cpp:191-203`.
**main.tex ref:** `\cref{outdep}` — `\cref{def:outdep}`, `\cref{lem:outdep-diagonal}`, `\cref{lem:outdep-transducer}` (all written this session, propositions **unproved**)

**Gates:** (Phases 2-3 are unimplemented, so the three phase-scoped gates stay
open; each records what Phase 1 closed.)
- [x] glossary        — *closed 2026-07-30*, in the commit that wrote this PRD.
      All four gaps landed at once (*Dependent output set*, *Dependency set*,
      *Live-letter region*, *Determinacy witness*), plus *Print a transducer*;
      nothing phase-scoped is outstanding.
- [ ] tests           — **Phase 1 closed 2026-07-30**, written concurrently
      against the frozen *Interfaces & types* block: U6 (6 cases, incl. the
      set-vs-singleton linchpin) and O2 (9 round-trip fixtures), plus
      `PrintTransducer.NormalisesAcceptanceAway` locking the acceptance
      decision below. Suite 437/437. **Phase 2 — U1-U5 authored** (same
      concurrent shape) in `tests/dependent_outputs_test.cpp`, against the
      frozen Phase 2 block, on a branch where `include/ltlf_ek/dependent_outputs.hpp`
      does not exist yet; not built, not run — the developer branch lands the
      header and the launcher merges + builds + runs `ctest`. Gate stays open
      until that merge is green. **Still open:** the Phase 2 merge/build/ctest
      step, and O1/O3/O4 (Phase 3).
- [ ] code-review     — **Phase 1 closed 2026-07-30**, domain + generic, both
      clean of must-fix. Fixed in-diff: `delta_dfa()` const-correctness, the
      two `undetermined_variable` preconditions, acceptance normalisation, the
      untracked-test/CMake mismatch, a missing `<vector>`, the round-trip
      preconditions, and the stale `main.tex` §-anchors in the touched files
      (§101→§108, §103→§110, §107→§114-115). See *Developer comments*.
- [ ] theory-review   — **Phase 1 closed 2026-07-30**: no `code-bug`. The
      cofactor predicate is sound *and complete* for sets, and the "at most
      one" reading is sanctioned by `main.tex` §114-115. One `\cl` sentence
      written into `main.tex` under `\cref{lem:outdep-diagonal}` distinguishing
      the shared at-most-one half from `\cref{def:probDefTransducer}`'s total
      λ. **Open:** `\cref{lem:outdep-transducer}` and the equirealizability
      claim, which only Phase 2-3 code can evidence.

## Goal

Build a separate binary that takes an $\text{LTL}_f$ formula $\varphi$ and an
$\mathcal{I}/\mathcal{O}$ split, finds a **maximally dependent set of output
variables** $\Xdep \subseteq \mathcal{O}$ (`\cref{def:outdep}`), and emits that
dependency as a $\Tout$ in this project's transducer file format plus an updated
part file with $\Xdep$ recorded as $\Oknown$. This mechanises what
`main.tex:57-58` states informally — that when output variables are dependent,
the dependency *is* external knowledge inherent to the formula — so
`ltlf-ek-synth` can be fed real, derived external knowledge instead of
hand-authored fixtures.

The notion is adapted from *Dependent Variables in Reactive Synthesis*
(arXiv:2401.11290, tool `DepSynt`, https://github.com/eliyaoo32/DepSynt), which
works over infinite words and an NBA. The $\text{LTL}_f$ adaptation is **not** a
port: see *Behaviour* for the two deviations that make it both simpler
(deterministic $A$ ⇒ no compatible-pair search) and stricter (totality is
mandatory, not optional).

Supersedes nothing. Adjacent: `docs/prd/transducer-file-format.md` (the format
being emitted), `docs/prd/cli-wrapper.md` (the consuming binary's flags),
`docs/prd/ltlfsynt-oracle.md` (the external oracle reused here).

## Ubiquitous-language terms used

Existing glossary terms: *Goal formula* ($\varphi$), *Inputs / Outputs*,
*Free inputs / Known inputs / Free outputs / Known outputs*
($\Ifree,\Iknown,\Ofree,\Oknown$), *Governed variables (V)*, *Closed universe of
APs*, *External knowledge strategy* ($\Tout$), *Transducer*, *Output-labeled
transducer*, *Role* (`t_out`), *Observed / produced slice*
($\Sigma_0 = \mathcal{I}\cup\Ofree$, $\Sigma_1 = \Oknown$), *Transducer file
format (%%LAMBDA block)*, *Parse a transducer*, *Output function (lambda)*,
*Transition function (delta)*, *Letter*, *Cube*, *Goal DFA construction*
(`ltlf_to_dfa`), *NFA / DFA for the Goal*, *Consistency (cons)* and its
partiality clause, *Controller verifier*. (Note `trivial_transducer` is a real
function in `output_labeled_transducer.hpp` but has **no** glossary entry — it is
referenced below only as a rejected option, so this PRD adds no dependency on it.)

**Glossary gaps — run `/glossary` before `/developer`.** Four new domain
concepts, none of which exist in `docs/GLOSSARY.md` today:

1. **Dependent output set** — $\Xdep$, `\cref{def:outdep}`. C++: the
   `dependent` member of `DependentOutputs`. Do not call it: the known set, the
   governed set (that is $V$, a *partition* notion — $\Xdep$ is a *result*),
   dependencies (bare).
2. **Dependency set** — $\Ydep = (\mathcal{I}\cup\mathcal{O})\setminus\Xdep$,
   `\cref{def:outdep}`; equals $\Sigma_0$ for `Role::t_out` exactly. C++: not
   materialised (derived from the partition). Do not call it: the observed set
   (that is $\Sigma_0$, which it coincides with only for `t_out`).
3. **Live-letter region** — $\liveset{s}$, `\cref{lem:outdep-diagonal}`: the
   letters out of $s$ whose successor can still reach acceptance. C++: file-local
   in the analysis. Do not call it: `emits_region` (that is a *transducer*
   $\lambda$-agreement region, a different object), live edges, useful letters.
4. **Determinacy witness** — the shared per-variable functionality predicate.
   C++: `undetermined_variable`. Do not call it: `is_functional` (it returns a
   witness, not a bool), `functional`, `check_lambda`.

Also new but **infrastructure, not domain** (no glossary entry, per the
`bench.hpp` / `cli.hpp` precedent): `print_transducer`,
`OutputLabeledTransducer::delta_dfa`, and the `ltlf-ek-deps` argument plumbing.

## Behaviour / semantics (from main.tex)

All three anchors were written into `latex/main.tex` this session (§`outdep`),
with the prose under `\cl` notes and both lemmas **explicitly unproved**.

**I1 — the dependency criterion (`\cref{lem:outdep-diagonal}`).** With $A$ the
deterministic Goal automaton, $\Xdep$ is dependent on $\Ydep$ **iff** for every
**reachable live** state $s$ of $A$, no two letters $v,v' \in \liveset{s}$ have
$v\cap\Ydep = v'\cap\Ydep$ and $v\cap\Xdep \neq v'\cap\Xdep$. Equivalently:
$\liveset{s}$ read as a relation $\Ydep \to \Xdep$ is **functional**. This is
where the DepSynt architecture departs: their Algorithm 1 searches *compatible
pairs* over $Q\times Q$ because their automaton is an NBA; determinism collapses
the compatible pairs to the diagonal $\{(s,s)\}$, so **there is no pair search
and the check is linear in $|A|$**.

**I2 — liveness is our own backward BFS, not a Spot primitive.**
`live(s)` = some **accepting** state is reachable from $s$ (including $s$
itself). `spot::purge_dead_states` must **not** be used: `ltlf_to_dfa`
(`src/ltlf_to_dfa.cpp`) marks the final states $F_D$ with **no** absorbing
self-loops — `src/solve_dfa.cpp:42-44` adds those itself when reducing
reachability to Büchi — so Büchi-semantics dead-state purging ("cannot reach an
accepting *cycle*") would purge the final states outright. Compute liveness as a
backward BFS from `state_is_accepting` over reversed edges. This is what
`\cref{lem:outdep-diagonal}` makes explicit where DepSynt assumes it away
("wlog all states and edges that are not part of an accepting run are removed").

**I3 — analysis on the pruned view, emission on the complete DFA
(`\cref{lem:outdep-transducer}`).** Liveness prunes only the *analysis*. The
emitted $\Tout$ uses:
- $\delta_{out} = \delta_A$ of the **complete** DFA — free, since `ltlf_to_dfa`
  already calls `spot::complete_here`. Do **not** purge before emitting.
- $\lambda_{out}(s) = \liveset{s}$ **totalised**: on $\Ydep$-assignments that
  $\liveset{s}$ does not cover, commit a fixed default $\Xdep$-cube.

**I4 — totality is a soundness requirement, not a convenience.** DepSynt sets
$\lambda^X = \bot$ when the successor set is empty. Ported literally that is
**wrong here**: by `\cref{def:consistency}`'s partiality clause an undefined
$\delta$ or $\lambda$ makes the letter **inconsistent**, and an inconsistent
letter is skipped for *every* party — so a partial $\Tout$ deletes $\Ifree$
letters and constrains the **environment**, which $\Tout$ has no right to do
($\Sigma_1 = \Oknown$, `main.tex:125`). Witness, and a required test fixture:
$\varphi = G(\lnot a)\wedge G(x)$, $\mathcal{I}=\{a\}$, $\mathcal{O}=\{x\}$. One
live state, $\liveset{s} = (\lnot a \wedge x)$, which *is* functional from
$\{a\}$ to $\{x\}$, so $x$ is reported dependent with
$\lambda(s,\lnot a) = x$ and $\lambda(s,a)$ uncovered. Plain synthesis of
$\varphi$ is **unrealizable** (the environment plays $a$); with a partial
$\Tout$ every $a$-letter becomes inconsistent, the environment cannot play $a$,
and synthesis reports **realizable**. Totalising restores agreement.

**I5 — the default cube is pinned.** `default_X` = the **all-negative** cube
over $\Xdep$ (every dependent output false). Any fixed choice is sound (the
defaulted letters are outside $L(\varphi)$), but it must be *fixed*, or the
emitted file is not reproducible and the round-trip oracle becomes flaky.
Symbolically:
`lambda_s = R_s | (!bdd_exist(R_s, X_cube) & default_X)`.

**I6 — greedy accumulation, lexicographic order.** $\Xdep \gets \emptyset$; for
each $z \in \mathcal{O}$ in `std::set<std::string>` order, if
$\Xdep\cup\{z\}$ is dependent on
$(\mathcal{I}\cup\mathcal{O})\setminus(\Xdep\cup\{z\})$ then
$\Xdep \gets \Xdep\cup\{z\}$. The test **must** use the accumulated $\Xdep$;
singleton-union is unsound. Witness, and a required test fixture:
$\varphi = G(x \leftrightarrow y)$, $\mathcal{I}=\{a\}$,
$\mathcal{O}=\{x,y\}$ — $\{x\}$ is dependent on $\{a,y\}$ and $\{y\}$ on
$\{a,x\}$, but $\{x,y\}$ is **not** dependent on $\{a\}$ (at each $a$, both
$x=y=\top$ and $x=y=\bot$ stay live, so $\liveset{s}=(x\leftrightarrow y)$ is
non-functional from $\{a\}$ to $\{x,y\}$). Result is subset-**maximal**, not
maximum; the order therefore picks *which* maximal set is returned, and
lexicographic is chosen over DepSynt's first-appearance-in-$\varphi$ because it
is stable under rewriting $\varphi$ to an equivalent form. **Changing the order
later is a PRD-change event**, since it changes the tool's output.

Note the automaton, the live set and every $\liveset{s}$ are **independent of
$\Xdep$** — only the functionality test consumes $\Xdep$. So `ltlf_to_dfa`, the
liveness BFS and the per-state $\liveset{s}$ are computed **once**, before the
greedy loop; the loop is then $|\mathcal{O}|$ candidate tests, each
$O(|Q_A|\cdot|\Xdep|)$ BDD operations. Rebuilding the DFA per candidate would be
a $|\mathcal{O}|$-fold waste of the dominant cost.

**I7 — the functionality check is already in-tree.** `\cref{lem:outdep-diagonal}`'s
condition is exactly the $\lambda$-functionality validation at
`src/transducer_io.cpp:191-203`: per produced variable $x$, no observation may
admit both polarities —
`bdd_exist(R & bdd_ithvar(x), X_cube) & bdd_exist(R & bdd_nithvar(x), X_cube) != bddfalse`
⇒ non-functional. Per-variable, $|\Xdep|$ operations, **no fresh BDD variables
and no renamed copy of the relation**. It is correct for sets, not just
singletons: if some $\Ydep$-point admitted two distinct $\Xdep$-tuples they
would differ in a coordinate, and that coordinate would admit both polarities.
This predicate is **extracted** to a shared library function (see *Interfaces*),
with `parse_transducer` becoming its first caller and keeping its exact existing
error text.

**I8 — scope is outputs only, and that is a turn-order constraint.** For
$\Xdep\subseteq\mathcal{O}$, DepSynt's $\Ydep=(\mathcal{I}\cup\mathcal{O})\setminus\Xdep$
**equals** $\mathcal{I}\cup\Ofree$, which is exactly $\Sigma_0$ for
`Role::t_out` (`main.tex:125`) — a verbatim fit. Input dependencies are **out of
scope** and are *not* a parameter change: `Role::t_in` has
$\Sigma_0=\Ifree$, so a $\Tin$'s $\lambda$ may never observe $\mathcal{O}$, and
$\Ydep=(\mathcal{I}\cup\mathcal{O})\setminus\Xdep$ would let it — violating the
turn order at `main.tex:83`. A $\Tin$ would need the strictly stronger notion
"dependent on $\mathcal{I}\setminus\Xdep$ alone", i.e. a different algorithm.
See *Open theory questions*.

**I9 — part-file co-management.** The tool **owns** `output_free` /
`output_known` (it repartitions $\mathcal{O}$ into $\Ofree \uplus \Xdep$) and
**passes `input_free` / `input_known` through verbatim**, so a future
input-dependency tool composes by owning those two keys and passing ours
through — disjoint keys, order-independent, either tool may run first. It
**refuses** a non-empty `output_known` on input (`std::invalid_argument` /
usage error): `main.tex:125` has exactly one $\Sout$ producing all of $\Oknown$,
so there is no "compose two $\Tout$s" notion, and an already-governed output
$o\in\Ydep$ would let our $\lambda_{out}$ observe a variable produced in the
same turn-order phase.

**I10 — a $\Tin$ present on input does not refine the analysis (Phase 1-3
scope).** If `input_known` is non-empty the analysis still runs on $A$ alone,
ignoring any $\Tin$. This is **sound but incomplete**: a $\Tin$ shrinks the
trace set, so more outputs *could* be dependent, and a $\lambda_{out}$ correct
on $L(\varphi)$ stays correct on any subset of it. Refining the analysis by
$\Tin$ is deferred — see *Open theory questions*.

## Interfaces & types

**Freeze confidence: high.** Every signature below is a thin composition of
existing glossary types; the only genuinely new logic (I7) already exists in-tree
as inline code being extracted, and the return struct is a plain aggregate of
three glossary terms. Nothing here is being invented from scratch.

### Phase 1 — shared predicate + writer

```cpp
// include/ltlf_ek/transducer_io.hpp  (next to parse_transducer)

// The determinacy witness (docs/GLOSSARY.md): if `relation`, read as a relation
// from its non-`produced` variables to `produced`, is NOT functional, return the
// name of a `produced` variable that some observation leaves undetermined;
// nullopt iff functional.  Per-variable cofactor form --- no fresh BDD
// variables, |produced| operations.  Extracted from the inline check formerly at
// src/transducer_io.cpp:191-203, whose error text is preserved by its caller.
//
// relation      --- a bdd over (observed ∪ produced) only.
// produced      --- variable NAMES of the produced slice (Sigma1, or Xdep).
// produced_cube --- the variable-cube of the same set (docs/GLOSSARY.md "Cube").
// aut           --- supplies register_ap / the shared bdd_dict.
std::optional<std::string> undetermined_variable(
    bdd relation, const std::set<std::string>& produced, bdd produced_cube,
    const spot::twa_graph_ptr& aut);

// Emit one transducer in the transducer file format (HOA delta, then --END--,
// then the %%LAMBDA block) --- the exact counterpart of parse_transducer, so
// parse_transducer(print_transducer(t)) round-trips.  Sigma0/Sigma1 are NOT
// written (the format does not carry them; they come from role + partition).
void print_transducer(std::ostream& out, const OutputLabeledTransducer& t);
```

```cpp
// include/ltlf_ek/output_labeled_transducer.hpp  (new accessor)

  // The delta transition structure ONLY.  As with the constructor argument, the
  // twa's omega-acceptance is MEANINGLESS here --- never read it as transducer
  // finality (see the class comment).  Exposed so print_transducer can emit the
  // HOA half of the file format.
  spot::twa_graph_ptr delta_dfa() const;
```

### Phase 2 — the analysis

```cpp
// include/ltlf_ek/dependent_outputs.hpp  (new header)

// Result of the maximally-dependent-output search (\cref{def:outdep}).
struct DependentOutputs {
  // Xdep: the maximally dependent output set, lexicographic-greedy (I6).
  std::set<std::string> dependent;
  // `partition` with `dependent` moved output_free -> output_known, and
  // input_free / input_known passed through verbatim (I9).
  VariablePartition partition;
  // The Tout of \cref{lem:outdep-transducer}: delta = the COMPLETE Goal DFA,
  // lambda = the totalised live-letter region (I3, I5), Role::t_out slices from
  // `partition`.  nullopt iff `dependent` is empty --- there is no Tout to
  // build when Oknown is empty (see Edge cases).
  std::optional<OutputLabeledTransducer> t_out;
};

// Find a maximally dependent output set of `phi` and materialise it as external
// knowledge.  Built on the shared `dict` (\cref{lem:outdep-diagonal},
// \cref{lem:outdep-transducer}; docs/prd/output-dependencies-tool.md).
//
// Throws std::invalid_argument if `partition.output_known` is non-empty (I9),
// if an AP of `phi` lies outside `partition.universe()` (the closed-universe
// rule), or if `phi` is unsatisfiable (Edge cases).
DependentOutputs dependent_outputs(const spot::formula& phi,
                                   const VariablePartition& partition,
                                   const spot::bdd_dict_ptr& dict);
```

### Phase 3 — the binary

New target `ltlf-ek-deps` (`src/ltlf_ek_deps.cpp`), plus a part-file **writer**
alongside the existing `parse_partition_file`:

```cpp
// include/ltlf_ek/cli.hpp
void print_partition_file(std::ostream& out, const VariablePartition& p);
```

Flags, mirroring `ltlf-ek-synth`'s style:

| flag | meaning |
|---|---|
| `--formula F` | the $\text{LTL}_f$ formula (required) |
| `--part-file F` | read a part file (mutually exclusive with the CSV pair) |
| `--inputs a,b` / `--outputs x,y` | the $\mathcal{I}/\mathcal{O}$ split as CSV |
| `--emit-part F` | write the updated part file; must differ from `--part-file` |
| `--transducer F` | write the $\Tout$ file |
| `--verbose` | per-candidate accept/reject with the undetermined variable |

Both out-flags optional ⇒ pure-query mode. stdout gets one line,
`dependent outputs: x   (of x, y)`. Exit codes: `0` success (including
$\Xdep=\emptyset$), `2` usage error, `3` $\varphi$ unsatisfiable, `1` internal.

**If implementation proves this contract wrong:** that is a PRD-change event —
update this section and propagate to any in-flight test branch; the developer
does not silently re-shape the interface on its own branch.

## Implementation phases

- **Phase 1 — plumbing.** `undetermined_variable` (extracted, with
  `parse_transducer` rewired to it and its error text unchanged),
  `print_transducer`, `OutputLabeledTransducer::delta_dfa`. **Green checkpoint:**
  compiles; the existing suite is unaffected (`transducer_io_test.cpp` still
  passes with its exact expected messages); the new round-trip oracle passes.
  Stubs nothing.
- **Phase 2 — the analysis.** `dependent_outputs` + `DependentOutputs` on the
  DFA route: liveness backward BFS (I2), $\liveset{s}$, the greedy loop (I6), the
  totalised $\lambda$ (I3, I5). **Green checkpoint:** compiles; the four unit
  fixtures of *Test oracles* U1-U4 pass. Stubs the binary — library-only, no CLI.
- **Phase 3 — the binary.** `ltlf-ek-deps`, `print_partition_file`, part-file
  co-management (I9), exit codes, CMake target. **Green checkpoint:** compiles;
  the end-to-end equirealizability oracle O1 passes against `ltlfsynt`, and the
  part-file round-trip/pass-through oracle O3 passes.

Each phase leaves the tree compiling and independently testable. The **NFA +
compatible-pairs route is deliberately not a phase here** — it is a separate
follow-up PRD (see *Open theory questions*).

## Edge cases

- **$\Xdep=\emptyset$** (nothing dependent): exit `0`, write the part file
  (`output_known:` empty), and **write no transducer file** — `src/ltlf_ek_synth.cpp:214`
  *rejects* `--known-output-transducer` when `output_known` is empty
  ("ambiguous"), so emitting a `trivial_transducer` would produce a file that
  breaks the pipeline it feeds. `t_out` is `nullopt`; stdout says
  `dependent outputs: none`.
- **$\varphi$ unsatisfiable** ($L(\varphi)=\emptyset$, i.e. the initial state is
  not live): every $\Xdep$ is **vacuously** dependent (no pair $w,w'$ exists),
  so the greedy loop would confidently return $\Xdep=\mathcal{O}$. Detect it
  (initial state not live) and exit `3` with a message, writing **no** artifacts.
  `dependent_outputs` throws `std::invalid_argument`.
- **$\mathcal{O}=\emptyset$**: the greedy loop is empty, $\Xdep=\emptyset$ —
  same as the first case, not an error.
- **$\mathcal{I}=\emptyset$**: legal; $\Ydep=\mathcal{O}\setminus\Xdep$ and the
  functionality check still applies (with no $\Ydep$ variables at all, dependence
  means $\liveset{s}$ pins a single $\Xdep$-tuple per state).
- **Non-empty `output_known` on input**: refuse (I9), exit `2`.
- **Non-empty `input_known` on input**: legal — passed through verbatim, and
  ignored by the analysis (I10). Not an error, and worth an explicit test so
  the pass-through does not silently regress.
- **`--emit-part` equal to `--part-file`**: refuse, exit `2` (a crash mid-write
  must not destroy the co-managed file).
- **An AP of $\varphi$ outside `universe()`**: `std::invalid_argument`, per the
  closed-universe rule.
- **A state whose $\liveset{s}$ is empty** (a live state all of whose successors
  are dead — impossible by definition of live, so **assert** rather than
  handle; if it fires, liveness is computed wrong).
- **Every letter defaulted at a dead state**: dead states are still emitted (I3
  keeps the complete DFA) and their $\lambda$ is wholly the default cube. Not an
  error; exercised by U4.

## Test oracles (for /test-writer)

**Unit fixtures (Phase 2).** Each is a hand-verified $\varphi$ over a tiny
partition:

- **U1 — dependent, non-vacuous.** $\varphi = G(a \leftrightarrow x)$,
  $\mathcal{I}=\{a\}$, $\mathcal{O}=\{x\}$ ⇒ $\Xdep=\{x\}$; one live state,
  $\liveset{s}=(a\leftrightarrow x)$ functional; $\lambda(s,a)=x$,
  $\lambda(s,\lnot a)=\lnot x$.
- **U2 — not dependent.** $\varphi = G(a \rightarrow x)$, same partition ⇒
  $\Xdep=\emptyset$; at $\lnot a$ both $x$ and $\lnot x$ stay live. Also
  asserts `t_out == nullopt`.
- **U3 — singleton-union is unsound (I6).** $\varphi = G(x\leftrightarrow y)$,
  $\mathcal{I}=\{a\}$, $\mathcal{O}=\{x,y\}$ ⇒ $\Xdep$ is a **singleton**
  (`{x}` under lexicographic order), **not** `{x,y}`. This test is the direct
  guard on the accumulated-$\Xdep$ requirement: a singleton-union implementation
  returns `{x,y}` and fails here.
- **U4 — totality (I4).** $\varphi = G(\lnot a)\wedge G(x)$,
  $\mathcal{I}=\{a\}$, $\mathcal{O}=\{x\}$ ⇒ $\Xdep=\{x\}$, **and**
  $\lambda(s,a)$ must be *defined* (the default cube), not `nullopt`. Assert on
  `t_out->lambda(s, v)` for a letter with $a$ true. A partial implementation
  returns `nullopt` and fails here.
- **U5 — order determinism (I6).** Reuses U3's $\varphi = G(x\leftrightarrow y)$,
  which has **two** distinct maximal dependent sets, `{x}` and `{y}` (each is
  dependent, and their only common superset `{x,y}` is not). Assert the
  lexicographic one, `{x}`, is returned — and that it is returned on repeated
  calls in one process (a set-iteration-order or hash-order bug shows up here).
- **U6 — `undetermined_variable`** (Phase 1): direct table test — functional
  relations return `nullopt`, non-functional ones return the offending variable
  name; the set case $(x\leftrightarrow y)$ over $\Ydep=\emptyset$ returns a
  name even though each variable alone looks fine.

**Round-trip oracle (Phase 1) — O2.** For every transducer fixture already in
`tests/transducer_io_test.cpp` and for the U1/U4 emitted transducers:
`parse_transducer(print_transducer(t))` equals `t` — same state count, same
initial state, BDD-equal edge guards per (src,dst), BDD-equal $\lambda$ per
state. BuDDy canonicalises, so `==` is semantic equality (same argument as the
*Build-equivalence metamorphic oracle*).

**End-to-end equirealizability oracle (Phase 3) — O1, the linchpin.** From
`\cref{lem:outdep-transducer}`: for a corpus of $\varphi$, the verdict of
`ltlf-ek-synth --formula φ --part-file <emitted> --known-output-transducer <emitted>`
must equal the verdict of `ltlf-ek-synth --formula φ --inputs … --outputs …`
with no external knowledge, **and** equal Spot's `ltlfsynt` on $\varphi$ (the
external independent oracle, reusing `docs/prd/ltlfsynt-oracle.md`'s harness).
This is the test that catches I4 — with a partial $\Tout$,
$\varphi = G(\lnot a)\wedge G(x)$ flips from unrealizable to realizable and the
oracle fails. Run it over the existing *Generated corpus* formulas, restricted
to cases where $\Xdep\neq\emptyset$ (and assert that a non-trivial fraction of
the corpus *does* yield a non-empty $\Xdep$, else the oracle is vacuous).

**Controller-verifier oracle (Phase 3) — O4.** Where the emitted $\Tout$ is
non-trivial and synthesis succeeds, `verify_controller(phi, vars, trivial_t_in,
t_out, t_c)` must accept — the internal linchpin oracle, independent of the
method that produced $t_c$.

**Part-file co-management oracle (Phase 3) — O3.** Feed a part file with a
non-empty `input_known`; assert the emitted part file preserves `input_free` and
`input_known` **byte-for-byte-equivalently** (as sets) and repartitions only the
two output keys; assert `parse_partition_file(print_partition_file(p)) == p`;
assert a non-empty `output_known` input is refused with exit `2`; assert
`--emit-part` equal to `--part-file` is refused.

## Open theory questions touched

- **Both new lemmas are unproved.** `\cref{lem:outdep-diagonal}` (the diagonal
  collapse) and `\cref{lem:outdep-transducer}` (the transducer construction and
  its equirealizability claim) are stated in `main.tex` §`outdep` under `\cl`
  notes with no proofs. O1 is empirical evidence for the second, not a proof.
  For `/theory-review`.
- **Input dependencies need a different notion (I8).** $\Xdep\subseteq\mathcal{I}$
  dependent on $\mathcal{I}\setminus\Xdep$ *alone* — strictly stronger than
  `\cref{def:outdep}`, since $\Sigma_0=\Ifree$ for `Role::t_in` forbids
  observing $\mathcal{O}$. This is the notion the **commented-out**
  `latex/main.tex:498-503` block gropes toward ("a potential set of dependent
  input variables $D\subseteq I$"), including its own suggestion of deciding
  dependence by *counting synthesis strategies*. Left commented (author's call);
  a separate PRD.
- **Refining the analysis by a given $\Tin$ (I10)** — sound-but-incomplete
  today. Whether running the check on $A\times\Tin$ finds strictly more
  dependent outputs, and whether the resulting $\lambda_{out}$ is still valid
  when $\Tin$ is later replaced, is unexamined.
- **The NFA + compatible-pairs route.** A faithful DepSynt port on
  `ltlf_to_nfa` (MONA-backed, single-exponential) with Algorithm 1's pairs BFS
  and a subset construction for the transducer, cross-checked against this
  route as a metamorphic oracle (same $\Xdep$ for the same $\varphi$). Deferred
  to its own PRD. Note the *complexity* motive does **not** transfer: DepSynt's
  $2^k$-vs-$\Omega(2^{k\log k})$ win is about Safra-style NBA determinization,
  which has no finite-word counterpart — both routes are 2EXP worst case here.
  The remaining motive is the cross-check and practical automaton size. Its
  hazard is edge-level pruning: on an NFA a live state may still have edges on
  no accepting run, and getting that wrong makes `δ(p,σ)≠∅` over-approximate,
  yielding a **wrong "dependent"** verdict with no diagnostic.
- **Trace-termination semantics (`main.tex:96`)** — newly load-bearing here.
  I4's totality argument ("the defaulted letters lose the system the game
  exactly as in plain synthesis") and O1's equirealizability both assume the
  system-controlled-termination reachability reading that `solve_dfa` and
  `verify_controller` already share. If that reading changed, both would need
  revisiting.
- **`main.tex` has no $\text{LTL}_f$ preliminaries** (already tracked under the
  `FP` entry): `\cref{def:outdep}` quantifies over $L(\varphi)$ as finite
  non-empty traces without the paper ever defining that, and the $i <
  \min(|w|,|w'|)$ bound is meaningful only against such a definition.

## Definition of done

- All three phases landed; the tree compiles and `ctest` is green.
- `ltlf-ek-deps` builds as its own CMake target and its output feeds
  `ltlf-ek-synth` unmodified (O1 exercises exactly that composition).
- U1-U6, O1-O4 written and passing.
- `/glossary` run for the four gaps in *Ubiquitous-language terms used*.
- `parse_transducer`'s existing error messages unchanged after the I7
  extraction (asserted by the existing `transducer_io_test.cpp`).
- `/theory-review` run against `main.tex` §`outdep` — both lemmas are unproved
  and the code is their only evidence.
- The four gates in the header ticked by the skills that perform them.

## Where this stands (as of 2026-07-30, after Phase 2)

**Landed:** Phases 1-2, on branch `feat/output-dependencies` — Phase 1 as
commit `30c39cd` (`undetermined_variable`, `print_transducer`,
`OutputLabeledTransducer::delta_dfa()`, plus U6 and O2); Phase 2 as commit
`498ff63` (`include/ltlf_ek/dependent_outputs.hpp`, `src/dependent_outputs.cpp`
— `DependentOutputs`, `dependent_outputs`), verbatim against the frozen
*Interfaces & types* Phase 2 block. Suite still 437/437 (Phase 2 adds no
tests of its own — library-only per its Green checkpoint, `/test-writer`'s
job next). Hand-checked against the four *Test oracles* fixtures (U1 `G(a<->x)`,
U2 `G(a->x)`, U3/U5 `G(x<->y)`, U4 `G(!a)&G(x)`) plus the I9/unsat edge cases
with a throwaway scratch program (not committed — a diligence check, not a
substitute for `/test-writer`'s U1-U6): all match the PRD's hand-worked
verdicts, including U4's totalised `lambda(s,a)` and U5's same-process
determinism.

**Branch topology, since it is not obvious from the log.** `feat/output-dependencies`
sits on `master`, whose tip `529a564` is a *cherry-pick* of the commit that wrote
this PRD (the original `43dd79e` was authored on `docs/bootcamp` and is now
orphaned; `docs/bootcamp` was reset to `74b3275`, the bootcamp doc alone).
Nothing is pushed: `master` is 11 commits ahead of `origin/master` and
`feat/output-dependencies` is local-only. Note agent worktrees branch from
**`origin/master`**, not local `master`, so an agent's base can be well behind
the branch it will be merged into — check before assuming a file is identical
across the gap.

**Loose ends, in the order worth taking them:**

1. **Phase 3 — `ltlf-ek-deps`, `print_partition_file`.** The substantive next
   step: the binary, part-file co-management (I9), exit codes, the CMake
   target, and O1/O3/O4. `dependent_outputs` is ready to drive it; see
   *Developer comments* for the one design call Phase 2 made in the space
   Phase 1 left open (`register_ap` pre-registration).
2. **U1-U5 unit tests (Phase 2) are not yet written.** They are `/test-writer`'s
   job, not the developer's; the scratch check above is evidence the code is
   ready for them, not a substitute.
3. **The `latex/` submodule is dirty and uncommitted.** `/theory-review` wrote a
   `\cl` sentence into `main.tex` under `\cref{lem:outdep-diagonal}` separating
   the at-most-one half shared with `\cref{def:probDefTransducer}` from that
   definition's total $\lambda$. Per the Overleaf workflow it needs a **fetch
   first** (never force). The insert shifts `main.tex` line numbers after 526
   by +1.
4. **The `main.tex` §-anchor resync is partial.** Phase 1 fixed only the files it
   touched (§101→§108, §103→§110, §107→§114-115, and the align block
   §124-133→§121-131). Still stale: `include/ltlf_ek/transducer.hpp:24,47`,
   `include/ltlf_ek/role.hpp:11,24`, `docs/GLOSSARY.md`'s *Determinacy witness*
   entry, and **this file's** own citation of `latex/main.tex:498-503` for the
   commented-out input-dependency block, which now lives at 550-554. Prefer
   `\cref` labels over § numbers where possible — this drift is out-of-band and
   recurs on every Overleaf pull.
5. **Two agent worktrees are still on disk** under `.claude/worktrees/`
   (`agent-ac7f2089d5d620d08` = the developer's, `agent-a4e09005d5ed06878` = the
   test-writer's). Both are fully merged into `30c39cd` and safe to remove.
   `.claude/worktrees/` is untracked, so **never `git add -A`** while they exist.

## Developer comments / PRD disagreements

**2026-07-30 — `delta_dfa()` returns `spot::const_twa_graph_ptr`, not
`spot::twa_graph_ptr`.** The *Interfaces & types* Phase 1 block froze the
mutable form. Both consumers are read-only (`spot::print_hoa` takes a
`const_twa_ptr`; O2 reads `num_states()`), and the mutable form is a hole in the
class: `OutputLabeledTransducer` checks
`lambda_by_state_.size() == num_states()` only in its constructor and indexes
with unchecked `operator[]`, so `t.delta_dfa()->new_states(1)` through a `const`
reference makes `emits_region(q)` read past the end with no diagnostic. Const
closes it at no cost to any caller. Phase 2 must therefore copy
(`spot::make_twa_graph`) if it wants a mutable delta.

**2026-07-30 — `print_transducer` normalises acceptance away.** Not specified
either way in the PRD. `spot::print_hoa` copies whatever acceptance the delta
twa carries, and by I3 the emitted $\Tout$'s $\delta_{out}$ *is* the Goal DFA,
whose acceptance marks $F_D$. Copying it through would publish an artifact
advertising $\omega$-acceptance for what is finite-word reachability without
absorbing self-loops — the same $\omega$-vs-finite confusion I2 warns about for
`purge_dead_states`, but baked into a file a user or `autfilt` may read. The
writer therefore emits the canonical `Acceptance: 0 t` of
`docs/prd/transducer-file-format.md`. Nothing is lost: `parse_transducer`
ignores acceptance on the way back in.

**2026-07-30 — Phase 2 pins `register_ap`'s mutation by pre-registering the
whole universe up front, rather than changing the frozen signature.** The
first Developer comment below flags that `undetermined_variable`'s
`register_ap` call silently appends an unknown AP to the automaton it is
handed — for the greedy loop, that automaton is `dfa`, which I3 also emits as
$\delta_{out}$ — and asks Phase 2 to decide whether to eliminate it (a
signature change) or merely keep it pinned. `dependent_outputs` keeps the
signature and pins it harder: right after building `dfa`, it registers every
AP of `partition.universe()` on `dfa` once, before the greedy loop runs. This
makes the mutation independent of which candidate is tested and in what
order (the only alternative was letting each per-candidate `detail::cube_of`
call register whichever output happens not to occur in $\varphi$, contingent
on greedy order) — and it is semantically inert either way: a produced
variable outside `relation`'s support reads as *undetermined* (both polarities
reachable, since nothing pins it), which is the mathematically correct
verdict for a variable $\varphi$ never constrains, not a bug the
pre-registration papers over.

**2026-07-30 — `undetermined_variable` asserts its two preconditions.** The
frozen signature takes `produced` and `produced_cube` separately (deliberately —
Phase 2's greedy loop reuses one cube per candidate) and takes `aut` for
`register_ap`. Both are silent-failure surfaces, and one fails in the *unsound*
direction: a `produced_cube` missing a member of `produced` makes
`with1 & with0` unconditionally empty, so an unconstrained variable reads as
functional and Phase 2 emits a **wrong "dependent" verdict with no diagnostic**;
and `register_ap` silently *appends* an unknown AP to the caller's automaton,
which by I3 is the automaton about to be emitted as $\delta_{out}$. The
signature is unchanged; both are pinned by `assert` under `#ifndef NDEBUG`.
