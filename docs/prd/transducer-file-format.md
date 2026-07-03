# PRD: Transducer file format & parser

**Status:** draft
**Interface:** free function `parse_transducer(...)` → `OutputLabeledTransducer` (concretises the `Transducer` base; consumed by `Synthesis::synthesize` as `t_in` / `t_out`)
**main.tex ref:** §Transducers (§98–128), `\cref{def:enabled}`, `\cref{def:probDefTransducer}`

**Gates:**
- [ ] glossary        — new terms in `docs/GLOSSARY.md` C++ column
- [ ] tests           — unit + oracle coverage per "Test oracles" below
- [ ] code-review     — domain (`/code-reviewer`) + generic (`/code-review`)
- [ ] theory-review   — code ↔ math faithfulness vs `main.tex`

## Goal

Give the library a way to read an **external knowledge strategy** ($\Tin$ or
$\Tout$) from disk and build an `OutputLabeledTransducer` from it. Today the only
construction path is the in-library constructor
(`docs/prd/concrete-transducer.md`); there is no on-disk representation, so a user
cannot supply their own $\Tin/\Tout$ and no method can be exercised on external
input. This PRD specifies **one transducer's file format** (its $\delta$ and its
$\lambda$) and the parser that turns it into an `OutputLabeledTransducer`. The
rest of the CLI — how $\varphi$ is supplied, how the `VariablePartition` is
supplied, method selection, and controller *output* serialization — is **out of
scope** (separate future PRDs). The `VariablePartition` and the transducer's
**role** ($\Tin$ vs $\Tout$) are inputs *to the parser*, passed by that
future front-end.

## Ubiquitous-language terms used

- **Transducer** — $\tau=(Q,\Sigma,\delta,\lambda,q_0)$; **no acceptance
  condition**. Here, its on-disk representation.
- **Output-labeled transducer** — `OutputLabeledTransducer`; the concrete class
  the parser builds (Spot `twa_graph` for $\delta$ + per-state BDD for $\lambda$).
- **Transition function (delta)** — $\delta:Q\times 2^{\mathcal{I}\cup\mathcal{O}}\to Q$; on disk = HOA edges.
- **Output function (lambda)** — the lambda-split $\lambda:Q\times\Sigma_0\to\Sigma_1$; on disk = the `%%LAMBDA` block.
- **Observed / produced slice ($\Sigma_0$ / $\Sigma_1$)** — `sigma0_cube` /
  `sigma1_cube`. **Derived** from role + partition, *not* stored as the source of
  truth (see below).
- **Free/Known inputs & outputs** — $\Ifree,\Iknown,\Ofree,\Oknown$; `VariablePartition`.
- **External knowledge strategy** — the strategy $\Tin/\Tout$ implements.
- **Consistency (cons)** — `consistent(...)`; the downstream consumer of `lambda`.
- **Letter** — $v\in 2^{\mathcal{I}\cup\mathcal{O}}$, a full BDD cube.

**Glossary gaps — run `/glossary` (see Definition of done):**

- **`parse_transducer`** — the new public entry point (a domain operation:
  materialise a transducer from its external representation).
- **`Role`** — `enum class Role { t_in, t_out }`; selects which align-block
  columns give $\Sigma_0/\Sigma_1$.
- **Transducer file format** — the HOA-`\delta` + `%%LAMBDA`-`\lambda`
  representation itself, and the `%%LAMBDA` block grammar.

## Behaviour / semantics (from main.tex)

The file must reconstruct a transducer faithful to §Transducers. Nothing here
changes the math; it is a serialization of the already-specified object.

1. **Two parts: $\delta$ (HOA) then $\lambda$ (`%%LAMBDA`).** $\delta$ is a
   standard deterministic DFA over $\Sigma=2^{\mathcal{I}\cup\mathcal{O}}$ (§101,
   §105) encoded as a **Spot HOA automaton**; $\lambda$ — the turn-order
   restricted output $\lambda:Q\times\Sigma_0\to\Sigma_1$ (§103) — has **no HOA
   equivalent** and is carried in a companion block. See "File format" below.
2. **Acceptance is ignored.** A transducer has no $F$ (§101). The HOA
   `acc-name`/`Acceptance:` is parsed by Spot but **never read** as finality —
   only states, initial state, and edge guards are used. (Same contract as
   `OutputLabeledTransducer`.)
