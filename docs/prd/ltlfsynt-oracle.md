# PRD: `ltlfsynt` external-tool oracle (known-input assumption reduction)

**Status:** draft
**Interface:** new GoogleTest suite `tests/ltlfsynt_oracle_test.cpp` + CMake
`find_program(ltlfsynt)` wiring; **not** a `Synthesis` method. Drives the built
`ltlf-ek-synth` binary and Spot's `ltlfsynt` binary as **subprocesses** and
compares only the realizability verdict.
**main.tex ref:** §Problem Definition (the $\Tin$ known-input semantics), the
*enabled* predicate `\cref{def:enabled}` (§107–116) and the Case-A totality that
justify the assumption reduction; the method under test is Method 2
(`alg:dfa_product`). No new algorithm.

**Gates:**
- [ ] glossary        — new terms in docs/GLOSSARY.md C++ column
- [ ] tests           — unit + oracle coverage
- [ ] code-review     — domain (/code-reviewer) + generic (/code-review)
- [ ] theory-review   — code ↔ math faithfulness vs main.tex

## Goal
Give the project an **external, independent** realizability oracle that does not
originate from our own code: for a class of curated fixtures, run
`ltlf-ek-synth` (the CLI over Method 2, `docs/prd/cli-wrapper.md`) on a Goal
formula $\varphi$ with a **known-input** external-knowledge strategy $\Tin$, and
check that Spot's `ltlfsynt` returns the **same** REALIZABLE/UNREALIZABLE verdict
on the *equirealizable* plain-$\text{LTL}_f$ synthesis problem obtained by
encoding $\Tin$ as an $\text{LTL}_f$ **assumption** $\psi_{in}$:

> `ltlf-ek-synth --dfa-product --formula phi --part-file P --known-input-transducer T --realizable`
> is equirealizable with
> `ltlfsynt --ins=Ifree,Iknown --outs=Ofree -f "(psi_in) -> (phi)" --semantics=Mealy --realizability`

This complements — does **not** supersede — the *in-process* monolithic baseline
(`EmptyKnowledgeMatchesMonolithicBaseline`, `tests/dfa_product_test.cpp:160`),
which uses Spot's `ltlf_to_mtdfa_for_synthesis`. That baseline is **Moore**, so
it is restricted to turn-order-invariant $\varphi$ and cannot see the known-input
regime. `ltlfsynt` defaults to `--semantics=Mealy`, matching our convention
(`main.tex` §86, $S_C$ observes $\Sigma_0=\mathcal{I}$), so this oracle (a) is a
genuinely *external* process, and (b) newly covers Mealy-sensitive formulas
(`o <-> i`) **and** the $\Tin$ reduction.

**Scope:** known input $\Tin$ only (plus the $\mathcal{V}=\emptyset$ degenerate
subset, where $\psi_{in}=\top$ and there is no transducer to encode). The
known-**output** $\Tout$ reduction is different (a guarantee/conjunction, not an
assumption) and is deferred to `docs/BACKLOG.md`.

## Why the reduction is equirealizable (the correctness argument)
Known-input problem: the environment picks $\Ifree$; $\Tin$ forces $\Iknown$
**deterministically** from the $\Ifree$-history ($\Sigma_0=\Ifree$,
$\Sigma_1=\Iknown$, glossary "Role"); the Mealy controller sees the current
$\mathcal{I}=\Ifree\cup\Iknown$ and picks $\Ofree$; win iff $\varphi$.

Reduction: move $\Iknown$ to `--ins` (environment-visible), keep $\Ofree$ on
`--outs`, and synthesize $\psi_{in}\!\rightarrow\!\varphi$, where $\psi_{in}$ is
the $\text{LTL}_f$ language of $\Tin$ (all $(\Ifree,\Iknown)$ traces $\Tin$
produces). Because $\Tin$ is deterministic and total in the committed **Case-A**
regime (glossary "Open theory questions → Partial transducers", §107–116,
`\cref{def:enabled}`), any environment deviation from $\psi_{in}$ makes the
implication vacuously true, so the environment is effectively pinned to
$\Iknown=\Tin(\cdots)$ — and the controller sees the same current letter under
Mealy turn order. Hence the plain game's winning region equals the known-input
game's: **equirealizable**. Verified end-to-end on two seed fixtures (below).

