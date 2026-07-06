# Glossary — Ubiquitous Language

This project has **one** vocabulary, shared by the math (`main.tex`), the prose,
and the C++ code. Every domain concept has exactly one canonical term and one
canonical C++ identifier. If you find yourself inventing a synonym, either reuse
the existing term or update this file via `/glossary` — do not let drift happen.

**How to read an entry**

- **Term** — the canonical English name.
- **`main.tex`** — the math symbol / name and where it is defined.
- **Definition** — one sentence.
- **C++** — the canonical identifier (type, function, or member).
- **Do not call it** — rejected synonyms that must never appear.

> Maintained by `/glossary`. Enforced by `/developer` and `/code-reviewer`
> (a new public C++ identifier for a domain concept must appear here).

---

## Problem Definition

### Goal formula
- **`main.tex`:** $\varphi$, an $\text{LTL}_f$ formula over $\mathcal{I}\cup\mathcal{O}$ (§Problem Definition).
- **Definition:** the specification to synthesize a controller for.
- **C++:** `phi` (`spot::formula`).
- **Do not call it:** spec, goal (unqualified), target.

### Inputs / Outputs
- **`main.tex`:** $\mathcal{I}$, $\mathcal{O}$ with $\mathcal{I}\cap\mathcal{O}=\emptyset$.
- **Definition:** environment-controlled vs system-controlled variables.
- **C++:** `VariablePartition::inputs()` / `::outputs()`.
- **Do not call it:** in/out vars, signals.

### Governed variables (V)
- **`main.tex`:** $\mathcal{V}\subseteq(\mathcal{I}\cup\mathcal{O})$, $\mathcal{V}=\Iknown\cup\Oknown$.
- **Definition:** the variables decided by external knowledge strategies.
- **C++:** `VariablePartition::known()`.
- **Do not call it:** external vars, dependent vars, known set (as a bare noun).

### Free inputs / Known inputs / Free outputs / Known outputs
- **`main.tex`:** $\Ifree,\Iknown,\Ofree,\Oknown$ (§Problem Definition align block).
- **Definition:** the four-way split — free = decided by env / controller,
  known = produced by an external strategy.
- **C++:** `VariablePartition::input_free / input_known / output_free / output_known`.
- **Do not call it:** unknown/uncontrolled (for free), fixed (for known).

### External knowledge strategy
- **`main.tex`:** $\Sin,\Sout$ (input/output knowledge strategies).
- **Definition:** pure functions producing the known variables from history.
- **C++:** modeled as `Transducer` (their regular representation); the input one
  and output one are `t_in` / `t_out`.
- **Do not call it:** oracle, helper, assumption.

### Controller (system strategy)
- **`main.tex`:** $S_C$ / $T_C$, signature $\ldots\to 2^{\Ofree}$ (Def. probDef / probDefTransducer).
- **Definition:** the strategy we synthesize; produces the free outputs.
- **C++:** `Controller`.
- **Do not call it:** solution, policy, winning strategy.

### Agreement
- **`main.tex`:** "$\varphi$ *agrees* with $S$" — $v_t\cap X = S(v_0\cdots v_{t-1}, v_t)$ (§Problem Definition).
- **Definition:** a trace is consistent with a strategy at every step.
- **C++:** `agrees(...)` (per-trace); cf. `consistent` (per-letter, below).
- **Do not call it:** satisfies, matches.

## Automata & Transducers

### Transducer
- **`main.tex`:** $\tau=(Q,\Sigma,\delta,\lambda,q_0)$ (§Transducers).
- **Definition:** a DFA with a **lambda-split** output function
  $\lambda:Q\times\Sigma_0\to\Sigma_1$ implementing a strategy. Has **no**
  acceptance condition (unlike the Goal automata).
- **C++:** `Transducer` (abstract base).
- **Do not call it:** Mealy machine, automaton (bare), FST.

### Output-labeled transducer
- **`main.tex`:** — (no separate symbol; a concrete realization of $\tau$, §Transducers).
- **Definition:** the concrete `Transducer` whose $\delta$ reuses a
  `spot::twa_graph` (used purely as a deterministic transition structure —
  **acceptance ignored**) and whose $\lambda$ is stored as an explicit per-state
  output relation, one BDD over $\Sigma_0\cup\Sigma_1$ per state.
- **C++:** `OutputLabeledTransducer` (constructed from a `spot::twa_graph_ptr`,
  the per-state `lambda` BDDs, and `sigma0_cube` / `sigma1_cube`).
