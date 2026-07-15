# PRD: oracle assumption-faithfulness guard (+ delay-$\Tin$ fixture fix)

**Status:** implemented — `tests/ltlfsynt_oracle_test.cpp` (uncommitted, branch
`master`)
**Interface:** test-only. Extends the `tests/ltlfsynt_oracle_test.cpp` suite
(`docs/prd/ltlfsynt-oracle.md`); no production C++, no new `Synthesis` method.
Reuses `parse_transducer` and `ltlf_to_dfa` from the library (already linked into
`unit_tests`).
**main.tex ref:** the conjecture `\cl` note after `\cref{def:probDefTransducer}`
(main.tex:133–135); `\cref{def:consistency}` (Case-A totality) for why a $\Tin$ run is
well-defined; §86 for Mealy turn order.

**Gates:**
- [x] glossary        — "Produced-trace language" ($\psiin/\psiout$) + "Faithfulness
      guard" added (2026-07-05, `/glossary`); "monolithic/assumption reduction" entry
      deferred by decision (open conjecture, main.tex:133, not implemented)
- [x] tests           — the guard + corrected corpus + guard meta-oracle
- [x] code-review     — domain (/code-reviewer) + generic (/code-review)
- [x] theory-review   — code ↔ math faithful (2026-07-05, `/theory-review`); no
      code-bug. main.tex:135 "known soundness boundary" is a stale misdiagnosis
      (doc-bug) — a `\cl` correction is drafted in the review report for the
      Overleaf edit, not applied here.

## Goal
The `ltlfsynt` external oracle (`docs/prd/ltlfsynt-oracle.md`) pairs, per fixture,
a **hand-authored** $\text{LTL}_f$ assumption $\psi_{in}$ with a $\Tin$ transducer
**file**, asserting both denote the same language, and deliberately shares **no
code** between the two paths to stay independent. That independence is exactly
what let the *delay* $\Tin$ fixture drift: its $\psi_{in}$ was written
`(!k) & G(X(k <-> a))`, which asserts $(k \leftrightarrow a)$ at position $t{+}1$
— i.e. **copy-from-step-1** ($k_t \leftrightarrow a_t$ for $t\ge1$), **not** delay
($k_t = a_{t-1}$). The oracle's "Excluded class" divergence witness
$\varphi=\text{X[!]}(a \rightarrow \text{X[!]}\,k)$ was therefore comparing the
**delay transducer** (EK side, REALIZABLE) against a **copy-from-1 assumption**
(`ltlfsynt` side, UNREALIZABLE). Two different languages; no soundness boundary.

This PRD (a) **corrects** the delay $\psi_{in}$ so the fixture matches its
transducer, turning the "excluded class" witness into a passing **load-bearing
flip**, and (b) adds a **faithfulness guard**: a mechanical, author-blind-spot-
independent cross-check that each fixture's $\psi_{in}$ and its transducer file
agree, so this class of drift cannot recur silently. It does **not** attempt to
prove the monolithic conjecture (main.tex:133) — it only removes a false
counterexample and rails against the mistake that produced it.

## Ubiquitous-language terms used
All canonical (docs/GLOSSARY.md) unless flagged:

- **External knowledge strategy** ($\Tin$) → `Transducer` (`t_in`), materialised
  by `parse_transducer(in, partition, Role::t_in, dict)`.
- **Output-labeled transducer** → `OutputLabeledTransducer` — the parsed object
  the guard **runs** via `delta` / `lambda`.
- **Transition function / Output function** → `delta(q, v)` / `lambda(q, v)`.
- **Observed / produced slice** $\Sigma_0,\Sigma_1$ → for $\Tin$,
  $\Sigma_0=\Ifree$, $\Sigma_1=\Iknown$ (`sigma_slices`, `Role::t_in`).
- **Free/Known inputs** $\Ifree,\Iknown$ → `VariablePartition`.
- **Goal DFA construction** → `ltlf_to_dfa(psi_in, dict)` — reused to evaluate
  finite-$\text{LTL}_f$ membership of a concrete trace against $\psi_{in}$.
