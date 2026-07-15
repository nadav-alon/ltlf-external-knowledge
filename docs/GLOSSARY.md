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

### Closed universe of APs
- **`main.tex`:** $\mathcal{I}\cup\mathcal{O}$ (§Problem Definition; no dedicated macro).
- **Definition:** the set every atomic proposition must lie in — an AP of
  $\varphi$, of a transducer file's HOA header, or of a `%%LAMBDA` formula
  outside it is a validation error (`std::invalid_argument`), never silently
  registered. This is the *variable* set; the set of *letters* over it is the
  **Letter alphabet** below.
- **C++:** `VariablePartition::universe()` (= `inputs()` ∪ `outputs()`;
  introduced by `docs/prd/architecture-cleanup.md`, replacing the four
  hand-built `universe` locals).
- **Do not call it:** all APs, AP set (bare), vocabulary, alphabet (that is
  the letter set $2^{\mathcal{I}\cup\mathcal{O}}$, not the variable set).

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

### Turn order
- **`main.tex`:** the single canonical per-step order (§83); *named* "turn order"
  at §107; committed to **Mealy** by the `\na` at §100.
- **Definition:** the order the parties move in at **every** step — the
  environment picks $\Ifree$, then $\Sin$ produces $\Iknown$, then $S_C$ produces
  $\Ofree$, then $\Sout$ produces $\Oknown$ — where *each party observes every
  earlier party's moves and nothing later*. It is what makes each $\lambda$'s
  **observed slice** $\Sigma_0$ well-defined (see *Observed / produced slice*:
  $\Ifree$ for $\Tin$, $\mathcal{I}\cup\Ofree$ for $\Tout$, $\mathcal{I}$ for
  $T_C$), and it is **Mealy** — $S_C$ sees the *same* step's $\mathcal{I}$, not
  only earlier steps. A Moore option is anticipated but unbuilt (§100 `\na`; see
  *Open theory questions*).
- **C++:** **no single identifier** — turn order is *encoded*, and each
  *Representation* encodes it differently. The two are easy to confuse and are
  not interchangeable:
  - **explicit:** turn order is **structural** — `solve_dfa`'s `split_2step`
    builds the env-moves-then-system-moves arena, so the BDD variable order is
    irrelevant to correctness. Separately, `LetterAlphabet` (`product.hpp`)
    registers APs in the block order $\Ifree,\Iknown,\Ofree,\Oknown$ so that a
    **letter's index bits** are turn-ordered (`ifree_index(idx)` masks the low
    bits). That is an *indexing* convention, is the class's **own invariant, not a
    caller obligation**, and holds at **any** BDD level order.
  - **mtdfa:** turn order is carried **only** by the **BDD variable order on the
    shared `bdd_dict`** — there is no split arena, and the MTDFA game reads the
    move order straight off the variable levels. So here it *is* a **caller
    obligation**: `register_turn_order_aps(vars, dict)` establishes it at dict
    setup; `require_turn_order_aps(vars, dict)` throws `std::invalid_argument` if
    it is violated. See *Game solving*, `docs/prd/mtdfa-product.md` Phase 0/Q2.

  Two properties of the mtdfa encoding are load-bearing and non-obvious.
  **(1) The rule `require_turn_order_aps` enforces is the *necessary and
  sufficient* one:** every $\Ifree$ variable strictly **above** every
  *controllable* ($\Ofree\cup\Iknown\cup\Oknown$ — $\Iknown,\Oknown$ are
  controllable-but-forced, see *Game solving*). Order *among* the controllables is
  **free**. It deliberately does **not** demand the registrar's canonical
  $\Ifree,\Iknown,\Ofree,\Oknown$ sequence: that is a convention, and a check that
  rejected a correct-but-non-canonical order would be a bug source, not a guard.
  **(2) Violating it does not crash** — it silently reinterprets the game as
  **Moore** and returns a wrong "unrealizable". And since `register_ap` is
  **idempotent**, the order can only ever be *established* before an AP's first
  registration — **never repaired afterwards**. That is why the ordering lives at
  dict setup rather than inside a method, and why there is no
  `ltlf_ek::ltlf_to_mtdfa` wrapper (see *Goal DFA construction*).