- **Do not call it:** BddTransducer, ExplicitTransducer, SpotTransducer, Mealy
  machine, labeled DFA.

### Controller-as-transducer view
- **`main.tex`:** — (no symbol; `\cref{def:probDefTransducer}` §129 already calls
  $T_C$ a transducer, but this *materialisation* is code-only).
- **Definition:** materialise a synthesized `Controller`'s strategy graph as a
  `Role::t_c` `OutputLabeledTransducer` ($\Sigma_0=\mathcal{I},\Sigma_1=\Ofree$):
  $\lambda_C$ is read off the Mealy strategy edges (the $\Ofree$ slice of the
  matched edge guard), $\delta_C$ off the edge destinations — the same "delta via
  edges, output derived" idiom `OutputLabeledTransducer` uses. Lets the controller
  be consumed uniformly wherever a `Transducer` is expected (e.g. the *Controller
  verifier* product).
- **C++:** `controller_as_transducer(controller, vars)` → `OutputLabeledTransducer`.
- **Do not call it:** to_transducer, as_transducer (bare), wrap, lift.

### Transition function (delta)
- **`main.tex`:** $\delta:Q\times 2^{\mathcal{I}\cup\mathcal{O}}\to Q$.
- **Definition:** successor over the *full* letter; tracks full state history.
- **C++:** `Transducer::delta(q, v)`.
- **Do not call it:** step, next, move.

### Output function (lambda)
- **`main.tex`:** $\lambda:Q\times\Sigma_0\to\Sigma_1$ (the turn-order-restricted output).
- **Definition:** what the transducer commits to; it is passed the **full letter**
  $v$ and internally reads only its $\Sigma_0$ slice (abuse-of-notation,
  `main.tex` footnote §87), returning a cube over $\Sigma_1$.
- **C++:** `Transducer::lambda(q, v)`.
- **Do not call it:** output (bare), emit, label.

### Observed / produced slice (Σ₀ / Σ₁)
- **`main.tex`:** $\Sigma_0,\Sigma_1\subseteq\Sigma$ (§Transducers §105); instantiated
  per transducer in the align block (§124–133).
- **Definition:** the cube of variables a transducer may **observe** ($\Sigma_0$)
  versus the cube it **produces** ($\Sigma_1$); for $\Tin$ this is
  $(\Ifree,\,\Iknown)$, for $\Tout$ it is
  $(\mathcal{I}\cup\Ofree,\,\Oknown)$, and for the controller $T_C$ it is
  $(\mathcal{I},\,\Ofree)$ (see *Role* `t_c`).
- **C++:** as `bdd` cubes, `sigma0_cube` / `sigma1_cube` (construction-time data
  of `OutputLabeledTransducer`); as variable-**name** sets, the `SigmaSlices`
  members `sigma0` / `sigma1` (`std::set<std::string>`), derived from
  `(partition, role)` by `sigma_slices` (see *Role*) — the same slice in
  `bdd`-free form, e.g. to build the cubes or a trivial transducer.
- **Do not call it:** input/output mask, visible set (bare), domain/range; for
  the name-set form, not `SliceNames` (the former file-private struct).

### Produced-trace language
- **`main.tex`:** $\psiin,\psiout$ (conjecture note after `\cref{def:probDefTransducer}`, `main.tex:133`).
- **Definition:** the $\text{LTL}_f$ language of the traces a transducer
  produces — $\psiin$ for $\Tin$, $\psiout$ for $\Tout$; it is what the
  monolithic conjecture (`main.tex:133`) feeds in as the assumption (for $\Tin$)
  or guarantee (for $\Tout$) pinning a strategy's governed variables.
- **C++:** — (no dedicated type; a hand-authored `spot::formula`/string per
  oracle fixture, e.g. `psi_in` in `tests/ltlfsynt_oracle_test.cpp`). **Not**
  auto-derived from the transducer — that would defeat oracle independence
  (see *Faithfulness guard*).
- **Do not call it:** the assumption / the guarantee (those name its *role* in
  the reduction, not the language), trace set, transducer language (bare).

### Cube
- **`main.tex`:** — (no symbol; a BuDDy/BDD representation primitive, not a domain object).
- **Definition:** a `bdd` that is a conjunction of literals — geometrically a
  subcube of the Boolean hypercube over $\mathcal{I}\cup\mathcal{O}$. It plays
  **two distinct roles**, which is the whole reason to name it here:
  - a **value cube** fixes variables to truth values (polarity matters); a
    *full* value cube fixing every variable is a **letter** (see below);
  - a **variable cube** (varset) is a positive-literal cube that only *names a
    set of variables*, e.g. `sigma0_cube`, passed as the "which variables"
    argument to `bdd_exist` / `bdd_restrict`.
