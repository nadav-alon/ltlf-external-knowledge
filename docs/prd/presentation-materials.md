# PRD: Presentation materials — tools, capabilities, example runs

**Status:** draft
**Interface:** none — **documentation only**. No header, no source file, no test is added or changed. The deliverable is `docs/presentation/` plus captured transcripts.
**Recommended workflow:** sequential — there is no interface to freeze and nothing for `/test-writer` to bind to; a single `/developer` session writes prose and captures real command output.
**main.tex ref:** none. The document describes **shipped tooling**, not mathematics. Where it must name a concept it uses `docs/GLOSSARY.md` spellings verbatim.

**Gates:**
- [ ] glossary        — no new term is introduced; the gate closes by confirming every domain word used is already in `docs/GLOSSARY.md`, spelled exactly
- [ ] tests           — no code changes; the gate closes by confirming `ctest` is green **before and after**, i.e. that this PRD touched nothing executable
- [ ] code-review     — domain (/code-reviewer) on the transcripts: do the example runs actually demonstrate what the prose claims
- [ ] theory-review   — light: no math added; confirm no prose overstates a conjecture (notably the monolithic reduction)

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
