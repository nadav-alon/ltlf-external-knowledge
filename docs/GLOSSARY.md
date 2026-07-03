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
- **`main.tex`:** $\Sigma_0,\Sigma_1\subseteq\Sigma$ (§Transducers §103); instantiated
  per transducer in the align block (§112–120).
- **Definition:** the cube of variables a transducer may **observe** ($\Sigma_0$)
  versus the cube it **produces** ($\Sigma_1$); for $\Tin$ this is
  $(\Ifree,\,\Iknown)$, for $\Tout$ it is
  $(\mathcal{I}\cup\Ofree,\,\Oknown)$.
- **C++:** `sigma0_cube` / `sigma1_cube` (`bdd` cubes; construction-time data of
  `OutputLabeledTransducer`).
- **Do not call it:** input/output mask, visible set (bare), domain/range.

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
- **C++:** `spot::twa_graph_ptr` (built via a `LtlfToNfa` / `LtlfToDfa` wrapper).
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
- **`main.tex`:** — (the align-block choice of $\Sigma_0/\Sigma_1$, §122–128).
- **Definition:** which external knowledge strategy a transducer file
  materialises, selecting the observed/produced slices: `t_in` ⇒
  $\Sigma_0=\Ifree,\Sigma_1=\Iknown$; `t_out` ⇒
  $\Sigma_0=\mathcal{I}\cup\Ofree,\Sigma_1=\Oknown$.
- **C++:** `enum class Role { t_in, t_out }`.
- **Do not call it:** direction, kind, mode (mode is the reserved Mealy/Moore axis).

### Parse a transducer
- **`main.tex`:** — (no symbol; materialisation of $\tau$ from its file).
- **Definition:** read one transducer file (HOA $\delta$ + `%%LAMBDA` $\lambda$)
  on a shared `bdd_dict` and build the concrete `OutputLabeledTransducer`,
  orienting $\lambda$ from `(partition, role)` and validating $\delta$
  determinism, $\lambda$ functionality, AP scope, and state coverage.
- **C++:** `parse_transducer(in, partition, role, dict)`.
- **Do not call it:** load, read (bare), deserialize, from_hoa.

## Algorithms

### Consistency (cons)
- **`main.tex`:** $\cons(q_{in},q_{out},v)$ (Method 1) — the per-letter filter.
- **Definition:** $v$'s V-variables are exactly what $\Tin,\Tout$ output.
- **C++:** `consistent(t_in, q_in, t_out, q_out, v)`.
- **Do not call it:** agrees, valid, matches, feasible.

### Product
- **`main.tex`:** $P$ (Methods 1 & 2); states $S\times Q_{in}\times Q_{out}$.
- **Definition:** the Goal automaton crossed with both knowledge transducers,
  keeping only consistent transitions (Method 2 sends the rest to $\bot$).
- **C++:** `ProductState` / the `*Product` synthesis classes.
- **Do not call it:** composition, join, cross.

### Sink (⊥)
- **`main.tex`:** $\bot$, the self-looping failing state (Method 2).
- **Definition:** absorbing non-accepting state for inconsistent letters.
- **C++:** `kSink`.
- **Do not call it:** dead state, trap, reject.

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

## The five methods

| Term | `main.tex` | C++ |
|---|---|---|
| NFA product | Method 1 (§nfa) | `NfaProduct` |
| DFA product | Method 2 (§fulldfa) | `DfaProduct` |
| On-the-fly DFA product | Method 3.1 (§otfdfa) | `OtfDfaProduct` |
| On-the-fly aggregated | Method 3.2 (§otfagg) | `OtfAggProduct` |
| Dynamic aggregation | Method 3.3 (§dynamicagg) | `OtfDynAggProduct` |

Common interface: `Synthesis::synthesize(phi, vars, t_in, t_out)`.

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

**Resolved (kept here so they are not re-flagged as novel):**

- **Line-84 parameter gap** — *resolved.* `main.tex` §86 now names the missing
  variable set as $\Sigma_0\in\{\Ifree,\ \mathcal{I}\cup\Ofree,\ \mathcal{I}\}$
  and writes the intersection $v_t\cap\Sigma_0$; the code's `sigma0_cube`
  instantiates exactly that slice (see *Observed / produced slice*).
- **Partial transducers** — *resolved.* Settled by `main.tex` §107–116 and the
  *enabled* predicate (`\cref{def:enabled}`), valid for all methods. The
  `std::optional` return on `Transducer::delta` / `::lambda` (`nullopt` =
  undefined) is **final**, not tentative: a non-enabled letter is skipped
  (Methods 1, 3) or routed to the $\bot$-sink (Method 2). The project commits to
  the Case-A regime, so partial and total transducers are language-equivalent.
  See `docs/prd/concrete-transducer.md`.