- **Do not call it:** player order, move order, step order, canonical order
  (bare), priority; **variable order / AP order (bare)** — those name one
  *encoding*, not the concept; mode (reserved for the Mealy/Moore axis, see
  *Role*). For the mtdfa functions, not `register_ek_ap_order` (stutters inside
  `ltlf_ek::`) or `assert_mtdfa_ap_order` (it **throws**; `assert` implies an
  `NDEBUG`-compiled-out check) — both were `docs/prd/mtdfa-product.md` Phase 0
  placeholders, superseded here; nor `set_var_order`, `force_ifree_first`,
  `check_ap_order`.

### Controller (system strategy)
- **`main.tex`:** $S_C$ / $T_C$, signature $\ldots\to 2^{\Ofree}$ (Def. probDef / probDefTransducer).
- **Definition:** the strategy we synthesize; produces the free outputs.
- **C++:** `Controller`.
- **Do not call it:** solution, policy, winning strategy.

### Agreement
- **`main.tex`:** "$\varphi$ *agrees* with $S$" — $v_t\cap \Sigma_1 = S(v_0\cdots v_{t-1},\ v_t\cap \Sigma_0)$ (§90).
- **Definition:** a trace is consistent with a strategy at every step: what $S$ is
  *responsible for* ($\Sigma_1$) is exactly what it produces from the history plus
  what it is *allowed to see* ($\Sigma_0$) — the two slices *Turn order* fixes.
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
- **C++:** `Transducer::delta(q, v)` (per-letter). The **symbolic** (whole-set)
  form is `Transducer::delta_edges(q)` → `std::vector<std::pair<bdd, unsigned>>`,
  the deterministic δ out of $q$ as `(guard, dst)` pairs (for
  `OutputLabeledTransducer`, its `twa_graph` out-edges; acceptance ignored). A
  letter covered by no guard is δ-undefined there (partial transducer). Same δ,
  region form — lets a symbolic product build compute successors without
  enumerating letters (new; `docs/prd/symbolic-dfa-product.md`).
- **Do not call it:** step, next, move; for the symbolic form, not `successors`,
  `transitions`, `out_edges`/`edges` (bare).

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

### Letter alphabet
- **`main.tex`:** $\Sigma$, with $\Sigma=2^{\mathcal{I}\cup\mathcal{O}}$
  (§Transducers §105); the enumeration / index structure is code-only.
- **Definition:** the full-letter alphabet materialised as an explicitly
  enumerated vector of letters, LSB-first over a **fixed $\Ifree$-first
  variable order** ($\Ifree$, $\Iknown$, $\Ofree$, $\Oknown$; lexicographic
  within each block) — i.e. *Turn order*, encoded as **letter-index bit
  positions**, so a letter index's low bits are exactly its $\Ifree$
  combination (`ifree_index(idx)`). The AP registration order is the class's
  own invariant, not a caller obligation — this is the type that replaces the
  former `io_vars`-ordering comment-contract between the *Product* core and
  the *Controller verifier*.
  **Not the mtdfa route's ordering contract** (*Turn order*, mtdfa): that one is
  about actual **BDD variable levels** on the shared dict and *is* a caller
  obligation. This class's indexing is level-agnostic — it works at any BDD order,
  and constructing a `LetterAlphabet` does **not** discharge the mtdfa obligation.
  (It registers the same blocks in the same sequence, so on a *fresh* dict it
  would establish that order incidentally — do not rely on that.)
- **C++:** `LetterAlphabet` (`product.hpp`; consumed by `build_product`,
  whose `ProductNode` edges index into `letters()`). Introduced by
  `docs/prd/architecture-cleanup.md`, absorbing the former free function
  `all_letters` (now file-local to `src/product.cpp`).
