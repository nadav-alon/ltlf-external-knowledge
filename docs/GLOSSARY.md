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
- **Do not call it:** external vars, known set (as a bare noun); **dependent
  variables** — but note this is now a *distinction*, not a ban: since
  `\cref{def:outdep}` (see *Dependent output set*) "dependent" is a technical
  term, and the two are related but not equal. $\Xdep$ is an analysis **result**
  that *becomes* $\Oknown\subseteq\mathcal{V}$ once extraction has run, whereas
  $\mathcal{V}$ is a partition **input** the caller supplies. So a variable may
  be governed without being dependent (a hand-authored $\Tout$), and calling
  $\mathcal{V}$ "the dependent variables" wrongly asserts it came from
  `dependent_outputs`.

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
- **C++:** `spot::twa_graph_ptr` (the automaton *object*; the DFA $A$ is built by
  `ltlf_to_dfa` — see *Goal DFA construction* — and the NFA $N$ by `ltlf_to_nfa`
  — see *Goal NFA construction* — both below).
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
  `spot::minimize_mtdfa` (Moore minimisation) is an optional, separately-measured
  knob on `MtdfaProduct` (constructor param `minimize_mtdfa`, CLI
  `--minimize-mtdfa`, default off; `docs/prd/mtdfa-product.md` Phase 2) — applied
  to the product mtdfa, not the Goal mtdfa alone.
- **Do not call it:** MTBDD (that is the node structure **one** state is held in,
  not the automaton), symbolic DFA (*symbolic* is **taken** — see *Product*,
  `build_product_symbolic`), multi-terminal BDD, mtdfa (bare, in prose — the type
  is `spot::mtdfa_ptr`, the concept is "an MTDFA").

### MTNFA (multi-terminal NFA)
- **`main.tex`:** — (no symbol; a **code-only** data structure on the
  *Representation* axis, the nondeterministic sibling of *MTDFA*. The `\na` at
  `main.tex:335` gestures at MTDFA for Method 3 but commits to no NFA form.)
- **Definition:** the NFA $N$ (*NFA / DFA for the Goal*) held as **one MTBDD per
  state** — the same per-state-MTBDD shape as *MTDFA*, except each **terminal
  encodes a set** of destination states (an interned index into a side table,
  index $0=\emptyset$), not a single destination. That set-valued terminal is
  exactly the nondeterminism, $\delta_N(s,v)\in 2^{S_N}$. Branching stays
  deterministic (one path per letter), so the diagram keeps BDD **canonicity** —
  unlike a general nondeterministic decision diagram (nBDD/nFBDD; the trade-off the
  `docs/BACKLOG.md` "nondeterministic decision diagrams" investigation worried
  about). Acceptance is a per-state $F_N$ bit (the reversal's sole final state),
  **not** on the terminals. A non-covered letter keeps the $\emptyset$ terminal
  (partial $\delta_N$, **no** sink state — `alg:nfa_product` tolerates an empty
  $\delta_N(s,v)$). Determinized into a *MTDFA* by `mtnfa_to_mtdfa` (see *Goal
  automaton determinization*).
- **C++:** `Mtnfa` (`include/ltlf_ek/mtnfa.hpp`) — `std::vector<bdd> states`,
  `std::vector<bool> accepting`, `unsigned initial`, an owned **`mutable`**
  `detail::StateSetPool` interpreting the set-terminals, `spot::bdd_dict_ptr`, `aps`,
  and `spot::twa_graph_ptr source_nfa`. Introduced by `docs/prd/mtnfa.md`, **landed
  2026-07-17**. The last two members are not free-standing domain data: `pool` is
  `mutable` because `mtnfa_to_mtdfa` takes `const Mtnfa&` yet must intern newly-unioned
  successor sets into *this* pool (the terminals in `states` are meaningful only
  relative to it — logical constness), and `source_nfa` keeps the source `twa_graph`
  alive so the AP variables `states`' BDDs reference stay registered on `dict` for the
  `Mtnfa`'s lifetime (the `OutputLabeledTransducer::delta_dfa_` pattern; a `bdd_dict`
  frees an owner's variables when that owner's last reference dies). The set-terminal
  substrate
  (interning + the memoized union apply) is `detail::StateSetPool`
  (`include/ltlf_ek/detail/state_set_pool.hpp`) — internal plumbing, **no domain
  entry of its own** (cf. the `bench.hpp` `BenchTimer`/`BenchScope` infra).
- **Do not call it:** symbolic NFA, nondeterministic mtdfa / nondeterministic
  MTDFA, MTBDD-NFA (MTBDD is the node structure of **one** state, not the
  automaton), nBDD / nFBDD (those are the *lose-canonicity* diagrams the backlog
  investigation contrasts — the MTNFA keeps canonicity), set-terminal DFA, mtnfa
  (bare, in prose — the type is `Mtnfa`, the concept is "an MTNFA").

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
  block (`main.tex:130`, $\lambda_C:Q_C\times2^{\mathcal I}\to2^{\Ofree}$). Unlike
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

### Print a transducer
- **`main.tex`:** — (no symbol; serialization of $\tau$, §Transducers) — the
  exact inverse of *Parse a transducer* above.
- **Definition:** write one transducer to its file representation — the HOA for
  $\delta$, then `--END--`, then the `%%LAMBDA` block with one boolean formula
  per state. As with the reader, $\Sigma_0/\Sigma_1$ are **not** written: the
  format does not carry them (see *Transducer file format*), so a $\Tout$ file
  is unusable without the part file naming $\Oknown$ — which is why
  *Output-dependency extraction* must emit both artifacts, not just the
  transducer.
- **C++:** `print_transducer(out, t)` (`transducer_io.hpp`, beside
  `parse_transducer`; `docs/prd/output-dependencies-tool.md` Phase 1). Reads
  $\delta$ through `OutputLabeledTransducer::delta_dfa()`, whose $\omega$-acceptance
  is **meaningless** (see *Output-labeled transducer*) — the accessor exists for
  this writer and carries the same warning. Round-trips by construction:
  `parse_transducer(print_transducer(t))` reproduces $t$ (same states, BDD-equal
  guards and $\lambda$).