## Ubiquitous-language terms used
All already in `docs/GLOSSARY.md`:

- **Goal formula** $\varphi$ → `phi`.
- **Inputs / Outputs** $\mathcal{I},\mathcal{O}$ and the four-way split
  $\Ifree,\Iknown,\Ofree,\Oknown$ → `VariablePartition`.
- **External knowledge strategy** ($\Tin$) → `Transducer` (`t_in`), materialised
  by `parse_transducer(..., Role::t_in, ...)`.
- **Observed / produced slice** $\Sigma_0,\Sigma_1$ → `sigma_slices` — here
  $\Sigma_0=\Ifree,\Sigma_1=\Iknown$ (glossary "Role").
- **Consistency** $\cons$ → `consistent` — the per-letter filter the product
  enforces; $\psi_{in}$ is its $\text{LTL}_f$ image for the input transducer.
- **DFA product** (Method 2) → `DfaProduct` — the method the CLI wraps and the
  oracle exercises.
- **Transducer file format (`%%LAMBDA` block)** → the on-disk $\Tin$ fixtures.

**Possible glossary gap (flag for `/glossary`, do not block):** the **assumption
reduction** — encoding a known-input $\Tin$ as the $\text{LTL}_f$ assumption
$\psi_{in}$ so that $\psi_{in}\!\rightarrow\!\varphi$ is equirealizable — is a
genuine domain concept, but it lives **only in test fixtures** (the pairing is
hand-authored, never in production code; see *Interfaces & types*), so it likely
does not warrant a C++ identifier. Confirm with `/glossary` whether the concept
deserves an entry even without code.

## Behaviour / semantics (from main.tex)
The oracle asserts a **verdict-only** (realizability boolean) equivalence. It
adds no synthesis semantics; it only wires two external tools and compares.

1. **Curated fixtures — the trusted contract.** Each fixture hand-pairs a
   transducer **file** (fed to `ltlf-ek-synth`) with an $\text{LTL}_f$ formula
   $\psi_{in}$ (fed to `ltlfsynt`) that the author asserts encodes the *same*
   $\Tin$ language. The two paths share **no code**, which is what makes the
   check independent. `/test-writer` must not auto-derive $\psi_{in}$ from the
   transducer (rejected in the interview: general automaton→$\text{LTL}_f$ is not
   always possible, and shared conversion code would defeat independence).

2. **Mealy semantics on both sides.** Always pass `--semantics=Mealy` to
   `ltlfsynt` (this is its default, but pin it explicitly). This matches the
   controller's $\Sigma_0=\mathcal{I}$ (`main.tex` §86). Do **not** use
   `--semantics=Moore`.

3. **Verdict comparison, not exit codes.** `ltlf-ek-synth --realizable` prints
   `REALIZABLE`/`UNREALIZABLE` to stdout (exit 0 / 20); `ltlfsynt
   --realizability` prints `REALIZABLE`/`UNREALIZABLE` to stdout (exit **0 / 1**
   — different from ours). Parse the printed **word** for the verdict; use the
   exit code only to distinguish "a verdict was produced" from "the tool
   errored" (any exit outside each tool's {realizable, unrealizable} set is a
   fixture/setup bug, not a mismatch — fail loudly with the captured stderr).

