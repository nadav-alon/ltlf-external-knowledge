---
name: test-writer
description: Write GoogleTest unit tests for this LTLf-external-knowledge project — small tests for most functions, plus domain oracles (cross-method metamorphic equivalence, controller verifier, monolithic baseline). Use after implementing or changing code in src/ or include/ltlf_ek.
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

---

Framework: **GoogleTest** (fetched by CMake; tests in `tests/`, added to the
`unit_tests` target in `CMakeLists.txt`). Priority: **small unit tests for most
functions that sensibly have one**, backed by a few strong domain oracles. The
theory reference `main.tex` lives at **`latex/main.tex`** (git submodule →
Overleaf).

## Before writing

- Read `docs/GLOSSARY.md` (use canonical names in test names and comments) and
  the code under test. Read the PRD's "Test oracles" section if present.

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

## Rules

- **Small over sprawling.** Prefer many tiny tests to a few giant ones. A test
  name should read like a sentence about one behaviour.
- Add every new test file to the `unit_tests` target in `CMakeLists.txt`.
- Cover edge cases from the PRD: partial/undefined transducers, unrealizable,
  empty variable sets, the ⊥ sink, aggregation knowledge-loss.
- Do **not** assert behaviour that contradicts `main.tex`; if a "correct" value
  is unclear, that's a `/theory-review` question, not a guessed assertion.

## Build & run

- `cmake --build build -j && ctest --test-dir build --output-on-failure`.
- Report results honestly. If tests fail, show the failure; if you stubbed an
  oracle you couldn't complete, say so.

## Definition of done

- New/changed functions have unit tests where sensible; the suite builds.
- `ctest` is green (or failures are reported with output, not hidden).
- Metamorphic / verifier oracles added or extended when a method changed.
- If a `docs/prd/` PRD backs this feature, tick its **`tests`** gate with the
  commit/PR ref (only once the suite is actually green). Gate vocabulary is
  defined in `/grill-prd`.