- **C++:** `bdd`; variable-cube members are suffixed `_cube` (`sigma0_cube`,
  `sigma1_cube`). Prefer the name **letter** when it is a full value cube.
- **Do not call it:** term, product, bitmask, mask; do not call a variable cube
  a "valuation" (it carries no truth values).

### Letter
- **`main.tex`:** $v\in 2^{\mathcal{I}\cup\mathcal{O}}$.
- **Definition:** one full assignment of all variables at a step — i.e. a *full
  value cube* (see Cube).
- **C++:** `bdd v` (a full cube over I∪O).
- **Do not call it:** symbol, event, valuation.

### NFA / DFA for the Goal
- **`main.tex`:** $N$ (Method 1), $A$ (Method 2); `LtlfToNfa` / `LtlfToDfa`.
- **Definition:** the automaton recognizing the models of $\varphi$.
- **C++:** `spot::twa_graph_ptr` (the automaton *object*; the DFA is built by
  `ltlf_to_dfa` — see *Goal DFA construction* below).
- **Do not call it:** the graph, the machine.

## Transducer I/O

### Transducer file format (%%LAMBDA block)
- **`main.tex`:** — (serialization of $\tau$, §Transducers; no math symbol).
- **Definition:** one self-contained on-disk transducer: a Spot **HOA** automaton
  for $\delta$ (acceptance ignored), then — after HOA's `--END--` — a **`%%LAMBDA`
  block** giving $\lambda$ as one boolean formula per HOA state
  (`state <n>: <formula>`, `false` = undefined there). $\Sigma_0/\Sigma_1$ are
  **not** stored; they are derived from role + partition. See
  `docs/prd/transducer-file-format.md`.
- **C++:** the format read by `parse_transducer`; no dedicated type.
- **Do not call it:** HOA transducer (bare), lambda file, `.hoa`.

### Role
- **`main.tex`:** — (the align-block choice of $\Sigma_0/\Sigma_1$, §124–133).
- **Definition:** which strategy a transducer represents, selecting its
  observed/produced slices: `t_in` ⇒ $\Sigma_0=\Ifree,\Sigma_1=\Iknown$; `t_out`
  ⇒ $\Sigma_0=\mathcal{I}\cup\Ofree,\Sigma_1=\Oknown$; `t_c` ⇒
  $\Sigma_0=\mathcal{I},\Sigma_1=\Ofree$ — the **controller** row of the align
  block (`main.tex:125`, $\lambda_C:Q_C\times2^{\mathcal I}\to2^{\Ofree}$). Unlike
  `t_in`/`t_out` (external knowledge from a file), a `t_c` transducer is usually
  the synthesized `Controller` viewed as a transducer (see *Controller-as-transducer
  view*), or a controller read from a `--controller` file.
- **C++:** `enum class Role { t_in, t_out, t_c }`; the `(partition, role)` ⇒
  $(\Sigma_0,\Sigma_1)$ derivation is `sigma_slices(partition, role)` returning
  `SigmaSlices` (see *Observed / produced slice*).
- **Do not call it:** direction, kind, mode (mode is the reserved Mealy/Moore
  axis); for the derivation, not `derive_slices` (the former file-private name).

### Parse a transducer
- **`main.tex`:** — (no symbol; materialisation of $\tau$ from its file).
- **Definition:** read one transducer file (HOA $\delta$ + `%%LAMBDA` $\lambda$)
  on a shared `bdd_dict` and build the concrete `OutputLabeledTransducer`,
  orienting $\lambda$ from `(partition, role)` and validating $\delta$
  determinism, $\lambda$ functionality, AP scope, and state coverage.
- **C++:** `parse_transducer(in, partition, role, dict)`.
- **Do not call it:** load, read (bare), deserialize, from_hoa.

## Algorithms

### Goal DFA construction (LtlfToDfa)
- **`main.tex`:** `LtlfToDfa`$(\varphi)$ (Method 2, `alg:dfa_product`);
  black-boxed algorithm, no math symbol beyond the automaton $A$ it returns.
