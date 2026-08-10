# Run report — 2026-08-10, presentation materials (`#0`)

**PRD:** [`docs/prd/presentation-materials.md`](../prd/presentation-materials.md) — one phase, docs-only.
**Branch:** `presentation-materials` (worktree branch `worktree-presentation-materials`, merged in).
**Wave:** unattended day-run, wave 1 slot 1. Picked by Step 0 Rule 2 — first *Now / next*
backlog item with a PRD on `master`. Launch gate **clean**: no interface to freeze, no new
glossary term, checkpoint = `ctest` green + a diff confined to `docs/`.

## What landed

| File | What it is |
|---|---|
| `docs/presentation/tools-and-capabilities.md` | the deliverable — five sections, 430 lines |
| `docs/presentation/inputs/part-a-k-o.txt` | the shared variable partition (free input `a`, known input `k`, free output `o`) |
| `docs/presentation/inputs/tin-eventual-k.hoa` | $\Tin$ for the realizable example — `k` forced true from step 2 on |
| `docs/presentation/inputs/tin-delay-k.hoa` | $\Tin$ for the unrealizable example — $k_t = a_{t-1}$ |
| `docs/presentation/inputs/tin-o5-partial-delta-dead.hoa` | the O5 witness — $\delta$-dead after one step |
| `docs/presentation/inputs/o5-reduced-formula.ltlf` | the monolithic reduction, for `ltlfsynt` |
| `docs/presentation/transcripts/*.txt` | 7 files, one per example: literal stdout/stderr/exit code |

Nothing outside `docs/` changed. No header, no source, no test.

## The examples, and whether they ran

All seven ran, live, and were **re-run independently by the review pass** — every one
reproduced its stored transcript exactly.

| # | Example | Result |
|---|---|---|
| a | realizable with a non-trivial $\Tin$ (`X[!] k`) | controller emitted (2-state Mealy HOA), exit 0 |
| b | same $\varphi$, knowledge swapped → unrealizable | `UNREALIZABLE`, exit 20 |
| c | same instance under `DfaProduct` and `NfaProduct` | both `REALIZABLE` — MONA-backed path agrees |
| d | `ltlf-ek-deps --direction in` / `out` on `F(a ^ b)` | `dependent inputs: a`; `dependent outputs: none` |
| e | controller verification (`--model-check`) | `SAFE`, exit 0 |
| f | the **O5 boundary** | ours `UNREALIZABLE` (exit 20) vs `ltlfsynt` `REALIZABLE` (exit 0) — the predicted divergence, reproduced live |
| g | `--benchmark` report shape (added by the developer, see below) | JSON written, exit 0 |

**No capability was found not to work.** `mona` (`/usr/bin/mona`) and `ltlfsynt`
(`/home/cowclaw/opt/spot-2.15.1/bin/ltlfsynt`, by absolute path — it is a shell alias)
were both present; neither PRD edge case fired.

`ltlf-ek-synth` has **no `--help`**: an unrecognised flag reports a usage error to stderr
instead. The flag table in the document was therefore built from the argument parser in
`src/ltlf_ek_synth.cpp`, not from `--help` as the PRD's *Unattended-ready* line assumed.

## Gates

- **glossary** ✅ — no new term; every domain word matches `docs/GLOSSARY.md` spelling.
- **tests** ✅ — `ctest` **585/585** green **before and after** (both exit 0), and
  `git status --porcelain` is empty outside `docs/`. The PRD adds nothing executable, so
  this gate is precisely the claim that it touched nothing.
- **theory-review** ✅ — light prose pass by the `theory-reviewer` agent. Four findings, all
  fixed (below). `latex/` untouched: no `\cl` note was warranted, and the O5 material is
  already written and waiting as `docs/handoffs/2026-08-09-cl-notes-partiality.patch`.
- **code-review** ⬜ **open** — the domain half (`/code-reviewer`) ran and is clean after the
  fix round; the generic half (`/review <PR#>`) is pending. The gate stays open until both
  have run. See "Blocked / open" below.

## Review findings, and what was done

**Domain review — 3 must-fix, all applied.** The gate this PRD actually sets is *"do the
example runs demonstrate what the prose claims"*, so the review re-ran the commands and
diffed against the transcripts. The transcripts were faithful; the **prose around them**
was not, in three places:

1. **§2 flag table said "five more flags are recognised but not yet implemented".** It is
   **three** (`--otf-dfa-product`, `--otf-agg-product`, `--otf-dyn-agg-product`) — eight
   flags over five methods, per `src/ltlf_ek_synth.cpp:54-64`.
2. **§4g called `input_parsing` a "canonical benchmarking stage".** It is deliberately a
   free-form, *non-canonical* span (`src/bench.cpp:97`, `canonical=false`) — and the stored
   transcript beside it says `"canonical":false`. The prose contradicted its own evidence,
   and *Canonical benchmarking stage* is a glossary term.
3. **§4 claimed "all six examples share the partition".** Example (d) does not — it runs a
   different tool over `a,b,o` with no knowledge at all.

**Theory review — 4 findings, all applied.** The one that mattered:

4. **§4f ruled `main.tex`'s consistency filter *unsound* for the synthesis verdict.** That
   decides which tool is right, which is exactly what §4f's own opening and closing
   sentences disclaim, and what the pending `\cl` note refuses to decide ("Which side is
   right is not settled here"). Rewritten to keep the *mechanism* — a missing $\delta$ or
   $\lambda$ deletes a letter for every party at once, so a partial and a totalized $\Tin$
   have the same traces but need not have the same verdict — and drop the ruling.
5. §1 described $\lambda_{in}$ as reading "the history so far", which reads as **Moore**;
   $\Sigma_0 = \Ifree$ of the *current* step, as the doc's own O5 input file shows.
6. §4f's "happens **exactly** when the trace continues" was false — the environment can also
   falsify $\psi_{in}$ by breaking `k <-> a`, since `k` is a free input to `ltlfsynt`.
7. §5 cited **§1** for the O5 caveat (§1 carries the *star-free* caveat; O5 is §4f), and
   said comparisons "inherit the O5 **divergence**" where the divergence is known only on
   *partial* transducers.

Verified clean and left alone: $\psi_{in} = (k \leftrightarrow a) \wedge \lnot(X[!]\,1)$ for
the O5 witness, the turn order, §4b's realizability flip, and the star-free hedging in §1
(which matches `docs/GLOSSARY.md:1555-1570` and `benchmark-suite.md` Stop-list 1 wording).

## Findings deferred — "consider", not acted on

Seven, all recorded in the PRD's *Developer comments / PRD disagreements* section. The three
worth the user's eye:

- **§1's capability-separation witness is unnamed**, while the document's own two 2-state
  transducers *are* LTLf-expressible — a reader could take one of them for the witness.
  Naming the input-triggered parity toggle (`benchmark-suite.md:112`) would close that, and
  would also join this document to Tuesday's benchmark families.
- **§4d asserts "each of `{a}`, `{b}` is dependent alone, `{a,b}` jointly is not"** — a real
  and interesting claim, but **no stored transcript demonstrates it**; the run only prints
  `dependent inputs: a`. Either demonstrate it or cut it.
- **§4g writes its report to `/tmp/report.json`**, outside the repo, while the document
  promises every command is re-runnable exactly as written.

The other four are wording: the O5 file is partial in *both* $\delta$ and $\lambda$ (called
"$\delta$-partial"); "known to have a counterexample" could be scoped "on partial
transducers"; §5's "declared expressibility tiers" paraphrases the glossary's *Comparability
tier*; and §4 says "six examples" where there are seven, (g) being deliberately outside the
six.

## Developer judgement calls the PRD did not settle

1. **A seventh example, `stage-timing-shape` (§4g).** The capability table has a "stage
   timing" row, and the PRD requires every row to have a demonstrating example — but
   Stop-list 4 forbids quoting timings. Resolved by reproducing the literal JSON (Stop-list
   1) and framing it *everywhere* as report shape on a two-state toy, explicitly not a
   timing claim, pointing at `benchmark-suite.md`. **This is the one place the two Stop-list
   items pull against each other**, and it is worth a sentence of confirmation.
2. **All instances are original**, not lifted from `docs/prd/cli-wrapper.md` or the tests.
   Two structural facts forced the design, and both are more interesting than the examples:
   - **A same-step property is realizable regardless of $\Tin$'s shape** (turn order — $\Tin$
     moves before the controller), so "knowledge is load-bearing" needs a *future*-step
     property. Hence `X[!] k` rather than something simpler.
   - **In `ltlf-ek-deps`, conjoining an input-forcing conjunct with any non-tautological
     output-safety conjunct kills the input-side result structurally** — the system's own
     possible self-violation hands the environment an unconditional escape hatch. That is
     why §4d shows one non-empty and one correctly-empty result rather than two non-empty
     ones.

## Questions for the evening grill

1. **Is §4g's existence right?** It is the one judgement call that sits between two Stop-list
   items. Keep it, or drop the "stage timing" capability row and lose nothing else?
2. **The §4d decomposition claim** — demonstrate it (a third `ltlf-ek-deps` run showing `{b}`
   dependent alone would do it) or cut it? Right now the document asserts something it does
   not show, which is the one habit this PRD was written to prevent.
3. **`ltlf-ek-synth` has no `--help`.** The PRD's *Unattended-ready* rationale assumed it
   did. Not a blocker here, but it is a gap for anyone outside the project who is handed
   these binaries after the presentation — worth a backlog item?
4. **Does the document need a "what is not done yet" section?** It currently reads as a
   capability tour with no boundary. The audience on 08-12 will ask, and the honest answer
   (three unwired method flags, the open termination question, the monolithic reduction
   unproved) is already scattered through the text.
5. **The parked `\cl` notes** (`docs/handoffs/2026-08-09-cl-notes-partiality.patch`) are
   still unapplied — §4f now leans on exactly that material. Thursday's slot has them; worth
   confirming they land before the presentation rather than after.

## Budget

- Phases run: **1** (the PRD's only phase).
- Build/test repair rounds: **0** (nothing executable changed; `ctest` green both sides).
- Review fix rounds: **1** of 2 — one combined round covering all 7 must-fixes.
- Agents spawned: `developer` ×2 (write, fix), `theory-reviewer` ×1. No `test-writer`: the
  PRD's *Test oracles* section says "None — this PRD adds no code."

## Blocked / open

Nothing is blocked on the user. The only open item is the **`code-review` gate**, whose
generic half (`/review <PR#>`) runs against the PR — see the gate line in the PRD for its
exact state.
