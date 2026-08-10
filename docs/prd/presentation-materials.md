# PRD: Presentation materials — tools, capabilities, example runs

**Status:** implemented 2026-08-10 (unattended day-run, branch `presentation-materials`); `docs/presentation/tools-and-capabilities.md` + `docs/presentation/transcripts/` + `docs/presentation/inputs/`. See `docs/runs/2026-08-10-presentation-materials.md`.
**Interface:** none — **documentation only**. No header, no source file, no test is added or changed. The deliverable is `docs/presentation/` plus captured transcripts.
**Recommended workflow:** sequential — there is no interface to freeze and nothing for `/test-writer` to bind to; a single `/developer` session writes prose and captures real command output.
**main.tex ref:** none. The document describes **shipped tooling**, not mathematics. Where it must name a concept it uses `docs/GLOSSARY.md` spellings verbatim.

**Gates:**
- [x] glossary        — 2026-08-10: every domain word used (the five methods, Representation, knowledge transducer, variable partition, goal formula, Controller, consistency filter, Transducer file format, Role, Dependent output set, Dependent input set, Controller verifier, Generated corpus) matches `docs/GLOSSARY.md` spelling; no new term introduced
- [x] tests           — 2026-08-10: `ctest` **585/585** green before *and* after the change (`build/testtmp/ek_test_before.log`, `ek_test_after.log`, both exit 0); `git status --porcelain` shows nothing outside `docs/`, i.e. this PRD touched nothing executable
- [ ] code-review     — domain half done 2026-08-10 (`/code-reviewer`: 3 must-fix, all applied — see the run report); generic half (`/review <PR#>`) pending, gate stays open until both have run
- [x] theory-review   — 2026-08-10, light pass by the `theory-reviewer` agent on the prose: 1 overstatement (§4f ruled `main.tex`'s consistency filter *unsound*, contradicting §4f's own "not a bug in either tool"), 1 Moore-reading of $\lambda_{in}$ (§1), 1 false "exactly" (§4f), 1 wrong cross-ref (§5) — **all four fixed**; $\psi_{in}$ for the O5 witness, the turn order, and §4b's realizability flip verified clean, and the monolithic reduction is nowhere called a theorem

**Unattended-ready:** **yes.** Nothing here is a decision the user owns: the audience and the deliverable list are fixed by the 2026-08-12 presentation, the tools are shipped and their flags are readable from `--help` and `docs/prd/cli-wrapper.md`, and every judgement call is bounded by the Stop-list below.

## Stop-list

1. **Never invent, edit, prettify, or reconstruct command output.** Every transcript must be the literal captured stdout/stderr of a command that was actually run. If output is too long, truncate it visibly with an ellipsis marker and say what was cut — never paraphrase it.
2. **A failing or hanging example is recorded as-is, not fixed.** If a documented capability does not work, that is a finding for the run report and the example is marked accordingly. **Do not change code, tests, or flags to make an example succeed** — this PRD touches nothing executable.
3. **Do not present a conjecture as a theorem.** The monolithic reduction to plain LTLf synthesis is unproved and as of 2026-08-09 has a divergence witness on partial transducers (O5, `docs/runs/2026-08-09-acceptance-mark-edgeless.md`). Any comparison to `ltlfsynt` must carry that caveat in the text.
4. **Benchmark numbers are out of scope here.** They come from `docs/prd/benchmark-suite.md` Phase 2 on 2026-08-11. This document links to them; it does not measure or quote timings of its own.

## Goal

Produce the non-benchmark half of the 2026-08-12 progress presentation: a written, self-contained account of **what tooling this project ships, what it can do, and what it looks like when run**, backed by real captured transcripts. The audience is people outside the project, so the document must be legible without the paper — it explains capability in terms of what goes in and what comes out, not in terms of `main.tex` algorithms.

It exists as its own PRD because it is fully automatable (run the shipped binaries, capture output, write prose) and needs none of the user's evening: the part that requires the user is reading it, not producing it.

## Ubiquitous-language terms used

