---
name: developer
description: Implement a synthesis method or feature for this LTLf-external-knowledge C++ project, against a PRD (docs/prd/) and the ubiquitous-language glossary, using thin wrappers over Spot. Use when writing or extending C++ in include/ltlf_ek and src/. Enforces glossary naming; ends by suggesting tests + review.
---

# Developer

## Delegation guard (read first)

Implementation is meant to run on the **`developer` agent (Sonnet)** — cheaper,
and it keeps the bulk tool-output (file reads, compile logs) out of the main
Opus context — not inline on the main session.

- **If you were spawned as the `developer` agent** (your agent prompt told you to
  execute directly): this guard does not apply — skip it and go to *Before
  writing code*.
- **Otherwise you are the main session:** spawn the `developer` agent
  (`subagent_type: developer`) to run this skill on the requested scope, relay
  its result to the user, and **stop**. Do **not** implement inline. This holds
  even when the user typed `/developer` directly — the slash command always ends
  up on Sonnet via the agent.
  - **Check the PRD's `Recommended workflow` before spawning.** If it is
    **`concurrent`**, don't spawn the developer alone: launch `/developer` and
    `/test-writer` at the same time, each on its own worktree, both bound to the
    frozen *Interfaces & types* contract — then **own integration yourself**
    instead of just stopping: merge both (disjoint `src/`+`include/` vs `test/`,
    so clean; never `git add -A` with both worktrees live), build, and run
    `ctest` once foreground. If the tests land first, the developer finishes
    red→green against them. If it is **`sequential`** or absent, spawn the
    developer alone as below. The modes are defined in `/grill-prd`.
  - **Keep the spawn prompt tight.** Name the PRD and the concrete deltas to
    implement, then defer to this skill — the agent already reads it. Do **not**
    restate the skill's steps/checklists back at the agent: echoing "verify X,
    consider Y" reopens settled questions and invites the agent to re-derive
    (burning tokens). State what's authoritative and what's out of scope; let the
    skill supply the rest.

---

Implement C++ for this project. Architecture: **thin domain wrappers over Spot**
(`spot::twa_graph`, `spot::formula`, `bdd` letters) with **custom types for the
non-standard pieces** (lambda-split `Transducer`, NFAs, product states). All five
methods share `Synthesis::synthesize(phi, vars, t_in, t_out)`.

The theory reference `main.tex` lives at **`latex/main.tex`** (git submodule →
Overleaf).

## Before writing code

1. **Read the PRD** for this feature in `docs/prd/` if one exists. If none
   exists and the task is non-trivial, recommend `/grill-prd` first.
   - **If the PRD has an *Implementation phases* section, do one phase per
     session** — the phase named in your scope, or the earliest not-yet-landed
     one. The phases exist to cap per-session agent cost; do **not** implement
     several phases (or the whole PRD) in a single spawn. Land your phase to its
     green checkpoint, report which phase is done and which is next, and stop.
2. **Read `docs/GLOSSARY.md`.** Names in code MUST match the C++ column. If a
   concept has no entry, you may not invent a name — stop and run `/glossary`
   (or tell the user to), then continue.
3. **Read the relevant `latex/main.tex` algorithm** (the paper, a submodule
   mirroring Overleaf) and the existing interfaces in
   `include/ltlf_ek/`. Fit the existing `Synthesis` / `Transducer` shapes; do
   not fork parallel abstractions.

## Stay in scope — don't re-derive what's settled