- **Do not call it:** write, dump, serialize, to_hoa, save_transducer,
  `print_hoa` (Spot's own primitive, the same rule that rejects `ltlf2dfa`).

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

  The Method-1 NFA route is split: the NFA construction
  `LtlfToNfa`/`ltlf_to_nfa` is named below (*Goal NFA construction*); the explicit
  `NfaToDfa`/`nfa_to_dfa` (the product's subset determinization) is under *Goal
  automaton determinization* (`docs/prd/nfa-product.md`).
- **Do not call it:** to_dfa, ltlf2dfa (that is Spot's own header/primitive),
  build_dfa, translate.

### Goal NFA construction (LtlfToNfa)
- **`main.tex`:** `LtlfToNfa`$(\varphi)$ (`\cref{alg:ltlftonfa}`,
  Method 1 §`nfa`); black-boxed, returns the NFA $N$ of *NFA / DFA for the Goal*.
- **Definition:** build the **nondeterministic** finite automaton $N$ for the
  Goal formula $\varphi$ via the three-step pipeline $\mirror{\varphi}$ →
  `PastLtlfToDfa` → `Reverse` (see the three entries below),
  giving $L(N)=L(\varphi)$ at single-exponential size (`\cref{thm:nfa-mirror-size}`,
  proof TBD). Counterpart to *Goal DFA construction* but the NFA, not the DFA —
  only the *Product* is later determinized. Like `ltlf_to_dfa`, finiteness is in
  **acceptance marks, not an extra AP** (the sole final state is the reversal's
  $F_N=\{s_{D,0}\}$), the alphabet stays $2^{\mathcal{I}\cup\mathcal{O}}$, and it
  is built on the transducers' shared `bdd_dict`; **unlike** `ltlf_to_dfa` it is
  nondeterministic and **not** completed (`\cref{alg:nfa_product}` tolerates an
  empty $\delta_N(s,v)$).
- **C++:** `ltlf_to_nfa(phi, dict)` → `spot::twa_graph_ptr` (see *NFA / DFA for
  the Goal*). PRD `docs/prd/ltlf-to-nfa.md`, **landed** (all three phases). The
  mtdfa-representation counterpart is `ltlf_to_mtnfa` (see *Goal MTNFA construction*),
  which composes this with the `Mtnfa` lift.
- **Do not call it:** to_nfa, ltlf2nfa, build_nfa, translate; **not** `NfaToDfa`
  (that is the later product-determinization step, `nfa_to_dfa` — see *Goal
  automaton determinization*).

### Formula mirror
- **`main.tex`:** $\mirror{\varphi}$ (`\cref{def:mirror}`, §`nfa`; macro `\mirror`).
- **Definition:** the past-$\text{LTL}_f$ formula obtained from $\varphi$ by the
  syntactic substitution sending each future operator to its temporal dual,
  satisfying $w,0\models\varphi \iff \rev{w},|w|-1\models\mirror{\varphi}$ for
  every **non-empty** trace $w$ (read backwards, evaluated at its last position).
- **C++:** — (no dedicated identifier / no past-formula type). Spot's `op` enum
  has **no** past operators (`Y/S/O/H`), so the mirror is never materialized as a
  `spot::formula`; per `docs/prd/ltlf-to-nfa.md` it is **folded into the M2L-Str
  encoder** of `past_ltlf_to_dfa` — a future-operator walk emitted against
  decreasing positions, which is the same MONA source as the past dual at
  increasing positions.
- **Do not call it:** dual (bare), reverse formula, mirror image, negation,
  complement; not `reverse` (that is the *automaton* reversal below).

### Reverse-language DFA (PastLtlfToDfa)
- **`main.tex`:** `PastLtlfToDfa`$(\mirror{\varphi})$ (§`nfa`,
  `\cref{alg:ltlftonfa}` line `alg:ltlftonfa:mona`); returns the DFA $D$ with
  $L(D)=\{\,\rev{w} : w,0\models\varphi\,\}$ (the **reverse language** of $\varphi$).
- **Definition:** the black box realizing the mirror-to-DFA step via **MONA**
  (`/usr/bin/mona`, v1.4) — the C++ encoder emits M2L-Str for $\varphi$'s
  mirror, shells out to `mona`, and parses MONA's minimal DFA (deterministic,
  complete) back into a `spot::twa_graph` over the shared `bdd_dict`. Single
  exponential in $|\varphi|$. Because the *Formula mirror* is folded into the
  encoder, the C++ entry point takes the **future** $\varphi$ and applies the
  mirror internally (the composition of `PastLtlfToDfa` after the *Formula mirror*).
- **C++:** `past_ltlf_to_dfa(phi, dict)` → `spot::twa_graph_ptr` — **settled** by the
  implementation (`src/past_ltlf_to_dfa.cpp`, PRD `docs/prd/ltlf-to-nfa.md` landed);
  no longer tentative. Internal; the MONA-output parser is a separate `detail` helper
  (`src/mona_dfa.cpp`).
- **Do not call it:** mona (bare), to_dfa, `ltlf_to_dfa` (that is Method 2's
  *future* DFA $A$, double-exponential), reverse_dfa, ltlf2dfa.

### Automaton reversal (Reverse)
- **`main.tex`:** `Reverse`$(D)$ (`\cref{alg:ltlftonfa}` line
  `alg:ltlftonfa:reverse`; §160–169).
- **Definition:** turn the deterministic $D$ into the NFA $N$ for $\varphi$ by
  reversing every edge ($s\xrightarrow{v}t$ in $D$ becomes $t\xrightarrow{v}s$ in
  $N$), adding a **fresh** initial state $s_{N,0}$ with
  $\delta_N(s_{N,0},v)=\{s:\delta_D(s,v)\in F_D\}$, and taking $F_N=\{s_{D,0}\}$.
  The result is **not** completed (an NFA may have an empty $\delta_N(s,v)$);
  dead / unreachable states left by reversing MONA's complete $D$ are purged.
- **C++:** `reverse_dfa_to_nfa(dfa)` → `spot::twa_graph_ptr` — **settled** by the
  implementation (`src/reverse_dfa_to_nfa.cpp`, PRD `docs/prd/ltlf-to-nfa.md` landed);
  no longer tentative. Internal. It gives an edgeless final state an explicit
  `bddfalse`-guarded self-loop so its $F_N$-membership survives a later
  `state_is_accepting` read — the precondition `nfa_to_mtnfa` documents.
- **Do not call it:** transpose, flip, `mirror` (that is the *formula* mirror
  above), `NfaToDfa` (opposite direction, later step), determinize.

### Goal MTNFA construction (ltlf_to_mtnfa / nfa_to_mtnfa)
- **`main.tex`:** `\algname{LtlfToNfa}`$(\varphi)$ (`\cref{alg:ltlftonfa}`) in the
  **mtdfa** *Representation*; no dedicated symbol (returns the *MTNFA* form of $N$).
- **Definition:** build the *MTNFA* for the Goal formula $\varphi$. `nfa_to_mtnfa`
  is the **lift** of an explicit `twa_graph` NFA into an `Mtnfa`: per state, fold
  every out-edge $(\text{cond},\text{dst})$ by set-union of its guarded singleton —
  **overlapping guards merge** (that overlap is the nondeterminism), an uncovered
  letter stays $\emptyset$ — the NFA analog of Spot's `twadfa_to_mtdfa` (**ours**;
  Spot has no `twanfa_to_mtnfa`). `ltlf_to_mtnfa` composes `ltlf_to_nfa`$(\varphi)$
  (see *Goal NFA construction*, MONA-backed) with that lift. Same `(phi, dict)`
  shape and shared-`bdd_dict` precondition as `ltlf_to_nfa`; APs come from
  $\varphi$'s support.
- **C++:** `ltlf_to_mtnfa(phi, dict)` / `nfa_to_mtnfa(nfa)` → `Mtnfa` (see *MTNFA*)
  (`include/ltlf_ek/mtnfa.hpp`; `docs/prd/mtnfa.md`, **landed 2026-07-17**).
  `nfa_to_mtnfa` carries two acceptance preconditions inherited from
  `spot::twa_graph::state_is_accepting`: `nfa` must use state-based acceptance
  (`prop_state_acc()`, else it **throws**), and an accepting state with **no**
  out-edges reads back as non-accepting — so an edgeless final state needs an explicit
  `bddfalse`-guarded self-loop first, as `reverse_dfa_to_nfa` emits. `ltlf_to_nfa`
  satisfies both.
- **Do not call it:** to_mtnfa, ltlf2mtnfa, build_mtnfa, translate, `twanfa_to_mtnfa`
  (no such Spot primitive — our lift is `nfa_to_mtnfa`); **not** `mtnfa_to_mtdfa`
  (the reverse-direction determinization below).

### Goal automaton determinization (NfaToDfa)
- **`main.tex`:** `\algname{NfaToDfa}`$(P)$ (`\cref{alg:nfa_product}` line
  `alg:nfa_product:determinize`); black-boxed subset construction.
- **Definition:** subset-determinize an NFA into a DFA. A DFA state is a set
  $R\subseteq S$; the initial DFA state is $\{s_0\}$,
  $\delta_D(R,v)=\bigcup_{s\in R}\delta(s,v)$, and $R$ is accepting iff
  $R\cap F\neq\emptyset$. Realized per *Representation* below. In `main.tex` it runs
  on the **product** $P$; both PRDs also run it on the Goal NFA $N$ **alone** as
  their **isolated oracle** ($L=L(N)=L(\varphi)$, checked against an independent
  DFA), and it generalizes to the $(R,q_{in},q_{out})$ product states under the
  reachability invariant `main.tex:241` (a single transducer-state pair per
  reachable subset — the product determinizations `NfaProduct` / `MtnfaProduct`).
- **C++:** per *Representation* —
  - **explicit:** `nfa_to_dfa(nfa)` → `spot::twa_graph_ptr` — a **generic** subset
    construction (partition- and transducer-agnostic: enumerates full minterms over
    `nfa->ap()`), **state-based Büchi** output (matching the abused-DBA `as_twa`
    convention of `ltlf_to_dfa`, so `solve_dfa` reads $D$ like $A$; accepting iff a
    subset meets `nfa`'s finals). It **skips the empty subset** (missing edge =
    reject; incomplete output, no sink of its own) — so a $\cons$-consistent letter
    on which the goal dies must be made a *non-empty* subset **upstream** by the
    caller completing $N$ (`spot::complete_here`, as `NfaProduct` does), while a
    genuinely absent (non-$\cons$) letter stays skipped. `include/ltlf_ek/nfa_to_dfa.hpp`;
    `docs/prd/nfa-product.md`, landed 2026-07-17; never returns `nullptr`.
  - **mtdfa:** `mtnfa_to_mtdfa(nfa)` → `spot::mtdfa_ptr` — a symbolic BFS over
    reachable subsets, reusing the *MTNFA*'s set-union apply for the successor
    MTBDD, then relabeling set-terminals to `spot::mtdfa` terminals
    $2\cdot\mathrm{idx}(R')+b$ with $\emptyset\mapsto$ `bddfalse`. Here the empty
    subset is **kept** as `bddfalse` (the mtdfa rejecting sink — completion is
    implicit, cf. *MTDFA*), *not* skipped: that substrate difference is why the
    explicit route needs `spot::complete_here` and this one does not.
    `include/ltlf_ek/mtnfa.hpp`; `docs/prd/mtnfa.md`, **landed 2026-07-17**; never
    returns `nullptr`. **Precondition:** the initial state must not be accepting —
    $R_0=\{s_{N,0}\}$ is seeded at output index 0 and never rediscovered as a
    destination subset, so its acceptance bit is never read (asserted;
    `ltlf_to_mtnfa` always satisfies it).

    Applied to the **product** $P$ — which is where `\cref{alg:nfa_product}` actually
    calls `\algname{NfaToDfa}` — the mtdfa form is `mtnfa_product_to_mtdfa` (see
    *Product*), which **fuses** `alg:nfa_product`'s `:cons` and `:determinize` lines
    into a single symbolic pass rather than building $P$ and then determinizing it.
    So one C++ identifier realizes two `main.tex` steps here; the two entries are
    deliberately cross-referenced rather than merged.
- **Do not call it:** determinize (bare — direction-specific; cf. *Automaton
  reversal*'s rejected `determinize`), powerset (Spot's explicit primitive),
  subset_construction, `NfaToDfa` (bare CamelCase — that is the algorithm name;
  the C++ is representation-specific); for the explicit form, not `nfa2dfa`,
  `to_dfa`, `subset_dfa`; for the mtdfa form, not `mtnfa2mtdfa`, `to_mtdfa`
  (and Spot owns no `mtnfa_to_mtdfa` — it is ours).

### Consistency (cons)
- **`main.tex`:** $\cons(q_{in},q_{out},v)$ (`\cref{def:consistency}`, §206) — the per-letter filter.
- **Definition:** $v$'s V-variables are exactly what $\Tin,\Tout$ output.
- **C++:** `consistent(t_in, q_in, t_out, q_out, v)` — the two-transducer
  conjunction `emits(t_in, q_in, v) && emits(t_out, q_out, v)` (see *Output
  agreement*); same §206 concept, delegating the λ-agreement to `emits`.
- **Do not call it:** agrees, valid, matches, feasible.

### Output agreement (emits)
- **`main.tex`:** — (no symbol; code-only) — one conjunct of $\cons$
  (`\cref{def:consistency}`, §206), applied per transducer.
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
  per-letter/symbolic forms *skip* a non-agreeing letter (`\cref{def:consistency}`,
  partiality clause), the automaton form **also skips** it — a missing edge, not a
  sink (sink dropped 2026-07-15). The two coincide by construction now, so the
  question this once flagged for `/theory-review` is **closed**: there is no
  sink-vs-skip encoding gap left to argue.
- **Do not call it:** agrees (that is per-trace *Agreement*), consistent /
  consistent_with (that is the two-transducer $\cons$), commits, produces (that
  is the *Produced-trace language*), matches; for the symbolic form, not
  `emits_set`, `agreeing_region`, `lambda_region`; for the automaton form, not
  `cons_dfa` / cons-DFA / *Consistency automaton* (that names the two-transducer
  $\cons$, which has no automaton form — this object takes **one** transducer),
  `lambda_dfa`, `agreement_dfa` (that is per-trace *Agreement*), filter automaton.

### Determinacy witness
- **`main.tex`:** — (no symbol; it *decides* the $\lambda$-functionality
  requirement implicit in $\lambda:Q\times\Sigma_0\to\Sigma_1$ being a function,
  §110 / `\cref{def:probDefTransducer}`, and the dependency condition of
  `\cref{lem:outdep-diagonal}` — the same predicate serves both).
- **Definition:** decide whether a `bdd` relation, read as a relation **from** its
  observed variables **to** a named produced set, is **functional** — and if not,
  return *which* produced variable some observation leaves undetermined.
  Per-variable cofactor form: no observation may admit a produced variable both
  true and false,
  `bdd_exist(R & x, cube) & bdd_exist(R & !x, cube) != bddfalse` ⇒ not functional.
  Two properties make it the right primitive rather than a convenience. It is
  correct for **sets**, not only singletons: two distinct produced tuples over one
  observation must differ in some coordinate, and that coordinate then admits both
  polarities — so $|{\rm produced}|$ tests suffice. And it needs **no fresh BDD
  variables and no renamed copy** of the relation, unlike the textbook
  $R\wedge R'\wedge(X\not\equiv X')$ encoding. The **empty relation `bddfalse` is
  functional**, vacuously — both cofactors are empty, so no variable is ever
  reported. That is not a degenerate corner to guard against but the mechanism by
  which a state with an empty *Live-letter region* correctly imposes no
  dependency constraint; `\cref{lem:outdep-diagonal}` is likewise vacuous there,
  since it quantifies over pairs drawn **from** $\liveset{s}$.
- **C++:** `undetermined_variable(relation, produced, produced_cube, aut)` →
  `std::optional<std::string>` (`nullopt` ⟺ functional)
  (`transducer_io.hpp`; `docs/prd/output-dependencies-tool.md` Phase 1). **Two
  callers, one implementation:** `parse_transducer`'s λ-functionality validation
  (extracted from the former inline loop at `src/transducer_io.cpp:191–203`, whose
  error text it preserves) and *Output-dependency extraction*'s dependency test,
  which passes a *Live-letter region* rather than a λ.
- **Do not call it:** `is_functional` / `functional` (it returns a **witness**, not
  a bool), `check_lambda` (it is λ-agnostic — the dependency caller passes a letter
  region, not an output function), `lambda_functional`, non-functional (that names
  the failure, not the predicate), `undetermined_var` (abbreviated).

### Product
- **`main.tex`:** $P$ (Methods 1 & 2); states $S\times Q_{in}\times Q_{out}$
  (generalized in code to $S\times Q_1\times\cdots\times Q_n$).
- **Definition:** the Goal automaton crossed with both knowledge transducers,
  keeping only consistent transitions (a non-consistent letter is skipped, as in
  all methods — `\cref{def:consistency}`, §206; partiality note §214).
- **C++:** `ProductState` (a struct — `unsigned goal` + `std::vector<unsigned>
  taus` — generalized to N transducers), the reusable `agreeing_successor`
  (lazy per-letter step) and `build_product` (eager per-letter driver → neutral
  map) in `product.hpp`, plus the `*Product` synthesis classes. See
  `docs/prd/transducer-product.md`. The **symbolic** builder is
  `build_product_symbolic(goal, taus, init)` → `ProductGuards` (new;
  `docs/prd/symbolic-dfa-product.md`), which computes the per-destination guards
  directly from `delta_edges`/`emits_region` + Goal out-edges, no minterm loop;
  `DfaProduct` uses it while the per-letter `build_product` stays for
  `verify_controller` and as the build-equivalence reference. The
  **nondeterministic** builder is
  `build_product_nondet(goal, taus, init, alphabet)` → `ProductGuards` (new;
  `docs/prd/nfa-product.md`, landed 2026-07-17): the per-letter driver for a
  **nondeterministic** Goal automaton (the NFA $N$), where one letter yields
  **many** goal successors — for each $s'\in\delta_N(s,v)$ it ORs $v$ into the guard
  of $\langle s',\delta_{in}(q_{in},v),\delta_{out}(q_{out},v)\rangle$, so
  `materialize_product` emits a **nondeterministic** `twa_graph` (later
  subset-determinized by `nfa_to_dfa`, see *Goal automaton determinization*).
  `NfaProduct`-only; precondition `goal` complete (`spot::complete_here`).
  `ProductGuards` is
  the shared neutral per-dst guard map both builds emit
  (`map<ProductState, {bool acc; map<ProductState, bdd> guard}>`);
  `materialize_product(pg, init, dict, vars)` → `spot::twa_graph_ptr` turns it into the game
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
  Method **1**'s product in the mtdfa representation is
  `mtnfa_product_to_mtdfa(goal, taus, vars)` → `spot::mtdfa_ptr`
  (`include/ltlf_ek/mtnfa_product.hpp`; `docs/prd/mtnfa-product.md`, **landed**):
  the $\cons$-filtered product of the Goal *MTNFA* with the
  transducers, **fused** with its subset determinization (see *Goal automaton
  determinization*) into one symbolic BFS whose state is $(R,q_{in},q_{out})$ — a
  subset of $S_N$ plus one state per transducer, legitimate by the reachability
  invariant at `main.tex:241`. $\cons$ is applied as the region
  $\bigwedge_k$ `emits_region(q[k])` and the letter space is carved by the
  `delta_edges` guards, so no letter is ever enumerated; `taus` is the same
  N-transducer generalization `build_product*` uses. Like the Method-2 mtdfa route it
  has **no** `ProductState`/`ProductGuards`/`materialize_product` — and, being fused,
  **no intermediate product object at all**, which is why it emits no separable
  `determinize` benchmarking sub-span (contrast `NfaProduct`, which does).
  Method **3.1**'s product is `otf_product_to_mtdfa(phi, taus, vars, dict)` →
  `spot::mtdfa_ptr` (`include/ltlf_ek/otf_mtdfa_product.hpp`;
  `docs/prd/otf-mtdfa-product.md`): the $\cons$-filtered product of the Goal with
  the transducers, where the **Goal side is built on the fly** by *Forward
  progression* — so `\cref{alg:otfdfa_product}`'s $\cons$ filter is fused with the
  Goal **construction** itself, and **no Goal automaton object ever exists**. Its
  BFS state is $\langle[\psi], q_{in}, q_{out}\rangle$ interned on
  `(progress_row([\psi]), q)`; $\cons$ is the region
  $\bigwedge_k$ `emits_region(q[k])` and the letter space is carved by the
  `delta_edges` guards, exactly as in `mtnfa_product_to_mtdfa`, whose skeleton it
  mirrors. Two deviations from `\cref{alg:otfdfa_product}` are deliberate and
  documented in the PRD: acceptance is **transition**-based (the pseudocode's
  state-keyed $F_P$ over-accepts, I4), and a `bddtrue` leaf **collapses to the
  accepting sink**, pruning that branch — which makes $L(P)$ enforce $\cons$ only
  *up to* irrevocable satisfaction, so **there is no language-equality oracle on
  $P$** (verdicts are cross-checked instead, as on every mtdfa build).
  **This makes the builder unsafe to reuse blind:** $L(P)$ is a strict
  **over-approximation** of the true $\cons$-filtered product, sound *only* for a
  consumer that stops at the first accepting transition — `solve_mtdfa` under
  system-controlled termination, and `verify_controller`, whose $Bad$ fixpoint
  never propagates through an accepting state. Any consumer that reads $L(P)$
  itself — **Method 3.2's aggregation above all**, a language-equality oracle,
  model checking over $P$ — gets wrong answers with no diagnostic. The caveat is
  on the declaration in `include/ltlf_ek/otf_mtdfa_product.hpp`, not only here.
  This is the
  third sibling of `build_product_symbolic` / `mtnfa_product_to_mtdfa`; like them
  it has no `ProductState`/`ProductGuards`/`materialize_product`, and like the
  Method-1 mtdfa build it needs **no** final terminal-remap pass (the destination
  index is known when the terminal is emitted).
- **Do not call it:** composition, join, cross; for the symbolic pieces, not
  `symbolic_product`/`product_symbolic` (build), `GuardMap`/`product_map`
  (`ProductGuards`), `materialize`/`to_twa` (bare, for `materialize_product`);
  for the nondeterministic build, not `nondet_product`, `build_nfa_product`,
  `build_product_nfa` (for `build_product_nondet`); for the Method-1 mtdfa build, not
  `build_product_mtnfa`, `mtnfa_product` (bare — it *returns* an `spot::mtdfa`, and
  the name must say so), `product_mtnfa_to_mtdfa`, `mtnfa_to_mtdfa` (that is the
  Goal-NFA-alone determinization, a different function); for the Method-3.1 build,
  not `build_product_otf`, `otf_product` (bare — same rule: it *returns* an
  `spot::mtdfa`), `on_the_fly_product`, `otf_mtdfa_product` (that is how the
  `Synthesis` **class** `OtfMtdfaProduct` would be spelled, not the builder).

### Forward progression
- **`main.tex`:** `FP`$(\psi,w)$ returning $(\psi',b)$ (`\cref{alg:fp}`, §`otf`);
  the algorithm **body** is a `TODO` stub — see *Open theory questions*.
- **Definition:** advance a formula by one letter, returning the progressed
  formula $\psi'$ and the bit $b$ = "the trace **ending at** this letter satisfies
  $\psi$". Realized **symbolically**: one MTBDD covering *every* letter at once —
  a **row**, in the *MTDFA* sense of one MTBDD per state — never a per-letter
  call. A leaf of that row is `bddfalse` ($\varphi$ irrevocably violated),
  `bddtrue` (irrevocably satisfied), or a terminal $2\cdot\mathrm{idx}(\psi')+b$;
  so $b$ rides on the **transition**, not on $\psi'$ (two different sources can
  reach the same $\psi'$ with different $b$ — the finding behind
  `docs/prd/otf-mtdfa-product.md` I4).
- **C++:** `ForwardProgression` (`include/ltlf_ek/progression.hpp`), with
  `progress_row(psi)` → `bdd` (the row) and `decode(terminal)` →
  `std::pair<spot::formula,bool>` (the leaf decode); `docs/prd/otf-mtdfa-product.md`.
  A deliberately **single-file** wrapper over
  `spot::ltlf_translator::ltlf_to_mtbdd`: Spot's own header marks that class
  *"Semi-internal… Do not rely on the interface to be stable"*, so confining it to
  one seam — rather than spreading the $2\cdot\mathrm{idx}+b$ decode through the
  product build — **is the point of the type**. `simplify_terms` is pinned true
  and not exposed (see *Canonical representative*).
  **There is no per-letter entry point.** `\cref{alg:fp}`'s per-letter form is
  *derived* — `bdd_restrict(row, v)` then `decode` — the same
  symbolic-vs-per-letter split as `emits`/`emits_region` and `delta`/`delta_edges`,
  except that here only the symbolic form is an API.
- **Do not call it:** derivative, unfold, step; **`progress(psi, w)`** (the name
  this entry reserved until 2026-07-28 — no such function exists or will),
  `progress` (bare), `progress_region` (it returns a **row**, a
  letter→destination map, *not* a region of letters — contrast `emits_region`),
  `ltlf_to_mtbdd` (Spot's own primitive, the same rule that rejects `ltlf2dfa`),
  `FP` / `Fp` as a C++ identifier.

### Canonical representative
- **`main.tex`:** $[\psi]$ (`main.tex:340`, §`otf`) — the representative of $\psi$
  after progression, *"so that semantically equal progressed formulae collapse to
  the same state"*.
- **Definition:** what makes two progressed formulae the **same** state.
  **Inherited from Spot, never computed by us:** it is `propeq_representative` —
  light rewrites ($G\alpha\wedge\alpha\equiv G\alpha$,
  $F\alpha\vee\alpha\equiv F\alpha$, $(\alpha U\beta)\vee\beta\equiv\alpha U\beta$,
  …), then the formula encoded as a BDD over its maximal **temporal** subformulas
  as atoms and interned by that encoding, first-seen-wins. Spot applies it when it
  **mints a terminal**, so `ForwardProgression::decode` already *returns* $[\psi]$.
  It is **propositional** equivalence, weaker than the semantic equivalence
  `main.tex:340` literally claims — flagged for `/theory-review`.
  A second, coarser merge sits **on top** of it in the *Product*: states whose
  **row** is identical are fused (Spot's `fuse_same_bdds`, applied componentwise
  on the goal part).
- **C++:** — (**no identifier**, deliberately. Re-canonicalizing on top of Spot's
  representative would coarsen differently from `spot::ltlf_to_mtdfa` and so
  de-sync state counts from the `MtdfaProduct` baseline that Method 3.1 is
  measured against; `docs/prd/otf-mtdfa-product.md` I7.)
- **Do not call it:** normal form, key, hash; **`canonical(psi)`** (the name this
  entry reserved until 2026-07-28 — retired, no such function), propeq (bare, in
  prose — that names Spot's mechanism, not the concept), equivalence class.

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

  Both **solve a product that already exists**. Method 3.1 Phase 2
  (`docs/prd/otf-mtdfa-product.md`) anticipates a **third** shape — solving *fused
  into* the construction, feeding `spot::backprop_graph` as rows are discovered and
  aborting once the initial state is determined, which is what `main.tex:336`'s
  `\na` calls the missing *"hanging fruit optimization"*. Its C++ name is
  **not canonical yet** (the PRD's `otf_solve_fused` is explicitly tentative and
  its design is deferred to that phase's own grill) — recorded here only so no
  competing name is invented in the meantime.

  They differ in **where the governed variables are projected**, and that is the
  interesting part: `solve_dfa` projects **arena-side** (`bdd_exist` per edge
  guard, `src/solve_dfa.cpp:49`), which `solve_mtdfa` **cannot** do — an MTDFA's
  destination lives inside its terminal, so quantifying would cross destinations.
  Instead `solve_mtdfa` makes $\Iknown,\Oknown$ **controllable** ($\cons$ pins each
  to exactly one legal value, so it is a *forced* move, not a real choice) and
  projects **strategy-side**, off the `twa_graph` that `mtdfa_strategy_to_mealy`
  returns. Both discharge the same `main.tex:303` `\na` by different routes — see
  *Open theory questions*.
- **Do not call it:** solve (bare), `solve_game` / `mtdfa_winning_strategy` (those
  are Spot's primitives, not our wrappers), synthesize (that is the `Synthesis`
  method), realize.

## Dependency extraction

The terms below belong to a **separate binary**, `ltlf-ek-deps`
(`docs/prd/output-dependencies-tool.md`), not to any `Synthesis` method: it
*produces* the external knowledge the five methods *consume*. The notion is
adapted from *Dependent Variables in Reactive Synthesis* (arXiv:2401.11290, tool
`DepSynt`) from infinite-word LTL to $\text{LTL}_f$; both `main.tex` anchors are
**unproved lemmas** (see *Open theory questions*).

### Dependent output set
- **`main.tex`:** $\Xdep$ (`\cref{def:outdep}`, §`outdep`).
- **Definition:** a set of output variables whose value at every step is forced by
  the history together with the current values of *all* other variables — i.e.
  dependent on the *Dependency set* $\Ydep$. It is **subset-maximal, not
  maximum** (finding a maximum one is intractable), so the greedy scan's
  iteration order selects *which* maximal set is returned; that order is
  lexicographic and is a **contract**, since changing it changes the tool's
  output. Distinct from *Governed variables (V)*: $\Xdep$ is an analysis
  **result** that becomes $\Oknown$, not a partition input.
- **C++:** `DependentOutputs::dependent` (`std::set<std::string>`), one member of
  the `dependent_outputs` result alongside the updated `VariablePartition` and the
  emitted $\Tout$.
- **Do not call it:** governed variables / $\mathcal{V}$ / the known set (those are
  the partition notion — see *Governed variables (V)*), dependencies (bare),
  dependent variables (bare — the term is **output**-specific; the input notion is
  strictly stronger and unbuilt, see *Open theory questions*), maximal set (bare),
  maximum dependent set (it is not maximum).

### Dependency set
- **`main.tex`:** $\Ydep=(\mathcal{I}\cup\mathcal{O})\setminus\Xdep$ (`\cref{def:outdep}`).
- **Definition:** the variables $\Xdep$ is dependent **on**. Once $\Oknown=\Xdep$
  it equals $\mathcal{I}\cup\Ofree$ — which is exactly $\Sigma_0$ for `Role::t_out`
  (`main.tex:127`). **That coincidence is the whole reason extraction emits a
  $\Tout$** rather than some new object, and it is emphatically *not* a
  coincidence for `t_in`: a $\Tin$ observes only $\Ifree$, so $\Ydep$ would let
  $\lambda_{in}$ read $\mathcal{O}$ and break the *Turn order*.
- **C++:** — (no identifier; derived as `partition.inputs()` ∪
  `partition.output_free` and never stored, since the partition already determines
  it).
- **Do not call it:** observed slice / $\Sigma_0$ (they coincide for `t_out`
  **only** — conflating them is what makes the input case look like a parameter
  change), the free variables, the remaining variables, $Y$ (bare, in prose).

### Live-letter region
- **`main.tex`:** $\liveset{s}$ (`\cref{lem:outdep-diagonal}`).
- **Definition:** at a state $s$ of the Goal DFA, the region of letters whose
  successor is **live**. It is the object both halves of extraction run on:
  dependency holds iff every reachable live $s$ has $\liveset{s}$ **functional**
  from $\Ydep$ to $\Xdep$ (*Determinacy witness*), and $\liveset{s}$ **totalised**
  with a default cube *is* the emitted $\lambda_{out}$
  (`\cref{lem:outdep-transducer}`).
  **Liveness is reflexive, and that is load-bearing:** some accepting state is
  reachable from $s$ **including $s$ itself**, so an accepting $s$ is live even
  when every successor of $s$ is dead. Reflexivity is what places the **last**
  letter of a trace in the $\liveset{s}$ of the state that emits it: under an
  irreflexive reading, $\varphi=\lnot X[!]\mathtt{tt}$ over $\mathcal{I}=\{a\}$,
  $\mathcal{O}=\{x\}$ gives every state an empty region and so reports $\{x\}$
  dependent, which `\cref{def:outdep}` denies. The price is that $\liveset{s}$
  may be **empty at a live $s$** — exactly at a *terminal accepting state*, where
  $L(\varphi)$ is finite (e.g. $\varphi=a\wedge\lnot X[!]\mathtt{tt}$). That is
  **legal, not a contradiction**: no trace of $L(\varphi)$ reads a letter there,
  so the state carries no constraint (vacuously functional — see *Determinacy
  witness*) and $\lambda_{out}$ defaults every letter. Read
  $\liveset{s}=\emptyset$ at non-live $s$ too, so $\lambda_{out}$ is defined at
  every state of the complete $\delta_{out}$. Asserting the *negation* of this
  — "a live state must have a live successor" — aborts Debug builds on any
  finite-language $\varphi$ (fixed `9f8d295`); the strongest true invariant is
  the disjunction *live **non-accepting** ⇒ has a live successor*.
  Liveness is computed by our own backward BFS from `state_is_accepting`, **not**
  `spot::purge_dead_states` — that primitive is Büchi ("reaches an accepting
  *cycle*") and `ltlf_to_dfa` gives final states no absorbing self-loops, so it
  would purge $F_D$ outright. The BFS must **skip `bddfalse` edges**: an
  unsatisfiable guard can never be taken, so it must not propagate liveness, and
  such edges do occur here (the terminal accepting state of
  $(a\leftrightarrow x)\wedge\lnot X[!]\mathtt{tt}$ carries a `bddfalse`
  self-loop). Independent of $\Xdep$, hence computed once outside the greedy loop.
- **C++:** — (no public identifier; file-local to the extraction, computed from
  the Goal DFA's out-edges and the live-state set).
- **Do not call it:** `emits_region` (that is a **transducer**'s λ-agreement
  region over $\Sigma_0\cup\Sigma_1$ — a different object on a different automaton;
  see *Output agreement*), live edges, useful letters, surviving letters, the
  guard union (that would ignore liveness, which is precisely the load-bearing
  part).

### Output-dependency extraction
- **`main.tex`:** `\cref{lem:outdep-transducer}` (the construction);
  `\cref{def:outdep}`, `\cref{lem:outdep-diagonal}` (what it decides). No
  algorithm name — it is not one of *The five methods*.
- **Definition:** find a *Dependent output set* of $\varphi$ and materialise it as
  external knowledge: greedy lexicographic accumulation over $\mathcal{O}$, each
  candidate $\Xdep\cup\{z\}$ tested by the *Determinacy witness* on every
  reachable live state's *Live-letter region*. **The accumulated $\Xdep$ must be
  used** — dependence of singletons does not compose ($G(x\leftrightarrow y)$:
  each of $\{x\},\{y\}$ is dependent, $\{x,y\}$ is not). Emits $\delta_{out}$ =
  the **complete** Goal DFA and $\lambda_{out}$ = the totalised
  $\liveset{s}$; **both totalities are soundness requirements**, since by
  `\cref{def:consistency}`'s partiality clause a missing $\delta$ or $\lambda$
  makes a letter inconsistent for *every* party — so a partial $\Tout$ would
  delete $\Ifree$ letters and illegally constrain the environment, which is
  $\Tout$'s single most dangerous failure mode.
- **C++:** `dependent_outputs(phi, partition, dict)` → `DependentOutputs`
  (`include/ltlf_ek/dependent_outputs.hpp`), driven by the `ltlf-ek-deps` binary
  (`docs/prd/output-dependencies-tool.md`). Emits **two** artifacts — the
  transducer file and an updated part file — because the format stores no
  $\Sigma_0/\Sigma_1$ (see *Print a transducer*). The binary **owns** the
  `output_free`/`output_known` keys and passes `input_free`/`input_known` through
  verbatim, so a future input-dependency tool composes on disjoint keys.
- **Do not call it:** `find_dependencies`, `extract_knowledge`,
  `maximal_dependent_set` (it also returns the partition and the $\Tout$, not just
  the set), `depsynt` (that is the LTL tool this adapts, not ours — the same rule
  that rejects `ltlf2dfa`), `output_dependencies` (the PRD *file* name is prose;
  the identifier names the **result** — the dependent outputs), `OutputDependencies`
  (for the struct — it holds the dependent outputs, hence `DependentOutputs`).

## The five methods

| Term | `main.tex` | C++ (explicit) | C++ (mtdfa) |
|---|---|---|---|
| NFA product | Method 1 (§nfa) | `NfaProduct` | `MtnfaProduct` |
| DFA product | Method 2 (§fulldfa) | `DfaProduct` | `MtdfaProduct` |
| On-the-fly DFA product | Method 3.1 (§otfdfa) | `OtfDfaProduct` (unbuilt) | `OtfMtdfaProduct` |
| On-the-fly aggregated | Method 3.2 (§otfagg) | `OtfAggProduct` | — |
| Dynamic aggregation | Method 3.3 (§dynamicagg) | `OtfDynAggProduct` | — |

Common interface: `Synthesis::synthesize(phi, vars, t_in, t_out)`.

**Five rows, five methods.** The last two columns are the *Representation* axis
(below), **not** more methods — a sixth row would assert a sixth method, which is
exactly what `MtdfaProduct` is not. Methods 1, 2 **and 3.1** have mtdfa
implementations
(`docs/prd/mtnfa-product.md` — `MtnfaProduct`, landed;
`docs/prd/mtdfa-product.md` — `MtdfaProduct`, landed;
`docs/prd/otf-mtdfa-product.md` — `OtfMtdfaProduct`). `main.tex:335`'s `\na`
anticipated one for Method 3, and that PRD **is** the anticipated adjustment,
made in code first — `main.tex` still commits to no MTDFA definition. Method 3.1
is the first cell where the **explicit** column is deliberately left unbuilt:
*Forward progression* yields an MTBDD natively, so flattening it into a
`twa_graph` only to re-solve would be pure loss.
`make_synthesis_method` (`cli.hpp`) selects a **cell**: `--dfa-product` →
`DfaProduct`, `--mtdfa-product` → `MtdfaProduct`, `--nfa-product` → `NfaProduct`,
`--mtnfa-product` → `MtnfaProduct`, `--otf-mtdfa-product` → `OtfMtdfaProduct`.
That flag shape names a
method×representation cell rather than a method, and is a **known wart** — it does
not scale (a Method-3 mtdfa route would want `--mtdfa-otf-dfa-product`). Accepted
deliberately over an orthogonal `--representation=` selector; **revisited 2026-07-27**
when `MtnfaProduct` made a second method gain an mtdfa route, and **kept**: the two
mtdfa flags stay unambiguous because each representation has its own automaton type
in the name (`mtdfa` / `mtnfa`), so the wart is still latent rather than live. It
becomes live at Method 3, whose three sub-methods would need six flags.
**Revisited again 2026-07-28** at Method 3.1 — the predicted moment — and **kept a
third time**, on the same test: `--otf-mtdfa-product` keeps the automaton type in
the name, so it names its cell unambiguously, and the still-unbuilt explicit cell
keeps the reserved-not-wired `--otf-dfa-product` (`src/cli.cpp`'s
`kRecognisedNotWired`). The six-flag scenario is **not** discharged — it returns at
3.2/3.3, which would want `--otf-mtdfa-agg-product` and
`--otf-mtdfa-dyn-agg-product`. Re-decide there, not here.

### Representation
- **`main.tex`:** — (no symbol; the `\na` at `main.tex:335` gestures at MTDFA for
  Method 3, but no definition commits to the axis).
- **Definition:** *prose note, not a domain entry* — pinned here to fix the spelling
  and stop drift. Which data structure a method holds its automata in: **explicit**
  (`spot::twa_graph`, see *NFA / DFA for the Goal*) or **mtdfa** (`spot::mtdfa`, see
  *MTDFA*). It is **orthogonal to the method**: `main.tex` has five methods, and a
  representation changes *how* one is executed, never *which* one it is.
  `MtdfaProduct` is Method 2 in the mtdfa representation, `MtnfaProduct` is
  Method 1 in it, and `OtfMtdfaProduct` is Method 3.1 in it — **not** a sixth,
  seventh and eighth method.
  The axis reaches **four** entries: *Goal DFA construction*, *Goal automaton
  determinization*, *Product*, and *Game solving* each name a per-representation C++
  identifier. *Forward progression* is **not** a fifth: it exists only in the mtdfa
  representation (there is no explicit counterpart to pair it against), so it names
  one identifier, not a per-representation pair.
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
  Methods 3.2/3.3 only). Two methods' same-named canonical stages are comparable
  **only when they charge the same work to it**; a stage only *some* methods emit
  is fine (comparability means "when both emit it, they compare").
  **One live exception, and it is not a bug** (`docs/prd/otf-mtdfa-product.md`,
  2026-07-29): `OtfMtdfaProduct` fuses the Goal construction into the product, so
  it emits **no** `automaton_construction` and its `product_construction` absorbs
  work every other method charges to `automaton_construction`. Comparing
  `product_construction` alone across that boundary is *wrong* — it flatters the
  eager method by exactly the Goal-build cost. Compare
  `automaton_construction + product_construction` **summed**; `game_solving`
  still compares directly. Expect the same of Methods 3.2/3.3, which fuse at
  least as much. The registry is **soft**: adding, renaming,
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
- **`main.tex`:** — (no symbol; test-only, `docs/prd/oracle-faithfulness-guard.md`;
  widened to $\Tout$ by `docs/prd/ltlfsynt-oracle-known-output.md`).
- **Definition:** a mechanical, author-blind-spot-independent cross-check that an
  oracle fixture's produced-trace language $\psi$ and its transducer **file**
  denote the same language, by driving the two artifacts the author already wrote
  against each other — the transducer's run engine (`parse_transducer`) versus
  $\psi$'s finite-$\text{LTL}_f$ membership (`ltlf_to_dfa`) — never a third
  hand-labeled trace (which would inherit the author's blind spot). It is
  **`Role`-generic** (see *Role*): under `t_in` it guards the pair
  $(\Tin,\psiin)$, under `t_out` the pair $(\Tout,\psiout)$, taking its
  observed/produced slices from `sigma_slices` instead of hard-coding
  $(\Ifree,\Iknown)$ — so under `t_out` the enumeration fixes all of
  $\mathcal{I}\cup\Ofree$ per step ($\Tout$'s $\Sigma_0$ legitimately contains
  $\Ofree$) and the mutation flips a single $\Oknown$ bit. Fails iff $\psi$ is too
  **strong** (rejects a trace the transducer produced) or too **weak** (accepts a
  single-bit $\Sigma_1$ mutation of one). Mutation soundness carries across both
  roles because agreement is a **per-trace** predicate, so it does not matter that
  $\Tout$'s $\Sigma_0$ contains system-controlled variables.
  **One caveat, and it is load-bearing** (Phase 1 theory review, 2026-08-02): the
  guard's own **negative control** is currently weaker on the $\Tout$ side than on
  the $\Tin$ side. Table J-bad's deliberately over-strong $\psiout$ is
  **unsatisfiable outright**, so the meta-oracle asserting the guard fires proves
  only "it fires on an unsatisfiable formula" — not the $\Tin$ analogue's stronger
  claim that it fires on a genuinely-too-strong *satisfiable* one. Until that
  fixture is replaced (proposed $(\lnot x)\land G(X(x\leftrightarrow a))$; a PRD
  change, **not** done), do not read a green $\Tout$ guard as evidence of equal
  strength to a green $\Tin$ one.
- **C++:** `run_faithfulness_guard(transducer_src, psi, partition, role)` →
  `GuardResult { bool ok; std::string detail; }` (test-local, anonymous namespace
  in `tests/ltlfsynt_oracle_test.cpp`; not a library API). `role` is deliberately
  **not defaulted**: a defaulted `Role` is exactly how a $\Tout$ pair could
  silently be guarded under $\Tin$ slices and pass vacuously.
- **Do not call it:** faithfulness check/test (bare), oracle guard, sanity check;
  for the formula parameter, not `psi_in` (it is `psi` — `role` decides which
  $\psi$ it is).

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

- **`FP` is unspecified** — `\cref{alg:fp}`'s body is still a `TODO` in
  `main.tex`. **The code now commits to an answer** (`docs/prd/otf-mtdfa-product.md`):
  `FP` is Spot's own LTLf progression, reached through *Forward progression*'s
  wrapper. **`/theory-review` has now ruled (2026-07-29): `underspecified`, not a
  code-bug** — the commitment is sound, but `main.tex` must record four things it
  already depends on. (1) The **weak `X`** reading
  (`FP`$(X\psi,w)=(\psi,\top)$, `FP`$(X[!]\psi,w)=(\psi,\bot)$) — and note
  `main.tex` has **no $\text{LTL}_f$ preliminaries at all**, so this is unwritten
  paper-wide, not merely unwritten in `\cref{alg:fp}`. (2) $b$ rides the
  **transition**, not $\psi'$ (see the next entry). (3) All four corners of
  $\psi'\in\{\mathtt{tt},\mathtt{ff}\}$ against $b$ are reachable —
  $(\mathtt{tt},\bot)$ from `X[!]tt`, $(\mathtt{ff},\top)$ from `!X[!]tt` — so no
  rule may key on $\psi'$ alone; this is exactly why the mtdfa collapse is safe,
  since Spot's constant leaves replace only the *pairs*
  $(\mathtt{tt},\top)$/$(\mathtt{ff},\bot)$. (4) The returned $\psi'$ is already
  $[\psi']$, so `\cref{alg:otfdfa_product}` must not re-apply $[\cdot]$. A `\cl`
  note carrying all four is **written into `latex/main.tex`** (unpushed); the
  `\cref{alg:fp}` *stub itself* is still untouched.
- **Aggregated final-state overwrite** — Alg. "On The Fly Aggregated DFA
  Product": inserting into $F_P$ keyed on $[\psi']$ may be overwritten when the
  same $[\psi']$ later returns $b=\bot$; unresolved whether to remove.
  **Sharpened 2026-07-28 and no longer confined to 3.2** — the same defect is
  present in **un-aggregated** Method 3.1, `\cref{alg:otfdfa_product}` line
  `alg:otfdfa_product:final_insert`, because $b$ is **not a function of**
  $[\psi']$. Witness, verified against the linked libspot: both
  `G(a -> Xb)` and `X[!]G(a -> Xb)` progress under $\{\lnot a,\lnot b\}$ to the
  successor `G(a -> Xb)`, with $b=\top$ and $b=\bot$ respectively — so a
  state-keyed $F_P$ marks that state accepting and the product **over-accepts**.
  The author's uncertainty was therefore well-founded, and the answer is that
  $F_P$ must be **transition**-keyed, not state-keyed. `OtfMtdfaProduct` gets this
  right for free (mtdfa's $2d+b$ terminals), i.e. the implementation is strictly
  more faithful to $\text{LTL}_f$ than the pseudocode.
  **`/theory-review` has now RULED (2026-07-29): confirmed `doc-bug` — the
  pseudocode is unsound as written**, and it produced a strictly stronger
  witness than the one above, needing only a **single source state**:
  $\varphi=(c \wedge G(a \rightarrow Xb)) \vee (\lnot c \wedge X[!]G(a \rightarrow
  Xb))$ with trivial total $\Tin,\Tout$. The letters $\{c,\lnot a,\lnot b\}$ and
  $\{\lnot c,\lnot a,\lnot b\}$ both leave the **initial** state for
  $G(a \rightarrow Xb)$ with $b=\top$ and $b=\bot$; a state-keyed $F_P$ then
  accepts the one-letter word $\{\lnot c,\lnot a,\lnot b\}$, which does not
  satisfy $\varphi$. Under system-controlled termination that is not cosmetic:
  it tells the system it may stop and win where $\varphi$ is false, so
  `SolveDfa` can return a controller *Controller verifier* rejects. Repair:
  either $F_P \subseteq S_P \times 2^{\mathcal{I} \cup \mathcal{O}}$, or widen the
  state with $b$. A `\cl` note is **written into `latex/main.tex`** (unpushed);
  the pseudocode itself is untouched, so **Method 3.2 must fix this before it can
  be built** — it is the first method that cannot dodge it.
- **On-the-fly game solving** — Method 3 builds the product on the fly but still
  solves at the end; the hanging-fruit on-the-fly *solving* is not done. The same
  `\na` continues (`main.tex:335`): *"This likely requires adjusting the definitions
  for MTDFA usage"* — i.e. the author anticipates a *Representation* change for
  Method 3. **Updated 2026-07-28** (the old "only Method 2 has one today" went
  stale when `MtnfaProduct` landed): Methods 1, 2 and 3.1 all have mtdfa routes
  now, and `docs/prd/otf-mtdfa-product.md` is the Method-3 adjustment the `\na`
  anticipated. Its **Phase 2** *would* implement on-the-fly *solving*
  (`backprop_graph`, early abort) behind a knob, but **Phase 2 is NOT started**
  (2026-07-29): only the construction half landed, and `OtfMtdfaProduct`'s
  `otf_solve` knob throws `std::logic_error` rather than silently falling back.
  So **both** halves of this `\na` stay open — the engineering half is merely
  scoped, and the **definitional** half is untouched: `main.tex` still solves at
  the end and still commits to no MTDFA definition. Phase 2 also has a
  counter-argument on record now: 3.1's benchmark win is already ~5000× *and
  flat* where $\cons$ prunes, so on-the-fly solving would optimize a term that is
  no longer the bottleneck there.
- **Governed-variable projection** (`main.tex:303` `\na`) — *"Because the resulting
  game is being limited to transitions consistent with the external knowledge
  transducers, which govern the variable set $\mathcal{V}$, it can project these
  variables out without loss."* The supporting argument is drafted but **commented
  out** (`main.tex:305–306`), so the claim is currently unbacked in the live text.
  Both *Game solving* routes depend on it and discharge it **differently** —
  `solve_dfa` arena-side, `solve_mtdfa` by pinning the variables as forced
  controllable moves and projecting strategy-side. Flagged for `/theory-review`
  (`docs/prd/mtdfa-product.md`). Newly load-bearing; not previously listed here.
- **Mealy is baked into the signatures; no Moore option** (`main.tex:103` `\na`) —
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
  **Newly load-bearing 2026-07-28:** Method 3.1's accepting-sink collapse (a
  `bddtrue` leaf prunes its branch, *Product*) is sound **only** under the
  system-controlled-termination reading — the system reaching irrevocable
  satisfaction may stop there and win. If that reading changed, the collapse would
  have to be revisited, not merely re-benchmarked.

- **Both dependency-extraction lemmas are unproved** (§`outdep`, written
  2026-07-30 under `\cl` notes; `docs/prd/output-dependencies-tool.md`).
  `\cref{lem:outdep-diagonal}` claims the compatible-pair search of
  arXiv:2401.11290 collapses to the diagonal when the Goal automaton is
  deterministic, making the check per-state and linear in $|A|$;
  `\cref{lem:outdep-transducer}` claims the totalised construction is a valid
  $\Tout$ **and** that `\cref{def:probDefTransducer}` with it is equirealizable
  with plain $\text{LTL}_f$ synthesis of $\varphi$. The second is the load-bearing
  one — it is what licenses the end-to-end `ltlfsynt` oracle — and its only
  evidence is that oracle passing, which is empirical, not a proof. Note the
  totality argument **depends on the trace-termination reading** below: the
  defaulted letters lose the system the game only under system-controlled
  termination. Separately, `\cref{lem:outdep-diagonal}` is **underspecified on
  liveness** (found 2026-07-30, via a Debug abort on finite-language $\varphi$):
  it never says whether "reachable" is reflexive, never notes that $\liveset{s}$
  can be **empty at a live $s$**, and leaves $\liveset{s}$ undefined at non-live
  $s$ even though `\cref{lem:outdep-transducer}` needs $\lambda_{out}$ at every
  state of a total $\delta_A$. All three are now settled in *Live-letter region*
  above and in the code (`9f8d295`); the `\cl` note stating them in `main.tex` is
  **written into `main.tex`** (2026-07-31, uncommitted and unpushed) by the
  theory review under `/code-reviewer` on the Phase 3 diff, alongside a second
  note on `\cref{lem:outdep-transducer}` recording that its "outside
  $L(\varphi)$" justification is a statement about prefixes and that its
  equirealizability claim is read under system-controlled termination. The
  citation shift both notes caused has already been repaired by
  `scripts/check-main-tex-refs.py --fix`, including this file's
  `latex/main.tex:556–561` below.
- **Input dependencies need a different notion** — `\cref{def:outdep}` is
  output-only, and $\Ydep$ cannot simply be re-pointed: `Role::t_in` observes only
  $\Ifree$, so a dependent *input* must be dependent on $\mathcal{I}\setminus\Xdep$
  **alone**, ignoring $\mathcal{O}$ — a strictly stronger condition and a
  different algorithm. This is the notion the **commented-out**
  `latex/main.tex:556–561` block gropes toward ("a potential set of dependent
  input variables $D\subseteq I$"), including its own alternative of deciding
  dependence by *counting synthesis strategies*. Left commented — the author's
  call, not to be uncommented by a skill.

**Resolved (kept here so they are not re-flagged as novel):**

- **Line-84 parameter gap** — *resolved.* `main.tex` §86 now names the missing
  variable set as $\Sigma_0\in\{\Ifree,\ \mathcal{I}\cup\Ofree,\ \mathcal{I}\}$
  and writes the intersection $v_t\cap\Sigma_0$; the code's `sigma0_cube`
  instantiates exactly that slice (see *Observed / produced slice*).
- **Partial transducers** — *resolved.* Settled by `\cref{def:consistency}`, whose
  **partiality clause** ("a missing $\delta$ or $\lambda$ value is equivalent to an
  inconsistent letter") is valid for all methods. The `std::optional` return on
  `Transducer::delta` / `::lambda` (`nullopt` = undefined) is **final**, not
  tentative: a non-enabled letter is skipped (all methods).
  The project commits to the Case-A regime, so partial and total transducers are
  language-equivalent. See `docs/prd/concrete-transducer.md`.
  - **Terminology note (2026-07-16).** *Enabled* was once its own `main.tex`
    definition (`\label{def:enabled}`, "Enabled letter" = δ **and** λ defined at
    both states **and** $\cons$). An Overleaf update **removed it** and folded
    partiality into `\cref{def:consistency}` itself, so **`\cref{def:enabled}` no
    longer resolves** and every reference was repointed to `def:consistency`.
    *Enabled* survives as **this project's** term for the same predicate — it is
    ours now, not `main.tex`'s. Cite `def:consistency`, never `def:enabled`.