4. **Discriminating fixtures are mandatory (the whole point).** A suite where
   every fixture is REALIZABLE proves nothing — a stub returning REALIZABLE would
   pass. Every fixture set MUST:
   - **span both verdicts** (some REALIZABLE, some UNREALIZABLE under the
     known-input problem), and
   - be **load-bearing**: for each flip fixture, also run `ltlfsynt` on the
     **bare** $\varphi$ (drop $\psi_{in}$, keep $\Iknown$ on `--ins`) and assert
     the verdict **differs** — proving the assumption $\psi_{in}$ actually
     changed the outcome and the reduction is not a no-op.
   - Beware the project's LTLf gotchas when authoring hardness: **system-controlled
     termination** + **weak/early-stop `X`** let the system dodge naive
     obligations, so a flip needs `X[!]` on the *known* input under **forced
     continuation** (the system controls continuation, so it can force the next
     step to exist where $\psi_{in}$ guarantees the known value). See *Test
     oracles* — a first, "obvious" flip attempt (`G(o <-> X[!] k)`) was **not**
     load-bearing for exactly this reason and was discarded; and the *Excluded
     class* shows the dual failure (strong-`X` in $\psi_{in}$ or on $k$ under
     nesting breaks the reduction).

## Interfaces & types
No production C++. New/changed artefacts:

- **`tests/ltlfsynt_oracle_test.cpp`** — a GoogleTest suite that, per fixture:
  writes the part-file + `%%LAMBDA` transducer file to a temp dir, runs
  `ltlf-ek-synth --dfa-product --realizable ...` (capturing stdout), runs
  `ltlfsynt --semantics=Mealy --realizability -f "(psi_in) -> (phi)"` (capturing
  stdout), and asserts the parsed verdicts are equal (plus the load-bearing
  guard). Reuse the subprocess/temp-file harness pattern already in
  `tests/ltlf_ek_synth_test.cpp` (`RunCli`, `ShellQuote`, the `--realizable`
  path); the CLI binary is located via the existing
  `LTLF_EK_SYNTH_BINARY` compile definition (`CMakeLists.txt:68`). Capturing
  **stdout** (not just the exit code) requires a shell redirect to a temp file
  and reading it back — extend the harness accordingly.
- **CMake wiring** — `find_program(LTLFSYNT_EXECUTABLE ltlfsynt)` at configure
  time; if found, `target_compile_definitions(unit_tests PRIVATE
  "LTLFSYNT_BINARY=\"${LTLFSYNT_EXECUTABLE}\"")`, mirroring `LTLF_EK_SYNTH_BINARY`.
  Honour an env override `LTLFSYNT_BIN` at runtime (takes precedence over the
  compile-time path). When neither yields a runnable binary, every test
  `GTEST_SKIP()`s — so a clean CI box without Spot's CLI tools is a no-op, not a
  failure.

