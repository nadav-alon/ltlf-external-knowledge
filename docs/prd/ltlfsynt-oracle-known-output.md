# PRD: `ltlfsynt` external-tool oracle — known-**output** ($\Tout$) guarantee reduction

**Status:** draft
**Interface:** extends the existing GoogleTest suite `tests/ltlfsynt_oracle_test.cpp`;
**not** a `Synthesis` method and **no production C++**. Drives the built
`ltlf-ek-synth` and Spot's `ltlfsynt` as subprocesses and compares only the
realizability verdict. The one changed signature is the test-local
*Faithfulness guard* helper, generalized over `Role`.
**Recommended workflow:** concurrent — freeze confidence *high* (the only
signature falls straight out of `Role` + `sigma_slices`). In practice there is no
`/developer` half at all: this is a `/test-writer`-only PRD, so "concurrent"
means the two phases carry no cross-agent contract to churn.
**main.tex ref:** `\cref{def:probDefTransducer}` (the $\Tout$ half of the problem)
and the `\na` conjecture note immediately following it ($\psiin \to (\varphi \land
\psiout)$ — the claim this oracle tests); the $\Tout$ signature
$\lambda_{out}: Q_{out} \times 2^{\mathcal{I}\cup \Ofree} \to 2^{\Oknown}$ in the
§Transducers align block; `\cref{def:consistency}` (the partiality clause and the
Case-A totality the reduction leans on). The method under test is Method 2
(`\cref{alg:dfa_product}`). No new algorithm.

**Gates:**
- [ ] glossary        — new terms in docs/GLOSSARY.md C++ column
- [ ] tests           — unit + oracle coverage
- [ ] code-review     — domain (/code-reviewer) + generic (/code-review)
- [ ] theory-review   — code ↔ math faithfulness vs main.tex