3. **$\Sigma_0/\Sigma_1$ are derived, and they *orient* $\lambda$.** A $\lambda$
   entry such as `a <-> k` is a **symmetric relation**; the file alone cannot say
   which variable is observed vs produced. Orientation comes from the cubes
   derived from **role + partition** (the paper's align block, §122–128):
   - `Role::t_in`  → $\Sigma_0=\Ifree$, $\Sigma_1=\Iknown$
   - `Role::t_out` → $\Sigma_0=\mathcal{I}\cup\Ofree$, $\Sigma_1=\Oknown$

   $\Sigma_1$ (produced = the *known* vars) is **mode-invariant**. $\Sigma_0$
   (observed) is the mode-dependent slice — the derivation encodes the **Mealy**
   turn order; a future Moore mode would give a smaller/empty $\Sigma_0$ (see
   Open questions). Evaluation orients the relation exactly as the existing
   `OutputLabeledTransducer::lambda`: `bdd_restrict` the $\Sigma_0$ slice, then
   `bdd_exist` to keep $\Sigma_1$.
4. **Abuse of notation matches the code.** `main.tex` §Method 1 projects the
   arguments explicitly ($v\cap\Ifree$, $v\cap(\mathcal{I}\cup\Ofree)$); the §87
   footnote licensing "$\lambda(q,v)$ on the full letter" is commented out. The
   parser is consistent with either reading: it stores the *relation*, and
   evaluation reads only the $\Sigma_0$ slice — the orientation cubes are what
   make the projection well-defined regardless of which convention the paper
   settles on.
5. **Partiality (§107–116, `\cref{def:enabled}`).** Both $\delta$ and $\lambda$
   may be partial:
   - **$\delta$ partial** ⇒ **missing HOA edges** (an incomplete automaton). A
     letter satisfying no out-edge guard yields `nullopt` — native to HOA, no
     sentinel needed.
   - **$\lambda$ partial** ⇒ a `%%LAMBDA` entry whose $\Sigma_0$ slice has **no
     $\Sigma_1$ completion** (`bddfalse` slice) yields `nullopt`. Writing
     `state q: false` makes $\lambda$ undefined at $q$ for *every* observation.
   The project commits to the **Case-A** regime (undefined only on letters `cons`
   would reject) ⇒ language-equivalent to the totalisation. A non-enabled letter
   is skipped (Methods 1, 3) or routed to `⊥` (Method 2); the parser does not
   need to know which method consumes it.

## File format

One self-contained file per transducer: a Spot HOA automaton for $\delta$,
then — **after HOA's `--END--`** (Spot stops there, so trailing text is ours) — a
`%%LAMBDA` block for $\lambda$.

```
HOA: v1
States: 2
Start: 0
AP: 2 "a" "k"           /* declares the full I∪O alphabet used by δ and λ */
acc-name: all
Acceptance: 0 t         /* parsed but IGNORED — a transducer has no F */
--BODY--
State: 0
  [0] 1                 /* edge guard is a BDD over I∪O; missing edge = δ partial */
State: 1
  [t] 1
--END--

%%LAMBDA
state 0: a <-> k        /* observe a, produce k = a          */
state 1: false          /* λ undefined at q1 (partial)       */
```

**`%%LAMBDA` grammar / rules:**

- **`%%LAMBDA`** sentinel introduces the block. States are keyed by **HOA state
  number**.
- **Exactly one `state <n>: <formula>` per HOA state.** The entry count must
  equal `num_states` (matches the `OutputLabeledTransducer` constructor
  invariant); a **missing** state is a **parse error** (catches typos), not an
  implicit undefined.
- **`<formula>`** is a boolean formula over the $\Sigma_0\cup\Sigma_1$ APs,
  parsed by `spot::parse_formula` → BDD (thin wrapper). `false` = totally
  undefined $\lambda$ at that state.
- **The block carries no slice declaration.** $\Sigma_0/\Sigma_1$ are derived
  from role + partition (above); the file never restates them. (An earlier draft
  allowed optional `sigma0:`/`sigma1:` lines validated against the derivation;
  dropped — the redundancy buys little readability and adds a failure mode where
  a file's slice text can disagree with its partition.)
- The **HOA `AP:` header declares the full $\mathcal{I}\cup\mathcal{O}$
  alphabet** (even APs unused in a guard), so every $\lambda$-formula name binds
  against the shared `bdd_dict`.

## Interfaces & types

New header `include/ltlf_ek/transducer_io.hpp` (+ `src/transducer_io.cpp`):

```cpp
namespace ltlf_ek {

enum class Role { t_in, t_out };   // selects Σ0/Σ1 align-block columns

// Parse one transducer file (HOA δ + %%LAMBDA λ) into an OutputLabeledTransducer.
//   partition — classifies every AP (Ifree/Iknown/Ofree/Oknown); orients λ.
//   role      — t_in ⇒ Σ0=Ifree,Σ1=Iknown; t_out ⇒ Σ0=I∪Ofree,Σ1=Oknown.
//   dict      — SHARED bdd_dict; t_in, t_out and (later) φ's automaton must all
//               register APs in one dict or the (v & guard) tests are meaningless.
// Throws std::invalid_argument (with file/line context) on any malformed input,
// non-deterministic δ, out-of-Σ0∪Σ1 AP in a λ formula, non-functional λ, a
// missing/extra state entry, or a sigma0/sigma1 mismatch.
OutputLabeledTransducer parse_transducer(std::istream& in,
                                         const VariablePartition& partition,
                                         Role role,
                                         spot::bdd_dict_ptr dict);

}  // namespace ltlf_ek
```

- **Return by value** — the concrete `OutputLabeledTransducer`, directly usable
  as a `const Transducer&` in `synthesize(...)`; matches the project's
  free-function style (`collect_aps`, `consistent`). (A `std::filesystem::path`
  overload is a trivial convenience wrapper.)
- **Black-boxes reused, not implemented:** `spot::parse_aut` (HOA → `twa_graph`),
  `spot::parse_formula` (λ formula → BDD), `spot::is_deterministic`. No new Spot
  wrappers; `LtlfToDfa` / `SolveDfa` / `progress` are untouched.
- **Derivation helper:** compute `sigma0_cube`/`sigma1_cube` from
  `(partition, role)` — a small internal function (candidate for
  `variables.hpp` if reused). Keep the `mode` axis in mind (default Mealy) so a
  future Moore mode slots in **without changing the file format**.

**Parse-time validation (all → `std::invalid_argument`):**

1. **$\delta$ deterministic** — reject a non-deterministic HOA automaton up front
   (`spot::is_deterministic`), rather than letting `delta()` throw mid-synthesis
   on a letter satisfying two guards.
2. **$\lambda$ functional** — for each state, fixing any $\Sigma_0$ observation
   leaves **at most one** $\Sigma_1$ completion (the $\Sigma_1$-projection of each
   observation is a single cube). Reject e.g. `a | k` under $\Sigma_0=\{a\}$
   (fixing `a=1` leaves `k` free). This is the transducer **well-formedness**
   invariant the backlog wanted the explicit $\lambda$ to make checkable.
3. **AP scope** — every AP in a $\lambda$ formula lies in $\Sigma_0\cup\Sigma_1$.
4. **State coverage** — exactly one `%%LAMBDA` entry per HOA state.

## Edge cases

- **Empty $\Sigma_0$** ($\Ifree=\emptyset$, or a future Moore transducer): every
  $\lambda$ formula is over $\Sigma_1$ only; `bdd_restrict` with an empty
  observation is a no-op — a constant output per state. Must parse and evaluate.
- **Empty $\Sigma_1$** (empty governed set $\mathcal{V}=\emptyset$): the
  transducer produces nothing; `state q: true` ⇒ `lambda` returns `bddtrue`
  (empty cube). This is the monolithic-baseline transducer.
- **State totally undefined** — `state q: false` ⇒ `lambda(q,·)=nullopt`
  everywhere; still a valid (Case-A) partial transducer.
- **HOA with acceptance / colors** — accepted syntactically, ignored
  semantically; document so no reader mistakes it for finality.
- **AP declared but unused in any guard** — legal (needed so $\Sigma_1$ vars that
  no edge constrains still register in the dict).
- **Non-deterministic / incomplete $\delta$** — non-deterministic ⇒ reject at
  parse (validation 1); incomplete ⇒ **allowed** (partial $\delta$).
- **Formula naming an AP outside $\mathcal{I}\cup\mathcal{O}$** — rejected (AP
  scope check); the partition is the closed universe of APs.
- **Trailing garbage after `%%LAMBDA`** — strict parse; unknown lines error
  rather than being ignored.

## Test oracles (for /test-writer)

- **Round-trip / unit fixture:** author a 2–3 state file (the example above);
  assert `initial_state`, a few `delta` successors, `lambda` outputs, and that a
  missing edge ⇒ `nullopt` delta and `state …: false` ⇒ `nullopt` lambda.
- **Orientation:** the *same* file parsed with a partition putting `a` in
  `input_free` vs `input_known` yields transducers with **swapped**
  $\Sigma_0/\Sigma_1$ and correspondingly transposed `lambda` — proves
  orientation comes from the partition, not the formula.
- **Derived-slice equality:** parsed `sigma0_cube`/`sigma1_cube` equal the cubes
  built directly from `(partition, role)`.
- **WF rejection:** `a | k` under $\Sigma_0=\{a\}$ is rejected (non-functional
  $\lambda$); non-deterministic HOA is rejected; a missing `state` entry is
  rejected — each with a distinct diagnostic.
- **Abuse-of-notation property** (inherited): `lambda(q, v)` equals
  `lambda(q, v & sigma0_cube)` — out-of-$\Sigma_0$ vars never change the output.
- **Shared-dict integration:** parse $\Tin$ and $\Tout$ against one `bdd_dict`,
  feed both to `DfaProduct`, and check `consistent(...)` fires on the intended
  letters — the end-to-end path the in-library PRD could not exercise from disk.
- **Monolithic baseline:** with $\mathcal{V}=\emptyset$ (transducers governing
  nothing), synthesis must equal plain $\text{LTL}_f$ synthesis of $\varphi$.

## Open theory questions touched

- **Mealy/Moore mode axis (not in `main.tex`).** Deriving $\Sigma_0$ from the
  align block bakes in the **Mealy** turn order. A future Moore synthesis would
  make $\Sigma_0$ mode-dependent (history-only ⇒ $\Sigma_0=\emptyset$), while
  $\Sigma_1$ stays fixed. This is *conceptual* (no existing `\na` in `main.tex`).
  Design keeps it cheap: $\Sigma_0/\Sigma_1$ are explicit construction-time cubes
  and the file format is mode-agnostic, so adding a `mode` parameter to the
  derivation is the only change. Flag for `/theory-review` whether `main.tex`
  should name the mode explicitly; **do not** resolve here.
- **Non-deterministic knowledge ($\lambda$ returns a set) — Later backlog.** The
  BDD-relation encoding already *represents* this; only the WF functional check
  would relax (behind the same future mode flag). No change now.
- **Partiality / `enabled` — RESOLVED** (`\cref{def:enabled}`, Case-A). The file
  format signals partiality via missing edges / `false` entries; nothing new
  open. (The $\bot$-sink-vs-skip lemma `\cref{lem:sink_skip}` predates this PRD.)

## Definition of done

- `include/ltlf_ek/transducer_io.hpp` + `src/transducer_io.cpp` implement
  `parse_transducer` and `Role`; wired into `CMakeLists.txt`; build green.
- Format = HOA ($\delta$, acceptance ignored) + post-`--END--` `%%LAMBDA` block
  ($\lambda$ as per-state boolean formulas), shared `bdd_dict`.
- All four parse-time validations enforced (deterministic $\delta$; functional
  $\lambda$; AP scope; full state coverage), each throwing
  `std::invalid_argument` with context.
- $\Sigma_0/\Sigma_1$ derived from `(partition, role)`; no slice text in the file.
- Tests above pass (`/test-writer`).
- `/glossary` run — register `parse_transducer`, `Role`, and the transducer file
  format / `%%LAMBDA` block.
- `/theory-review` run on the Mealy/Moore mode note.
- `/code-reviewer` clean.

## Developer comments / PRD disagreements

Deviations made during implementation from what this PRD specified, with
rationale. (Disagreements with the PRD live here, not in source comments;
divergences from `main.tex` go to `/theory-review`.)

- _(none yet)_