- **Letter** $v\in2^{\mathcal{I}\cup\mathcal{O}}$ / **Cube** → the per-step
  assignments the run produces.

**Glossary gaps (flag for `/glossary`, do not block):**
- **Produced-trace language of a transducer** — the $\text{LTL}_f$ language
  $\psi_{in}$ (resp. $\psi_{out}$) of the traces $\Tin$ (resp. $\Tout$) produces
  (main.tex:133 names it informally, "the $\text{LTL}_f$ languages of the traces
  produced by $\Tin,\Tout$"). The guard is precisely a check on this concept; it
  may warrant a prose entry even without a C++ identifier.
- **Faithfulness guard** / **assumption reduction** — test-only concepts; likely
  no C++ identifier (the pairing is hand-authored in fixtures). Confirm with
  `/glossary` whether a prose note is wanted.

## Behaviour / semantics (from main.tex)

### 1. Corrected delay $\psi_{in}$
Replace the delay assumption string, everywhere it appears (Table D rows and the
excluded-class test), with a **pure safety** encoding of $k_t=a_{t-1}$, $k_0=\bot$:

```
psi_in(delay) = (!k) & G(a -> X k) & G(!a -> X !k)
```

This uses **weak** `X` in the guarded form, so it imposes safety only and adds no
forced-continuation obligation, and — unlike the naive `(!k) & G(a <-> X k)` — it
does **not** spuriously force `a` true at the last step (that trap comes from weak
`X k` returning true at the final position, making `a <-> X k` collapse to `a`).
Verified end-to-end (both binaries):

| pairing for $\varphi=\text{X[!]}(a\to\text{X[!]}\,k)$ | EK (`ltlf-ek-synth`) | reduction (`ltlfsynt`) |
|---|---|---|
| delay transducer / **corrected** $\psi_{in}$ | REALIZABLE | **REALIZABLE** |
| delay transducer / old `G(X(k<->a))` (copy-from-1) | REALIZABLE | UNREALIZABLE |
| bare $\varphi$ (drop $\psi_{in}$) | — | UNREALIZABLE |

So the witness becomes a **passing** row that **agrees** (both REALIZABLE) and is
**load-bearing** (bare $\varphi$ is UNREALIZABLE). Corrected $\psi_{in}$ validated
independently: accepts the delay word `(a&!k)(!a&k)`, rejects the copy word
`(a&!k)(!a&!k)`, is satisfiable, and does not force `a` at the last step.

### 2. The faithfulness guard (mechanically-generated traces)
For each known-input fixture (a $\Tin$ file + its $\psi_{in}$ string), the guard
proves *non-equivalence* would be caught by cross-checking the **two artifacts
the author already wrote against each other** — never against a third
author-supplied artifact (a hand-labeled trace would inherit the author's blind
spot and pass vacuously). Concretely:

```
guard(transducer_file, psi_in, partition):
    tau   = parse_transducer(transducer_file, partition, Role::t_in, dict)   # trusted run engine
    A_psi = ltlf_to_dfa(psi_in, dict)                                        # finite-LTLf membership
    for seq in Ifree_sequences(up_to N=5):        # exhaustive if (2^|Ifree|)^N <= 4096
        trace = run(tau, seq)                      #   else random-sample K=4096, fixed seed
        ASSERT accepts(A_psi, trace)               # (a) psi_in not too STRONG
        for m in single_bit_Iknown_mutations(trace):
            ASSERT not accepts(A_psi, m)           # (b) psi_in not too WEAK
```

- **`run(tau, seq)`** — drive $\delta$ from $q_0$; at each step the letter's
  $\Iknown$-slice is $\lambda(q,\cdot)$ evaluated on the current $\Ifree$-slice
  (the $\Ofree$ bits are don't-cares — pick a canonical value, e.g. false).
  Well-defined because $\Tin$ is deterministic and **total** in the committed
  Case-A regime (`\cref{def:consistency}`, glossary "Partial transducers — resolved").
- **`accepts(A_psi, trace)`** — walk the concrete finite trace through
  `ltlf_to_dfa(psi_in)` and test the finite-acceptance mark at the reached state.
  Membership of a *concrete* finite trace is unambiguous, so this is safe to
  evaluate with the same finite-$\text{LTL}_f$ machinery the EK side uses; it
  feeds **no verdict**, so verdict-independence (the oracle's core property) is
  untouched.
- **Mutation soundness** — flipping one $\Iknown$ bit at one position yields a
  trace whose required $\Iknown$ (a *function* of the fixed $\Ifree$-history)
  is uniquely violated, so it is genuinely out of $\psi_{in}$'s language. Relies
  on $\Tin$ determinism + totality; note this in a comment.

### 3. Guard meta-oracle (prove the guard isn't a no-op)
A guard that never fires is worthless. Add one test that runs the guard on the
**known-bad** pairing — the delay transducer with the **old** `G(X(k <-> a))`
string — and asserts it **fails** (the mutation/acceptance check trips). This
pins that the guard actually detects the drift it was built for.

## Interfaces & types
No production C++. In the test target:

- **New test helpers** (in `tests/ltlfsynt_oracle_test.cpp`, or a sibling
  `tests/oracle_faithfulness_test.cpp` sharing the harness):
  - `run_transducer(const OutputLabeledTransducer&, const std::vector<bdd>& ifree_seq) -> std::vector<bdd>`
    — the full-trace run above.
  - `accepts_ltlf(const spot::twa_graph_ptr& a_psi, const std::vector<bdd>& trace) -> bool`
    — finite-acceptance walk.
  - an enumeration/sampling driver for $\Ifree$-sequences (N=5, cap 4096, fixed
    seed) and a single-bit $\Iknown$ mutation generator.
  - `run_faithfulness_guard(transducer_src, psi_in, partition)` tying them
    together, called once per distinct $(\Tin,\psi_{in})$ pair.
- **Reused as-is:** `parse_transducer` (`transducer_io.hpp`), `ltlf_to_dfa`
  (`ltlf_to_dfa.hpp`), the `VariablePartition`, and the existing subprocess/
  temp-file harness (`RunEkSynth`, `RunLtlfsynt`, `ScopedTempFile`).
- **Changed fixtures:** the delay $\psi_{in}$ string in the Table D rows and in
  the (un-disabled, renamed) former `DISABLED_ExcludedClassStrongXOnKnownInput...`
  test.

## Edge cases
- **Empty knowledge ($\mathcal{V}=\emptyset$, Table E)** — no $\Tin$,
  $\psi_{in}=\top$; the guard is vacuous → **skip** (nothing to cross-check).
- **Partial transducer** — a $\Ifree$-sequence reaching an undefined
  $\delta/\lambda$ (`nullopt`) produces no trace → skip that sequence. Only arises
  outside Case-A; out of the current corpus's scope but handle rather than crash.
- **$\Ofree$ don't-cares in membership** — $\psi_{in}$ mentions only
  $\Ifree\cup\Iknown$; fix $\Ofree$ to a canonical value when building letters so
  acceptance is well-defined and $o$-independent.
- **Wide $\Ifree$** — exhaustive enumeration blows up as $(2^{|\Ifree|})^N$; the
  cap → seeded random sampling keeps it deterministic and bounded.
- **`ltlfsynt` absent** — unchanged: the verdict rows `GTEST_SKIP()`; the guard
  itself needs only the **library** (`parse_transducer`, `ltlf_to_dfa`), so it can
  run even where `ltlfsynt` is missing.
- **$\Tout$ / known-output** — out of scope (the oracle is $\Tin$-only); the guard
  is written for $\Tin$ ($\Sigma_0=\Ifree,\Sigma_1=\Iknown$). A $\Tout$ analogue
  is deferred with the $\Tout$ oracle in `docs/BACKLOG.md`.

## Test oracles (for /test-writer)
- **Corrected witness row** — `ltlf-ek-synth`=REALIZABLE, `ltlfsynt` on
  `(psi_in_delay_corrected) -> (X[!](a -> X[!] k))`=REALIZABLE, bare
  $\varphi$=UNREALIZABLE (load-bearing). Now a normal passing agreement, no longer
  `DISABLED_`.
- **Guard over the whole $\Tin$ corpus** — run `run_faithfulness_guard` on
  const-true `G(k)`, const-false `G(!k)`, copy `G(k <-> a)`, and delay (corrected)
  fixtures; all pass. (Only delay's string changes; the guard confirms the other
  three were already faithful.)
- **Guard meta-oracle** — the guard **fails** on the known-bad delay/`G(X(k<->a))`
  pairing (§ Behaviour #3). Encode as an explicit "expected the guard trips"
  assertion, not a silently-skipped case.
- **Re-verify Table A–E** — after the delay-string change, re-run every corpus
  row through both binaries; only Table D rows are affected and must still agree.
  A row that fails to reproduce is a signal to investigate, not to adjust the
  expectation (the oracle's standing rule).

## Open theory questions touched
- **The "known soundness boundary" in main.tex:135 is false as stated.** The `\cl`
  note claims "a known soundness boundary is a strong-$\text{X}$ continuation
  obligation on an $\Iknown$ variable under nesting, where the system-controlled
  continuation of a total strategy is not reproduced by the implication." The
  witness that motivated it is a $\psi_{in}$↔transducer **mis-encoding**, not a
  reduction unsoundness (the correct delay $\psi_{in}$ makes the reduction agree
  with EK). Two further facts sharpen this for `/theory-review`: (i) the "strong-X
  $\psi_{in}$ flips it back to REALIZABLE" observation is **vacuous** —
  `(!k) & G(X[!](k<->a))` is **UNSAT** in $\text{LTL}_f$ (a `G(X[!]…)` cannot hold
  at a finite trace's last position), so the implication is trivially realizable
  with an unsatisfiable antecedent; (ii) with the corrected $\psi_{in}$ the corpus
  contains **no** genuine divergence. `/theory-review` should correct the `\cl`
  note (under a `\cl`, per the LaTeX workflow) — the diagnosis, not necessarily
  the conjecture, is what's wrong.
- **The monolithic conjecture (main.tex:133) is still open.** This PRD removes its
  one cited counterexample; it does **not** prove equirealizability. Do not
  upgrade the conjecture to a theorem here. The $\Tout$ guarantee half and the
  empty-word convention remain unexamined.
- **Prose corrections entangled with the code change** (to be done by
  `/developer` when landing, so doc and test stay coherent): rewrite the
  `docs/prd/ltlfsynt-oracle.md` "Excluded class" and "Open theory questions →
  Soundness boundary" sections (the divergence was a fixture bug), and update the
  `docs/BACKLOG.md` monolithic-reduction "Soundness boundary (must be
  characterised)" seed accordingly. `main.tex` is **not** edited by `/developer` —
  it is flagged above for `/theory-review`.

## Definition of done
- Delay $\psi_{in}$ corrected to `(!k) & G(a -> X k) & G(!a -> X !k)` in the
  Table D rows and the former excluded-class test; that test is enabled, renamed,
  and asserts agreement (both REALIZABLE) with the load-bearing bare-$\varphi$
  guard.
- Faithfulness guard implemented (N=5, cap 4096, fixed seed; both directions;
  don't-care $\Ofree$; partial/empty handled) and run over all four $\Tin$
  fixtures — green.
- Guard meta-oracle present and passing (guard fires on the old `G(X(k<->a))`
  delay pairing).
- Full Table A–E re-verified against both binaries where `ltlfsynt` is present;
  suite skips cleanly where it is not; the guard runs regardless (library-only).
- `docs/prd/ltlfsynt-oracle.md` "Excluded class"/"Open theory questions" and the
  `docs/BACKLOG.md` seed corrected; `main.tex:135` flagged for `/theory-review`.
- `/glossary` consulted on the flagged gaps ("produced-trace language",
  "faithfulness guard").

## Developer comments / PRD disagreements
_(2026-07-05, `/developer`)_

- **No PRD disagreements.** The corrected $\psi_{in}$ string, the corrected-witness
  table, and the guard pseudocode were encoded as given; nothing needed
  overriding.
- **Test authorship deviates from the usual `/developer` → `/test-writer`
  handoff.** This PRD is test-only (no production C++), and the launching agent's
  scope explicitly assigned the guard helpers and the corrected fixture to
  `/developer` directly rather than deferring to `/test-writer`. All test code
  above (`run_transducer`, `accepts_ltlf`, the enumeration/sampling driver,
  `single_bit_iknown_mutations`, `run_faithfulness_guard`, the renamed
  load-bearing test, and the four-fixture + meta-oracle guard tests) therefore
  landed in this PRD's implementation step, not a follow-up `/test-writer` pass.
  `ctest` is green: 169/169, zero `DISABLED_`. `/test-writer` should still do its
  normal job on the broader Table A–E re-verification (see below) and may
  restructure/extend the guard helpers if it sees fit.
- **One design choice not pinned by the PRD: `run_faithfulness_guard`'s return
  type.** The PRD's Interfaces section doesn't specify what
  `run_faithfulness_guard` returns. Implemented as a plain
  `GuardResult { bool ok; std::string detail; }` (no gtest assertions inside),
  precisely so the guard meta-oracle (§ Behaviour #3) can assert the guard
  **fails** on the known-bad pairing without red-flagging the whole suite;
  callers wrap it in `EXPECT_TRUE`/`EXPECT_FALSE` at each call site.
- **`Ifree_sequences(up_to N=5)` interpreted as every length 1..5, not a single
  length-5 batch**, so the guard also exercises the shorter traces where a
  last-position weak-`X` trap can hide. Length **0** (the empty trace) is
  deliberately excluded: this project's $\text{LTL}_f$ traces are non-empty
  (`main.tex` §85), so `ltlf_to_dfa`'s initial state is non-accepting for
  *every* $\psi_{in}$, even a trivially-true one
  (`tests/ltlf_to_dfa_test.cpp:TriviallyTrueRejectsEmptyWordButAcceptsAfterOneStep`).
  Including length 0 made all four fixtures spuriously fail assertion (a) before
  this exclusion was added — confirmed by actually running the guard, not by
  hand-derivation.
- **Ofree don't-cares**: not materialised at all, rather than fixed to a
  canonical `false`. None of the four $\Tin$ fixtures' transducer files or
  $\psi_{in}$ strings mention an $\Ofree$ AP (`o`), so on each guard call's
  fresh, private `bdd_dict` no `o` variable is ever registered — there is
  nothing to fix. The PRD's edge case is handled by construction (no crash, no
  spurious constraint), not by an explicit canonical-value step; noted here so
  a future $\Ofree$-mentioning fixture isn't assumed already covered.
- **Doc staleness flagged, not fixed (out of assigned scope):**
  `docs/prd/ltlfsynt-oracle.md`'s **Table D** section (its `psi_in` string and
  row verdicts) still shows the pre-correction copy-from-step-1 string and its
  verdicts, and its "Gates" tests bullet / "Definition of done" bullet still
  describe the old `DISABLED_` excluded-class test and the old case count. Only
  the "Excluded class" and "Open theory questions → Soundness boundary" sections
  were reassigned to this landing step; the broader Table A–E re-verification
  (which would refresh Table D's doc text and the Gates/DoD bullets) is
  `/test-writer`'s job per this PRD's own scope note.
- **Glossary gate left unticked.** Both flagged gaps ("produced-trace language of
  a transducer", "faithfulness guard" / "assumption reduction") remain
  unconfirmed with `/glossary`; per this PRD's own read, they likely warrant at
  most a prose note (no C++ identifier — the new helpers are test-local,
  anonymous-namespace functions, not public library API). Left for `/glossary`
  to decide.
- **`main.tex:135` (the `\cl` note misdiagnosing the "known soundness boundary")
  is flagged for `/theory-review`, not edited here** — `main.tex` is Overleaf-only
  and out of `/developer`'s scope. See the PRD body, § Open theory questions.