## Goal
Close the deferred half of `docs/prd/ltlfsynt-oracle.md`. That PRD gave the
project an external, independent realizability oracle for a known-**input**
$\Tin$, encoded as an $\text{LTL}_f$ **assumption** $\psiin$, and explicitly
scoped the known-**output** $\Tout$ case out ("a guarantee/conjunction, not an
assumption ... deferred to `docs/BACKLOG.md`"). This PRD builds that guarantee
half, and then the **composed** form, so the oracle finally tests the whole of
`main.tex`'s conjecture rather than its antecedent:

> `ltlf-ek-synth --dfa-product --formula phi --part-file P --known-output-transducer T --realizable`
> is equirealizable with
> `ltlfsynt --ins=Ifree --outs=Ofree,Oknown -f "(phi) & (psi_out)" --semantics=Mealy --realizability`

and, mixed with a known input,

> `ltlfsynt --ins=Ifree,Iknown --outs=Ofree,Oknown -f "(psi_in) -> ((phi) & (psi_out))" ...`

This **does not supersede** `docs/prd/ltlfsynt-oracle.md` — it extends it in
place, and Tables A–E of that PRD stay exactly as they are.

**Relationship to the deps oracle (no overlap).**
`tests/ltlf_ek_deps_test.cpp`'s O1 already runs a $\Tout$ through `ltlfsynt` —
but against **bare $\varphi$**, with *no* $\psiout$ conjunct
(`docs/prd/output-dependencies-tool.md` "Test oracles" O1). That is sound only
because a deps-derived $\Tout$ is extracted **from $\varphi$ itself**, so
`\cref{lem:outdep-transducer}` claims it constrains nothing that $L(\varphi)$
did not already constrain. A general, hand-authored $\Tout$ has no such
property and needs a real guarantee conjunct. So O1 tests one specific
$\Tout$-construction's *harmlessness*; this PRD tests the *reduction* for an
arbitrary $\Tout$. Complementary, not redundant.

## Why the reduction is equirealizable (the correctness argument)
Known-output problem (`\cref{def:probDefTransducer}`, §Transducers align block):
the environment picks $\Ifree$; the Mealy controller sees $\mathcal{I}$ and picks
$\Ofree$; then $\Tout$ produces $\Oknown$ from $\Sigma_0=\mathcal{I}\cup\Ofree$
($\Sigma_1=\Oknown$, glossary *Role*). Win iff every agreeing trace satisfies
$\varphi$.

Reduction: keep $\Ifree$ on `--ins`, move $\Oknown$ onto `--outs` **beside**
$\Ofree$, and synthesize $\varphi \land \psiout$.

1. **The system's freedom over $\Oknown$ is illusory.** $\Tout$ is deterministic
   and total in the committed **Case-A** regime (glossary *Open theory questions
   → Partial transducers*, `\cref{def:consistency}`), so $\psiout$ pins $\Oknown$
   to exactly **one** value given the history and the current
   $\mathcal{I}\cup\Ofree$. Any other choice falsifies the conjunct and loses
   immediately. It is a **forced move**, not a real choice. This is precisely the
   argument `solve_mtdfa` already depends on, where $\Iknown,\Oknown$ are made
   *controllable-but-forced* because "$\cons$ pins each to exactly one legal
   value" (glossary *Game solving*).
2. **Simultaneous choice is the right shape, not an approximation.** In the EK
   problem $S_C$ picks $\Ofree$ from (history, $\mathcal{I}$) and $\Tout$ then
   picks $\Oknown$ from (history, $\mathcal{I}\cup\Ofree$); composing the two
   gives $\Oknown$ as a function of (history, $\mathcal{I}$). A `ltlfsynt` Mealy
   strategy choosing the **pair** $(\Ofree,\Oknown)$ from (history,
   $\mathcal{I}$) has exactly that signature. Both players' sub-moves belong to
   the *same* player, so sequentialising them neither adds nor removes
   information. No turn-order loss.
3. **Both directions.** EK-winning ⇒ reduction-winning: drive $\Oknown$ per
   $\Tout$; the trace satisfies $\varphi$ and, by construction, $\psiout$.
   Reduction-winning ⇒ EK-winning: a winning strategy must satisfy $\psiout$, so
   its $\Oknown$ choices coincide with $\Tout$'s on every reachable history;
   the induced EK trace is the same trace and satisfies $\varphi$.

**The asymmetry that matters operationally** (and the reason this is not a
copy-paste of the $\Tin$ oracle): deviation is punished in **opposite
directions**. A deviating environment makes the $\Tin$ implication *vacuously
true* — a benign escape hatch. A deviating system makes the $\Tout$ conjunction
*false* — the punishment **is** the pinning mechanism. Consequences pinned in
*Behaviour* #2 and #5 below.

## Ubiquitous-language terms used
All already in `docs/GLOSSARY.md`; no new terms, no gaps:

- **Goal formula** $\varphi$ → `phi`.
- **Inputs / Outputs** and the four-way split $\Ifree,\Iknown,\Ofree,\Oknown$ →
  `VariablePartition`.
- **External knowledge strategy** ($\Tout$) → `Transducer` (`t_out`), materialised
  by `parse_transducer(..., Role::t_out, ...)`.
- **Observed / produced slice** $\Sigma_0,\Sigma_1$ → `sigma_slices` — here
  $\Sigma_0=\mathcal{I}\cup\Ofree$, $\Sigma_1=\Oknown$ (glossary *Role*).
- **Produced-trace language** $\psiout$ → no C++ type; a hand-authored
  `spot::formula`/string per fixture, deliberately **not** auto-derived (glossary
  entry: auto-derivation "would defeat oracle independence").
- **Consistency** $\cons$ → `consistent`; **Output agreement** → `emits`.
- **Turn order** — the load-bearing property Table G is designed to detect.
- **Role** → `Role::t_out`.
- **Faithfulness guard** → `run_faithfulness_guard` (generalized here; see
  *Interfaces & types*).
- **DFA product** (Method 2) → `DfaProduct`, the method the CLI wraps.
- **Transducer file format (`%%LAMBDA` block)** → the on-disk $\Tout$ fixtures.

**Glossary follow-up (flag for `/glossary`, do not block):** the *Faithfulness
guard* entry currently describes a $\Tin$-only object — it names
`run_faithfulness_guard(transducer_src, psi_in, partition)` and speaks of
"$\psiin$" and "a single-bit $\Iknown$ mutation". Phase 2 generalizes it over
`Role`; the entry needs its signature and prose widened to both roles. This is a
**wording update to an existing entry**, not a new term.

## Behaviour / semantics (from main.tex)
The oracle asserts a **verdict-only** (realizability boolean) equivalence. It
adds no synthesis semantics. Inherited unchanged from
`docs/prd/ltlfsynt-oracle.md` *Behaviour*: curated hand-paired fixtures (#1),
`--semantics=Mealy` pinned explicitly on every invocation (#2), verdict compared
by the printed **word** with unexpected exit codes failing loudly on captured
stderr (#3). New or changed for the $\Tout$ half:

1. **$\Oknown$ goes on `--outs`, never `--ins`.** $\Sout$ is a **system-side**
   helper (`\cref{def:probDefTransducer}`; $\Oknown\subseteq\mathcal{O}$), so the
   reduction is a **conjunction**, not an implication. Putting $\Oknown$ on
   `--ins` would model an adversarial $\Oknown$ — a different game entirely, and
   an easy and silent mis-encoding.

2. **The load-bearing flip direction is INVERTED relative to Tables A–C.** This is
   the single most important thing for `/test-writer` to internalise. $\Tin$
   constrains the *environment*, so knowledge can only **help**: its flips are
   bare **U** → with-assumption **R**. $\Tout$ constrains the *system's own*
   $\Oknown$, so relative to plain synthesis it strictly **removes choice** and
   can only **hurt**: its flips are bare **R** → with-guarantee **U**. A
   discriminating $\Tout$ fixture is therefore a $\varphi$ that the guarantee
   makes *unrealizable*. Hunting for the $\Tin$ shape here finds nothing. Every
   ✅-flip row in Tables F–J below is R→U; every ✅-flip row in Tables A–C is
   U→R.

3. **Load-bearing guard = drop $\psiout$, keep $\Oknown$ on `--outs`.** Run
   `ltlfsynt --ins=… --outs=Ofree,Oknown -f "phi"` and assert the verdict
   **differs**. $\psiout=\top$ is the *extremal* weakening — it hands the system
   total freedom over $\Oknown$ — so no weaker-but-nonzero guarantee could
   discriminate better. This is also the direct answer to the backlog's seed
   ("verify the conjunction pins it"): a $\psiout$ too weak to pin $\Oknown$
   would fail to move the verdict, and the guard fails.

4. **$\Tout$ fixtures must be TOTAL.** By `\cref{def:consistency}`'s partiality
   clause a missing $\delta$ or $\lambda$ makes a letter inconsistent for *every*
   party, so a partial $\Tout$ would delete $\Ifree$ letters and illegally
   constrain the environment — glossary *Output-dependency extraction* calls this
   "$\Tout$'s single most dangerous failure mode". Every fixture in this PRD is
   total (single-state `[t]` self-loop, or the 2-state delay whose $\delta$ is
   total on `a`). Partial-$\Tout$ coverage is deliberately **out of scope** — see
   *Open theory questions*.

5. **The weak-`X` trap bites HARDER on the guarantee side, and silently.** An
   over-strong $\psiout$ (using `X[!]`, forcing continuation) makes the
   conjunction unsatisfiable at a trace end the system would legally stop at, so
   the reduction reports **UNREALIZABLE** where EK reports **REALIZABLE**. That
   presents as *"our tool is broken"*, not *"my fixture is wrong"* — the exact
   misdiagnosis that cost `docs/prd/ltlfsynt-oracle.md` a phantom "soundness
   boundary" on the $\Tin$ side. Note the asymmetry with $\Tin$, where an
   over-strong $\psiin$ is *vacuously dischargeable* and merely makes the test
   pass for the wrong reason. **Verified, not hypothesised** — see the negative
   control in *Test oracles* (Table J-bad), which reproduces exactly this
   divergence on 2 of 4 rows. This is why Phase 2 (the *Faithfulness guard*) is
   not optional polish.

## Interfaces & types
**Freeze confidence: high.** There is **no production C++**. The single changed
signature is a test-local helper whose new parameter falls straight out of the
existing `Role` enum and the `sigma_slices` derivation — nothing is being
invented here.

- **`tests/ltlfsynt_oracle_test.cpp`** — extended in place. New corpus row
  structs and parameterized suites mirroring the existing `KnownInputRow` /
  `KnownInputOracleTest` shape:
  - a **known-output** row carrying `{name, t_out_src, psi_out, phi, expect_realizable,
    load_bearing}` and its suite, invoking
    `ltlf-ek-synth --dfa-product --realizable --part-file P --known-output-transducer T --formula phi`
    against `ltlfsynt --ins=a --outs=o,x --semantics=Mealy --realizability -f "(phi) & (psi_out)"`;
  - a **mixed** row additionally carrying `{t_in_src, psi_in}` and its suite,
    invoking both `--known-input-transducer` and `--known-output-transducer`
    against `ltlfsynt --ins=a,k --outs=o,x … -f "(psi_in) -> ((phi) & (psi_out))"`;
  - AP-naming guards for both new corpora, mirroring
    `LtlfsyntOracleApNaming.KnownInputCorpusApsMatchPartFile`.

  Reused **as-is**: `CliResult`, `ShellQuote`, `RunSubprocess`, `RunEkSynth`,
  `ResolveLtlfsyntBinary`, `IsRunnable`, `ParseEkSynthVerdict`,
  `ParseLtlfsyntVerdict`, `LtlfsyntOracleTest`'s `GTEST_SKIP` fixture. **No CMake
  change** — `find_program(LTLFSYNT_EXECUTABLE ltlfsynt)`, `LTLFSYNT_BINARY`,
  the `LTLFSYNT_BIN` env override and the skip wiring already exist and are
  unchanged.

- **`run_faithfulness_guard` — generalized over `Role`** (Phase 2). Current
  signature (test-local, anonymous namespace):

  ```cpp
  GuardResult run_faithfulness_guard(const std::string& transducer_src,
                                     const std::string& psi_in,
                                     const VariablePartition& partition);
  ```

  becomes:

  ```cpp
  GuardResult run_faithfulness_guard(const std::string& transducer_src,
                                     const std::string& psi,
                                     const VariablePartition& partition,
                                     Role role);
  ```

  with the observed/produced slices derived by the existing
  `sigma_slices(partition, role)` rather than hard-coded to
  $(\Ifree,\Iknown)$ — so for `Role::t_out` the enumerated observation sequences
  range over $\mathcal{I}\cup\Ofree$ and the "too weak" mutation flips a single
  **$\Oknown$** bit instead of an $\Iknown$ bit. `GuardResult` is unchanged. The
  existing $\Tin$ call sites pass `Role::t_in` explicitly (do **not** default the
  parameter — a defaulted `Role` is exactly how a $\Tout$ pair would silently get
  guarded under $\Tin$ slices and pass vacuously).

  **One substantive behavioural note for the implementer, not a discovery task.**
  Under `Role::t_in` the enumeration fixes only $\Ifree$ and leaves $\Ofree$ a
  don't-care, which is sound because no corpus $\psiin$ mentions an $\Ofree$ AP —
  the comment inside `run_transducer` records exactly this ("none of the four Tin
  fixtures' transducer files or psi_in strings mention an Ofree AP"). That
  reasoning **does not carry over**: a $\Tout$'s $\Sigma_0$ *contains* $\Ofree$,
  so $\psiout$ legitimately mentions it and the enumeration must fix all of
  $\mathcal{I}\cup\Ofree$. The consequence is a larger per-step branching factor
  ($2^{|\mathcal{I}|+|\Ofree|}$ rather than $2^{|\Ifree|}$).

  **Bounds and determinism are pinned — do not re-tune them.** Keep
  `kGuardMaxSeqLen = 5`, `kGuardEnumCap = 4096`, `kGuardSampleCount = 4096` and
  `kGuardSampleSeed = 20260705` **unchanged**, and let the existing
  `pow_saturating` cap decide exhaustive-vs-sampled enumeration for the larger
  $\Tout$ alphabet exactly as it already does for $\Tin$. Concretely, for the
  corpus partition ($\mathcal{I}\cup\Ofree=\{a,o\}$, so 4 observations per step)
  $4^5 = 1024 \le 4096$, i.e. the $\Tout$ pairs still enumerate **exhaustively**
  and no sampling path is taken; the seed matters only if a future fixture widens
  the partition. Raising `kGuardMaxSeqLen` to compensate for the bigger alphabet
  would be the wrong lever — it grows the space it is meant to bound.

**If implementation proves this contract wrong:** that is a PRD-change event —
update this section and propagate to any in-flight branch; the implementer does
not silently re-shape the helper's signature on its own branch.

## Implementation phases

- **Phase 1 — the corpus.** Add the $\Tout$-only tables (F–J) and the mixed
  tables (M1–M2) as parameterized suites, plus the two AP-naming guards. Purely
  **additive**: no shipped code is touched, `run_faithfulness_guard` is not
  called on the new pairs yet. *Green checkpoint:* the suite compiles and every
  new row passes with the verdicts tabulated below (and skips cleanly where
  `ltlfsynt` is absent); Tables A–E remain byte-identically green.
- **Phase 2 — the guard.** Generalize `run_faithfulness_guard` over `Role`,
  update the existing $\Tin$ call sites to pass `Role::t_in`, apply it to every
  $(\Tout,\psiout)$ pair from Phase 1, and add the meta-oracle asserting the
  guard **fires** on the known-bad over-strong pairing (Table J-bad). *Green
  checkpoint:* all of Phase 1 still green, every $\Tout$ pair passes its guard,
  and the meta-oracle confirms a non-vacuous guard.

Phase 1 lands first deliberately: Phase 2 is the only part that can regress a
gate-closed helper the shipped $\Tin$ corpus depends on, so it lands with the new
corpus already green underneath it as a regression net.

## Edge cases
- **`ltlfsynt` absent / not runnable** — `GTEST_SKIP()` via the existing fixture;
  never a hard failure. Unchanged.
- **Empty $\Oknown$ with `--known-output-transducer` given** — the CLI already
  rejects this (`src/ltlf_ek_synth.cpp:214`, "`--known-output-transducer` given
  but output_known is empty (ambiguous …)"), and symmetrically rejects a
  non-empty `output_known` with no transducer (`:211`). Both are existing
  `cli_test` territory; this PRD does not re-test them, but `/test-writer` must
  not author a fixture that trips either.
- **Empty $\Ofree$ with non-empty $\Oknown$** — legal and interesting: $\Tout$'s
  $\Sigma_0$ collapses to $\mathcal{I}$, and the system has no free moves at all
  while $\Oknown$ is still forced. Include one smoke fixture, mirroring the
  existing `EmptyOutputsAcceptedByBothTools*` pair.
- **Verdict-vs-error ambiguity** — unchanged from the $\Tin$ PRD: `ltlfsynt`'s
  exit 1 *is* UNREALIZABLE, so a parse error exiting 1 would masquerade. Parse
  the printed word; treat "no verdict word on stdout" as an error and fail with
  captured stderr.
- **AP naming** — $\psiout$, $\psiin$ and $\varphi$ must reference exactly
  $\Ifree\cup\Iknown\cup\Ofree\cup\Oknown$; a typo'd AP silently becomes a fresh
  free input to `ltlfsynt` and can flip the verdict. Assert fixture AP sets
  against the part file, as the $\Tin$ corpus already does.
- **$\Tout$ HOA AP set differs from $\Tin$'s.** A $\Tin$ fixture declares
  `AP: 2 "a" "k"` ($\Sigma_0\cup\Sigma_1$). A $\Tout$'s is
  $\mathcal{I}\cup\Ofree\cup\Oknown$ — `AP: 3 "a" "o" "x"` in the $\Tout$-only
  partition, `AP: 4 "a" "k" "o" "x"` in the mixed one. Getting this wrong is a
  parse-time error, not a silent wrong answer, but it will cost a cycle.
- **Partial $\Tout$** — deliberately excluded, see *Behaviour* #4 and *Open
  theory questions*.

## Test oracles (for /test-writer) — the full verified corpus
**Every row below was executed against both the built `ltlf-ek-synth` and
`ltlfsynt` (Spot 2.15.1) during the PRD grill on 2026-07-31, and all 47 rows
agree.** `/test-writer`'s job is the **mechanical** translation into GoogleTest,
not fixture design. A row that fails to reproduce is a signal to investigate (a
real `DfaProduct` bug or Spot version drift), **not** to quietly adjust the
expectation.

**Verdict** = the printed `REALIZABLE`/`UNREALIZABLE` word (**R** / **U**),
asserted equal between `ltlf-ek-synth` and the reduction. **bare** = the
load-bearing guard's verdict (`ltlfsynt` on $\varphi$ alone, $\Oknown$ still on
`--outs`). **LB** ✅ = verdict and bare differ, so $\psiout$ carried the outcome.

### Tables F–J — known-output ($\Tout$-only) regime

Part file: `input_free: a`, `input_known:` (empty), `output_free: o`,
`output_known: x`.
`ltlf-ek-synth --dfa-product --realizable --part-file P --known-output-transducer T --formula phi`
vs `ltlfsynt --ins=a --outs=o,x --semantics=Mealy --realizability -f "(phi) & (psi_out)"`.

The four single-state $\Tout$ files (drop-in; `AP: 3 "a" "o" "x"`, $\delta$ = one
state self-looping on `[t]`, `acc-name: all` / `Acceptance: 0 t`):

```
%%LAMBDA        %%LAMBDA         %%LAMBDA           %%LAMBDA
state 0: !x     state 0: x       state 0: o <-> x   state 0: a <-> x
 (const-FALSE)   (const-TRUE)     (COPY from Ofree)  (COPY from I)
 psi_out=G(!x)   psi_out=G(x)     psi_out=G(x<->o)   psi_out=G(x<->a)
```

**Table F — $\Tout$ const-false, $\psiout=\text{G}(\lnot x)$:**

| $\varphi$ | verdict | bare | LB? |
|---|---|---|---|
| `F(x)` | U | R | ✅ flip |
| `x` | U | R | ✅ flip |
| `X[!] x` | U | R | ✅ flip |
| `G(!x)` | R | R | — |
| `!x` | R | R | — |
| `o` | R | R | — |
| `G(o <-> a)` | R | R | — |
| `F(x) \| o` | R | R | — (the disjunct escapes the guarantee) |

**Table G — $\Tout$ copy-from-$\Ofree$ ($x=o$), $\psiout=\text{G}(x \leftrightarrow o)$
— the turn-order payoff table:**

| $\varphi$ | verdict | bare | LB? |
|---|---|---|---|
| `G(x <-> !o)` | U | R | ✅ flip |
| `x & !o` | U | R | ✅ flip |
| `X[!](x & !o)` | U | R | ✅ flip |
| `G(x <-> o)` | R | R | — |
| `F(x & o)` | R | R | — |
| `G(a -> x)` | R | R | — |
| `x <-> o` | R | R | — |
| `G(x -> o)` | R | R | — |

*Why this table is the payoff (a `/test-writer` comment worth writing):* it is the
**only** family in the corpus that can detect a turn-order error. A constant
$\Tout$, or one copying from $\mathcal{I}$, would behave identically if $\Tout$
observed merely $\mathcal{I}$ instead of $\mathcal{I}\cup\Ofree$. This one would
not — and the failure is even sharper than a wrong verdict: under a wrong
$\Sigma_0=\mathcal{I}$ the AP `o` falls outside $\Sigma_0\cup\Sigma_1$, so
`parse_transducer`'s **Validation 3, AP scope** (`src/transducer_io.cpp:185-190`,
"names AP 'o' outside Sigma0 ∪ Sigma1") rejects the fixture outright rather than
computing a wrong answer. It plays the role `o <-> i` plays for Mealy in Table E.

*(Note for precision: it is **not** the λ-functionality check that catches this.
`undetermined_variable` quantifies out only the produced cube, so it would still
read `o <-> x` as functional — the guard here is AP scope, Validation 3, not
Validation 4.)*

**Table H — $\Tout$ copy-from-$\mathcal{I}$ ($x=a$), $\psiout=\text{G}(x \leftrightarrow a)$:**

| $\varphi$ | verdict | bare | LB? |
|---|---|---|---|
| `G(x <-> !a)` | U | R | ✅ flip |
| `F(x)` | U | R | ✅ flip |
| `G(x <-> a)` | R | R | — |
| `G(a -> x)` | R | R | — |
| `G(x <-> o)` | R | R | — |
| `x & !a` | U | U | — (both U: `a` is free, so `x & !a` is env-hard anyway) |

**Table I — $\Tout$ const-true, $\psiout=\text{G}(x)$:**

| $\varphi$ | verdict | bare | LB? |
|---|---|---|---|
| `G(!x)` | U | R | ✅ flip |
| `F(!x)` | U | R | ✅ flip |
| `!x` | U | R | ✅ flip |
| `G(x)` | R | R | — |
| `x` | R | R | — |
| `o` | R | R | — |

**Table J — $\Tout$ one-step delay ($x_t = a_{t-1}$, $x_0=\bot$), 2-state.**
`AP: 3 "a" "o" "x"`; state 0 = "prev $a$ false" (initial), state 1 = "prev $a$
true"; $\delta$ branches on the current $a$ (AP index 0):

```
--BODY--
State: 0
  [!0] 0
  [0] 1
State: 1
  [!0] 0
  [0] 1
--END--
%%LAMBDA
state 0: !x
state 1: x
```

$\psiout = (\lnot x) \land \text{G}(a \rightarrow \text{X}\,x) \land
\text{G}(\lnot a \rightarrow \text{X}\,\lnot x)$ — the **corrected weak-`X`**
guarded-safety shape, structurally identical to `kPsiInDelayCorrected`. **Not**
`X[!]`: see Table J-bad.

| $\varphi$ | verdict | bare | LB? |
|---|---|---|---|
| `x` | U | R | ✅ flip |
| `F(x)` | U | R | ✅ flip |
| `X[!] x` | U | R | ✅ flip |
| `G(!x)` | R | R | — |
| `G(x <-> o)` | R | R | — |
| `G(x -> a)` | R | R | — |
| `G(a -> X[!] x)` | U | U | — (both U: `X[!]` forces a continuation the delay cannot promise at trace end) |

**Table J-bad — NEGATIVE CONTROL, the Phase-2 meta-oracle witness. Encode this
as a *deliberate divergence*, never as a passing agreement.** Same delay $\Tout$
file, paired with the **over-strong** $\psiout = (\lnot x) \land \text{G}(a
\rightarrow \text{X[!]}\,x) \land \text{G}(\lnot a \rightarrow \text{X[!]}\,\lnot x)$:

| $\varphi$ | EK | reduction | diverges? |
|---|---|---|---|
| `G(!x)` | R | **U** | ✅ **DIVERGES** |
| `G(x <-> o)` | R | **U** | ✅ **DIVERGES** |
| `x` | U | U | — |
| `F(x)` | U | U | — |

This is the $\Tout$ analogue of
`FaithfulnessGuardMetaOracle.FiresOnOldCopyFromStepOnePsiInDelayPairing`. Phase
2 must assert the generalized `run_faithfulness_guard` **fires** (reports "too
STRONG") on this pairing — proving the guard is non-vacuous and that it, not
hand-verification, is what stands between a fixture bug and a phantom tool bug.

### Tables M1–M2 — mixed ($\Tin$ **and** $\Tout$) regime

Part file: `input_free: a`, `input_known: k`, `output_free: o`,
`output_known: x`. $\Tin$ = the copy fixture ($k=a$, `AP: 2 "a" "k"`,
`state 0: a <-> k`), $\psiin=\text{G}(k \leftrightarrow a)$ — reused verbatim
from Table C. $\Tout$ files here declare `AP: 4 "a" "k" "o" "x"`.
`ltlfsynt --ins=a,k --outs=o,x --semantics=Mealy --realizability -f "(psi_in) -> ((phi) & (psi_out))"`;
bare = `ltlfsynt --ins=a,k --outs=o,x … -f "phi"`.

**Table M1 — $\Tin$ copy, $\Tout$ copy-from-$\Ofree$ ($\psiout=\text{G}(x \leftrightarrow o)$):**

| $\varphi$ | verdict | bare | LB? |
|---|---|---|---|
| `G(k <-> a)` | R | U | ✅ flip **U→R** (assumption half) |
| `G(a -> k)` | R | U | ✅ flip **U→R** (assumption half) |
| `G(x <-> !o)` | U | R | ✅ flip **R→U** (guarantee half) |
| `x & !o` | U | R | ✅ flip **R→U** (guarantee half) |
| `G(x <-> o)` | R | R | — |
| `G(o <-> k)` | R | R | — |
| `F(k & !a)` | U | U | — (both U: copy makes $k \land \lnot a$ unsatisfiable) |

**Table M2 — $\Tin$ copy, $\Tout$ const-false ($\psiout=\text{G}(\lnot x)$):**

| $\varphi$ | verdict | bare | LB? |
|---|---|---|---|
| `G(a -> k)` | R | U | ✅ flip **U→R** |
| `G(k <-> a) & G(!x)` | R | U | ✅ flip **U→R** |
| `F(x)` | U | R | ✅ flip **R→U** |
| `x` | U | R | ✅ flip **R→U** |
| `G(!x)` | R | R | — |

*Why the mixed tables are worth their weight (a `/test-writer` comment):* M1 and
M2 each exhibit **both flip directions in a single table** — the assumption half
turning U into R, the guarantee half turning R into U, against the same $\varphi$
partition and the same pair of transducers. That is the strongest available
evidence that the *composed* reduction $\psiin \to (\varphi \land \psiout)$ — the
`\na` conjecture after `\cref{def:probDefTransducer}` verbatim — is under test,
rather than one half riding along inertly on the other.

**Corpus guarantees (Definition-of-done inputs):** every table spans both
verdicts; F, G, H, I, J each carry ≥2 ✅-flip rows in the R→U direction; M1 and
M2 each carry ✅-flip rows in **both** directions; no table is all-R or all-U;
J-bad is encoded as a divergence, never as an agreement. `/test-writer` may add
rows, **but only** ones re-verified against both binaries.

## Open theory questions touched
- **The monolithic conjecture itself is still unproved.** The `\na` after
  `\cref{def:probDefTransducer}` states $\psiin \to (\varphi \land \psiout)$
  equirealizability as *to be proven*, and `docs/BACKLOG.md` tracks the proof as
  its own item. This PRD supplies **empirical** evidence for the guarantee and
  composed halves — exactly what the `\na` says it is for ("Will be used to
  verify correctness of the methods introduced here") — and does **not** prove
  it. The argument in *Why the reduction is equirealizable* is a sketch for
  `/theory-review`, not a proof; in particular the Case-A totality of $\Tout$ is
  used as a premise, not established.
- **Trace-termination semantics** (`\cref{def:probDef}`'s `\na`). The guarantee
  half is *more* exposed to this than the assumption half: Table J's corrected
  weak-`X` $\psiout$ versus J-bad's `X[!]` differ **only** in whether the system
  may stop, and that alone flips two verdicts. Whether the reduction is
  equirealizable under a *different* termination reading is not settled here.
  Flag any surfaced disagreement to `/theory-review` rather than adjusting a
  fixture.
- **Partial $\Tout$ — excluded, and it belongs to a known live bug.**
  `docs/BACKLOG.md` (Later) tracks "Acceptance mark lost on an edgeless accepting
  state", whose reachability condition is precisely a **partial** transducer, and
  which explicitly notes "Any fix must still ship coverage for a partial `t_out`
  too (the fixture only exercises a $\delta$-dead `t_in`)". Authoring a partial
  $\Tout$ fixture here would encode a **currently-wrong** verdict, as
  `MtnfaProductExpectedDivergence.*` already does for the `t_in` side. That
  coverage is the acceptance-mark item's to own, not this PRD's — noted so it is
  not rediscovered as an oversight.
- **Method-2 arena input partition ($\Ifree$ vs full $\mathcal{I}$)** — deferred
  in `docs/prd/dfa-product.md`. Unchanged: this oracle gives independent external
  evidence on whichever choice is live but does not resolve it.
- **Governed-variable projection** (`main.tex` `\na`, glossary *Open theory
  questions*). The reduction's step-1 argument ("$\psiout$ pins $\Oknown$ to
  exactly one value, so it is a forced move") is the **same shape** as the
  argument `solve_mtdfa` uses to make $\Oknown$ controllable-but-forced. If
  `/theory-review` ever finds that argument wanting, this oracle's reduction is
  affected too — they stand or fall together. Recorded so the coupling is
  visible; not resolved here.
- No `\na`/stub in `main.tex` is modified by this PRD.

## Definition of done
- The $\Tout$-only tables (F–J) and mixed tables (M1–M2) are encoded in
  `tests/ltlfsynt_oracle_test.cpp` with the verdicts above, and pass locally
  where `ltlfsynt` is present; the suite skips cleanly where it is not.
- Every ✅-flip row carries a passing load-bearing guard (bare verdict differs),
  with $\Oknown$ kept on `--outs` in the guard invocation.
- Table J-bad is encoded as a **deliberate divergence** (never a passing
  agreement), and Phase 2's meta-oracle asserts the guard fires on it.
- `run_faithfulness_guard` takes a `Role` (not defaulted), derives its slices via
  `sigma_slices`, and passes on every $(\Tout,\psiout)$ pair in the corpus; the
  existing $\Tin$ call sites pass `Role::t_in` and Tables A–E stay green.
- AP-naming guards cover both new corpora.
- One empty-$\Ofree$-with-non-empty-$\Oknown$ smoke fixture exists.
- `ctest` green; no production C++ changed; no CMake change.
- `/glossary` run to widen the *Faithfulness guard* entry to both roles.
- `docs/BACKLOG.md` item "#1 $\Tout$ oracle" moved to **Done** with the outcome,
  and its two surviving follow-ups (partial-$\Tout$ coverage; generated $\Tout$
  in the corpus) left pointing at their existing owners.

## Developer comments / PRD disagreements
_(none yet — filled in by `/developer` or `/test-writer`)_
