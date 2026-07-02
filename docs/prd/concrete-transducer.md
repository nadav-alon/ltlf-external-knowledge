# PRD: Concrete Transducer construction path

**Status:** draft · **main.tex ref:** §Transducers (§98–126), `\cref{def:probDefTransducer}`
**Interface:** concretises the `Transducer` abstract base (`include/ltlf_ek/transducer.hpp`); consumed by `Synthesis::synthesize` as `t_in` / `t_out`

## Goal

Give the `Transducer` abstraction a real in-library construction path. Today it
is an abstract base (`delta`, `lambda`, `initial_state`) with **no way to build a
concrete instance**, so no synthesis method can be exercised with real external
knowledge and `DfaProduct` cannot be tested end-to-end. This PRD (a) sharpens the
abstract signature — pinning down the **lambda-split** slices and **partiality**
— and (b) adds one concrete implementation plus a factory that reuses Spot for
the transition structure. The external file format / CLI parser is **out of
scope** (a later PRD).

## Ubiquitous-language terms used

- **Transducer** — $\tau=(Q,\Sigma,\delta,\lambda,q_0)$; the abstract base. Note
  a transducer has **no acceptance condition** (unlike the Goal automata).
- **Transition function (delta)** — $\delta:Q\times 2^{\mathcal{I}\cup\mathcal{O}}\to Q$; `Transducer::delta`.
- **Output function (lambda)** — the lambda-split $\lambda:Q\times\Sigma_0\to\Sigma_1$; `Transducer::lambda`.
- **Letter** — $v\in 2^{\mathcal{I}\cup\mathcal{O}}$, a `bdd` cube; `bdd v`.
- **Free/Known inputs & outputs** — $\Ifree,\Iknown,\Ofree,\Oknown$; `VariablePartition::input_free/…`.
- **External knowledge strategy** — the strategies $\Tin,\Tout$ implement; `t_in` / `t_out`.
- **Consistency (cons)** — `consistent(...)`, which calls `lambda` on both transducers.

**Glossary — registered via `/glossary` (done):**

- **`OutputLabeledTransducer`** — concrete `Transducer` (backed by a
  `spot::twa_graph_ptr` for $\delta$ + a per-state BDD relation for $\lambda$).
  Construction is via its constructor (no separate free-function factory).
- **Observed / produced slice ($\Sigma_0$ / $\Sigma_1$)** — C++ `sigma0_cube` /
  `sigma1_cube`; $\Tin$: $(\Ifree,\Iknown)$, $\Tout$: $(\mathcal{I}\cup\Ofree,\Oknown)$.
- **`lambda` signature** — glossary corrected to `lambda(q, v)` (full letter in),
  which implies renaming the `bdd visible` parameter in `transducer.hpp` → `v`.
- **Partiality** — **settled** in `main.tex` §Transducers (partial-transducer
  paragraph) + `\cref{def:enabled}`: `delta`/`lambda` may be undefined, and a
  letter is *enabled* iff both transducers are **defined** at it **and** `cons`
  holds. The `std::optional` signature is now backed by theory (no longer
  tentative). The project uses only the **Case-A** regime (undefined solely on
  letters `cons` already rejects ⇒ language-equivalent to the totalisation).

## Behaviour / semantics (from main.tex)

1. **Tuple, no acceptance.** $\tau=(Q,\Sigma,\delta,\lambda,q_0)$ over
   $\Sigma=2^{\mathcal{I}\cup\mathcal{O}}$ (§101). It "implements" a strategy
   $\Sigma^*\to\mathrm{Image}(\lambda)$; there is **no** $F$.
2. **delta is standard and total-tracking.** $\delta:Q\times 2^{\mathcal{I}\cup\mathcal{O}}\to Q$
   tracks the *entire* history including variables not visible to $\lambda$ (§105).
3. **lambda is the turn-order-restricted output.** $\lambda:Q\times\Sigma_0\to\Sigma_1$,
   $\Sigma_0,\Sigma_1\subseteq\Sigma$ (§103). Per §112–120:
   - $\Tin$: $\lambda_{in}:Q_{in}\times 2^{\Ifree}\to 2^{\Iknown}$
   - $\Tout$: $\lambda_{out}:Q_{out}\times 2^{\mathcal{I}\cup\Ofree}\to 2^{\Oknown}$
