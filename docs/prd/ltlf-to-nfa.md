# PRD: LtlfToNfa (Method 1 — NFA construction)

**Status:** implemented — Phase 1 (MONA subprocess + DFA parser) landed,
`src/mona_dfa.cpp` + `include/ltlf_ek/detail/mona_dfa.hpp` (branch
`worktree-prd-ltlf-to-nfa`, uncommitted at PRD-edit time). Phase 2 (folded
mirror encoder + `past_ltlf_to_dfa`) and Phase 3 (`Reverse` + public
`ltlf_to_nfa`) are still pending.
**Interface:** adds the black-box helper `ltlf_to_nfa` (`LtlfToNfa`); **not** a `Synthesis` method. The `NfaProduct` class, `NfaToDfa` determinization, and the product/`SolveDfa` wiring of Method 1 are separate later PRDs.
**Recommended workflow:** concurrent — the public signature is high-confidence (a thin analog of `ltlf_to_dfa`, and the NFA's shape is pinned exactly by `main.tex`'s reverse formulas §160–169). The language-equivalence oracle binds to the public contract + the math, so it parallelizes. The *internal* parser/encoder phase boundaries are tentative, so P1/P2's per-function unit tests run sequential-within-phase (developer lands the internal function, then its unit test binds).
**main.tex ref:** §`nfa` (Method 1), subsection "LTLf to NFA", Algorithm `alg:ltlftonfa` ("Ltlf To Nfa"); `def:mirror`; `thm:nfa-mirror-size`.

**Gates:**
- [ ] glossary        — new terms in docs/GLOSSARY.md C++ column
- [ ] tests           — unit + oracle coverage
- [ ] code-review     — domain (/code-reviewer) + generic (/code-review)
- [ ] theory-review   — code ↔ math faithfulness vs main.tex

## Goal

