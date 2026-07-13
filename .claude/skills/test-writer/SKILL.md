---
name: test-writer
description: Write GoogleTest unit tests for this LTLf-external-knowledge project — small tests for most functions, plus domain oracles (cross-method metamorphic equivalence, controller verifier, monolithic baseline). Use after implementing or changing code in src/ or include/ltlf_ek — or, in the concurrent workflow, against a frozen Interfaces & types contract before the implementation exists.
---

# Test writer

## Delegation guard (read first)

Test-writing is meant to run on the **`test-writer` agent (Sonnet)** — cheaper,
and it keeps the bulk tool-output (file reads, `ctest` logs) out of the main
Opus context — not inline on the main session.

- **If you were spawned as the `test-writer` agent** (your agent prompt told you
  to execute directly): this guard does not apply — skip it and go to *Before
  writing*.
- **Otherwise you are the main session:** spawn the `test-writer` agent
  (`subagent_type: test-writer`) to run this skill on the requested scope, relay
  its result to the user, and **stop**. Do **not** write tests inline. This holds
  even when the user typed `/test-writer` directly — the slash command always
  ends up on Sonnet via the agent.
  - **Keep the spawn prompt tight.** Name the target functions/PRD and defer to
    this skill — the agent already reads it. Do **not** restate the skill's
    oracle layers/rules back at the agent: echoing "verify X, consider Y" reopens
    settled questions and invites re-derivation (burning tokens). State what's
    authoritative (the PRD's "Test oracles", any given golden values) and what's
    out of scope; let the skill supply the rest.

---

Framework: **GoogleTest** (fetched by CMake; tests in `tests/`, added to the
`unit_tests` target in `CMakeLists.txt`). Priority: **small unit tests for most
functions that sensibly have one**, backed by a few strong domain oracles. The
theory reference `main.tex` lives at **`latex/main.tex`** (git submodule →
Overleaf).

## Before writing

- Read `docs/GLOSSARY.md` (use canonical names in test names and comments) and
  the code under test. Read the PRD's "Test oracles" section if present.
- **Concurrent workflow — you run before the code exists.** When the PRD's
  `Recommended workflow` is `concurrent`, you run on your own branch *before*
  `/developer` has landed the implementation. Bind to the PRD's frozen
  **Interfaces & types** block — it is frozen for exactly this — and treat **that
  block, not the headers**, as the source of every signature you test against.
  Write the **domain oracles** (metamorphic, verifier, monolithic baseline) first
  (they bind to the public interface + the math), then **unit fixtures** on the
  frozen signatures.
  - **You cannot compile or run, and must not try.** The not-yet-written header
    means the target won't link — that is the **expected, correct** state on your
    branch, not a problem to solve. Do **not** write a scratch/`/tmp` stub or mock
    of the missing header to self-check, do **not** edit `src/`/`include/`, and do
    **not** loop on the build hoping it goes green. Your compile check is careful
    reading against the frozen block; the launcher builds against the real code
    once it lands.
  - **Don't re-derive what the PRD already froze — this is the mode's top token
    sink.** Reading five headers to reconstruct a signature the *Interfaces &
    types* block already pins is exactly the waste to avoid. Read that block, the
    PRD's *Test oracles* section, and **at most** the one support/fixtures header
    (or existing generator) you actually reuse — then write. Prefer reusing an
    existing corpus/fixture generator **by reference** over re-implementing its
    technique in a large self-contained file; keep the new file lean (see *Small
    over sprawling*).
  - If a frozen signature looks wrong, that is a **PRD-change event** for
    `/developer` / `/grill-prd` — flag it in your report, don't fix it. Modes are
    defined in `/grill-prd`.

## Stay in scope — don't re-derive what's settled

- **Trust the PRD's authoritative values.** Golden values, expected verdicts, and
  oracle rows the PRD provides — especially anything marked **"Verified"** — are
  the *expected* values to **encode into assertions**, not claims to re-establish
  by hand first. Compute an expected value yourself only where the PRD leaves one
  open. If a supposedly-settled value fails to reproduce, that is a signal to
  report (the standing "investigate, don't adjust" rule), not a licence to
  re-verify the whole corpus up front.
- **Write the check; let it run.** Your job is to *author* the test that exercises
  the binaries/oracles at runtime (`RunEkSynth` / `RunLtlfsynt`, cross-method,
  monolithic) — not to pre-run them manually before writing the assertion.
- **Don't do other skills' jobs.** An unclear "correct" value is a
  `/theory-review` question (already noted below), production-code changes are
  `/developer`, glossary authoring is `/glossary`. Flag such items in your report;
  do not perform them inline.

## The oracle layers (use the cheapest that establishes truth)

1. **Unit fixtures (primary).** Hand-built tiny inputs with hand-computed
   expected outputs. One test per function that has a sensible contract:
   `VariablePartition::split`, `consistent` on a small `(q_in,q_out,v)`, one
   full product transition per method, progression on a 1–2 step formula,
   final-state classification. Keep them tiny and deterministic.
2. **Metamorphic cross-method equivalence.** For the same `(phi, vars, t_in,
   t_out)`, the methods must agree on **realizability**, and the non-aggregating
   methods (`NfaProduct`, `DfaProduct`, `OtfDfaProduct`) must yield
   strategy-equivalent controllers.
   **Encode the asymmetry:** aggregation (`OtfAggProduct`, `OtfDynAggProduct`)
   may be *incomplete* — it can report unrealizable when others say realizable,
   but never the reverse. Assert the implication, not equality.
3. **Controller verifier (universal post-condition).** For any synthesized `T_C`,
   check that every trace agreeing with `t_in, t_out, T_C` satisfies `phi` (a
   language-inclusion check via Spot). Apply this to controllers produced in
   other tests, not just as a standalone case.
4. **Monolithic baseline (coarse fallback, not the centerpiece).** Encode the
   transducers' strategies as an LTLf formula and synthesize `Domain ∧ phi` with
   plain Spot/`ltlfsynt`; compare realizability. Use sparingly on small
   instances — it is a broad sanity check, not the main oracle, and treat a
   mismatch as "investigate", since the encoding itself can be wrong.

## Push the oracles past trivial inputs

The oracle *architecture* above is strong; its recurring weakness is being fed a
thin, hand-picked corpus (2–3 operators, ≤3 APs, nesting depth ≤2, always a
1-input/1-output or `a/k/o` partition). A bug that only surfaces on a deeper
formula or a wider partition passes every such case. When adding or changing a
method, push each oracle along the axes it is blind to:

- **Formula shape.** Nesting depth ≥3; mixed `U`/`R`/`W`/`F`/`G`/`X[!]`; several
  free inputs and several outputs, not just `i,o`. Include formulas that force a
  larger goal DFA (deep `X[!]` chains) so `ltlf_to_dfa`'s determinism/
  completeness is checked on more than one operator.
- **Mealy-sensitivity.** Formulas whose verdict depends on reading the *current*
  input and on the weak-X-at-final-position convention (`o <-> X i`, guarded
  `G(a -> X k)`) — the region the Moore monolithic baseline cannot cover (memory
  `ltlf-weak-x-and-termination-semantics`).
- **Knowledge flips.** More than one `(phi, T_in)` pair where knowledge flips the
  verdict, across *different* T_in shapes (const, copy, delay, partial). The
  external-knowledge thesis rests on these — one example is not coverage.
- **Partition shape.** Multiple known inputs, exercised `Oknown`, empty `Ofree`,
  larger |I∪O|.

**Prefer generated corpora over hand-picking.** The differential and metamorphic
oracles supply their own ground truth (`ltlf-ek-synth` vs `ltlfsynt`,
`synthesize` → `verify_controller`), so a **fixed-seed** random-formula /
random-partition generator piped through them finds these bugs at near-zero cost
per case and needs **no** hand-labeled expected value — the oracle is the label.
This does not conflict with "trust the PRD's golden values" (those stay the
labels for unit fixtures) or "small over sprawling" (one generator loop, not a
hundred hand-written cases). Keep the seed fixed so any failure reproduces, and
print the offending formula/partition on failure.

## Rules

- **Small over sprawling.** Prefer many tiny tests to a few giant ones. A test
  name should read like a sentence about one behaviour.
- Add every new test file to the `unit_tests` target in `CMakeLists.txt`.
- Cover edge cases from the PRD: partial/undefined transducers (non-enabled
  letters are *skipped*, not routed to a sink — see `drop-method2-sink.md` and
  `dfa_product_test`'s skip-not-sink fixtures), unrealizable, empty variable
  sets, aggregation knowledge-loss.
- Do **not** assert behaviour that contradicts `main.tex`; if a "correct" value
  is unclear, that's a `/theory-review` question, not a guessed assertion.

## Build & run

- **Keep build/ctest output out of your context.** `--output-on-failure` dumps
  every failing test's full stdout, and that dump is then re-read on every later
  turn — the single biggest driver of this agent's token cost. Route verbose
  output to a log and read only failures:
  - Build: `cmake --build build -j > /tmp/ek_build.log 2>&1 && echo OK || tail -40 /tmp/ek_build.log`.
  - Test: `ctest --test-dir build --output-on-failure > /tmp/ek_test.log 2>&1; grep -nE "FAILED|failed|Failed" /tmp/ek_test.log | head -40 || echo "all passed"`.
    Only when a specific test fails, pull *just that test's* block from the log —
    do not `cat` the whole thing, and do not re-run the full suite to re-see
    output you already captured.
- Report results honestly. If tests fail, show the failure (the relevant lines,
  not the whole log); if you stubbed an oracle you couldn't complete, say so.

## Definition of done

- New/changed functions have unit tests where sensible; the suite builds.
- `ctest` is green (or failures are reported with output, not hidden).
- Metamorphic / verifier oracles added or extended when a method changed.
- If a `docs/prd/` PRD backs this feature, tick its **`tests`** gate with the
  commit/PR ref (only once the suite is actually green). Gate vocabulary is
  defined in `/grill-prd`.