4. **Abuse of notation (footnote, §87).** Feeding a *superset* of the domain is
   allowed; the intersection with $\Sigma_0$ is implicit
   ($\Sin(\varepsilon,v)=\Sin(\varepsilon,v\cap\Iknown)$). This is why the C++
   `lambda(q, v)` takes the **full letter**.
5. **Consistency usage (§138–143).** `consistent` compares $v\cap\Iknown$ against
   $\lambda_{in}(q_{in},v)$ and $v\cap\Oknown$ against $\lambda_{out}(q_{out},v)$
   — i.e. `lambda` is always called with the full letter and must return a cube
   over its $\Sigma_1$ only.
6. **Partiality / the `enabled` predicate (§Transducers, `\cref{def:enabled}`).**
   A letter `v` is **enabled** at `(q_in, q_out)` iff `delta_in`, `lambda_in`,
   `delta_out`, `lambda_out` are all **defined** at `v` **and** `cons` holds.
   Non-enabled letters are skipped (Methods 1, 3.1) or routed to `⊥` (Method 2).
   Key consequences for the impl:
   - `enabled` **subsumes `cons`**: a `nullopt` from `delta` *or* `lambda`
     already makes the letter non-enabled — treat it exactly like a `cons`
     failure (skip / `kSink`). No separate "undefined" branch needed beyond the
     `optional` check.
   - `enabled` **guards `delta`**: `delta_in`/`delta_out` may only be applied on
     letters that passed the enabled test. The current call sites test `cons`
     and then apply `delta` unconditionally (`main.tex` pseudo-code has the same
     bug, flagged by a `\cl` note) — fix both callers so `delta` is never
     dereferenced on a non-enabled letter.
   - For total transducers `enabled` ≡ `cons`, so existing behaviour is
     unchanged; the `optional`/enabled path only matters for partial inputs.

## Interfaces & types

**Abstract base (`transducer.hpp`) — sharpen the signature:**

```cpp
class Transducer {
 public:
  virtual ~Transducer() = default;
  virtual unsigned initial_state() const = 0;

  // Successor of q under the full letter v (cube over I∪O).
  // nullopt = undefined (partial transducer).
  virtual std::optional<unsigned> delta(unsigned q, bdd v) const = 0;

  // Output committed at q: reads only its Σ0 slice of v, returns a cube over Σ1.
  // Full letter passed in (abuse-of-notation, main.tex §87); nullopt = undefined.
  virtual std::optional<bdd> lambda(unsigned q, bdd v) const = 0;
};
```

- **Design decisions locked in this grill:**
  - **Full letter in, implicit projection.** One abstract `Transducer`;
    `lambda(q, v)` takes the full letter; the impl reads only $\Sigma_0$ and
    returns only $\Sigma_1$. `t_in`/`t_out` stay interchangeable at the interface.
  - **Partiality via `std::optional`** (final; backed by `\cref{def:enabled}`).
    `nullopt` on `delta` *or* `lambda` = non-enabled letter ⇒ skip / `kSink`.

**Concrete impl (new, e.g. `output_labeled_transducer.hpp/.cpp`):**

```cpp
// delta reuses Spot's automaton graph purely as a deterministic transition
// structure — the twa is an ω-automaton but a Transducer has NO acceptance,
// so the acceptance condition is IGNORED. Navigate the unique edge whose guard
// is satisfied by v; if none, delta = nullopt (partial).
class OutputLabeledTransducer final : public Transducer {
 public:
  OutputLabeledTransducer(
      spot::twa_graph_ptr delta_dfa,    // Q, δ, q0 (acceptance ignored)
      std::vector<bdd> lambda_by_state, // per-state Σ0∪Σ1 output relation
      bdd sigma0_cube,                  // vars λ may observe  (Ifree | I∪Ofree)
      bdd sigma1_cube);                 // vars λ produces     (Iknown | Oknown)
  // ... Transducer overrides ...
};
```

- **lambda representation:** per state `q`, one BDD `out_[q]` over
  $\Sigma_0\cup\Sigma_1$ encoding the deterministic function. Evaluation:
  ```cpp
  bdd r = bdd_restrict(out_[q], v & sigma0_cube_);   // fix the Σ0 slice
  return bdd_exist(r, sigma0_cube_);                  // keep only Σ1
  ```
  Compact, BDD-native, no blowup in $|\Sigma_0|$.