- **Do not call it:** `all_letters` (the absorbed free function),
  alphabet (bare), Letters, LetterEnumeration, letter set, Sigma (bare —
  that is the math symbol, not the C++ type).

### NFA / DFA for the Goal
- **`main.tex`:** $N$ (Method 1), $A$ (Method 2); `LtlfToNfa` / `LtlfToDfa`.
- **Definition:** the automaton recognizing the models of $\varphi$, in the
  **explicit** *Representation* — states are graph nodes, acceptance is
  **state-based** ($F_D$ = the accepting states). Contrast *MTDFA* below, the
  same automaton in the mtdfa representation.
- **C++:** `spot::twa_graph_ptr` (the automaton *object*; the DFA is built by
  `ltlf_to_dfa` — see *Goal DFA construction* below).
- **Do not call it:** the graph, the machine.

### MTDFA (multi-terminal DFA)
- **`main.tex`:** — (no symbol; a Spot data structure, `\cite duret.25.ciaa`. The
  `\na` at `main.tex:335` gestures at it — *"This likely requires adjusting the
  definitions for MTDFA usage"* — but no definition commits to it.)
- **Definition:** a DFA held as **one MTBDD per state** (the `states[]` array),
  with each destination encoded in a *terminal* as $2d+b$ ($d$ = destination state,
  $b$ = the final bit) and `bddtrue`/`bddfalse` as the accepting/rejecting sinks.
  Acceptance is therefore **transition-based**, unlike the state-based *NFA / DFA
  for the Goal* — the two are **not** interchangeable without `mtdfa::as_twa()`,
  and that conversion is precisely the cost `docs/prd/mtdfa-product.md` exists to
  avoid. Because `bddfalse` already *is* the rejecting sink, completion is
  implicit: an MTDFA needs no `complete_here`.
- **C++:** `spot::mtdfa_ptr` (`spot/twaalgos/ltlf2dfa.hh`); built by
  `spot::ltlf_to_mtdfa`. Terminal manipulation is **not** public Spot API (only
  `ltlf_translator`, which Spot's own header marks *"Semi-internal… Do not rely on
  the interface to be stable"*), so a bespoke MTDFA product is unavailable — hence
  the *Output-agreement automaton* + `spot::product` route (see *Representation*).
- **Do not call it:** MTBDD (that is the node structure **one** state is held in,
  not the automaton), symbolic DFA (*symbolic* is **taken** — see *Product*,
  `build_product_symbolic`), multi-terminal BDD, mtdfa (bare, in prose — the type
  is `spot::mtdfa_ptr`, the concept is "an MTDFA").

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
- **C++:** per *Representation* —
  - **explicit:** `ltlf_to_dfa(phi, dict)` → `spot::twa_graph_ptr` (see *NFA / DFA
    for the Goal*). Pays `as_twa` + `complete_here`.
  - **mtdfa:** `spot::ltlf_to_mtdfa(phi, dict)` → `spot::mtdfa_ptr` (see *MTDFA*),
    called **directly**, stopping there — no `as_twa`, no `complete_here`
    (completion is implicit). **There is no `ltlf_ek` wrapper** — resolved by
    `docs/prd/mtdfa-product.md` Phase 0/Q2 (2026-07-15), which had reserved this
    slot for one that would force $\Ifree$-first AP registration on the shared
    `bdd_dict`. A wrapper **cannot** do that: `register_ap` is idempotent, so by
    the time a `Synthesis` method runs the APs are already registered and the
    wrapper could only validate. The ordering is a **dict-setup obligation**
    instead — see *Turn order*. (Had one survived, `ltlf_to_mtdfa` would have been
    unavailable as its name: it is Spot's own primitive, the same rule that rejects
    `ltlf2dfa` below.)

  The Method-1 NFA route (`LtlfToNfa`, `NfaToDfa`) is not yet named — future, when
  Method 1 lands.
- **Do not call it:** to_dfa, ltlf2dfa (that is Spot's own header/primitive),
  build_dfa, translate.

### Consistency (cons)
- **`main.tex`:** $\cons(q_{in},q_{out},v)$ (`\cref{def:consistency}`, §203) — the per-letter filter.
- **Definition:** $v$'s V-variables are exactly what $\Tin,\Tout$ output.
- **C++:** `consistent(t_in, q_in, t_out, q_out, v)` — the two-transducer
  conjunction `emits(t_in, q_in, v) && emits(t_out, q_out, v)` (see *Output
  agreement*); same §203 concept, delegating the λ-agreement to `emits`.
- **Do not call it:** agrees, valid, matches, feasible.

### Output agreement (emits)
- **`main.tex`:** — (no symbol; code-only) — one conjunct of $\cons$
  (`\cref{def:consistency}`, §203), applied per transducer.
- **Definition:** a full letter $v$ *agrees with* one transducer $\tau$'s
  committed output at state $q$: $\tau$'s $\lambda$ is defined at $(q,v)$ **and**
  $v$ lies in the $\Sigma_1$ cube it commits to (`(v & lambda) != bddfalse`).
  This is the $\Sigma_1$-agnostic λ-agreement atom — neutral about which
  $\Sigma_1$: $V$ for $\Tin/\Tout$, $\Ofree$ for $T_C$, so on $T_C$ it is **not**
  $\cons$ (since $\Ofree\notin V$). δ-definedness is **not** part of `emits` (the
  caller reads it off the successor), matching $\cons$'s λ-only shape.
- **C++:** `emits(t, q, v)` (`consistency.hpp`, per-letter); $\cons$ is
  `emits(t_in,…) && emits(t_out,…)`, and the *Product* filter is
  `all_of(taus, emits)`. See `docs/prd/transducer-product.md`. The **symbolic**
  (whole-set) form is `Transducer::emits_region(q)` → `bdd`: the region of every
  letter whose $\Sigma_1$ slice agrees with $\lambda$ at $q$. For
  `OutputLabeledTransducer` it is exactly `lambda_by_state_[q]` — because that
  relation ranges over $\Sigma_0\cup\Sigma_1$ **only**, region membership ⟺
  per-letter `emits(t,q,v)` (the load-bearing invariant is this variable scope,
  *not* λ-functionality — the equivalence holds even for a non-functional
  relation; `bddfalse` when λ is undefined, matching `emits`'s
  `nullopt`⇒`false`). The
  symbolic $\cons$ region is `emits_region(q_in) & emits_region(q_out)`, no
  separate function (new; `docs/prd/symbolic-dfa-product.md`; faithfulness to
  `\cref{def:consistency}` flagged for `/theory-review`).
  The **automaton** form is the *Output-agreement automaton* `emits_dfa(tau, dict)`
  → `spot::twa_graph_ptr` (new; `docs/prd/mtdfa-product.md`): the DFA accepting
  exactly those words whose every letter agrees with $\lambda$ along $\tau$'s run —
  $\delta$ from `delta_edges` guarded by `emits_region`, all of $Q$ accepting,
  uncovered letters to a rejecting sink; deterministic **and** complete by
  construction. Exactly as with the symbolic form, the automaton $\cons$ is the
  **intersection** `product(emits_dfa(t_in), emits_dfa(t_out))` and there is **no
  cons automaton** — $\cons$ has a per-letter form and nothing else. Where the
  per-letter/symbolic forms *skip* a non-agreeing letter (`\cref{def:enabled}`),
  the automaton form *sinks* it; that these coincide is load-bearing and is flagged
  for `/theory-review` (`docs/prd/mtdfa-product.md`).
- **Do not call it:** agrees (that is per-trace *Agreement*), consistent /
  consistent_with (that is the two-transducer $\cons$), commits, produces (that
  is the *Produced-trace language*), matches; for the symbolic form, not
  `emits_set`, `agreeing_region`, `lambda_region`; for the automaton form, not
  `cons_dfa` / cons-DFA / *Consistency automaton* (that names the two-transducer
  $\cons$, which has no automaton form — this object takes **one** transducer),
  `lambda_dfa`, `agreement_dfa` (that is per-trace *Agreement*), filter automaton.

### Product
- **`main.tex`:** $P$ (Methods 1 & 2); states $S\times Q_{in}\times Q_{out}$
  (generalized in code to $S\times Q_1\times\cdots\times Q_n$).
- **Definition:** the Goal automaton crossed with both knowledge transducers,
  keeping only consistent transitions (a non-consistent letter is skipped, as in
  all methods — `\cref{def:consistency}`, §203; partiality note §211).
- **C++:** `ProductState` (a struct — `unsigned goal` + `std::vector<unsigned>
  taus` — generalized to N transducers), the reusable `agreeing_successor`
  (lazy per-letter step) and `build_product` (eager per-letter driver → neutral
  map) in `product.hpp`, plus the `*Product` synthesis classes. See
  `docs/prd/transducer-product.md`. The **symbolic** builder is
  `build_product_symbolic(goal, taus, init)` → `ProductGuards` (new;
  `docs/prd/symbolic-dfa-product.md`), which computes the per-destination guards
  directly from `delta_edges`/`emits_region` + Goal out-edges, no minterm loop;
  `DfaProduct` uses it while the per-letter `build_product` stays for
  `verify_controller` and as the build-equivalence reference. `ProductGuards` is
  the shared neutral per-dst guard map both builds emit
  (`map<ProductState, {bool acc; map<ProductState, bdd> guard}>`);
  `materialize_product(pg, dict)` → `spot::twa_graph_ptr` turns it into the game
  automaton (state-based Büchi $F_P$), and `to_guard_map(graph, alphabet)`
  compresses the per-letter `build_product` output into `ProductGuards` (the
  former inline `guards[dst] |= letters[idx]` loop).
  All of the above is the **explicit** *Representation*. In the **mtdfa**
  representation the product is a **language intersection** —
  `spot::product(spot::product(goal_mtdfa, emits_in), emits_out)` over the
  *Output-agreement automaton* of each transducer lifted by
  `spot::twadfa_to_mtdfa` (new; `docs/prd/mtdfa-product.md`). There is **no**
  `ProductState`, `ProductGuards` or `materialize_product` on that route: Spot owns
  the state numbering and terminal encoding, so the *Build-equivalence metamorphic
  oracle* does not apply to it (nothing to diff), and the two representations are
  cross-checked on **realizability verdicts** instead.
- **Do not call it:** composition, join, cross; for the symbolic pieces, not
  `symbolic_product`/`product_symbolic` (build), `GuardMap`/`product_map`
  (`ProductGuards`), `materialize`/`to_twa` (bare, for `materialize_product`).

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
- **C++:** per *Representation*, both → `std::optional<Controller>` (`nullopt` =
  unrealizable) —
  - **explicit:** `solve_dfa(product, vars)`.
  - **mtdfa:** `solve_mtdfa(product, vars)` (new; `docs/prd/mtdfa-product.md`),
    wrapping `set_controllable_variables` → `mtdfa_winning_strategy` →
    `mtdfa_strategy_to_mealy`. **Precondition:** the *Turn order* AP-ordering
    contract (`require_turn_order_aps`) — this route reads turn order off the BDD
    variable order alone, and a violation returns a wrong verdict rather than
    failing.

  They differ in **where the governed variables are projected**, and that is the
  interesting part: `solve_dfa` projects **arena-side** (`bdd_exist` per edge
  guard, `src/solve_dfa.cpp:49`), which `solve_mtdfa` **cannot** do — an MTDFA's
  destination lives inside its terminal, so quantifying would cross destinations.
  Instead `solve_mtdfa` makes $\Iknown,\Oknown$ **controllable** ($\cons$ pins each
  to exactly one legal value, so it is a *forced* move, not a real choice) and
  projects **strategy-side**, off the `twa_graph` that `mtdfa_strategy_to_mealy`
  returns. Both discharge the same `main.tex:300` `\na` by different routes — see
  *Open theory questions*.
- **Do not call it:** solve (bare), `solve_game` / `mtdfa_winning_strategy` (those
  are Spot's primitives, not our wrappers), synthesize (that is the `Synthesis`
  method), realize.

## The five methods

| Term | `main.tex` | C++ (explicit) | C++ (mtdfa) |
|---|---|---|---|
| NFA product | Method 1 (§nfa) | `NfaProduct` | — |
| DFA product | Method 2 (§fulldfa) | `DfaProduct` | `MtdfaProduct` |
| On-the-fly DFA product | Method 3.1 (§otfdfa) | `OtfDfaProduct` | — |
| On-the-fly aggregated | Method 3.2 (§otfagg) | `OtfAggProduct` | — |
| Dynamic aggregation | Method 3.3 (§dynamicagg) | `OtfDynAggProduct` | — |

Common interface: `Synthesis::synthesize(phi, vars, t_in, t_out)`.

**Five rows, five methods.** The last two columns are the *Representation* axis
(below), **not** more methods — a sixth row would assert a sixth method, which is
exactly what `MtdfaProduct` is not. Only Method 2 has an mtdfa implementation today
(`docs/prd/mtdfa-product.md`); `main.tex:335`'s `\na` anticipates one for Method 3.
`make_synthesis_method` (`cli.hpp`) selects a **cell**: `--dfa-product` →
`DfaProduct`, `--mtdfa-product` → `MtdfaProduct`. That flag shape names a
method×representation cell rather than a method, and is a **known wart** — it does
not scale (a Method-3 mtdfa route would want `--mtdfa-otf-dfa-product`). Accepted
deliberately over an orthogonal `--representation=` selector; revisit if a second
method gains an mtdfa route.

### Representation
- **`main.tex`:** — (no symbol; the `\na` at `main.tex:335` gestures at MTDFA for
  Method 3, but no definition commits to the axis).
- **Definition:** *prose note, not a domain entry* — pinned here to fix the spelling
  and stop drift. Which data structure a method holds its automata in: **explicit**
  (`spot::twa_graph`, see *NFA / DFA for the Goal*) or **mtdfa** (`spot::mtdfa`, see
  *MTDFA*). It is **orthogonal to the method**: `main.tex` has five methods, and a
  representation changes *how* one is executed, never *which* one it is.
  `MtdfaProduct` is Method 2 in the mtdfa representation — **not** a sixth method.
  The axis reaches three entries: *Goal DFA construction*, *Product*, and *Game
  solving* each name a per-representation C++ identifier.
- **C++:** — (no identifier; a modifier, not a type. There is deliberately **no**
  `enum class Representation`: the axis is selected per-method by CLI flag, and each
  cell is its own `Synthesis` class.)
- **Do not call it:** symbolic (**taken** — see *Product*, `build_product_symbolic`,
  which is the *explicit* Method 2's product build, not this axis), method, Method 6,
  backend, mode (reserved for the Mealy/Moore axis), variant, flavour.

## Benchmarking

### Canonical benchmarking stage
- **`main.tex`:** — (no symbol; code-only observability, `docs/prd/benchmarking.md`).
  Benchmarking is infrastructure with no math; the stage *values* alias existing
  spine-algorithm terms.
- **Definition:** one of a small **closed, soft registry** naming the timing axes
  that are **comparable across methods** — `automaton_construction` (*Goal DFA
  construction*, or the future NFA build), `product_construction` (*Product*),
  `game_solving` (*Game solving / SolveDfa*), `aggregation` (*Aggregation*,
  Methods 3.2/3.3 only). Two methods' same-named canonical stages are defined to
  be comparable; a stage only *some* methods emit is fine (comparability means
  "when both emit it, they compare"). The registry is **soft**: adding, renaming,
  or re-mapping a value touches only the enum + its name table + this entry
  (never the generic collector/emitter) and is **not** a PRD-change event. Its
  counterpart is the **free-form sub-span** — an arbitrary-string nested span
  needing no registry change (the "a new phase needs no infra" tier).
- **C++:** `enum class Stage { automaton_construction, product_construction,
  game_solving, aggregation }` with `stage_name(Stage)` → `std::string_view`
  (`include/ltlf_ek/bench.hpp`). The recording / emission plumbing (`BenchScope`,
  `BenchTimer`, `BenchSpan`, `BenchReport`) is **infra, not a domain concept**, so
  — like the `cli.hpp` CLI plumbing — it gets **no** glossary entry.
- **Do not call it:** phase, step, part (for a canonical stage); metric /
  measurement (that is the recorded datum, not the axis); for the free-form tier,
  not "custom stage" (it is a *sub-span*, not a registry value).

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

### Generated corpus & its grading modes (differential / metamorphic round-trip)
- **`main.tex`:** — (no symbol; test-only harness, `docs/prd/generated-corpus-oracle.md`).
- **Definition:** *prose note, not a domain entry* — these are **generic testing
  methodology** terms (industry-standard differential / metamorphic testing), not
  project domain concepts, so they carry no canonical C++ *domain* identifier;
  pinned here only to fix the spelling and stop synonym drift. A **generated
  corpus** is the fixed-seed list of `(phi, partition, Tin)` cases the generator
  emits; it is graded by two **self-labeling** modes — the **differential** (the
  built `ltlf-ek-synth` vs Spot's `ltlfsynt` must agree on the verdict) and the
  **metamorphic round-trip** (`synthesize`$\to$`verify_controller` must accept the
  controller it produced) — plus the `ltlf_to_dfa` structural free-rider
  (determinism + completeness). The oracle *is* the label; there is no
  hand-authored expected value.
- **C++:** test-local (anonymous namespace in `tests/ltlfsynt_oracle_test.cpp`),
  **not** a library API: `BuildGeneratedCorpus()` / `GeneratedCase`, the
  generators (`generate_random_formula`, `strengthen_next`, `random_partition`,
  `random_tin`), and the three `GeneratedCorpus.*` / `GeneratedCorpusDifferential`
  test bodies. No canonical domain type — like the *Faithfulness guard*, it names
  a test artifact, not a `main.tex` concept.
- **Do not call it:** fuzzing / property test (bare — it is seeded + self-labeling,
  not shrinking random), golden corpus (there is no golden expected value),
  round-trip test (bare), differential test (bare — pair the noun with *oracle* /
  *round-trip* as above).

### Build-equivalence metamorphic oracle
- **`main.tex`:** — (no symbol; test-only, `docs/prd/symbolic-dfa-product.md`).
- **Definition:** *prose note, not a domain entry* — a **metamorphic** cross-check
  between **two builds of the same method**: `build_product_symbolic` and the
  per-letter `build_product` (compressed via `to_guard_map`) must yield the
  **identical** product game — same reachable `ProductState`s, same acceptance,
  and per-`⟨src,dst⟩` a **BDD-equal** guard (`==`; BuDDy canonicalises, so
  structural equality is semantic). It compares the **game / arena** ($P$, the
  input to `solve_dfa`), *not* realizability and *not* a `Controller` (winning
  strategies are non-unique — not comparable). This is the direct check on the
  symbolic rewrite's likeliest bug class (lost / mis-grouped transitions, the
  `|=`→`=` seeded bug); it complements — does **not** replace — the realizability
  oracles (*Controller verifier*, monolithic baseline, the corpus *differential*).
- **C++:** test-local (no library API); a dedicated assertion plus a library-only
  body over the *Generated corpus*.
- **Do not call it:** **differential** (bare — that is the corpus's *realizability*
  cross-check against `ltlfsynt`, a different oracle), build differential,
  product diff.

---

## Open theory questions (tracked, do not re-flag as novel)

These are the author's own `\na` notes / stubs in `main.tex`; `/theory-review`
is seeded with them:

- **`FP` is unspecified** — Algorithm "Forward Progression" is a `TODO`.
- **Aggregated final-state overwrite** — Alg. "On The Fly Aggregated DFA
  Product": inserting into $F_P$ keyed on $[\psi']$ may be overwritten when the
  same $[\psi']$ later returns $b=\bot$; unresolved whether to remove.
- **On-the-fly game solving** — Method 3 builds the product on the fly but still
  solves at the end; the hanging-fruit on-the-fly *solving* is not done. The same
  `\na` continues (`main.tex:335`): *"This likely requires adjusting the definitions
  for MTDFA usage"* — i.e. the author anticipates a *Representation* change for
  Method 3. Only Method 2 has one today (`docs/prd/mtdfa-product.md`).
- **Governed-variable projection** (`main.tex:300` `\na`) — *"Because the resulting
  game is being limited to transitions consistent with the external knowledge
  transducers, which govern the variable set $\mathcal{V}$, it can project these
  variables out without loss."* The supporting argument is drafted but **commented
  out** (`main.tex:302–303`), so the claim is currently unbacked in the live text.
  Both *Game solving* routes depend on it and discharge it **differently** —
  `solve_dfa` arena-side, `solve_mtdfa` by pinning the variables as forced
  controllable moves and projecting strategy-side. Flagged for `/theory-review`
  (`docs/prd/mtdfa-product.md`). Newly load-bearing; not previously listed here.
- **Mealy is baked into the signatures; no Moore option** (`main.tex:100` `\na`) —
  *"these signatures are commiting to a mealy turn order, and are not ready for
  adding a moore option. For that, the signatures would be dependent on the order
  of players."* Newly listed here because Phase 0/Q2 made it concrete rather than
  hypothetical: see *Turn order*. In the **explicit** *Representation* the
  Mealy commitment is structural (`split_2step`), but in the **mtdfa** one it is
  *nothing but* the BDD variable order — inverting that order is exactly what turns
  the game Moore, and it is a silent reinterpretation, not an error. Evidence the
  inversion is all a Moore option would take, mechanically: Spot's own `ltlfsynt`
  selects between the two with one flag on the same machinery
  (`bin/ltlfsynt.cc:481`, `unsigned val = mealy_semantics ? 1 : 2;`, over the
  comment *"For Mealy semantics, inputs should appear first in the MTBDDs. For
  Moore semantics, outputs should be first."*). The *math* is the open part, not
  the plumbing: §100 says the signatures themselves must change. Out of scope for
  `docs/prd/mtdfa-product.md`; `mode` stays reserved for this axis (see *Role*).
- **Trace-termination semantics** (`main.tex:96` `\na`) — `def:probDef` quantifies
  over "every trace that agrees" without saying who ends the trace. Both
  `solve_dfa` and the *Controller verifier* commit to the mainstream
  **system-controlled-termination reachability** reading (De Giacomo–Vardi). They
  **must** share it, or "every `solve_dfa` controller verifies" fails for a
  semantic, not a bug, reason — flagged for `/theory-review`
  (`docs/prd/controller-verifier.md`). `solve_mtdfa` adds a **third** consumer
  carrying **Spot's own** reading (`mtdfa_winning_strategy`); if it diverges,
  cross-method agreement between the two *Representation*s fails semantically
  rather than as a bug. Weak evidence it agrees: `ltlfsynt` runs that machinery and
  the existing differential already passes for `DfaProduct`.

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