Implement the NFA-construction black box of Method 1 (`\algname{LtlfToNfa}`,
`alg:ltlftonfa`): given a Goal formula $\varphi$, produce a **nondeterministic**
finite automaton $N$ recognizing $L(\varphi)$, as a `spot::twa_graph` over the
letter alphabet $2^{\mathcal{I}\cup\mathcal{O}}$ on a shared `bdd_dict`. The
construction is the three-step pipeline of `alg:ltlftonfa`:
`$\mirror{\varphi}$` (the *mirror*, a past-$\text{LTL}_f$ formula) →
`\algname{PastLtlfToDfa}` (a DFA $D$ with $L(D)=\operatorname{rev}(L(\varphi))$)
→ `\algname{Reverse}` (edge-reversal giving $N$). This is the counterpart to
`ltlf_to_dfa` (Method 2's `LtlfToDfa`, `docs/prd/dfa-product.md`) but yields the
**single-exponential NFA** instead of the (double-exponential) DFA, which is the
whole point of Method 1: only the *product* is later determinized, keeping the
blowup exponential in $\varphi$ alone.

Because Spot has **no past-$\text{LTL}_f$ operators** (verified: `spot::op` has
`X/F/G/U/R/W/M/strong_X`, no `Y/S/O/H`), `\algname{PastLtlfToDfa}` cannot reuse
`ltlf_to_mtdfa`; it is realized by shelling out to **MONA** (`/usr/bin/mona`,
v1.4) and parsing MONA's DFA output back into a `twa_graph` over the shared
`bdd_dict`. The whole encoder + subprocess + parser is written in C++, with **no
Python / external-tool runtime dependency** beyond the `mona` binary.

## Ubiquitous-language terms used

- **`LtlfToNfa` / `ltlf_to_nfa`** — the NFA construction. Reserved but **not yet
  defined** in `docs/GLOSSARY.md` ("*Goal DFA construction*" entry: "The Method-1
  NFA route (`LtlfToNfa`, `NfaToDfa`) is not yet named — future, when Method 1
  lands."). **Glossary gap — add on landing.**
- **NFA / DFA for the Goal** — the automaton object (`spot::twa_graph_ptr`); glossary present ($N$ Method 1, $A$ Method 2).
- **Letter / Letter alphabet** — $v\in 2^{\mathcal{I}\cup\mathcal{O}}$; glossary present.
- **Mirror ($\mirror{\varphi}$)** — `def:mirror`. **Glossary gap — no entry.**
- **`PastLtlfToDfa`** — the MONA black box producing $D$ with $L(D)=\operatorname{rev}(L(\varphi))$. **Glossary gap — no entry.**
- **`Reverse` (DFA→NFA reversal)** — the edge-reversal of `alg:ltlftonfa:reverse`. **Glossary gap — no entry.**
- **Cube / bdd_dict** — BDD/BuDDy primitives; glossary present (*Cube*).

> **Run `/glossary` before / alongside implementation** to add: `ltlf_to_nfa`,
> `mirror`, `past_ltlf_to_dfa` (the MONA driver), and the DFA→NFA `reverse`
> construction. These are new domain identifiers.

## Behaviour / semantics (from main.tex)

**Mirror (`def:mirror`).** $\mirror{\varphi}$ is the past-$\text{LTL}_f$ formula
obtained from $\varphi$ by sending each future operator to its temporal dual. Its
defining property, which the implementation must preserve:
$$w,0\models\varphi \iff \operatorname{rev}(w),|w|-1\models\mirror{\varphi}
\quad\text{for every \emph{non-empty} trace } w,$$
reading $w$ backwards and evaluating at its last position.

**PastLtlfToDfa (§154–159).** Produces
$D=(S_D,\;2^{\mathcal{I}\cup\mathcal{O}},\;\delta_D,\;s_{D,0},\;F_D)$ with
$$L(D)=\{\,\operatorname{rev}(w) : w,0\models\varphi\,\},$$
i.e. the **reverse language** of $\varphi$. $D$ is deterministic and (as MONA
emits it) complete. `thm:nfa-mirror-size` claims $|S_D|$ is single-exponential in
$|\varphi|$ (proof "To be determined" — see *Open theory questions*).

**Reverse (§160–169, `alg:ltlftonfa:reverse`).** $N$ keeps $D$'s states, adds a
fresh initial state $s_{N,0}$, and takes $D$'s initial state as the sole
accepting state:
$$S_N = S_D \cup \{s_{N,0}\}, \qquad F_N = \{s_{D,0}\},$$
$$\delta_N(s_{N,0}, v) = \{\, s : \delta_D(s,v)\in F_D \,\}, \qquad
\delta_N(t, v) = \{\, s : \delta_D(s,v) = t \,\}\ (t\in S_D).$$
That is: reverse every $D$-edge ($s \xrightarrow{v} t$ in $D$ becomes
$t \xrightarrow{v} s$ in $N$), and add out-edges from the fresh $s_{N,0}$ to the
$v$-predecessors of $D$'s accepting states. The result is an NFA for $\varphi$:
$L(N)=L(\varphi)$ over non-empty traces.

**Invariants that MUST hold:**
1. $N$'s alphabet is exactly $2^{\mathcal{I}\cup\mathcal{O}}$ — no extra AP; APs
   of $\mathcal{I}\cup\mathcal{O}$ absent from $\varphi$ are don't-cares (same
   convention as `ltlf_to_dfa`, which is over $\varphi$'s support).
2. $N$ carries finiteness in **acceptance marks, not an extra AP**, matching the
   `ltlf_to_dfa` / `as_twa(state_based=true)` convention: a state is accepting
   iff it is a final NFA state. $F_N$ is the single state $s_{D,0}$.
3. $N$ is **nondeterministic and partial** — `\algname{Reverse}` must **not**
   complete $N$ (unlike `ltlf_to_dfa`, which calls `complete_here`): `alg:nfa_product`
   reads a possibly-empty $\delta_N(s,v)$ and skips it, so a rejecting sink is
   neither needed nor wanted.
4. Non-empty-trace semantics: the empty word is rejected (project commitment,
   De Giacomo–Vardi; `main.tex` `def:mirror` is stated for non-empty $w$). $N$'s
   acceptance must agree with `ltlf_to_dfa(phi)` on this (both reject $\epsilon$).

## Interfaces & types

**Freeze confidence: high** for the public signature — it is a thin analog of
`ltlf_to_dfa` (`include/ltlf_ek/ltlf_to_dfa.hpp`), and the returned NFA's shape
(acceptance = single final state, alphabet, non-completion) is pinned exactly by
`main.tex` §160–169. The *internal* phase-boundary functions (MONA parser,
folded encoder) are **tentative** — invented here — and may be reshaped as
implementation proceeds; they are file-local `detail`, not public contract.

**Public (new header `include/ltlf_ek/ltlf_to_nfa.hpp`):**
```cpp
// LtlfToNfa(phi) --- main.tex Method 1 (alg:ltlftonfa), black-boxed there as
// \algname{LtlfToNfa}. Build the NONDETERMINISTIC finite automaton N for the
// LTLf Goal formula phi, returned as a spot::twa_graph on `dict`.
//
// Pipeline: mirror(phi) [past-LTLf] --> PastLtlfToDfa [MONA] --> D with
// L(D)=rev(L(phi)) --> Reverse --> N with L(N)=L(phi). Finiteness is carried in
// *acceptance marks, not an extra AP* (as ltlf_to_dfa): the sole accepting state
// is the reversal's F_N={s_{D,0}}. N is nondeterministic and NOT completed
// (alg:nfa_product tolerates an empty delta_N(s,v)).
//
// Precondition: `dict` MUST be the same spot::bdd_dict as T_in, T_out so N, the
// transducers and every letter share one variable numbering.
spot::twa_graph_ptr ltlf_to_nfa(const spot::formula& phi,
                                const spot::bdd_dict_ptr& dict);
```
Same `(phi, dict)` shape and shared-dict precondition as `ltlf_to_dfa`; **no**
`VariablePartition` argument (APs come from `phi`'s support on `dict`, exactly as
`ltlf_to_dfa`).

**Internal phase boundaries (tentative; `src/` + `detail/`, no glossary/public
commitment):**
- P1 — MONA driver + parser. A function that runs `mona` on generated M2L-Str
  source and parses its DFA output into a deterministic, complete
  `spot::twa_graph_ptr` $D$ over `dict` (accepting = MONA's accepting states).
  Working name `mona_output_to_dfa(mona_out, var_order, dict)` — **tentative**.
- P2 — folded mirror encoder + `PastLtlfToDfa`. A recursive
  `spot::formula` → M2L-Str-source function emitting the mirror (reversed-reading)
  semantics directly, and a `past_ltlf_to_dfa(phi, dict)` composing encode →
  `mona` → P1 parse → $D$. Working names **tentative**.
- P3 — `reverse_dfa_to_nfa(D)` → NFA `twa_graph_ptr`, plus the public
  `ltlf_to_nfa` wiring. Working name **tentative**.

**If implementation proves this contract wrong:** that is a PRD-change event —
update this section and propagate to any in-flight test branch; the developer
does not silently re-shape the public interface on its own branch. (The internal
`detail` names may churn freely within a phase — only the public `ltlf_to_nfa`
signature and the NFA-shape invariants above are frozen.)

## Novel mechanisms — how they are pinned

Two mechanisms here are **not lifted verbatim from `main.tex`** and are the
correctness-load-bearing risk; how each is pinned:

**(a) The folded mirror → M2L-Str encoder (P2).** MONA's M2L-Str is
first-order-over-positions, so encoding a future operator against *decreasing*
positions yields literally the same source as encoding its past dual against
increasing positions — the two steps of `alg:ltlftonfa` (`Mirror` then
`PastLtlfToDfa`) collapse into one recursive `spot::formula` walk, with **no
intermediate past-formula type** to invent. The **per-operator M2L-Str clauses**
(the FO body for `X`/`strong_X`, `U`, `R`, `W`, `M`, `F`, `G`, boolean cases, and
the position-domain / last-position conventions) are **deferred to `/developer`**,
implemented from the **canonical De Giacomo–Vardi / Fuggitti `LTLf2DFA`
$\text{LTLf}\!\to\!$M2L-Str encoding** (the community-standard reference), and
**verified by `/theory-review`** against `def:mirror`. *Rationale for deferral:*
the encoding is a large, mechanical, well-established table; hand-transcribing it
into this PRD would be error-prone and is not a design decision. The **backstop**
is the strong independent oracle $L(D)=\operatorname{rev}(L(\varphi))$ vs
`ltlf_to_dfa` (below) — a wrong clause fails it. The load-bearing subtleties the
theory-review must confirm: **weak vs strong `X`** under reverse reading (memory
gotcha), the **non-empty-trace** position domain, and **last-position evaluation**
($|w|-1$).

**(b) The MONA output parser (P1).** MONA (`mona -q -w`, or `-gw` GraphViz) emits
the minimal DFA with transition guards over the declared free variables as
0/1/`X` bit patterns in a fixed variable order. The parser must, per state,
recover the BDD guard over the ordered free vars and register each bit position
against its AP on `dict`, building $D$'s edges. *Deferred to `/developer`:* the
exact MONA output flavour to parse (`-w` textual table vs `-gw` DOT) — **both are
parseable; developer picks the more stable/greppable one**, documented in the
source. Pinned here: the parser output is a **deterministic, complete**
`twa_graph` over exactly `dict`'s APs with MONA's accepting set as $F_D$;
determinism + completeness are asserted (P1 structural tests). A nonzero `mona`
exit or unparseable output is a hard error (`std::runtime_error`).

## Implementation phases

Each phase compiles green and is independently testable; the parser (P1, depends
only on MONA's output format) and the encoder (P2) are genuinely separable.

- **Phase 1 — MONA subprocess + DFA parser.** Land the `mona` invocation and the
  output parser building $D$ as a `twa_graph_ptr` over `dict`. **Green checkpoint:**
  a **checked-in `.mona` fixture** (hand-written, small) runs through `mona` and
  parses into the expected `twa_graph`; structural tests assert $D$ is
  **deterministic + complete**, over exactly the fixture's APs, with the right
  accepting set. *May stub:* the encoder — drive P1 from the fixture / a known
  hand-written M2L-Str source, not from $\varphi$.
- **Phase 2 — folded mirror encoder → $D$.** Land the recursive
  `spot::formula` → M2L-Str encoder and `past_ltlf_to_dfa`, consuming P1's parser.
  **Green checkpoint:** $L(D)=\operatorname{rev}(L(\varphi))$ verified against the
  independent `ltlf_to_dfa(phi)` (exact determinize-and-compare on the reversed
  reference + membership fuzz) over the generated corpus.
- **Phase 3 — `Reverse` + public `ltlf_to_nfa`.** Land `reverse_dfa_to_nfa` (edge
  reversal, fresh $s_{N,0}$, $F_N=\{s_{D,0}\}$, **no** completion,
  `purge_unreachable`/`purge_dead` states) and the public header. **Green
  checkpoint:** $L(N)=L(\varphi)$ vs `ltlf_to_dfa(phi)` (exact equivalence +
  membership fuzz).

## Edge cases

- **$\varphi=\mathtt{1}$ (`tt`):** rejects the empty word, accepts every non-empty
  trace — $N$ must reflect this (cross-check vs `ltlf_to_dfa(1)`, which the
  existing DfaProduct tests already pin as non-empty-trace `1`).
- **$\varphi=\mathtt{0}$ (`ff`):** empty language; $N$ accepts nothing.
- **Purely boolean $\varphi$ (no temporal op):** constrains only the first
  position of a length-1-or-more trace.
- **APs of $\mathcal{I}\cup\mathcal{O}$ absent from $\varphi$:** don't-cares; $N$
  is over $\varphi$'s support on `dict`, product handles the rest (as `ltlf_to_dfa`).
- **MONA reject sink after reversal:** reversing a complete $D$ produces
  dead/unreachable states in $N$ (the sink's reversed edges, states that can't
  reach $F_N$); purge them — an optimization, not correctness.
- **MONA failure:** nonzero exit / empty / unparseable output ⇒ throw, never
  return a malformed automaton.
- **Empty word / non-empty-trace boundary:** the $|w|-1$ last-position evaluation
  and $\epsilon$-exclusion must agree with `ltlf_to_dfa`'s convention — flagged
  for `/theory-review` (below), and directly checked by the membership oracle on
  a length-1 trace.

## Test oracles (for /test-writer)

- **Primary (P3) — cross-construction metamorphic equivalence.** Determinize $N$
  via `spot::powerset` (`spot/twaalgos/powerset.hh`) to a DFA and check
  **finite-word language equivalence** against the independent `ltlf_to_dfa(phi)`
  (product-emptiness on the symmetric difference under finite/state-based
  acceptance). Two independent constructions (MONA-reverse vs Spot `ltlf_to_mtdfa`)
  agreeing is the strong oracle.
- **P2 checkpoint — reverse-language equivalence.** $L(D)=\operatorname{rev}(L(\varphi))$:
  reverse the reference (`ltlf_to_dfa(phi)` edge-reversed then determinized) and
  compare to $D$; or membership (below) on reversed traces.
- **Membership fuzz (scale).** Over the *generated corpus*
  (`tests/ltlfsynt_oracle_test.cpp` generators), for random **non-empty** traces
  $w$: $N$ accepts $w$ **iff** `ltlf_to_dfa(phi)` accepts $w$; for $D$, accepts
  $w$ **iff** `ltlf_to_dfa(phi)` accepts $\operatorname{rev}(w)$. Catches edge
  letters on formulas too large to determinize exactly.
- **Structural free-riders.** $N$ over exactly `dict`'s APs (closed universe);
  $F_N$ a single accepting state; $N$ **not** required deterministic (it is an
  NFA); $D$ deterministic + complete (P1).
- **P1 fixture round-trip.** Checked-in `.mona` fixture → `mona` → parsed
  `twa_graph` structurally matches expected; determinism + completeness asserted.

## Open theory questions touched

- **`thm:nfa-mirror-size` proof is "To be determined"** — the single-exponential
  size claim for $N$. Not an implementation blocker; leave for `/theory-review`.
- **Trace-termination / non-empty-trace semantics** (`main.tex:96` `\na`, tracked
  in glossary *Open theory questions* and `docs/prd/controller-verifier.md`): the
  mirror's $|w|-1$ last-position evaluation and $\epsilon$-exclusion must match
  `ltlf_to_dfa`'s De Giacomo–Vardi reading, or the equivalence oracle fails for a
  semantic (not bug) reason. `/theory-review`.
- **Weak vs strong `X` under the mirror** (memory `ltlf-weak-x-and-termination-semantics`):
  the future→past dual under reverse reading must handle `X` (weak) vs `strong_X`
  correctly. The likeliest encoder bug class. `/theory-review` against `def:mirror`.
- **`FP` unspecified** — not touched by this PRD (Method 1 uses the explicit
  product, not forward progression).

## Definition of done

- `include/ltlf_ek/ltlf_to_nfa.hpp` + `src/ltlf_to_nfa.cpp` (+ P1/P2/P3 `detail`)
  land; tree compiles green.
- All three phases green at their checkpoints (P1 fixture round-trip; P2
  $L(D)=\operatorname{rev}(L(\varphi))$; P3 $L(N)=L(\varphi)$).
- Primary equivalence oracle + membership fuzz pass over the generated corpus.
- Glossary updated (`/glossary`): `ltlf_to_nfa`, `mirror`, `past_ltlf_to_dfa`
  (MONA driver), DFA→NFA `reverse`.
- `code-review` (domain + generic) and `theory-review` gates ticked; the three
  open theory questions above are dispositioned by `/theory-review`.

## Developer comments / PRD disagreements

**2026-07-13 (Phase 1 landing).** No disagreements with the math or the
public contract (Phase 1 touches neither); four implementation choices the
PRD left open, recorded here for traceability:

- **Own translation unit + a `detail/` header, not "file-local".** The PRD's
  "Definition of done" sketches a single `src/ltlf_to_nfa.cpp` holding all of
  P1/P2/P3's `detail` helpers behind the one public
  `include/ltlf_ek/ltlf_to_nfa.hpp`. P1 instead landed as its own
  `src/mona_dfa.cpp` + `include/ltlf_ek/detail/mona_dfa.hpp` (declaring
  `ltlf_ek::detail::run_mona` / `::mona_output_to_dfa`), following the
  existing `include/ltlf_ek/detail/util.hpp` precedent. Reason: Phase 1's
  green checkpoint is a structural GoogleTest suite driving the MONA
  round-trip directly (PRD "Test oracles" / "Implementation phases" Phase 1),
  which needs a declaration reachable from `tests/`; a `static`/anonymous-
  namespace helper confined to one `.cpp` can't be unit-tested directly. The
  header stays under `detail/` (no glossary entry, not part of the
  `ltlf_to_nfa` public contract) so this is not a public-interface change —
  P2/P3 remain free to fold these into `ltlf_to_nfa.cpp` or keep them split.
- **MONA output format: `mona -q -w -n`** (see the header's doc-comment for
  the `-w` vs `-gw` rationale). `-n` (skip the counter-/satisfying-example
  ANALYSIS section) and `-q` (no progress bar) were added beyond the PRD's
  bare `-w`/`-gw` framing since MONA prints unrelated noise on both
  otherwise.
- **Checked-in fixture as a real file**, `tests/fixtures/mona/small_example.mona`
  (new `tests/fixtures/` directory — the codebase's other subprocess-input
  fixtures, e.g. `tests/ltlfsynt_oracle_test.cpp`'s `%%LAMBDA` transducers,
  are inline C-string literals). A real `.mona` file was used because the
  PRD's Phase 1 green checkpoint explicitly calls for "a checked-in `.mona`
  fixture".
- **`mona`-absent skip gate.** Mirrors the existing `LTLFSYNT_EXECUTABLE`
  policy (`CMakeLists.txt`): `find_program(MONA_EXECUTABLE mona)` +
  `MONA_FOUND` compile define + `GTEST_SKIP()` in `MonaDfaTest::SetUp()`, so
  a clean box without `mona` installed still builds and tests green. The
  parser-only tests (`MonaDfaParser.*`, hand-written `-w` text, no
  subprocess) always run regardless.