All existing; the document must use these spellings and no synonyms: **The five methods** (`DfaProduct`, `NfaProduct`, `MtdfaProduct`, `MtnfaProduct`, `OtfMtdfaProduct`), **Representation**, **Knowledge transducer** ($\Tin$, $\Tout$), **Variable partition** ($\Ifree$, $\Iknown$, $\Ofree$, $\Oknown$), **Goal formula** ($\varphi$), **Controller**, **Consistency filter** ($\cons$), **Transducer file format**, **Role**, **Dependent input set** ($\Xdep$), **Dependent output set** ($\Ydep$), **Controller verifier**, **Generated corpus**.

**No new term is introduced.** If the writing pass finds itself needing one, that is a Stop-list-class event: record it and stop rather than coining vocabulary in a presentation document.

## Behaviour / content

The deliverable is `docs/presentation/tools-and-capabilities.md`, with captured transcripts under `docs/presentation/transcripts/`. Five sections:

1. **What the project is, in one page.** Synthesis from an LTLf goal formula **under external knowledge** supplied as transducers, rather than folded into the formula. Why that is the interesting move: knowledge is arbitrary regular, formulas are not — a two-state transducer exists that no LTLf formula expresses at any size (stated with the caveat of `docs/prd/benchmark-suite.md`'s Stop-list 1, i.e. as a claim resting on the standard star-free correspondence, not as something proved here).

2. **The tools.** `ltlf-ek-synth` (synthesis; method selection across all five methods and eight method×Representation flags; `--benchmark`; controller output and verification) and `ltlf-ek-deps` (dependency extraction, `--direction in|out`). For each: purpose, the flags that matter, input and output formats — the part-file format and the transducer file format each shown as a real file.

3. **Capabilities, as a table.** One row per capability, one column for the tool/flag that provides it, one for a pointer to the example that demonstrates it. Includes: realizable and unrealizable verdicts; controller emission; controller verification; the two dependency directions; the five methods as interchangeable implementations of one interface; stage timing.

4. **Example runs.** At minimum, each with the exact command, the input files inline, and the literal output:
   - a small **realizable** instance with a non-trivial $\Tin$, showing the controller;
   - the **same $\varphi$ made unrealizable** by changing only the knowledge — the cleanest demonstration that knowledge is load-bearing;
   - the **same instance run under two different methods**, showing identical verdicts — the cross-method agreement property, in one screen;
   - `ltlf-ek-deps --direction in` and `--direction out` on one formula, showing $\Xdep$ and $\Ydep$;
   - a **controller verification** pass;
   - the **O5 boundary** as a known, deliberately-pinned divergence: our verdict, `ltlfsynt`'s verdict on the monolithic encoding, and one paragraph on why they differ (partiality deletes letters for both players). Marked as an open theory question, never as a bug.

5. **What is measured, and where.** A short pointer section to the benchmark suite and its outputs, so the two halves of the presentation join up.

## Interfaces & types

**Freeze confidence: n/a — no interface.** This PRD adds no declaration. The only "contract" is the output path (`docs/presentation/tools-and-capabilities.md`) and that transcripts live beside it under `transcripts/`, one file per example, named after the example.

**If implementation proves this contract wrong:** the only way that happens is a documented capability not working, which Stop-list 2 governs — record it, do not repair it here.

## Edge cases

- **`mona` absent** — `NfaProduct` / `MtnfaProduct` examples would fail. `mona` is present at `/usr/bin/mona` on this machine; if a run finds it missing, mark those examples skipped rather than dropping the methods from the capability table.
- **`ltlfsynt` is a shell alias**, not on `PATH` (`~/opt/spot-2.15.1/bin/ltlfsynt`), so a spawned process will not find it by name. Invoke it by absolute path; if it cannot be found, skip the O5 example and say so.
- **Long output** — controllers and automata print large. Truncate visibly (Stop-list 1) and keep the full capture in the transcript file.
- **Non-determinism in output** — if any command emits a path, timing, or ordering that varies between runs, either normalize it explicitly and say so, or pick an example that does not.

## Test oracles (for /test-writer)

**None — this PRD adds no code.** The verification that matters is mechanical and belongs to the writing pass itself: **every command block in the document must be re-runnable, and the transcript beside it must be its actual output.** The `code-review` gate checks exactly this by spot-running commands from the document and diffing against the stored transcript.

## Open theory questions touched

- **The monolithic reduction / O5.** Referenced by example 4f; the document reports the divergence and its cause, and asserts nothing about which verdict is "right" — that bottoms out on the open termination `\na` after `def:probDef`.
- **LTLf $\equiv$ star-free.** Referenced by section 1's capability-separation claim; stated as resting on the standard correspondence, which `main.tex` does not currently state.

## Definition of done

- `docs/presentation/tools-and-capabilities.md` exists with all five sections, using glossary spellings throughout.
- Every example has a stored transcript under `docs/presentation/transcripts/` that is the literal output of the documented command.
- The capability table has no row without a demonstrating example.
- `ctest` is green and `git diff --stat` shows **no** change outside `docs/`.
- Any capability found not to work is listed in the run report, unrepaired.

## Developer comments / PRD disagreements

**2026-08-10:**

- **"Stage timing" capability row — a call the PRD left open.** Stop-list 1
  ("never edit/reconstruct/paraphrase captured output") and Stop-list 4
  ("does not measure or quote timings of its own") pull in opposite
  directions for exactly one capability: `--benchmark`'s JSON report is, by
  construction, all numbers. Resolved by adding a seventh example (§4g,
  `stage-timing-shape`) beyond the PRD's six: the literal, unedited
  `--benchmark` output is reproduced in full (satisfying Stop-list 1), but
  framed throughout — in the doc prose and inside the transcript file itself
  — as showing the report's *shape* on a toy instance, explicitly not a
  timing claim, with a pointer to `docs/prd/benchmark-suite.md` for real
  numbers (satisfying the spirit of Stop-list 4: no claim is made *about*
  the numbers). Flagging this rather than silently picking a reading, since
  redacting the numbers instead (the other candidate resolution) would have
  been the Stop-list-1 violation instead.
- **All six example instances are original, not drawn from `docs/prd/cli-wrapper.md` or the test suite verbatim.** The PRD left the concrete $\varphi$/$\Tin$/$\Tout$ choices to the writing pass ("every judgement call is bounded by the Stop-list"). Two design constraints drove the picks, worth recording since they are not obvious from the doc alone: (1) turn order means any same-step property (e.g. $o\leftrightarrow k$) is realizable regardless of $\Tin$'s *shape*, so it cannot demonstrate "knowledge is load-bearing" (example 4b) — that needs a property about a *future* step's known variable, whose value is fixed by $\Tin$'s global definition rather than by the controller's own turn. (2) In `ltlf-ek-deps`, conjoining a genuine input-forcing conjunct with **any** non-tautological output-safety conjunct kills the input-side result: the system can always violate its own conjunct unconditionally, which the existential-over-outputs projection reads as "environment can still hope for violation via every letter," so nothing is ever forced. This is structural, not a bug, and is why example 4d's one formula shows a non-trivial dependent input alongside a correctly-*empty* (not merely absent) dependent output, rather than two non-empty results.

**2026-08-10, review findings deferred (not acted on):**

1. §1 line ~47 — the two-state capability-separation witness is unnamed; could cite the input-triggered parity toggle ("k holds iff an even number of a's have occurred"), the T3 witness of `docs/prd/benchmark-suite.md:112`. Risk it addresses: the doc's own two 2-state transducers ARE LTLf-expressible.
2. §4f line ~344 — the O5 file is called "$\delta$-partial"; it is partial in both $\delta$ (state 1 edgeless) and $\lambda$ (`state 1: 0`). Either alone suffices, so the argument is unaffected.
3. §4f line ~336 — "known to have a counterexample" could be scoped "on partial transducers", matching `docs/prd/benchmark-suite.md:286`.
4. §4d lines ~305-307 — the parenthetical "each of {a},{b} is dependent alone, {a,b} jointly is not" is a claim no stored transcript demonstrates; the run only prints `dependent inputs: a`.
5. §4g line ~401 — `--benchmark /tmp/report.json` writes outside the repo; a path under the repo or `$TMPDIR` would make "re-runnable exactly as written" independent of /tmp being writable.
6. §5 line ~419 — "declared expressibility tiers" paraphrases the glossary term *Comparability tier*, which §1 line ~51 uses correctly.
7. §4 — the section says "All six examples" / "These six are hand-picked" while (a)–(g) is seven; (g) is deliberately outside the six, but the count language reads oddly on first pass.