- **Definition:** build the deterministic finite automaton $A$ for the Goal
  formula $\varphi$; a thin wrapper over Spot (`ltlf_to_mtdfa` then
  `mtdfa::as_twa()`) that carries finiteness in acceptance marks — **no extra
  AP** — so the alphabet stays $2^{\mathcal{I}\cup\mathcal{O}}$. Built on the
  transducers' shared `bdd_dict`.
- **C++:** `ltlf_to_dfa(phi, dict)` — returns the `spot::twa_graph_ptr` (see
  *NFA / DFA for the Goal*). The Method-1 NFA route (`LtlfToNfa`, `NfaToDfa`)
  is not yet named — future, when Method 1 lands.
- **Do not call it:** to_dfa, ltlf2dfa (that is Spot's own header/primitive),
  build_dfa, translate.

### Consistency (cons)
- **`main.tex`:** $\cons(q_{in},q_{out},v)$ (Method 1) — the per-letter filter.
- **Definition:** $v$'s V-variables are exactly what $\Tin,\Tout$ output.
- **C++:** `consistent(t_in, q_in, t_out, q_out, v)`.
- **Do not call it:** agrees, valid, matches, feasible.

### Product
- **`main.tex`:** $P$ (Methods 1 & 2); states $S\times Q_{in}\times Q_{out}$.
- **Definition:** the Goal automaton crossed with both knowledge transducers,
  keeping only consistent transitions (a non-consistent letter is skipped, as in
  all methods — `\cref{def:enabled}`).
- **C++:** `ProductState` / the `*Product` synthesis classes.
- **Do not call it:** composition, join, cross.

### Forward progression
- **`main.tex`:** `FP`$(\psi,w)$ returning $(\psi',b)$ (Alg. Forward Progression).
- **Definition:** advance a formula by one letter; $b$ = satisfied-now bit.
  Note: `FP` is a **TODO stub** in `main.tex` — see open questions.
- **C++:** `progress(psi, w)`.
- **Do not call it:** derivative, unfold, step.

### Canonical representative
- **`main.tex`:** $[\psi]$ — the class of $\psi$ after progression.
- **Definition:** semantically-equal progressed formulae collapse to one state.
- **C++:** `canonical(psi)`.
- **Do not call it:** normal form, key, hash.

### Aggregation
- **`main.tex`:** Method 3.2 / 3.3 — key states on $[\psi]$ alone, collapsing
  transducer states.
- **Definition:** merge product states sharing a formula part; bounds size by
  the original DFA but **loses knowledge** (may be incomplete).
- **C++:** `aggregate(...)`.
- **Do not call it:** minimization, dedup, quotient.

### Game solving (SolveDfa)
- **`main.tex`:** `SolveDfa`$(P)$ (Methods 1 & 2, `alg:nfa_product:solve`
  / `alg:dfa_product:solve`); black-boxed, returns the controller $C$.
- **Definition:** solve the product game $P$ — the system tries to reach $F_P$ —
  and extract the `Controller`, or report unrealizable; a thin wrapper over
  Spot's synthesis pipeline (`set_synthesis_outputs` → `split_2step` →
  `solve_game` → `solved_game_to_mealy`). The $\Ofree$ variables are the
  synthesis outputs; the arena input partition ($\Ifree$ vs full $\mathcal{I}$)
  is a deferred `/theory-review` question (tracked in
  `docs/prd/dfa-product.md`).
- **C++:** `solve_dfa(product, vars)` → `std::optional<Controller>` (`nullopt` =
  unrealizable).
- **Do not call it:** solve (bare), `solve_game` (that is Spot's primitive, not
  our wrapper), synthesize (that is the `Synthesis` method), realize.

## The five methods

| Term | `main.tex` | C++ |
|---|---|---|
| NFA product | Method 1 (§nfa) | `NfaProduct` |
| DFA product | Method 2 (§fulldfa) | `DfaProduct` |
| On-the-fly DFA product | Method 3.1 (§otfdfa) | `OtfDfaProduct` |
| On-the-fly aggregated | Method 3.2 (§otfagg) | `OtfAggProduct` |
| Dynamic aggregation | Method 3.3 (§dynamicagg) | `OtfDynAggProduct` |

Common interface: `Synthesis::synthesize(phi, vars, t_in, t_out)`.

## Testing & oracles

### Faithfulness guard
- **`main.tex`:** — (no symbol; test-only, `docs/prd/oracle-faithfulness-guard.md`).
- **Definition:** a mechanical, author-blind-spot-independent cross-check that an
  oracle fixture's produced-trace language $\psiin$ and its $\Tin$ **file** denote
  the same language, by driving the two artifacts the author already wrote against
  each other — the transducer's run engine (`parse_transducer`) versus $\psiin$'s
  finite-$\text{LTL}_f$ membership (`ltlf_to_dfa`) — never a third hand-labeled
  trace (which would inherit the author's blind spot). Fails iff $\psiin$ is too
  **strong** (rejects a trace $\Tin$ produced) or too **weak** (accepts a
  single-bit $\Iknown$ mutation of one).
- **C++:** `run_faithfulness_guard(transducer_src, psi_in, partition)` →
  `GuardResult` (test-local, anonymous namespace in
  `tests/ltlfsynt_oracle_test.cpp`; not a library API).
- **Do not call it:** faithfulness check/test (bare), oracle guard, sanity check.

### Controller verifier
- **`main.tex`:** — (no symbol; decides the `\cref{def:probDefTransducer}`
  postcondition, §129–131; `docs/prd/controller-verifier.md`).
- **Definition:** the internal linchpin oracle — given $\varphi,\Tin,\Tout$ and a
  synthesized controller $T_C$, decide whether **every trace agreeing with
  $\Tin,\Tout,T_C$ satisfies $\varphi$** (`\cref{def:probDefTransducer}`). Built
  **directly on agreement**, not on the monolithic conjecture (`main.tex:133`);
  checks **reachability of $F_\varphi$ under adversarial env** (a one-player
  $\nu$-fixpoint on the $A_\varphi\times\Tin\times\Tout\times T_C$ product, since
  $T_C$ is fixed), *not* language inclusion. Reuses `ltlf_to_dfa` + `consistent`
  but **never** `solve_dfa`/`solve_game`, so it stays independent of the method it
  audits.
- **C++:** `verify_controller(phi, vars, t_in, t_out, t_c)` (and a `Controller`
  overload) → `VerifyResult { bool ok; std::optional<Witness> counterexample; }`;
  `Witness { std::vector<bdd> prefix; std::vector<bdd> cycle; }` is the
  counterexample lasso (empty `cycle` ⇒ a $\neg F_\varphi$ dead-end).
- **Do not call it:** model checker / model_check (bare — that is the *CLI flag*
  `--model-check`, not the function), checker, validator, `agrees` (that is the
  per-trace strategy-agreement predicate).

---

## Open theory questions (tracked, do not re-flag as novel)

These are the author's own `\na` notes / stubs in `main.tex`; `/theory-review`
is seeded with them:

- **`FP` is unspecified** — Algorithm "Forward Progression" is a `TODO`.
- **Aggregated final-state overwrite** — Alg. "On The Fly Aggregated DFA
  Product": inserting into $F_P$ keyed on $[\psi']$ may be overwritten when the
  same $[\psi']$ later returns $b=\bot$; unresolved whether to remove.
- **On-the-fly game solving** — Method 3 builds the product on the fly but still
  solves at the end; the hanging-fruit on-the-fly *solving* is not done.
- **Trace-termination semantics** (`main.tex:96` `\na`) — `def:probDef` quantifies
  over "every trace that agrees" without saying who ends the trace. Both
  `solve_dfa` and the *Controller verifier* commit to the mainstream
  **system-controlled-termination reachability** reading (De Giacomo–Vardi). They
  **must** share it, or "every `solve_dfa` controller verifies" fails for a
  semantic, not a bug, reason — flagged for `/theory-review`
  (`docs/prd/controller-verifier.md`).

**Resolved (kept here so they are not re-flagged as novel):**

- **Line-84 parameter gap** — *resolved.* `main.tex` §86 now names the missing
  variable set as $\Sigma_0\in\{\Ifree,\ \mathcal{I}\cup\Ofree,\ \mathcal{I}\}$
  and writes the intersection $v_t\cap\Sigma_0$; the code's `sigma0_cube`
  instantiates exactly that slice (see *Observed / produced slice*).
- **Partial transducers** — *resolved.* Settled by `main.tex` §107–116 and the
  *enabled* predicate (`\cref{def:enabled}`), valid for all methods. The
  `std::optional` return on `Transducer::delta` / `::lambda` (`nullopt` =
  undefined) is **final**, not tentative: a non-enabled letter is skipped
  (all methods, `\cref{def:enabled}`). The project commits to
  the Case-A regime, so partial and total transducers are language-equivalent.
  See `docs/prd/concrete-transducer.md`.
