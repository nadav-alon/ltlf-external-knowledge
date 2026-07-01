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
  $\lambda:Q\times\Sigma_0\to\Sigma_1$ implementing a strategy.
- **C++:** `Transducer` (abstract base).
- **Do not call it:** Mealy machine, automaton (bare), FST.

### Transition function (delta)
- **`main.tex`:** $\delta:Q\times 2^{\mathcal{I}\cup\mathcal{O}}\to Q$.
- **Definition:** successor over the *full* letter; tracks full state history.
- **C++:** `Transducer::delta(q, v)`.
- **Do not call it:** step, next, move.

### Output function (lambda)
- **`main.tex`:** $\lambda:Q\times\Sigma_0\to\Sigma_1$ (the turn-order-restricted output).
- **Definition:** what the transducer commits to, seeing only its allowed slice.
- **C++:** `Transducer::lambda(q, visible)`.
- **Do not call it:** output (bare), emit, label.

### Letter
- **`main.tex`:** $v\in 2^{\mathcal{I}\cup\mathcal{O}}$.
- **Definition:** one full assignment of all variables at a step.
- **C++:** `bdd v` (a cube over I∪O).
- **Do not call it:** symbol, event, valuation.

### NFA / DFA for the Goal
- **`main.tex`:** $N$ (Method 1), $A$ (Method 2); `LtlfToNfa` / `LtlfToDfa`.
- **Definition:** the automaton recognizing the models of $\varphi$.
- **C++:** `spot::twa_graph_ptr` (built via a `LtlfToNfa` / `LtlfToDfa` wrapper).
- **Do not call it:** the graph, the machine.

## Algorithms

### Consistency (cons)
- **`main.tex`:** $\cons(q_{in},q_{out},v)$ (Method 1) — the per-letter filter.
- **Definition:** $v$'s V-variables are exactly what $T_{in},T_{out}$ output.
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
- **Line-84 parameter gap** — the second argument of $S(\ldots,v_t)$ needs an
  intersection with a not-yet-defined variable set to match the signatures.