- **Trust the PRD's authoritative values.** Golden values, expected verdicts,
  witness rows, corrected formula strings, and API/Spot idioms the PRD presents
  as already established — especially anything it marks **"Verified"** — are
  inputs to **encode**, not claims to re-check. Do **not** re-run binaries,
  re-derive tables, or re-prove witnesses by hand before encoding them. If a value
  later fails to reproduce, that is a signal to flag (the oracle's standing rule),
  not a licence to re-verify everything up front.
- **Let the test target do its own runtime verification.** Where the suite itself
  exercises something at runtime (subprocess oracles like `RunEkSynth` /
  `RunLtlfsynt`, cross-method metamorphic checks, the monolithic baseline), your
  job is to **write** the check, not to pre-run it manually first.
- **Don't do other skills' jobs.** `main.tex` edits, theory reconciliation, full
  re-verification of an existing corpus, glossary authoring, and test authoring
  belong to `/theory-review`, `/glossary`, and `/test-writer`. When the PRD flags
  an item for one of those, note it in your final report and move on — do not
  perform it inline.

## While writing

- **Spot, not reinvention.** Use Spot for automata/BDD/formula machinery; only
  hand-roll what Spot lacks (the lambda-split output, NFA-as-domain-type, product
  bookkeeping). Prefer `spot::twa_graph_ptr`, `bdd` cubes for letters, the shared
  `spot::bdd_dict`.
- **Don't spelunk library source for signatures — write and let the compiler
  answer.** Repeated `grep`/search over Spot (or the project's own headers)
  hunting for exact function/constructor spellings is a top token sink. If the PRD
  pins an API (header + call form, e.g. `spot::is_complete` /
  `randltlgenerator` / the `SPOT_USES_STRONG_X` opt-in), that spelling is
  **authoritative — encode it, don't verify it by search.** For a project type
  (`VariablePartition::split`, `ltlf_to_dfa`, `trivial_transducer`) read its
  header in `include/ltlf_ek/` **once**, then move on. For a genuinely-unknown
  Spot signature, check its header **once**; if still unclear, write your best
  call and let `cmake --build` correct you — the build loop (already piped to a
  log, below) is cheaper than exhaustive source reading. Prefer writing code over
  reading Spot internals.
- **Glossary discipline (enforced).** Every new public identifier naming a domain
  concept must already be in `docs/GLOSSARY.md` under the C++ column, spelled
  exactly. Never introduce a synonym on the "Do not call it" list. New concept →
  glossary first.
- **Match `main.tex` semantics exactly**: the `cons` filter, the ⊥ sink
  self-loop (Method 2), consistency-skip for partial transducers (OTF),
  final-state classification via the progression bit, aggregation keyed on
  `[psi]` alone. If the code must diverge from `main.tex`, do **not** silently
  paper over it — surface it for `/theory-review`.
- **PRD disagreements go in the PRD, not the code.** When the implementation
  must deviate from the PRD (a wrong illustrative snippet, an over-constrained
  type, a renamed field), record the deviation and its rationale in a
  **"Developer comments / PRD disagreements"** section at the end of that
  `docs/prd/` file — dated, one entry per deviation. Do **not** narrate the
  disagreement in a source comment: code comments explain what the code does and
  cite the `main.tex` symbol, they are not the place to argue with the spec.
  (Divergences from `main.tex` *itself* still go to `/theory-review`, above.)
  Under the **`concurrent`** workflow a change to the frozen *Interfaces & types*
  signature is **more than a recorded disagreement**: a `/test-writer` branch is
  bound to the old signature, so surface it as a PRD-change event — flag it in
  your report (don't just log it) so it propagates to that branch, and do not
  quietly reshape the interface on your own branch.
- **Style:** match surrounding code (namespace `ltlf_ek`, `.hpp` headers with
  `#pragma once`, doc-comments that cite the `main.tex` symbol/algorithm). Keep
  black-boxes (`LtlfToDfa`, `SolveDfa`, `progress`) behind named wrappers so the
  glossary maps onto them even while stubbed.
- Keep functions small and unit-testable — `/test-writer` will want one test per
  function that sensibly has one.
- **If a `/test-writer` suite already exists (concurrent workflow, tests landed
  first), build against it and drive it green.** A red test is either a real code
  bug (fix the code) or a mismatch with the frozen *Interfaces & types* contract
  (a PRD-change event — see the disagreements bullet below); it is **not** a
  licence to edit the tests, which are `/test-writer`'s. Where no tests exist yet,
  implement to the contract as usual.

## Build & self-check

- Build with `cmake --build build -j` (configure `cmake -S . -B build` if needed).
- **Keep build output out of your context.** A raw `cmake --build` dump (Spot/
  CMake noise, warnings) is re-read on every subsequent turn and dominates the
  token bill. Pipe it to a log and surface only what you need:
  `cmake --build build -j > /tmp/ek_build.log 2>&1 && echo OK || tail -40 /tmp/ek_build.log`.
  On success you read one line; on failure you read only the tail. Do **not**
  `cat` the whole log, and do not re-run a clean build just to re-see output you
  already have — `grep -n error /tmp/ek_build.log` instead.
- The tree must **compile** before you're done. Do not leave it red.
- Stubs are fine, but a stub throws `std::logic_error("… not yet implemented")`
  or is clearly marked `TODO(developer)` — never a silent wrong answer.

## Update the PRD status

When the feature's code lands, edit its `docs/prd/` header:

- Flip **`Status:`** `draft` → `implemented — <commit or PR ref>`.
- Tick the **`glossary`** gate (with the ref) once every new domain identifier
  is in `docs/GLOSSARY.md`. Leave `tests`, `code-review`, `theory-review`
  unchecked — those are ticked by `/test-writer`, `/code-reviewer`,
  `/theory-review` when they run. (Gate/status vocabulary is defined in
  `/grill-prd`.)

Do this in the same edit where you write the "Developer comments / PRD
disagreements" entry — both are your bookkeeping on that file.

## Definition of done

- Code compiles (`cmake --build build`).
- Every new public domain identifier is in `docs/GLOSSARY.md`.
- PRD `Status:` set to `implemented — <ref>` and the `glossary` gate ticked.
- Any divergence from `main.tex` is flagged for `/theory-review`.
- Any deviation from the PRD is recorded in that PRD's "Developer comments /
  PRD disagreements" section — not narrated in code comments.
- **Suggest next steps** (do not auto-run): under the `sequential` workflow,
  `/test-writer` on the new functions, then `/code-review` (generic) and
  `/code-reviewer` (domain) before committing. Under `concurrent`, the
  `/test-writer` branch already ran — the launcher integrates it (merge + `ctest`)
  before the same two reviews.