- **Black-boxes:** none needed. `LtlfToDfa`, `SolveDfa`, `progress` are unrelated
  to this PRD; do not touch.
- **Callers to update for the `optional` signature:** `consistency.hpp` /
  `consistent(...)` and `dfa_product.cpp` (route `nullopt` to `kSink`).

## Edge cases

- **Partial delta / lambda:** `nullopt`. OTF methods skip; `DfaProduct` → `kSink`.
- **Non-deterministic or incomplete twa passed to the factory:** the wrapper
  assumes deterministic $\delta$; multiple satisfied guards is a construction
  error, zero satisfied guards is `nullopt` (partiality). Decide assert vs error.
- **ω-acceptance on the twa:** ignored by design; document loudly so no one reads
  it as transducer finality.
- **Empty $\Sigma_0$** (e.g. $\Ifree=\emptyset$): `lambda` ignores $v$ and returns
  a constant cube — `bdd_restrict`/`bdd_exist` with an empty cube must still work.
- **Empty governed set $\mathcal{V}=\emptyset$:** $\Sigma_1$ empty ⇒ `lambda`
  returns `bddtrue` (empty cube); such a transducer governs nothing (the
  monolithic-baseline case — see oracles).
- **Letter `v` that is not a full cube over I∪O:** out of contract; caller passes
  full letters.

## Test oracles (for /test-writer)

- **Unit — small fixture:** build a 2–3 state `BddTransducer` by hand; assert
  `initial_state`, a few `delta` successors, `lambda` outputs, and that an
  undefined letter yields `nullopt` on both `delta` and `lambda`.
- **Abuse-of-notation property:** for a letter `v`, `lambda(q, v)` equals
  `lambda(q, v & sigma0_cube)` — extra (out-of-$\Sigma_0$) variables never change
  the output.
- **lambda ⊆ Σ1:** the returned cube constrains only $\Sigma_1$ variables
  (`bdd_exist(result, sigma1_cube) == bddtrue`).
- **Consistency wiring:** with a transducer whose `lambda` is known, `consistent`
  returns true exactly on letters whose $\mathcal{V}$-slice matches.
- **Integration / metamorphic:** feed the concrete transducers to `DfaProduct`;
  once further methods land, cross-method equivalence uses the same fixtures.
- **Monolithic baseline:** with $\mathcal{V}=\emptyset$ (identity/empty
  transducers governing nothing), synthesis must equal plain $\text{LTL}_f$
  synthesis of $\varphi$.

## Open theory questions touched

- **Partiality compatibility — RESOLVED.** `main.tex` now defines partial
  transducers and the `enabled` predicate in §Transducers (`\cref{def:enabled}`),
  valid for all methods: `nullopt` $\delta$/$\lambda$ ⇒ non-enabled ⇒ skip
  (Methods 1, 3) / `⊥` (Method 2). The project commits to the **Case-A** regime,
  so partial and total inputs are language-equivalent. The `optional` signature
  is final. *(One sub-item remains open — the $\bot$-sink vs skip equivalence
  lemma — but that predates this PRD; see `main.tex` `\cl` note near Method 2.)*
- **Line-84 parameter gap (`\na`, §85).** The second argument of $S(\ldots,v_t)$
  needs an intersection with an as-yet-undefined variable set to match the
  strategy signatures. This is exactly the $\Sigma_0$ slice `lambda` receives —
  resolving it may pin whether the "full letter in" convention needs a named
  variable set. Leave for `/theory-review`; do not resolve here.

## Definition of done

- `Transducer` base signature updated (`std::optional` delta/lambda); compiles.
- `OutputLabeledTransducer` implemented, thin over Spot + BuDDy, with the
  ω-acceptance-ignored behaviour documented in-code.
- `consistent(...)` and `dfa_product.cpp` updated for the new signature; build green.
- `bdd visible` parameter in `transducer.hpp` renamed to `v` (full-letter convention).
- Tests above pass (`/test-writer`).
- `/glossary` run (done) — registered `OutputLabeledTransducer`, the Σ0/Σ1 slice,
  and the corrected `lambda(q, v)` signature.
- `/theory-review` run on the partiality treatment and the §85 gap.
- `/code-reviewer` clean.