Reused as-is: the built `ltlf-ek-synth` binary (whole CLI pipeline), Spot's
`ltlfsynt`, the CLI part-file format (`docs/prd/cli-wrapper.md` "Part-file
format"), the `%%LAMBDA` transducer format (`docs/prd/transducer-file-format.md`).

## Edge cases
- **`ltlfsynt` absent / not runnable** — `GTEST_SKIP()` (see CMake wiring); never
  a hard failure.
- **Empty knowledge ($\mathcal{V}=\emptyset$)** — degenerate fixtures with
  $\psi_{in}=\top$ and trivial transducers (no `--known-input-transducer`); the
  oracle reduces to `ltlf-ek-synth --inputs/--outputs` vs bare `ltlfsynt`. These
  are the **only** overlap with the existing Moore baseline, but here they run
  under Mealy — so include the Mealy-sensitive ones (`o <-> i`, `G(o <-> i)`)
  that `EmptyKnowledgeMatchesMonolithicBaseline` deliberately excludes.
- **Empty $\Ofree$** — legal; `--outs` is empty; the game has no system moves.
  `ltlfsynt` with an empty `--outs` must be confirmed to accept this (smoke-test
  it; if not, encode the no-output case differently or skip with a comment).
- **Verdict-vs-error ambiguity** — an unexpected exit code (e.g. `ltlfsynt`
  usage/parse error) must **fail the test with captured stderr**, never be
  silently read as UNREALIZABLE (its exit 1 == UNREALIZABLE, so a parse error
  that happens to exit 1 would masquerade — guard by parsing the printed word,
  and treat "no verdict word on stdout" as an error).
- **AP naming** — $\psi_{in}$ and $\varphi$ must reference exactly
  $\Ifree\cup\Iknown\cup\Ofree$; a typo'd AP silently becomes a fresh free input
  to `ltlfsynt` and can flip the verdict. `/test-writer` should assert the fixture
  AP sets match the part-file.

## Test oracles (for /test-writer) — the full verified corpus
This is the corpus to encode; `/test-writer`'s job is the **mechanical**
translation into GoogleTest, not fixture design. **Every row below was run
through both `ltlf-ek-synth` (the built binary) and `ltlfsynt` and agrees** — a
row that fails to reproduce is a signal to investigate (either a real
`DfaProduct` bug or a Spot version drift), not to quietly adjust the expectation.

Shared partition for the $\Tin$ tables: `input_free: a`, `input_known: k`,
`output_free: o` (part-file). `ltlfsynt` invocation:
`ltlfsynt --ins=a,k --outs=o --semantics=Mealy --realizability -f "(psi_in) -> (phi)"`.
`ltlf-ek-synth` invocation:
`ltlf-ek-synth --dfa-product --realizable --part-file P --known-input-transducer T --formula phi`.
Verdict = the printed `REALIZABLE`/`UNREALIZABLE` word (**R** / **U** below).
**LB** = load-bearing: the bare-$\varphi$ guard `ltlfsynt … -f "phi"` (drop
$\psi_{in}$) yields the *opposite* verdict, proving $\psi_{in}$ changed the
outcome.

The three single-state $\Tin$ files (drop-in; `AP: 2 "a" "k"`, $\delta$ = one
state self-looping on `[t]`):

```
%%LAMBDA          %%LAMBDA           %%LAMBDA
state 0: k        state 0: !k        state 0: a <-> k
  (const-TRUE)      (const-FALSE)      (COPY: k = a)
  psi_in = G(k)     psi_in = G(!k)     psi_in = G(k <-> a)
```

**Table A — $\Tin$ const-true, $\psi_{in}=\text{G}(k)$:**

| $\varphi$ | verdict | bare | LB? |
|---|---|---|---|
| `k` | R | U | ✅ flip |
| `X[!] k` | R | U | ✅ flip |
| `X[!](X[!] k)` | R | U | ✅ flip |
| `G(k)` | R | U | ✅ flip |
| `F(k)` | R | U | ✅ flip |
| `X[!](k & o)` | R | U | ✅ flip |
| `X[!](k -> o)` | R | R | — (agree, not flip) |
| `G(k -> o)` | R | R | — |

**Table B — $\Tin$ const-false, $\psi_{in}=\text{G}(\lnot k)$:**

| $\varphi$ | verdict | bare | LB? |
|---|---|---|---|
| `!k` | R | U | ✅ flip |
| `G(!k)` | R | U | ✅ flip |
| `X[!] !k` | R | U | ✅ flip |
| `F(!k)` | R | U | ✅ flip |
| `X[!] k` | U | U | — (both U: assumption can't force a pinned-false $k$ true) |
| `G(k)` | U | U | — |

**Table C — $\Tin$ copy ($k=a$), $\psi_{in}=\text{G}(k \leftrightarrow a)$:**

| $\varphi$ | verdict | bare | LB? |
|---|---|---|---|
| `X[!](k <-> a)` | R | U | ✅ flip |
| `G(a -> k)` | R | U | ✅ flip |
| `G(k -> a)` | R | U | ✅ flip |
| `G(o <-> k)` | R | R | — (Mealy-sensitive; Moore=U, see Table E note) |
| `X[!](a & k)` | U | U | — (both U: $a$ free, so `X[!] a`-hard even with $k=a$) |
| `F(k & !a)` | U | U | — (both U: copy makes $k \land \lnot a$ unsatisfiable) |

**Table D — $\Tin$ one-step delay ($k_t = a_{t-1}$, $k_0=\bot$), 2-state.**
Full transducer file (`AP: 2 "a" "k"`); state 0 = "prev $a$ false" (initial),
state 1 = "prev $a$ true"; $\delta$ branches on current $a$ (`[!0]`→0, `[0]`→1);
$\lambda$ emits the remembered previous $a$:

```
--BODY--
State: 0
  [!0] 0
  [0]  1
State: 1
  [!0] 0
  [0]  1
--END--
%%LAMBDA
state 0: !k
state 1: k
```

$\psi_{in} = (\lnot k) \land \text{G}(\,\text{X}(k \leftrightarrow a)\,)$ — note
the **weak** `X` (not `X[!]`): a total strategy imposes *safety only* and must
**not** force continuation (using `X[!]` here makes the assumption vacuously
dischargeable by early stop — see the divergence note below).

| $\varphi$ | verdict | bare | LB? |
|---|---|---|---|
| `X[!] k` | U | U | — |
| `G(a -> X[!] k)` | U | U | — |
| `F(k)` | U | U | — |
| `X[!](X[!] k)` | U | U | — |
| `k` | U | U | — |
| `G(o <-> k)` | R | R | — |

*Delay observation (worth a `/test-writer` comment):* a **past**-knowledge
strategy like delay rarely *flips* realizability, because a stateful controller
already remembers history on its own — knowledge only adds power when it lets the
system **know the present** it otherwise couldn't (copy) or **rely on a pinned
future** (constant). So Table D is deliberately *structural* coverage (a 2-state
$\delta$ + weak-`X` safety encoding), not a flip source.

**Table E — empty knowledge ($\mathcal{V}=\emptyset$), no transducer.**
`input_free: i`, `output_free: o`; `ltlf-ek-synth --inputs i --outputs o`;
`ltlfsynt --ins=i --outs=o --semantics=Mealy`; $\psi_{in}=\top$ (bare $\varphi$).
Every row agrees; the last two are the payoff — **Mealy-only** (Spot's Moore
`--semantics=Moore` *and* the in-process `EmptyKnowledgeMatchesMonolithicBaseline`
call them UNREALIZABLE; the Mealy oracle correctly agrees with us that they are
REALIZABLE):

| $\varphi$ | verdict | Moore | note |
|---|---|---|---|
| `o` | R | R | |
| `0` | U | U | empty-trace/false |
| `1` | R | R | |
| `i` | U | U | |
| `G(i -> o)` | R | R | |
| `X[!] i` | U | U | |
| `X[!] o` | R | R | |
| `F o` | R | R | |
| `G i` | U | U | |
| `o U i` | U | U | |
| `i U o` | R | R | |
| `G(o) \| i` | R | R | |
| `o <-> i` | R | **U** | **Mealy-only** — the whole point of an external Mealy oracle |
| `G(o <-> i)` | R | **U** | **Mealy-only** |

**Corpus guarantees (Definition-of-done inputs):** the suite spans both verdicts
in every $\Tin$ table; Tables A–C each contain ✅-flip rows whose load-bearing
guard is asserted (bare verdict differs); no table is all-R. `/test-writer` may
add rows, but **only** ones re-verified against both tools (see the excluded
class next).

### Excluded class — a genuine divergence witness (do NOT add these)
`ltlf-ek-synth` and `ltlfsynt` **disagree** on formulas that nest a **strong-`X`
continuation obligation on the known input** under an implication/temporal
operator. Verified witness, delay $\Tin$:

- $\varphi = \text{X[!]}(a \rightarrow \text{X[!]}\,k)$ → `ltlf-ek-synth` = **R**,
  `ltlfsynt` (`psi_in -> phi`) = **U**.

Diagnosis (base termination-control probes without any assumption — `X[!] o`,
`X[!](a -> X[!] o)`, `X[!] X[!] a`, … — **all agree**, so this is *not* a base
Mealy-semantics gap): the EK problem is **REALIZABLE** because the system
controls termination and the transducer is **total**, so the system continues and
the strategy supplies $k$; but the **assumption reduction** cannot reproduce
"total strategy across a system-chosen continuation" — as an implication
antecedent, $\psi_{in}$ is dischargeable exactly at the continuation boundary,
which flips the verdict. This is a **soundness boundary of the reduction**, not a
`DfaProduct` bug. `/test-writer` must **not** encode this class; if a *new* fixture
disagrees, treat it as: (a) inside this excluded class → drop with a comment, or
(b) outside it → escalate as a candidate `DfaProduct` bug. See *Open theory
questions*.

## Open theory questions touched
- **Soundness boundary of the assumption reduction (divergence witness).** The
  reduction $\psi_{in}\!\rightarrow\!\varphi$ is verified sound across the whole
  corpus above but **diverges** on $\text{X[!]}(a \rightarrow \text{X[!]}\,k)$
  (delay $\Tin$): EK says REALIZABLE (total strategy + system-controlled
  continuation), the reduction says UNREALIZABLE (§*Excluded class*). Is there a
  crisp syntactic characterization of the sound class — e.g. "$\psi_{in}$ pure
  safety **and** $\varphi$ places no strong-`X` obligation on a $\Iknown$ variable
  under nesting"? `/theory-review` should confirm the exclusion is principled (the
  reduction is *provably* unsound there) rather than a `DfaProduct` defect, and
  ideally state the sound fragment so the corpus rule is more than "re-verify each
  new row". This is the load-bearing safety rail for the oracle.
- **Empty-word / non-empty-trace convention alignment.** Our $\text{LTL}_f$
  rejects the empty word (`1` rejects it; glossary/memory
  "ltlf-weak-x-and-termination-semantics"). Both tools use Spot's $\text{LTL}_f$
  family, so they *should* agree, but this is an **assumption** the oracle relies
  on. Add one deliberate empty-trace-sensitive smoke fixture (e.g. `1`, `0`) and
  confirm agreement; if it fails, restrict the corpus and file a theory note —
  do **not** silently exclude. Leave the resolution to `/theory-review`.
- **Method-2 arena input partition ($\Ifree$ vs full $\mathcal{I}$).** Deferred
  in `docs/prd/dfa-product.md`. This oracle gives *independent external evidence*
  on whichever choice is live, but does not resolve it — flag any disagreement it
  surfaces to `/theory-review` rather than editing the arena.
- No `\na`/stub in `main.tex` is modified. `FP`/aggregation stubs are untouched
  (those methods are not wired in the CLI).

## Definition of done
- `tests/ltlfsynt_oracle_test.cpp` exists; CMake `find_program(ltlfsynt)` +
  `LTLFSYNT_BINARY` define + `LTLFSYNT_BIN` env override + `GTEST_SKIP` when
  absent are wired.
- The full verified corpus (Tables A–E) is encoded and passes locally (where
  `ltlfsynt` is present); the suite skips cleanly where it is not.
- The suite is **discriminating**: it spans both verdicts, every ✅-flip row
  carries a passing load-bearing guard (bare-$\varphi$ verdict differs), and the
  Mealy-only rows (`o <-> i`, `G(o <-> i)`) pass — coverage the Moore baseline
  cannot give.
- The excluded-class witness is **not** encoded as a passing agreement; if
  present at all, only as a documented `GTEST` comment / disabled case.
- Verdicts are compared by the printed `REALIZABLE`/`UNREALIZABLE` word;
  unexpected exit codes fail with captured stderr.
- Known-**output** $\Tout$ oracle logged in `docs/BACKLOG.md` as a follow-up.
- Glossary: confirm with `/glossary` whether the *assumption reduction* concept
  warrants an entry (expected: no code identifier, possibly a prose note).

## Developer comments / PRD disagreements
_(none yet — filled in by `/developer`)_
