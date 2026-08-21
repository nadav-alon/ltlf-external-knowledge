# Backlog

Personal "what I intend to do next" — a lightweight capture of intentions, **not**
the developer task tracker and **not** a grilling session. Jot the *what* and
*why* now; the decisions get made later (often via `/grill-prd` or `/grill-me`).

Move items between sections as they progress. Each item: a title, the intent,
and optional **seeds** — half-formed questions/ideas to feed the eventual grill.

---

## Now / next

> **For the automatic day-run** (`scripts/day-run.sh` fires on logon with no
> argument, so it selects by the launcher's Step-0 rule 2 — *the first item under
> this heading that has a `docs/prd/` file on `master`*).
>
> **Renumbered 2026-08-11.** Two items were inserted at the top, so `#1`–`#4`
> below mean something different from `#1`–`#3` in the week-plan table and the
> 08-08 ranking prose further down — those are a historical record and were
> deliberately left as they were written.
>
> - **`#1` and `#2` both have no PRD, so rule 2 skips both** — they are the
>   user's evening, not a day-run's, and they sit first because they are ranked
>   first, not because a launcher should pick them up. Do not treat the missing
>   PRD as a defect to repair, and do not write one unattended: `#1` is a
>   decision about branches and `#2` is a decision about what a benchmark family
>   should *be* — exactly the user decision Stop-list 2 of `benchmark-suite.md`
>   reserves.
> - **UPDATED 2026-08-18: `#3`'s Phase 2 is no longer blocked.** The half of
>   `#1` that blocked it is done — `worktree-bench-phase2` is merged into
>   `master` (`f83177f`) and the `knowledge-chain` families are cherry-picked
>   (`c542802`). A run reading `docs/prd/benchmark-suite.md` from `master` now
>   sees Phase 2 as landed and will not redo it. Phase 1 landed 2026-08-10
>   (PR #12); Phase 2 landed on `master` 2026-08-18.
> - **Phase 3 (the committed structural baseline) is what remains of `#3`,** and
>   it is now blocked **once**, not twice: `master` carries the families as of
>   2026-08-18, but `#2` still stands — Phase 3 bakes whatever family set exists
>   into a cell-exact baseline, pinning a set that is about to be redesigned.
>   **Second reason to hold:** `parity-t3`'s declared `expected_realizable` is
>   known-wrong and currently fails oracle 5 (621/622); baselining that cell
>   would freeze the bug.
> - **So the honest state was: there is currently no launchable phase.** With
>   `#1` and `#2` PRD-less and `#3` blocked on both, the correct end of day was
>   `DONE`; `#4` has no PRD either, and Method 3.2 / Method 3.1 Phase 2 both
>   fail the launch gate (see their entries). The 2026-08-12 run is **paused**
>   (`build/runs/PAUSED`) rather than left to discover this.
> - **Two run-time facts live in the PRD, deliberately not duplicated here:**
>   `ltlfsynt` must be handed an **absolute path**
>   (`~/opt/spot-2.15.1/bin/ltlfsynt`) — the bare name resolves through `PATH` to
>   a **2.14.4.dev** install and would silently race the wrong version (PRD,
>   *`ltlfsynt` invocation*) — and Stop-list item 5 carries a
>   **record-and-continue exception for the 08-11 run only**.

### `ltlf-ek-bench` dies on any per-case timeout — **inserted 2026-08-20, unnumbered**

> **Not a `/launcher` item.** This is a bug fix in an already-landed binary, not
> a PRD phase, and it deliberately carries no number so that nothing below it
> renumbers. Step-0 rule 2 must skip it: it owns no `docs/prd/` file of its own.

`RunCaseWithTimeout` (`src/ltlf_ek_bench.cpp:283`) bounds a case by running it on
a thread and **detaching** that thread when the deadline passes. The detached
worker is still inside Spot/MONA when the process exits, so **any** timing-out
case ends the whole run in a SIGSEGV — *before* `--out` is written. One slow cell
therefore destroys the entire sweep report, not just its own row.

Found by the 2026-08-20 Phase 1 day-run
(`docs/runs/2026-08-20-edf-phase1.md`, finding F2) and reproduced
deterministically: `ltlf-ek-bench --families=slippery-onehot
--subjects=nfa-product --n-min=3 --n-max=3 --timeout=10` exits **139** after
~11 s writing no JSON; the same command at `--timeout=3000` exits 0 with a
complete report. Latent since Phase 2 — no landed family was ever slow enough to
trip it.

**Why it is urgent rather than tidy:** `engineered-domain-families.md` Stop-list
8 *instructs* a timing-out row to be recorded and the other arms continued, and
that PRD's Phase 4 sweeps $n = 2\ldots6$ at a 60 s budget fully expecting one-hot
to time out. **Phase 4 cannot run until this is fixed.**

**Seeds.**
- A blocking Spot call cannot be cancelled in-thread, so the fix is structural:
  fork a child process per case and `SIGKILL` it on the deadline. That is a
  change to a settled part of the harness — a user decision, which is why the
  day-run recorded it instead of doing it.
- Cheaper stopgap worth pricing first: keep the detach, but end the process with
  `_exit()` after the report is flushed, so a leaked worker cannot run during
  static destruction. Buys correctness of the *report* without a process model.
- Either way the row must come out as `TIMEOUT`, distinct from "skipped" and
  from a zero — the same three-way distinction Monday's corpus pre-registration
  insisted on.

### Untangle the branch spaghetti — land what is stranded — **#1**

> **PARTIALLY CLOSED 2026-08-18 — option (a) below, code half only.**
> Done: `worktree-bench-phase2` merged into `master` as `f83177f`; `9929078`
> cherry-picked as `c542802`, **`src/` + `tests/` only**. Phase 2 and both
> `knowledge-chain` families are now on `master`, build clean, suite 621/622
> (the one failure is `parity-t3`'s pre-existing known-wrong declared verdict —
> see the PRD's *Known-open*). **This unblocks a `/launcher` run on `#3`.**
>
> **Still open, deferred to Friday's aggregation (step 1):** items 2 and 3 of
> the stranded list below — rescuing the 2026-08-11 Release workbook
> (`docs/runs/2026-08-11-benchmarks-release.json` and its `.xlsx`; they were
> added by `3abeedd`, which is not in
> `worktree-bench-phase2`'s history, so `9929078`'s edit to them has no base on
> `master`), `docs/presentation/benchmark-numbers.md`, and the `startup-floor.py`
> measurement. **`worktree-presentation-2026-08-12` must not be deleted until
> those land** — it is still their only home. The deck question at the bottom of
> this entry is also still the user's, and still unanswered.

**Read this before merging anything.** It is `#1` because everything else in
this section is worth less until it is done: `#2` would re-grill families whose
current version lives on a branch slated for deletion, and `#3` Phase 3 would
build a baseline from a `master` that has no families on it at all.

The tree is **not** in the shape the backlog above implies. Three branches hold
work, `master` holds the least of it, and one branch that is **not** slated to
merge is currently the only home of code that should survive. Facts, measured
2026-08-11:

| ref | at | what it has that `master` does not |
| --- | --- | --- |
| `master` = `origin/master` | `4b54fd0` | — (Phase 1 only, via PR #12) |
| `worktree-bench-phase2` | `0720cf9` | **benchmark-suite Phase 2**: `bench_suite.hpp`/`.cpp`, the `ltlf-ek-bench` binary, `scripts/bench_xlsx_export.py`, the `ltlfsynt` race. **No PR. Never merged.** |
| `worktree-presentation-2026-08-12` | `c62e8cd` | 9 commits: `0720cf9` merged in, the 2026-08-12 deck, the Release workbook, **and the two new `t2` families** |

**The thing to notice:** the presentation branch is **not going to be merged
into `master`** (it is a deck, plus a working copy of Phase 2). But three things
that *should* outlive it were committed onto it and exist nowhere else:

1. **`9929078`** — the `knowledge-chain` / `knowledge-chain-inert` families,
   their discrimination tests, and the tier-declaration test update. This is
   real `src/` + `tests/` work sitting on a throwaway branch.
2. **`docs/runs/2026-08-11-benchmarks-release.{json,xlsx}`** — the only Release
   sweep that exists. The workbook on `master`'s side of the tree is Debug and
   its numbers are superseded (see `docs/presentation/benchmark-numbers.md`).
3. **`docs/presentation/benchmark-numbers.md`** and the `startup-floor.py`
   measurement behind the `ltlfsynt` floor finding.

So the honest summary: **Phase 2 is done but unlanded, and the follow-up work to
Phase 2 is stranded one level deeper**, on a branch whose stated fate is to be
dropped. Nothing is lost yet — every commit is local and reachable — but a
`git worktree remove` or a branch delete in the wrong order would lose it.

**The decision this needs (user's, not a day-run's)** — the ordering is the
whole question, because whichever branch lands first decides what the other one
has to rebase onto:

- **(a) Phase 2 first, then the follow-up.** Merge `worktree-bench-phase2` into
  `master` (it is one clean commit, `0720cf9`), then cherry-pick `9929078` and
  the `docs/` additions on top. Keeps `master`'s history readable — Phase 2
  lands as Phase 2, the family work lands as its own change — and leaves the
  presentation branch genuinely disposable. Most steps, cleanest result.
- **(b) One PR off the presentation branch**, excluding
  `docs/presentation/slides/`. Fewest steps; but it merges a 9-commit branch
  whose commit messages are about a deck, and the Phase 2 merge commit comes
  along for the ride.
- **(c) Cherry-pick only what is stranded** onto a fresh branch off `master`,
  and leave Phase 2 unlanded. **Rejected unless deliberate** — `9929078` does
  not compile without Phase 2's `bench_suite.hpp`.

**Also part of this item, not separate:** the deck itself. It is a real
deliverable that will exist only in a worktree; decide whether
`docs/presentation/slides/` lands on `master` too (it has no code dependency
either way) or whether the built PDF is archived elsewhere and the branch
dropped.

Until this is settled, **do not run benchmark-suite Phase 3** — it would build a
cell-exact structural baseline from `master`, which has no families at all, and
a day-run reading `benchmark-suite.md` from `master` would conclude Phase 2 is
un-started and redo it. That is precisely the case `scripts/day-run.sh`'s pause
switch was written for, and the 2026-08-12 run is paused for it.

**Check before closing this item:** `git log --oneline master..worktree-bench-phase2`
and `git log --oneline master..worktree-presentation-2026-08-12` are both empty,
or their remainder is only the deck and is deliberate.

_**Ranked 2026-08-08, in a grill.** The ranking criterion **changed**, and that is
the thing to carry forward. Since the full-time job started (2026-08-01) the
binding constraint is no longer day-hours — the launcher supplies those — it is
**evening energy**. So items are now ranked by **grill-cost to reach a clean
unattended launch gate**, not by research value. Under the old criterion the two
method items below were the only candidates; under the new one they both lose,
because each needs several evenings before a launch gate is even conceivable._

_**The order (as ranked on 08-08 — these numbers are the OLD scheme, superseded
by the 08-11 renumbering at the top of this section): #1 the acceptance-mark bug
— now DONE and moved to *Done* — #2 the parametric benchmark suite (now `#3`),
#3 the input-dependency example gallery (now `#4`).** All three were promoted from
elsewhere — #1 from *Later*, and #2/#3 did not exist at all, because this file
tracks *things to build* and had no way to see *things to measure, verify
intuition against, or compare*. That blind spot is the second thing to carry
forward: the category is real and the backlog should keep a slot for it._

_Method **3.2** and Method **3.1 Phase 2** are **not dropped, just outranked** —
see their entries below, unchanged. 3.2 has no PRD *and* a proven blocker; 3.1
Phase 2's own PRD marks its *Interfaces & types* block "least settled; expect
revision" with `otf_solve_fused`'s name explicitly not canonical, so it fails the
unattended launch gate on both the freeze and the glossary check._

### The week of 2026-08-09 — one task per day

_Sized so each weekday costs **one short evening**, either a review or a grill,
never both. Assumes a Sun–Thu work week; shift by two days if that is wrong. The
shape that makes it work: **the day-run is always fed one item behind the
grill** — you grill in the evening, the launcher executes it while you are at
work, you review that output the next evening. The queue is currently **empty**,
which is why day one is a grill._

| Day | Unattended day-run | Evening (≤45 min) |
|---|---|---|
| **Sat 08-08** | — | Housekeeping: merge PR #7, fast-forward local `master`, prune the ten stale worktrees. Then **Grill A: acceptance-mark semantics** — weekend energy, and it is one decision. |
| **Sun 08-09** | `#1` acceptance-mark fix | *Light.* Review the PR, merge — **done** (merged `8c1b6b5`, deferred findings `c9bc742`). Then **Grill B pulled forward** (see below). |
| **Mon 08-10** | **two jobs, in this order:** `#0` presentation materials (short, docs-only), then `#2` benchmark **Phase 1** (metric sink + instrument the five methods) | *Light.* Review both — **both done** (`#0` merged PR #11; Phase 1 merged PR #12, `master` at `368f9e0`, `ctest` 603/603). |
| **Tue 08-11** (today) | `#2` benchmark **Phase 2** — registry + families + **timing sweep + xlsx workbook + ltlfsynt T1 race**. *This is what the no-arg `day-run.sh` picks; see the box at the top of this section.* | **Build the deck** from Monday's document + Tuesday's workbook. If there is energy left, close the **two open generic `/code-review` halves** — `acceptance-mark-on-edgeless-states` and `presentation-materials`; both PRs are merged, so this is `/review 11` / `/review 12`-style on the merged PR, or a local `/code-review` on the landed diff. Neither blocks the day-run. |
| **Wed 08-12** | — | **Present progress.** |
| **Thu 08-13** | `#2` benchmark **Phase 3** — committed structural baseline + exact `ctest` assertions (*may land Tuesday already if the budget outlasts Phase 2*) | *Low energy.* The three parked `\cl` notes for `main.tex` §`indep` are **already written into `latex/main.tex`** in the main checkout (uncommitted, 13 lines) — all that is left is committing and pushing them to Overleaf. Optionally **Grill C: the `#3` gallery**. |

_Two heavy evenings, both placed deliberately: Saturday has weekend energy,
Monday is the freshest weekday. The other three are review-only. Thursday is
reading-and-checking, not designing._

_**Revised 2026-08-09 evening.** The original table put `#3` (the gallery) on
Monday's day-run, but `#3` has **no PRD and was never scheduled a grill** — so
Monday would have repeated `docs/runs/2026-08-08-no-launchable-phase.md`. Since
`#1` landed and merged during the day, the evening was free, so **Grill B moved
up a day** and `#2` now fills Mon–Wed as three phases (the grill split it into
three, not two — see the PRD). `#3` is not dropped; its grill is Thursday's
optional slot. The carry-forward lesson: **the queue must be filled the evening
before, and an item with no PRD is not a queued item.**_

_**Revised again, later the same evening — a Wednesday 2026-08-12 progress
presentation.** It must include the tools and their capabilities, example runs,
and benchmark results **as a spreadsheet**, all in hand by **Tuesday evening**.
Two changes followed. (1) **Benchmark Phases 2 and 3 swapped**: the committed
structural baseline is regression protection with no presentation value, while
the timing sweep is the deliverable, so the sweep moved onto the critical path
and the baseline moved to Thursday. (2) **A second Monday job**,
[`docs/prd/presentation-materials.md`](prd/presentation-materials.md) — the
tools/capabilities/example-runs document, which needs none of the evening to
produce and only reading to consume. Three decisions taken with it: `openpyxl`
is installed **tonight** (the sandbox has GitHub-only egress and cannot install
it, and `pandas` alone cannot write `.xlsx`); Stop-list item 5 gets a
**record-and-continue exception for the 08-11 run only**, since stopping on a
non-reproducing ratio would trade the whole deliverable for a caveat; and the
**ltlfsynt T1 race is in**, because "what the standard tool cannot express at any
size" is the one claim an outside audience can evaluate. Watch: `ltlfsynt` is a
**shell alias**, so the runner must be given an absolute path._

_The $\Tout$ oracle **shipped 2026-08-03** (see Done), which retired the previous
"3.2 or the $\Tout$ oracle" pairing. **Method 3.1 is DONE** (see Done; it landed
as `OtfMtdfaProduct` in `0ce5fab`, closed every gate, and benchmarked
**POSITIVE** — up to 5488x over `MtdfaProduct` where $\cons$ prunes, the first
method to beat the standing champion). Its Phase 2 (`otf_solve_fused`) is spun
out below._

### Re-grill the benchmark families — they were not thought out well enough — **#2**

- **Why this is now first.** The families shipped as a by-product of building
  the *runner*; nobody ever grilled **what they should measure**. The
  2026-08-11 presentation session found the hole by accident, from an outsider
  question ("how come the product never has more states than the goal?"), and
  the answer was embarrassing: **every family pinned $\Tin$ to one state**, so
  `product_states <= goal_dfa_states` held *by construction*. The suite could
  not have detected a product blow-up if one existed. That is not a bug in any
  family — each does what it says — it is a **coverage question that was never
  asked**, and the kind only a grill catches.
- **STATE 2026-08-11:** two `t2` families (`knowledge-chain`,
  `knowledge-chain-inert`) were added *in the presentation branch* as a
  half-hour patch to make the presentation honest, **not** as the considered
  answer. They work (`knowledge-chain-inert` gives `product = n * goal`
  exactly, asserted cell-exact) and they immediately produced a new result —
  `OtfMtdfaProduct` is at its **worst** there, 0.55x — which is itself the
  argument for this item: the first serious look at a new axis moved a
  headline number. Treat them as evidence the axis matters, not as the family
  set.
- **What the grill has to settle.** The axes, before any code:
  - **Knowledge size** — now partly covered, but only by two hand-made
    families. Should $\lvert\Tin\rvert$ be a sweep parameter *independent* of
    $n$, so knowledge and formula can grow separately? Right now they are
    welded together.
  - **$\Tout$ is completely untested.** Every family uses
    `trivial_transducer` for $\Tout$. The output side has never been measured
    at all.
  - **Tier coverage.** Four `t1`, two `t2`, one `t3` — is that the mix we
    want, given `t2`/`t3` cost the `ltlfsynt` race? The `t2` pair skipped the
    race for convenience under time pressure; a deliberate choice might supply
    $\psi_{in}$ and keep the cross-validation.
  - **Realizable/unrealizable polarity.** The `summary` sheet is
    `realizable = true` only. Is the unrealizable half interesting, or noise?
  - **What a family is *for*.** `mirror-degenerate` is deliberately a trap
    (see its entry) and `parity-t3` has a **wrong** `expected_realizable`. A
    grill should decide which families are load-bearing, which are documentation
    of a dead end, and whether the second kind belongs in the same registry.
  - **Measurement hygiene, separately.** `cons-prunes`' headline ratio measured
    **4.37** and **3.60** on two Release sweeps of the same binary — ~20%
    jitter at `repeat=3`. Decide the repeat count from the spread rather than
    by eye, or stop quoting more than one significant figure.
- **Seeds.** Does a family need a *declared hypothesis* field ("this family
  exists to show X"), so a family that stops discriminating is detectable
  rather than merely committed? Stop-list 2 already says a degenerated family
  is a **user decision**, which suggests the registry should carry the claim it
  is meant to support.
- **No PRD, deliberately** — this is the grill that would produce one. It is
  evening work; a day-run must not pick it up (see the day-run note at the top).

### Parametric benchmark suite, committed and reproducible — **#3**

- **STATE 2026-08-10: Phase 1 landed** (PR #12 on `master`, `368f9e0`; `ctest`
  603/603). **Phase 2 is next** and is what the unattended run should pick up.
- **GRILLED 2026-08-09 → [`docs/prd/benchmark-suite.md`](prd/benchmark-suite.md);
  launch gate CLEAN.** Three phases — **and Phases 2 and 3 were swapped on
  2026-08-09**, so the order is: P1 metric sink + instrument the five methods;
  **P2 registry + families + timing binary + xlsx workbook + the `ltlfsynt` T1
  race**; **P3 committed structural baseline + exact `ctest` assertions**. The
  sweep moved onto the critical path because it is the presentation deliverable
  and the baseline is regression protection with no presentation value; the PRD
  is authoritative on this, and this line is here only so the two do not drift
  apart again. Every open question in the handoff below is closed, and
  `/glossary` ran the same evening: *Canonical size metric* (`SizeMetric`) and
  *Comparability tier* (`ComparabilityTier`) are in `docs/GLOSSARY.md`, so
  `/developer` has no term to stop on. **The glossary pass found a collision worth
  remembering:** the shipped *Canonical benchmarking stage* entry had already
  rejected "metric" as a synonym — a metric is the recorded **datum**, not an axis
  — so the new registry is `SizeMetric` (the **size** axis) beside `Stage` (the
  **time** axis), and "metric" bare still means the datum.
  **What the grill added beyond the handoff:**
  structural counts come from a **metric sink inside `bench.hpp`** (not an
  external recomputation, which cannot report a product size for the on-the-fly
  method at all, and not a `Synthesis` interface change); the registry is
  **subject-pluggable** with a generic `(family, params, subject, metric)` row key
  so a differently-shaped future benchmark needs no schema migration; and the
  monolithic internal baseline was **rejected** — as of today it would bake a
  conjecture with a known divergence witness (O5) into the reference column.
- **Handoff with the full grill state:
  [`docs/handoffs/2026-08-08-benchmark-suite.md`](handoffs/2026-08-08-benchmark-suite.md).**
  Read that before the next grill — it carries the evidence, four settled
  decisions, and the seven open questions. This entry is only the summary.
- **The finding that created this item:** the project's flagship empirical result
  is **not reproducible**. `docs/prd/otf-mtdfa-product.md:702` says in as many
  words that the harness behind the **5488x** headline is "throwaway (not
  committed)"; the same is true of `MtnfaProduct`'s 16x-slower negative result,
  from a *different* ad-hoc probe. `bench.hpp` is span-timing observability only —
  there is no family, no runner, no results store, no cross-method table.
- **Why it outranks the method work:** the evening ranking decisions are *already*
  being made on those vanished numbers (see Method 3.1 Phase 2 below, which is
  rejected on a 501 ms vs 249 ms measurement that cannot be re-run). And it is the
  ideal unattended day-run item — pure infrastructure, no theory, no `main.tex`,
  near-zero glossary load — so it clears the launch gate after one short grill
  instead of several. Its output is a table you read in the evening, which is
  exactly the artifact scarce evenings are starved of.
- **Settled already (details in the handoff):** two output layers with different
  lifecycles — deterministic **structural metrics** committed and `ctest`-asserted,
  **timings** generated and snapshotted to `docs/runs/` with provenance; `ltlfsynt`
  as a declared **comparability tier** (T1 compact / T2 blows up / T3 not
  encodable) rather than a phase; families designed as **matched T1/T3 pairs**; all
  five `Synthesis` implementations participate, losers included.
- **The sharp bit worth keeping in view:** LTLf $\equiv$ star-free, but `Transducer`'s
  $\delta$ is arbitrary regular — so a **2-state** $\Tin$ (parity of an
  input-triggered toggle) exists that **no LTLf formula expresses at any size**.
  ltlfsynt cannot be handed that problem at all. That is a capability separation,
  not a timing result, and it may be the paper's strongest argument for taking
  knowledge as a transducer rather than as a formula. **Unverified — check it
  before it goes in the paper.**

### Input-dependency worked-example gallery — **#4**

- **Intent:** a set of small, hand-checkable $(\varphi, \text{partition})$ examples
  where you can look at the emitted $\Xdep$ / $\Tin$ and *see* that it is right —
  built by generating candidates, auto-verifying each against the brute-force
  oracle, and writing up the instructive ones.
- **Why it is worth a slot:** `ltlf-ek-deps --direction in` shipped 2026-08-03 with
  **both lemmas stated unproved** (`\cref{lem:indep-diagonal}`,
  `\cref{lem:indep-transducer}`), and its entire evidence base is *statistical* —
  a brute-force oracle to length four, and ~800 random formulas. That is a real
  intuition debt on live, unproved theory: there is currently no example you have
  personally eyeballed and confirmed.
- **Why it fits the constraint:** generation and cross-checking are fully
  automatable (a day-run), while the part that needs you — reading the gallery and
  deciding whether it matches intuition — is *low-energy* work, not designing.
  It is the natural Thursday item.
- **Seeds:** which examples discriminate — a $\varphi$ where $\Xdep$ is
  non-obvious, one where the $\exists$-projection visibly matters (a universal
  projection would give a different answer), one where $\Xdep$ is empty for a
  surprising reason. Does the gallery live under `docs/` as prose, or as a
  committed fixture table the tests also read?

### Method 3.2 — on-the-fly **aggregated** product (`otfagg`, `\cref{alg:otfdfa_agg_product}`)
- **Intent:** the next unbuilt cell after 3.1. Aggregate on $[\psi]$ alone
  (collapsing transducer states), bounding the product by the size of the original
  DFA at the cost of losing knowledge on each aggregation.
- **Blocking, and now PROVEN not merely suspected:** `\cref{alg:otfdfa_product}`'s
  state-keyed $F_P$ **over-accepts** — theory review (2026-07-29) produced a
  one-state witness, $\varphi=(c \wedge G(a \rightarrow Xb)) \vee (\lnot c \wedge
  X[!]G(a \rightarrow Xb))$ with trivial transducers. the `\na` after `\cref{alg:otfdfa_agg_product}` (`main.tex:467`) asked
  whether to drop the $F_P$ insert; the answer is **re-key it on the transition**.
  3.1 dodges this for free (an mtdfa terminal $2d+b$ is transition-keyed); an
  aggregating method must face it. `\cl` note written into `latex/main.tex`,
  unpushed.
- **Hazard, flagged in the header:** `otf_product_to_mtdfa` is deliberately NOT
  language-exact (I5 collapses to the accepting sink once $\varphi$ is irrevocably
  satisfied, dropping $\cons$ on continuations). Sound for `solve_mtdfa` under
  system-controlled termination; **not** sound for a consumer that reads $L(P)$.
  Aggregation is exactly such a consumer — do not reuse the builder blind.

### Method 3.1 Phase 2 — fused construct-and-solve (`otf_solve_fused`, `--otf-solve`)
- **Intent:** feed the Phase 1 BFS into a `spot::backprop_graph` and abort as soon
  as the initial state is decided. Spec'd in `docs/prd/otf-mtdfa-product.md`;
  **needs its own grill** before coding.
- **Concrete lead from the benchmark:** in the no-pruning family, `game_solving` is
  consistently ~2x slower for `OtfMtdfaProduct` than `MtdfaProduct` at an
  *identical* state count (501 ms vs 249 ms at $n=20$) — same solver, same
  substrate, so the fused build's rows must differ in sharing or variable order.
  Worth understanding **before** Phase 2 fuses solving into that same build.
- **Worth weighing first:** 3.1's win is already 5488x where it matters, and it is
  *flat* — Phase 2 optimizes a term that is no longer the bottleneck in family A.
  The honest question is whether it beats 3.2 for the next slot — the $\Tout$
  oracle is no longer a competitor for it, having shipped 2026-08-03.

### `\cl` notes drafted by `/theory-review` for `main.tex` §`indep` (2026-08-03)

**Not yet applied.** `latex/` is an uninitialized submodule in the
`worktree-indeps-phase1` worktree, so these are parked here per `CLAUDE.md`.
Apply them from the main checkout, uncommitted, before pushing §`indep` to
Overleaf again. Context: `/theory-review` ran faithfulness mode on Phase 1 of
`docs/prd/input-dependencies-tool.md` and found **no `code-bug`** — these are
doc-side notes recording evidence the paper does not yet carry.

1. **After `\cref{lem:indep-diagonal}`'s existing `\cl` note (`main.tex:589`),
   record that the criterion is now machine-checked against `\cref{def:indep}`
   itself, not only against its sibling lemma:**

```latex
\cl[inline]{The criterion has been checked against~\cref{def:indep} directly and
not merely by analogy with~\cref{lem:outdep-diagonal}: a brute-force oracle
enumerates every trace of $L(\lnot\varphi)$ up to length four over
$\mathcal{I} \cup \mathcal{O}$ and searches for the pair $w, w'$
that~\cref{def:indep} forbids, independently of any automaton, and agrees with
the $\Xdep$ the implementation returns on every formula tried, including the
subset-maximality probe on each strict superset of $\Xdep$.
That is evidence for the reduction, not a proof of it, and in one direction
only: a bounded witness refutes input-dependence outright, whereas its absence
at length four is not input-dependence.}
```

2. **After `\cref{lem:indep-transducer}`'s existing `\cl` note
   (`main.tex:604`), record the equirealizability evidence and make explicit
   the step that the $\exists$ of $\liveproj{s}$ — rather than a $\forall$ —
   is what the equirealizability sketch actually leans on:**

```latex
\cl[inline]{Two things the sketch above leaves implicit.
First, the step ``any environment deviation from $\lambda_{in}$ enters a state
from which no continuation violates $\varphi$'' is precisely where
$\liveproj{s}$ must be the \textbf{existential} projection: a deviating $u$
lies outside $\liveproj{s} = \exists \mathcal{O}.\liveset{s}$ exactly when
\emph{every} output completion sends $\delta_{\Aneg}$ to a dead state, so the
system wins whatever it, or a given $\Tout$, plays next --- and since a dead
state is by reflexivity non-accepting, the trace already satisfies $\varphi$
and the system may stop immediately.
Under a universal projection the same step fails, so the quantifier is forced by
this proof obligation and is not a modelling choice.
Second, the equirealizability claim itself is unproved but has been tested: over
eight hundred random formulas and two partitions, every $\varphi$ for which the
construction returned a non-empty $\Xdep$ was realizable with the emitted $\Tin$
exactly when it was realizable without it.}
```

3. **`\cref{lem:indep-diagonal}` is stated for ``a deterministic automaton with
   $L(\Aneg) = L(\lnot\varphi)$'', and the implementation notes (I2 of the PRD,
   and the *Violation automaton* glossary entry) forbid obtaining it by flipping
   acceptance on $A_\varphi$ on the grounds that the equivalence is
   ``untested''. It has now been tested and it holds**, so the ban should read
   as a design choice rather than a soundness requirement — otherwise a future
   reader treats a correct construction as forbidden. No `main.tex` edit is
   required (the lemma is already generic); the wording to fix lives in
   `docs/prd/input-dependencies-tool.md` I2 and in `docs/GLOSSARY.md`'s
   *Violation automaton* entry.

## Later

### Two open generic `/code-review` halves — **low**, expected Thu 2026-08-13

- **What is open.** Both PRDs closed their *domain* half (`/code-reviewer`) and
  their theory review; only the **generic** half of the `code-review` gate is
  unticked. Neither blocks anything downstream — this is gate hygiene, which is
  why it sits in *Later* at low priority and is pencilled in for Thursday's
  low-energy slot.
- **`docs/prd/acceptance-mark-on-edgeless-states.md`** — PR **#10**, merged as
  `8c1b6b5` on 2026-08-09. Review range **`54facb3..c9bc742`** (21 files,
  +1496/-139): `54facb3` is the merge base, `c9bc742` the branch tip (it carries
  the four deferred findings acted on after the PR). Semantic C++ — this is the
  half worth actually doing.
- **`docs/prd/presentation-materials.md`** — PR **#11**, merged as `57d1348` on
  2026-08-10. Review range **`8c1b6b5..76bbfa7`** (18 files, +1368/-6), and it is
  **docs-only** (`docs/presentation/**`), so the generic pass is a prose and
  cross-reference check, not a bug hunt. Lowest value of the two; close it or
  consciously mark it N/A.
- **The prompts, and the one trap.** `/code-review` carries
  `disable-model-invocation`, so **no agent and no unattended run can invoke it**
  — it must be typed by a human in an interactive session. Do not queue this for
  the launcher; it is what `docs/prd/ltlfsynt-oracle-known-output.md`'s entry
  already recorded, and why `/launcher` Step 6a uses `/review <PR#>` instead.

  ```
  /code-review high 10          # or: /code-review high 54facb3..c9bc742
  /code-review high 11          # or: /code-review high 8c1b6b5..76bbfa7
  ```

  The PR-number form is the one to try first (both PRs are merged, which the
  skill handles); fall back to the explicit range if it cannot resolve the PR.
  Add `--fix` only if you want findings applied to the working tree. Then tick
  the `code-review` line in each PRD with the date and what the pass returned —
  the gate wants *both* halves named, not just "reviewed".

### X-shift second formulation of the input-dependency criterion (cross-check oracle)
- **Intent:** an independent second derivation of
  `\cref{lem:indep-diagonal}`'s criterion, to cross-check the shipped one.
  Rewrite $\varphi$ so every output atom $o$ becomes $X\,o$; position $t$ of the
  trace then carries $\mathcal{I}_t$ paired with $\mathcal{O}_{t-1}$, so the
  outputs inside a letter are already-played history and the **Moore restriction
  becomes structural** — the unprojected criterion (the one
  `docs/prd/output-dependencies-tool.md` already implements) applies verbatim,
  with no $\exists\mathcal{O}$ projection. Assert it reports the same $\Xdep$ as
  `dependent_inputs`.
- **Why it is deferred, not dropped** (decided while grilling
  `docs/prd/input-dependencies-tool.md`, 2026-07-31): it needs a `spot::formula`
  rewriter, an **un-shift register** on the emitted transducer so its $\delta$
  consumes real letters again ($|Q|\cdot 2^{|\mathcal{O}|}$ states), and a
  decision on what the extra trailing position means under weak $X$ vs `X[!]` —
  i.e. more new code in the test than in the shipped path. The shipped route is
  one `bdd_exist` over the output cube.
- **Seeds:** does the shift want $X$ or `X[!]`, and what happens at the last
  position? Is the un-shift register avoidable by reading $\lambda$ off the
  shifted automaton but $\delta$ off the unshifted one? Does the equivalence of
  the two formulations have a one-line proof, in which case it belongs in
  `main.tex` §`indep` rather than in a test?

### `main.tex` `\algname{NfaToDfa}` empty-subset rule is underspecified (LaTeX-only, from theory-review 2026-07-17)
- **Intent:** a *documentation* fix in `main.tex` (the latex submodule), not a code
  change. The `\algname{NfaToDfa}` black box (~main.tex:280) states no rule for the
  empty subset, and both sources of an empty $\delta_{prod}$ — a **non-$\cons$**
  letter and a **$\cons$-dead** letter ($\delta_N(s,v)=\emptyset$) — collapse to
  $\emptyset$ in the paper (main.tex:237–244). No uniform reading of the black box is
  sound: skip-both → spuriously realizable; sink-both → spuriously unrealizable. The
  explicit `NfaProduct` already corrects this by completing $N$ before the product
  (`complete_here`), exactly as Method 2 completes $A$ — but the paper is silent.
- **Fix:** apply the drafted `\cl[inline]{…}` note (verbatim in
  `docs/prd/nfa-product.md` "Open theory questions touched") after the reachability
  note at ~main.tex:253. **Verified faithful; code needs no change** — this is purely
  a clarity gap in the write-up.
- **Why Later:** main.tex is a submodule that only builds on Overleaf; batch it with
  the next LaTeX pass (re-run `/glossary` + `/theory-review` after the Overleaf pull,
  since line numbers drift).

### Investigate Nondeterministic Decision Diagrams for representing the NFA (Method 1)
- **Intent:** Method 1 — the NFA route (`LtlfToNfa` / `NfaProduct` / `NfaToDfa`,
  glossary *NFA / DFA for the Goal*) — **isn't built yet**. Before building it on
  an explicit `twa_graph`, probe *nondeterministic* decision diagrams (nBDD /
  nFBDD / nOBDD — decision diagrams carrying explicit "or"/nondeterminism nodes)
  as the NFA's native representation, the way MTBDD is the DFA's (Now/next #1).
  The pitch: an NFA is the natural symbolic object for Method 1 (no
  determinization until *after* the product), and nondeterministic DDs are known
  to be exponentially more succinct than their deterministic counterparts.
- **Why this is an *investigate*, not a build:** it may well conclude "no usable
  library, do the explicit thing." Two headwinds to establish up front: (1) the
  succinctness win (nFBDD ⊋ uFBDD ⊋ FBDD, exponential separations) is paid for by
  **losing canonicity** and cheap equivalence/complement — which is exactly what
  BDD-based product and fixpoint code leans on; (2) **Spot ships no nBDD type** —
  `mtdfa` is deterministic-DFA-shaped — so this means BuDDy-level or external
  machinery, i.e. real cost, not a library swap.
- **Seeds for grilling:**
  - **Which operations does `NfaProduct` actually need?** If it's only $\land$ with
    cons-guards plus reachability, nondeterminism nodes may be cheap. If it needs
    equivalence or complement, the canonicity loss probably kills it outright.
    Settle this first — it's the cheapest question that can end the investigation.
  - `NfaToDfa` runs **after** the product (the stage-mapping question deferred
    under benchmarking), so a symbolic NFA has to survive determinization. Does
    subset construction over an nDD land anywhere better than Spot's `powerset`?
  - **Honest baseline — the "semi-symbolic" shape** (explicit states, BDD-symbolic
    transition labels) is what Spot's `twa_graph` *already is*. Establish what a
    fully-symbolic nDD adds over that before assuming there's a gap to close.
  - **Does Method 1 survive Now/next #1?** If Method 2 goes fully MTDFA, ask
    whether the NFA route stays interesting as a distinct method or collapses into
    "the same pipeline without early determinization."
  - Literature starting points (from a quick unvetted search — verify these are the
    right entry point before leaning on them): knowledge-compilation succinctness
    for nFBDD/uFBDD/OBDD (arXiv `1802.04544`, `1811.02944`); "A Circus of Circuits"
    (arXiv `2404.09674`) for the decision-diagram ↔ circuit ↔ automata map.

### Symbolic `verify_controller` ν-fixpoint (spun off from symbolic DFA-product, 2026-07-12)
- **Intent:** the *Controller verifier*'s ν-fixpoint (`src/verify_controller.cpp`)
  is inherently **per-$\Ifree$-combo** — `StateInfo::edges` is an array indexed by
  `ifree_index`, and `compute_bad`/`extract_witness` enumerate $\Ifree$ choices as
  the adversary's moves. So it still pays the minterm loop via `build_product` +
  `LetterAlphabet`, which the symbolic DFA-product rewrite
  (`docs/prd/symbolic-dfa-product.md`) deliberately **left in place** (scoped to
  `DfaProduct` only). This item is the symbolic rewrite of the verifier's fixpoint.
- **Why deferred:** the verifier is an **audit path**, not the benchmarked
  synthesis path, and it is the project's **linchpin correctness oracle** — a
  symbolic ν-fixpoint over BDDs is a real re-architecture with its own theory/test
  burden, not worth folding into a perf change. Pursue **only** if the verifier's
  own construction cost ever shows up as a bottleneck.
- **Seeds for grilling:**
  - A symbolic one-player reachability/safety fixpoint replaces the
    $\Ifree$-combo enumeration — the $\exists\Ifree$ adversary move becomes a
    `bdd_exist` over the $\Ifree$ cube, the greatest fixpoint an iteration over
    BDD state sets. Reconcile with the current `Bad` nu-fixpoint spelling
    (`docs/prd/controller-verifier.md`).
  - Witness (lasso) extraction must survive the symbolic rewrite — currently it
    walks concrete $\Ifree$ choices; a symbolic version needs to pick a concrete
    witness letter out of a BDD region.
  - Reuse `emits_region`/`delta_edges` (added by `symbolic-dfa-product.md`) so the
    two symbolic builds share the contract.

### Co-generated $(\Tin,\psi_{in})$ family → known-knowledge differential (generated corpus v2)
- **PRD:** extends `docs/prd/generated-corpus-oracle.md` (v1 draft, grilled
  2026-07-06). v1 grades a fixed-seed generated corpus with two **self-labeling**
  oracles — the empty-knowledge `ltlfsynt` differential and the
  `synthesize`$\to$`verify_controller` metamorphic round-trip (incl. a **free-form
  random $\Tin$**). This item closes v1's one acknowledged gap.
- **The gap (from the v1 PRD "Open theory questions"):** on generated
  **known-knowledge** cases the metamorphic oracle is **one-directional** — it
  catches a wrongly-*realizable* controller (it verifies the $T_C$ `synthesize`
  returned) but **not** a wrongly-*unrealizable* verdict (no controller to verify,
  and no $\psi_{in}$ to feed `ltlfsynt`). The empty-knowledge differential covers
  the wrongly-unrealizable direction only for $\mathcal V=\emptyset$.
- **Why the obvious fixes don't work (settled in the grill, don't re-litigate):**
  the differential needs **both** a $\Tin$ file (for `ltlf-ek-synth`) **and** a
  $\psi_{in}$ string (for `ltlfsynt`), denoting the **same** language. Neither
  free-form direction gives that: **transducer→$\psi_{in}$** fails because a
  free-form table $\delta$ can be a modular counter (regular but **not** star-free
  ⇒ no LTLf $\psi_{in}$ exists; LTLf ⊊ regular); **random-$\psi_{in}$→transducer**
  fails because a random LTLf formula is usually **not a strategy** (non-functional:
  two $\Iknown$ for one history; or non-total: it constrains the env's *free*
  inputs).
- **The approach that works — co-generation from a bounded-memory source:** draw
  from the `const / copy / delay / window-boolean` family, where $\Iknown_t$ is a
  **total boolean function of a fixed window** $\ifree_t\ldots\ifree_{t-d}$. Such a
  function is *simultaneously* (a) a finite **deterministic total** transducer
  (state = last $d$ $\Ifree$-values — build the table directly) and (b) a **direct
  LTLf** formula (`G`-guarded safety over the window, star-free by construction).
  Emit **both** from one drawn parameter set `(which iknown, window depth d,
  boolean fn)` ⇒ provably equal, so no mis-encoding. Feeds a **third** test body: a
  known-knowledge differential (`ek-synth` vs `ltlfsynt` on
  $\psi_{in}\!\rightarrow\!\varphi$, load-bearing guard), giving **bidirectional**
  known-knowledge coverage. Complementary to v1's free-form table (which keeps its
  broader counter/parity-capable one-directional metamorphic coverage), not a
  replacement.
- **Seeds for grilling:**
  - **The weak-X trap still bites.** The original delay-fixture bug *was* a
    $\psi_{in}$ mis-encoding in exactly this family (weak-`X`-at-final-position vs
    `X[!]`). Co-generation must emit the corrected guarded-weak-`X` safety shape
    (like `kPsiInDelayCorrected` = `(!k) & G(a -> X k) & G(!a -> X !k)`), **not**
    `X[!]`.
  - **It feeds the faithfulness guard, doesn't retire it.** Run every co-generated
    pair through `run_faithfulness_guard` as a cheap library-only self-check — a
    co-generation bug is exactly what it catches.
  - **Middle path, if the family feels too narrow:** generate free-form random
    $\psi_{in}$, then **filter** — `ltlf_to_dfa`, accept only if it encodes a total
    deterministic $\Iknown$-function, extract the transducer from that DFA. Covers
    more $\psi_{in}$ shapes but adds a functionality check + rejection sampling +
    DFA→transducer extraction. Heavier; weigh vs the bounded-memory family.
  - Subsumes/relates to the known-**output** $\Tout$ oracle (**Done**, 2026-08-03): a
    co-generated $\Tout$ family would extend this to the guarantee half.

### Generated $\Tout$ / $\Oknown$ in the generated corpus (generated corpus v2)
- **PRD:** extends `docs/prd/generated-corpus-oracle.md` (v1 fully implemented,
  all 3 phases). v1 fixes $\Oknown=\emptyset$ and $\Tout$ = `trivial_transducer`
  always — the generator never exercises a non-trivial known-output strategy.
- **Intent:** extend the corpus generator to draw a random $\Oknown$ split and a
  random known-output transducer $\Tout$ (the guarantee/system-side half), so the
  metamorphic round-trip and differential cover known-output cases too.
- **Seeds for grilling:**
  - The co-generation constraint from the $(\Tin,\psi_{in})$ item applies here for
    a known-output *differential* ($\psi_{out}$ as a guarantee conjunct); pairs
    with the known-output $\Tout$ oracle (**Done**, 2026-08-03).
  - $\Tout$ observes $\mathcal I\cup\Ofree$ of the same step — the random-$\Tout$
    builder needs $\Sigma_0=\mathcal I\cup\Ofree,\Sigma_1=\Oknown$, not the
    $\Tin$ shape.

### Soak metamorphic body has no per-case time bound (from Phase 2, 2026-07-13)
- **Context:** the soak mode's escalating driver (`run_corpus`,
  `docs/prd/generated-corpus-soak-mode.md`, implemented) uses a **soft** deadline —
  the case in flight when the budget passes always finishes. For the `differential`
  body that in-flight case is bounded by the per-subprocess `ltlfsynt` timeout, but
  the **`MetamorphicRoundTrip`** body's `DfaProduct::synthesize` /
  `verify_controller` are **in-process Spot calls with no time bound**, so one large
  case at a high level can dominate the wall clock (observed: 105 s for a 20 s
  budget). The OOM/crash class is fixed (per-case `bad_alloc`→skip, lowered
  ceilings); this is the residual *time* overrun.
- **Why deferred:** you cannot safely interrupt a running in-process Spot synthesis
  mid-case, so a firm bound needs either a per-case complexity cap (a tighter
  width/tree ceiling for the metamorphic body specifically — trades escalation
  depth for a firmer budget) or running each case under a watchdog process. Not
  worth it until soak is run routinely enough that the overrun bites.
- **Also minor:** `run_corpus`'s `levels_reached` can overcount a zero-case level by
  1 (set at level entry, before the inner deadline check) — diagnostic-only
  cosmetic, noted so it isn't rediscovered as a bug.
- **Seeds for grilling:** _(tbd)_

### Formula shrinking on generated-corpus failure (generated corpus v2)
- **PRD:** extends `docs/prd/generated-corpus-oracle.md`. v1 has **no shrinking**:
  a failing case prints its `(phi, partition, index)` for manual reproduction, but
  the offending $\varphi$ is whatever size the generator emitted (≤~10 nodes).
- **Intent:** on a differential/metamorphic failure, shrink $\varphi$ (and maybe
  the partition / $\Tin$) to a minimal still-failing witness before reporting, so a
  surfaced `DfaProduct`/semantics bug lands as a small reproducer.
- **Seeds for grilling:** _(tbd)_

### `\cl` notes on partiality — WRITTEN 2026-08-09, waiting on one `git apply`

- **State:** the three notes drafted by `/theory-review` are now **written against
  `main.tex`** and shipped as
  [`docs/handoffs/2026-08-09-cl-notes-partiality.patch`](handoffs/2026-08-09-cl-notes-partiality.patch)
  (13 added lines, verified to apply cleanly to the submodule at `c80719b`). They
  are a patch and not a submodule edit only because the run was isolated in a
  worktree, where `latex/` cannot be written.
- **To land them**, from the main checkout:
  `git -C latex apply docs/handoffs/2026-08-09-cl-notes-partiality.patch`.
  Leave the submodule **uncommitted** per `CLAUDE.md` until you want an Overleaf
  round-trip. `scripts/check-main-tex-refs.py --fix` is **not** wanted yet — the
  submodule pointer is unchanged, so every existing `main.tex:NNN` citation is
  still correct; run it in the same commit as the eventual bump, after the notes
  are pushed to Overleaf.
- **What the three notes say.** Note 1, after the equirealizability `\na`
  following `\cref{def:probDefTransducer}`: records O5 as the conjecture's first
  divergence witness and says the conjecture needs a totality hypothesis or a
  ruling on runs leaving a transducer's domain. Note 2, on §`Transducers`'
  totalization sentence: that claim is true of the transducer and false of the
  synthesis verdict. Note 3, after the `\cref{alg:dfa_product}` prose: an
  edgeless product state is final iff its goal component is (PRD open question #3).
- **Provenance:** faithfulness review of `docs/prd/acceptance-mark-on-edgeless-states.md`
  (`d813834`), open theory questions #2 and #3, plus the O5 result. The code was
  found **faithful**; all three notes are doc-side.

### Prove the monolithic reduction $\psi_{in}\!\rightarrow\!(\varphi \land \psi_{out})$
- **Intent:** prove that synthesis with external information
  (`main.tex` `def:probDefTransducer`) is **equirealizable** with plain
  $\text{LTL}_f$ synthesis of $\psi_{in} \rightarrow (\varphi \land \psi_{out})$
  over the same partition — $\Iknown$ exposed as environment inputs, $\Oknown$
  kept as system outputs — where $\psi_{in},\psi_{out}$ are the $\text{LTL}_f$
  languages of the traces produced by $\Tin,\Tout$. Currently only a **conjecture**
  (added as a `\cl` note right after `def:probDefTransducer` in `main.tex`,
  2026-07-05). The known-**input** half ($\Tout$ absent, $\psi_{out}=\top$) is
  already cross-checked externally against Spot's `ltlfsynt`
  (`docs/prd/ltlfsynt-oracle.md`); this item is the *theory* generalising and
  proving the whole thing, incl. the known-output guarantee half.
- **Why:** it is the correctness backbone of the external `ltlfsynt` oracle and
  would justify a monolithic baseline for *every* method — but it is **not yet a
  theorem**. The oracle's one-time divergence witness (below) turned out to be a
  fixture bug, not a counterexample, so the proof is not blocked on carving out
  an exception; it is a clean conjecture to attack directly.
- **Seeds for grilling:**
  - The two knowledge halves are **asymmetric**: $\Tin$ is an **assumption**
    (implication antecedent, constrains environment-chosen $\Iknown$), $\Tout$ is
    a **guarantee** (conjunct, constrains system-chosen $\Oknown$). The proof must
    respect Mealy turn order (`main.tex` §86) for both.
  - **Divergence witness retired (2026-07-05, `docs/prd/oracle-faithfulness-
    guard.md`).** A delay-$\Tin$ witness, $\text{X[!]}(a \rightarrow
    \text{X[!]}\,k)$, once looked like a soundness-boundary counterexample (EK
    REALIZABLE, reduction UNREALIZABLE). It was a $\psi_{in}\leftrightarrow$
    transducer **mis-encoding** (the hand-authored $\psi_{in}$ was
    copy-from-step-1, not delay); with the corrected delay $\psi_{in}$
    (`(!k) & G(a -> X k) & G(!a -> X !k)`) the same pair **agrees** (both
    REALIZABLE). There is currently **no known divergence witness** for the
    conjecture, and no known sound-fragment carve-out is needed — **but one is
    now predicted, and about to be measured.** The 2026-08-08 acceptance-mark
    grill (#1) argued that a **partial $\Tin$** is exactly where the reduction
    and the methods must part company: once $\delta_{in}$ is undefined
    everywhere, the system can *continue past* the assumption boundary, making
    $\psi_{in}\!\rightarrow\!\varphi$ vacuously true, whereas
    `alg:dfa_product:final` leaves it stuck at an edgeless non-accepting product
    state. Witness $\varphi=X[!]\mathtt{tt}$ with a $\delta$-dead $\Tin$; it
    ships as oracle **O5** of
    [`docs/prd/acceptance-mark-on-edgeless-states.md`](prd/acceptance-mark-on-edgeless-states.md).
    **OBSERVED 2026-08-09, and it matched the prediction exactly:**
    `ltlf-ek-synth` → **UNREALIZABLE**, `ltlfsynt` on
    $\psi_{in}\!\rightarrow\!\varphi$ → **REALIZABLE**, with
    `run_faithfulness_guard` **passing** on $(\Tin,\psi_{in})$ (so Stop-list 2
    did not fire and this is not a mis-encoding). **This is the conjecture's
    first known divergence witness** — the sentence above ("no known divergence
    witness") is now false, and the partial-transducer fragment needs carving
    out. It ships as an `IMPORTANT`-headed test in
    `tests/ltlfsynt_oracle_test.cpp` that **pins a known boundary, not correct
    behaviour**; do not "fix" it. The consequence for the proof: it now needs
    either a **totality hypothesis** on $\Tin/\Tout$, or a ruling on runs that
    leave the transducer's domain — which bottoms out on the open termination
    `\na` after `def:probDef`, i.e. it is **not** independently decidable. A
    mechanical
    **faithfulness guard** now cross-checks every corpus $(\Tin,\psi_{in})$ pair
    against itself so this class of drift cannot recur silently
    (`tests/ltlfsynt_oracle_test.cpp`).
  - **`/theory-review` 2026-08-09 assessment of O5: genuine, not an artifact.**
    $\psi_{in}$ is exactly $\Tin$'s produced-trace language (length-1 traces with
    $k \leftrightarrow a$) and the faithfulness guard passes, so it is not a
    mis-encoding; the acceptance-mark fix is not the cause either, since the
    edgeless state in the witness is **non**-accepting and
    `ensure_acceptance_readable` no-ops on it. The divergence is the same
    phenomenon as the totalization claim in `main.tex` §`Transducers`: partiality
    **deletes letters for both players**, and the reduction's escape route is one
    of the deleted letters. Drafted `\cl` notes 1 and 2 in the item above.
  - Interacts with the **non-empty-trace / empty-word** convention (`1` rejects
    the empty word) and **system-controlled termination** — both bite exactly at
    trace-continuation boundaries, so any future divergence candidate should be
    checked against these first.
  - Relates to the deferred known-**output** $\Tout$ oracle already logged for
    `docs/prd/ltlfsynt-oracle.md`; proving this subsumes it.

### Harden `verify_controller` Witness bdd lifetime (non-blocking, from code-review 2026-07-06)
- **Intent:** the throwaway letter `registrar` `twa_graph` inside
  `verify_controller` is the sole owner of the AP registrations backing the
  `bdd` letters that escape into the returned `Witness`. It is destroyed on
  return, so those vars stay valid only because every *current* caller
  independently keeps $\mathcal I\cup\mathcal O$ registered (the CLI's
  `ap_registrar` lives the whole run; the tests hold the transducers). A library
  caller with a partition AP registered by neither $\varphi$ nor $\Tin/\Tout/T_C$
  could observe a corrupted witness letter after a later `register_ap`.
- **Why:** latent, not a live bug (no shipped call site triggers it) — but a
  fragile coupling worth removing.
- **Seeds for grilling:** either document a caller precondition on
  `verify_controller` ("keep $\mathcal I\cup\mathcal O$ registered for the
  `Witness`'s lifetime"), or build the letters without a throwaway graph so the
  registration lifetime $\ge$ the returned `Witness`.

### Infer lambda from transducer edge labels
- **Intent:** stop storing $\lambda$ as independent state and instead read it off
  $\delta$'s (surviving) edge labels — the $\Sigma_1$-projection of the enabled
  edge. Only sound under the **Case-A partial-transducer** representation
  (undefined = only the inconsistent completions dropped). Keep the explicit
  `lambda` for now: it lets us **verify a transducer obeys its own
  well-formedness invariant** (output is a function of the observation alone) for
  debugging, and it decouples the interface from the edge encoding.
- **Seeds for grilling:**
  - $\lambda$ may later return a **set** of possible outputs (non-deterministic
    knowledge) rather than a deterministic answer — inferring that from edges
    could be *less efficient*, so weigh before committing.
  - Where does the (WF) check live if `lambda` becomes derived — an assertion in
    the concrete class?

### Benchmarking / evaluation — ~~do **last**~~ **superseded 2026-08-08**
- **SUPERSEDED by "Parametric benchmark suite" (#2) in *Now / next*.** Kept for its
  history, but its headline advice is now **reversed**: "do last, before moving on
  to other methods" was written 2026-07-10, before three methods shipped their
  results via throwaway uncommitted harnesses. Deferring the suite is what made the
  5488x and 16x-slower numbers unreproducible, so it moves *ahead* of the remaining
  method work. See `docs/handoffs/2026-08-08-benchmark-suite.md`.
- **Intent:** address the eventual benchmarking needed to assess the methods —
  automaton construction times, synthesis times, controller size, etc.
- **Note (2026-07-10):** this is the **driver** for promoting the symbolic
  DFA-product rewrite (shipped, see Done) — benchmarking is the tool's eventual purpose,
  so the minterm loop's cost stops being an acceptable baseline. A first
  measurement pass here also sets the baseline the symbolic rewrite is judged
  against.
- **Grilled 2026-07-13 → PRD `docs/prd/benchmarking.md` (draft).** Decided: a
  two-tier design — a soft closed registry of canonical comparable `Stage`s
  (`automaton_construction` / `product_construction` / `game_solving` /
  `aggregation`) plus free-form nested sub-spans; an ambient thread-local RAII
  collector (`BenchScope`/`BenchTimer`) so the `Synthesis` contract and the
  free-function black-boxes stay frozen and a new phase needs no infra; whole-run
  wall `total`; structured-nested JSON via `--benchmark=FILE` (stdout untouched);
  always-compiled, runtime-gated no-op; time-only, `DfaProduct` wired now.
- **Deferred out of that PRD (track here):**
  - **Size metrics** — |states|/|edges| of the DFA & product, BDD node counts,
    controller size. The `BenchReport` container is designed to hold them; only
    time is populated in the first pass. Do next after the timing infra lands.
  - **Chrome-trace exporter** — a second serializer over the same generic
    `BenchReport` span tree (`ph:"X"`, `ts`/`dur`) for perfetto / `chrome://tracing`
    flame-chart viewing of one run. Pure add-on; wanted only once eyeballing a
    slow run matters.
  - **Wire the other four methods** — each adopts the frozen mechanism by adding
    span guards (no infra change); this is also where the **NFA-method stage
    mapping** convention gets settled (determinization runs *after* the product —
    is it folded into `product_construction` or its own reserved stage?).

### Link `libmona` directly instead of shelling out to the `mona` binary
- **Intent:** replace the `std::system("mona …")` subprocess call in `run_mona`
  (`src/mona_dfa.cpp`) with a direct link against `libmona`, dropping the fixed
  fork/exec + shell + process-startup cost paid on every NFA construction.
- **Note (2026-07-18, from benchmarking the three product methods):** on small
  formulas that ~1.5 ms fixed overhead *is* essentially all of `NfaProduct`'s
  `automaton_construction` stage (mona's own compute stays sub-10 ms, below its
  `-t` timer resolution). Lower priority than it looks: on **bigger** formulas the
  NFA path's real scaling cost turned out to be the in-process subset
  determinization (`nfa_to_dfa`, the `determinize` sub-span under
  `product_construction`, worst-case exponential — saw a 1.2 s spike), not mona.
  So this is a fixed-overhead win, not a scaling fix.
- **Seeds for grilling:** _(tbd)_ — libmona API surface vs the current `-w` text
  parse; also drops the temp-file write + `-w` table parsing, or keep those?

## Done

### Presentation materials — tools, capabilities, example runs — **DONE 2026-08-10** (was `#0`, this week only)

- **LANDED 2026-08-10** by the unattended day-run, branch `presentation-materials`
  — see [`docs/runs/2026-08-10-presentation-materials.md`](runs/2026-08-10-presentation-materials.md).
  `docs/presentation/tools-and-capabilities.md` (five sections) plus seven literal
  transcripts and five re-runnable input files. `ctest` **585/585** green before
  *and* after, nothing changed outside `docs/`. Moved to *Done* in the landing
  commit, as the entry required — leaving it in *Now / next* would have shadowed
  `#2` for the rest of the week.
- **All seven examples ran, and the review pass re-ran each and got its stored
  transcript back byte-for-byte.** No capability was found not to work; neither
  `mona`- nor `ltlfsynt`-absent edge case fired. **The O5 divergence reproduced
  live** — ours `UNREALIZABLE`, `ltlfsynt` on the monolithic reduction
  `REALIZABLE` — which is now demonstrable on demand rather than only recorded.
- **What the run turned up that was not in this entry.** The transcripts were
  faithful; the *prose around them* was not, in four places, and every one was a
  claim slightly stronger than the evidence: a wrong count of unwired method
  flags, `input_parsing` called a *Canonical benchmarking stage* when its own
  transcript says `canonical:false`, "all six examples share the partition" when
  (d) does not, and — the one that mattered — §4f **ruling `main.tex`'s
  consistency filter unsound**, i.e. deciding the open question the same section
  twice disclaims deciding. All fixed. The carry-forward: a docs-only PRD still
  needs the theory pass, because prose is where an unproved thing quietly becomes
  a proved one.
- **`ltlf-ek-synth` has no `--help`** — an unrecognised flag reports a usage error
  instead, so the flag table was reconstructed from the argument parser. Fine for
  the presentation; a gap for anyone handed the binaries afterwards.
- **Still open, for the evening:** whether §4g (the `--benchmark` shape example)
  should exist at all — it sits between Stop-list 1 and Stop-list 4 — and whether
  §4d's "dependence does not decompose" parenthetical should be demonstrated or
  cut, since no transcript shows it. Seven "consider" findings are parked in the
  PRD's *Developer comments* section.

### Acceptance mark lost on an edgeless accepting state — **DONE 2026-08-09** (found 2026-07-17; widened from one site to a class 2026-07-27) — was **#1**

- **Moved here 2026-08-10.** It stayed in *Now / next* after it landed, which
  meant the no-argument `day-run.sh` would have re-selected it (launcher Step-0
  rule 2 picks the first item with a PRD, done or not). Same lesson the
  presentation-materials entry records: **an entry that has landed belongs in
  *Done* the same day, or it shadows the next item.**
- **One gate is still open** and is an evening job, not a day-run job: the
  **generic** half of `code-review` in
  [`docs/prd/acceptance-mark-on-edgeless-states.md`](prd/acceptance-mark-on-edgeless-states.md)
  (the domain half and theory review are closed). It is deliberately not a
  reason to keep the item in *Now / next*.
- **LANDED 2026-08-09** by the unattended day-run, branch
  `acceptance-mark-edgeless` — see `docs/runs/2026-08-09-acceptance-mark-edgeless.md`.
  `detail::ensure_acceptance_readable` exists and is the sole implementation of
  the idiom at all four builder sites; `ctest` 582/582 green; domain review and
  theory review both clean (**no `code-bug`**). Outcome: the three broken methods
  (`DfaProduct`, `NfaProduct`, `MtdfaProduct`) now agree with the two immune ones
  on the whole {δ-dead, λ-undefined} × {$\Tin$, $\Tout$} partiality matrix, and
  `emits_dfa`'s empty-word exception in `docs/prd/mtdfa-product.md` is retired.
  **The one result worth the evening: O5 fired as predicted** — see "Prove the
  monolithic reduction" below; this run produced the project's first divergence
  witness. The three `doc-bug` `\cl` notes are now **written** and ship as a
  patch — see "`\cl` notes on partiality" under *Later*; one `git apply` lands
  them.
- **GRILLED 2026-08-08 → [`docs/prd/acceptance-mark-on-edgeless-states.md`](prd/acceptance-mark-on-edgeless-states.md);
  launch gate CLEAN.** One phase, concurrent workflow, no glossary gap. The
  semantics seed below is **settled**: an edgeless product state is accepting
  **iff its goal component is**, faithful to `alg:dfa_product:final` — the mark
  is restored, not reinterpreted. The fix locus is one named helper,
  `detail::ensure_acceptance_readable`, adopted by **all four** builders so the
  `bddfalse`-self-loop idiom stops being an unwritten convention.
- **What the grill turned up that was not in this entry:** the class is **three**
  broken methods, not two sites in the abstract — `DfaProduct` + `NfaProduct`
  (via `materialize_product`) and `MtdfaProduct` (via `emits_dfa`), against
  `MtnfaProduct` + `OtfMtdfaProduct` **immune** (both key acceptance on the
  incoming transition). And the *non-accepting* edgeless case, not the accepting
  one the seed asked about, is what discriminates the readings — it predicts the
  **first known divergence** from the `ltlfsynt` monolithic oracle. See the PRD's
  oracle O5 and the "Prove the monolithic reduction" item below.
- **The class:** a builder computes acceptance correctly, then attaches the mark
  **only inside a per-edge loop** — so a state with **zero** out-edges emits no edges
  and no marks, and Spot's `state_is_accepting` (which reads a state-based mark off a
  state's *first out-edge*) reads the flag back as `false`. **Lost in transit**, not
  deliberately reinterpreted. Four builders, two broken:
  - `materialize_product` (`src/product.cpp:341`) — **broken.** Affects `DfaProduct`
    and `NfaProduct`.
  - `emits_dfa` (`src/emits_dfa.cpp:49`) — **broken; found 2026-07-27** by the
    `MtnfaProduct` expected-divergence fixture on its first run. A $\delta$-dead
    transducer state still gets a state (via `discover(d)`) but zero edges, so
    `spot::twadfa_to_mtdfa` reads it non-accepting and the product intersection
    rejects. Affects **`MtdfaProduct`** — which this item previously implied was
    immune, because acceptance in the mtdfa representation rides the *incoming*
    terminal. The correction: that is true only when acceptance never passes through
    a `twa_graph`. `MtdfaProduct` launders it through `emits_dfa`
    → `twadfa_to_mtdfa`, so it is exposed; `MtnfaProduct` reads
    `any(goal.accepting[s])` straight off `Mtnfa::accepting` and is genuinely immune.
  - `nfa_to_dfa` (`src/nfa_to_dfa.cpp:105`) and `reverse_dfa_to_nfa` (`:40`) —
    **defend correctly**, with a `bddfalse`-guarded self-loop carrying the mark. That
    is the known-good idiom the two broken sites are missing.
- **Intent:** fix, and decide the semantics first. Fixing `emits_dfa` alone is a
  *partial* fix (the mtdfa route becomes right while the explicit route stays wrong —
  the methods still disagree, just along a different line), so the class wants fixing
  together, which is what makes the semantics call below load-bearing.
- **Confirmed fix shape, verified 2026-07-27:** adding the defensive self-loop to
  `emits_dfa` makes the divergence fixture pass — but breaks two existing tests,
  `EmitsDfa.UndefinedAtStateHasNoOutgoingEdgesForAnyLetter` (which deliberately pins
  the current edgeless shape) and `EmitsDfa.AcceptsTheEmptyWordAcrossEveryFixture`.
  So the fix is **not a drive-by**: it re-opens `emits_dfa`'s contract.
- **Reachability:** needs a **partial transducer** — a $\cons$-passing product state
  whose $\delta$ is undefined on every letter. Legal (`transducer.hpp:24`,
  `main.tex:116-117`) and explicitly handled by `build_product_nondet`. Reproduced end
  to end: $\varphi=b$, $\Ofree=\{b\}$, `t_in` with a delta-dead state 1 →
  **both** `DfaProduct` and `NfaProduct` say UNREALIZABLE where REALIZABLE is
  expected.
- **Pre-existing, NOT introduced by `NfaProduct`** (found by its domain review;
  deliberately deferred out of `docs/prd/nfa-product.md` on 2026-07-17 to keep that
  PRD's scope to Method 1 explicit). It affects `DfaProduct` equally, so fixing it
  **changes shipped `DfaProduct` verdicts on partial transducers** — a semantic
  change worth its own grill, which is why it isn't a drive-by.
- **Why the oracles were blind to it — the load-bearing lesson:**
  - the **cross-method** oracle couldn't see it while only the explicit route existed:
    `DfaProduct` and `NfaProduct` share `materialize_product` + `solve_dfa`, so they
    fail **identically and agree**. This changed on 2026-07-27: `MtnfaProduct` is
    genuinely immune, so the cross-method oracle now *would* catch the class — but only
    on a partial transducer, which is the next bullet;
  - the **generated corpus** can't see it: `random_tin` is deterministic + **total**
    by construction (the committed Case-A regime,
    `tests/ltlfsynt_oracle_test.cpp:1337`) and `t_out` is `trivial_transducer`, so
    the partial-transducer regime is simply unexercised at corpus scale.
  A green suite was fully consistent with this bug, and that is how it survived from
  2026-07-17 to 2026-07-27 with a second site undiscovered. **Now pinned:**
  `MtnfaProductExpectedDivergence.*` (`tests/mtnfa_product_test.cpp`) asserts the
  current wrong verdicts of **both** broken sites, so the class can no longer regress
  silently and the eventual fix has its regression test ready — flip both
  `EXPECT_FALSE`s to `EXPECT_TRUE`. Any fix must still ship coverage for a partial
  `t_out` too (the fixture only exercises a $\delta$-dead `t_in`).
- **Seeds for grilling:**
  - **Semantics first:** is a $\cons$-dead transducer state reached *after*
    acceptance a **win**? LTLf acceptance is at the end of a finite trace, so an
    accepting state where the trace can only stop looks like a win — but that is a
    $\Tin$/$\Tout$ partiality reading (`\cref{def:consistency}`), not something
    `alg:dfa_product` spells out. Settle this before touching code; the bug is real
    either way (the flag is *lost*, not deliberately reinterpreted).
  - Fix locus: the defensive self-loop in `materialize_product` (mirrors the two
    existing precedents), or make `ProductGuards`→graph carry acceptance
    out-of-band so no caller can lose it again?
  - Does the corpus want a **partial-transducer regime** (Case B) generally? This
    bug is evidence the total-by-construction corpus has a systematic blind spot,
    not just this one gap.

### Input-dependency extraction (`ltlf-ek-deps --direction in`) — **DONE 2026-08-03**
- **Shipped in two unattended day-run phases**, both on branch
  `input-dependencies-tool` (**PR #6**, draft): Phase 1 (shared `detail::`
  dependency core + `dependent_inputs`) and Phase 2 (the `--direction in|out`
  flag + the O1-in/O3-in/O4-in oracles). `ctest` **572/572**, all four gates
  closed, and the pre-existing suite passed **unedited**, so the extraction's
  regression bar held throughout. Reports:
  `docs/runs/2026-08-03-input-dependencies-phase{1,2}.md`.
- **Two things it leaves the grill**, both recorded as PRD "consider" items
  rather than fixed unattended: the measured $\Xdep\neq\emptyset$ rate is
  **7/150 = 4.7%**, clearing O1-in's vacuity floor by exactly one case; and
  I12's commutation oracle covers $\Tout$ but not $\Tin$, because conjoining an
  out-dependent constraint provably drives $\Xdep^{in}$ to $\emptyset$ (the
  input side analyses $L(\lnot\varphi)$, and $\lnot(A\land B)$ opens live escape
  routes), so a single $\varphi$ non-empty in **both** directions may not be
  constructible by conjunction at all.
- **PRD: GRILLED 2026-07-31 → `docs/prd/input-dependencies-tool.md`; launch gate
  CLEAN as of 2026-08-03.** Two phases: (1) extract the shared `detail::`
  dependency core and add `dependent_inputs`, (2) the `--direction` flag. The
  regression bar on Phase 1 is that the existing suite passes **unedited** — if a
  test needs editing, the public surface moved and that is a PRD-change event.
- **Intent:** extract the *environment*'s forced moves from $\varphi$ and
  materialise them as a $\Tin$, the way the output tool already extracts the
  system's as a $\Tout$. Every method's benchmarks and oracles currently run
  against a trivial or hand-written $\Tin$; this closes that gap.
- **The two differences from the output notion, both forced:** the analysed
  language is $L(\lnot\varphi)$ (the *Violation automaton*), and $\Ydep$ is
  $\mathcal{I}\setminus\Xdep$ — because $\Sin$ moves before the controller, so
  $\lambda_{in}$ may not read the current step's outputs (*Projected live-letter
  region*, and the projection is $\exists$, not $\forall$).
- **Known theory position:** both `\cref{lem:indep-diagonal}` and
  `\cref{lem:indep-transducer}` are stated **unproved**, exactly as the `outdep`
  lemmas are, and the equirealizability claim leans on `\cref{def:probDef}`'s
  unsettled termination reading *more heavily* than the output side does. Accepted
  at the same evidence bar as `outdep`: the code plus the O1-in oracle.

### `ltlfsynt` oracle — known-**output** ($\Tout$) reduction — **DONE 2026-08-03**
- **Outcome:** shipped in PR #3 (`a0d38e9`), both phases, **test-only — no
  production C++ and no CMake change**. Phase 1 (`9512239`) landed the corpus:
  Tables F–J (35 rows), M1–M2 mixed (12), J-bad (4), an empty-$\Ofree$ smoke
  fixture and two AP guards. Phase 2 (`7ee38ae`) generalized
  `run_faithfulness_guard` over `Role` — non-defaulted, slices from
  `sigma_slices` instead of hard-coded $(\Ifree,\Iknown)$ — and applied it to
  every distinct $(\Tout,\psiout)$ pair. `ctest` **543/543**, up from 534 before
  the PRD. All four gates closed; the PRD is `Status: implemented`.
- **The grill's three settled seeds all held**, and nothing in the corpus needed
  re-tuning: every row reproduced against both binaries exactly as tabulated.
- **One known weakness, deliberately left open** (it is a PRD change, i.e. the
  user's call): Table J-bad's over-strong $\psiout$ is **unsatisfiable outright**,
  so the guard meta-oracle only proves "it fires on an unsatisfiable formula",
  where the $\Tin$ analogue proves the stronger "fires on a
  satisfiable-but-wrong language". The $\Tout$ half of the oracle is therefore
  provably weaker than the $\Tin$ half until the satisfiable witness
  $(\lnot x)\land G(X(x\leftrightarrow a))$ is added. Recorded as a load-bearing
  caveat in `docs/GLOSSARY.md` *Faithfulness guard* so it cannot be lost, and it
  is the highest-value item if this oracle is ever revisited.
- **Also still open by decision:** two coverage `consider`s — no mixed row's
  $\Tout$ reads $k$ (so dropping $\Iknown$ from `t_out`'s $\Sigma_0$ is
  undetectable), no fixture has $\lambda$ reading **both** state and $\Sigma_0$,
  and there is no structural meta-assertion that any J-bad row still *diverges*.
- **Side effect worth knowing:** closing this PRD's last gate is what exposed
  that `/code-review` is unreachable from any unattended session, which is now
  fixed — `/launcher` Step 6a runs `/review <PR#>` instead. See
  `docs/unattended-workflow.md`.

### The pending `\cl` notes on `\cref{lem:outdep-diagonal}` / `\cref{lem:outdep-transducer}` — **DONE 2026-07-31**
- **Outcome:** both written into `latex/main.tex`, uncommitted, by the
  `/theory-review` run under `/code-reviewer` on the Phase 3 diff. The
  diagonal note landed **verbatim** as drafted here (reflexivity of "reachable",
  the $\lnot X[!]\mathtt{tt}$ witness for why an irreflexive reading is unsound,
  the empty-$\liveset{s}$-at-a-live-$s$ case, and $\liveset{s}=\emptyset$ at
  non-live $s$) — the draft is dropped from this file rather than duplicated.
- **Plus one that was not drafted here:** a second note on
  `\cref{lem:outdep-transducer}`, recording that "the defaulted letters lie
  outside $L(\varphi)$" is a statement about **prefixes** (an uncovered
  observation is one whose every completion goes to a non-live state), that
  "lose the system the game" is read under the system-controlled-termination
  semantics `\cref{def:probDef}`'s note leaves open, and a two-direction sketch
  of equirealizability keyed on totality and on winning strategies never
  leaving the live part.
- **Anchor drift, already repaired:** the two notes shifted the commented-out
  input-dependency block by +8 lines; `scripts/check-main-tex-refs.py --fix`
  rewrote the affected citations across `docs/GLOSSARY.md` and
  `docs/prd/output-dependencies-tool.md` in the same working tree. Still
  **unpushed** to Overleaf (fetch first, never force).

### `OtfMtdfaProduct` — Method 3.1, the on-the-fly DFA product — **DONE 2026-07-29**
- **Outcome: POSITIVE — the first method to beat the standing champion.** Landed in
  `0ce5fab` with all four gates closed and `ctest` 420/420. Benchmarked against
  `MtdfaProduct` over two controlled families (same $\varphi_n$, same game, only the
  $\cons$ pruning differs, state counts validated *before* timing per the
  `MtnfaProduct` lesson): **up to 5488x faster** where $\cons$ prunes — and *flat*
  (0.68 ms -> 0.94 ms) while `MtdfaProduct` goes exponential — at a **shrinking**
  1.4x-2.7x cost where it prunes nothing. Full tables in
  `docs/prd/otf-mtdfa-product.md` "Benchmark results, 2026-07-29".
- **What it actually beats:** not the product — `spot::product` prunes fine (14
  states at $n=12$) — but the **materialization** of the $2^n$-state goal that
  Method 2 must build first. Exactly `main.tex:350`'s `\na`.
- **What it cost:** one deliberate deviation from `\cref{alg:otfdfa_product}` (I5:
  collapse to the accepting sink once $\varphi$ is irrevocably satisfied), which
  makes $L(P)$ a strict superset of the paper's product language — equirealizable,
  same controller, but **only** because termination is system-controlled. Now
  documented on the declaration itself.
- **Left open:** Phase 2 and Method 3.2, both in Now/next; `--minimize-mtdfa`
  silently ignored for `--otf-mtdfa-product`; state numbering depends on
  unspecified C++ argument-evaluation order (shared with `src/mtnfa_product.cpp`).
- **Theory review found no code-bug**, but did find one in the *paper*: the
  state-keyed $F_P$ of `\cref{alg:otfdfa_product}` is unsound. Four `\cl` notes
  written into `latex/main.tex`, **unpushed**.

### `MtnfaProduct` — Method 1 in the mtdfa representation — **DONE 2026-07-28**
- **Outcome: landed, fully reviewed, and benchmarked — the benchmark verdict is
  NEGATIVE.** `MtnfaProduct` + `mtnfa_product_to_mtdfa` + `--mtnfa-product` all ship;
  glossary / tests / code-review (domain + generic) / theory-review all closed;
  `ctest` 400/400. **Method 1's late determinization does not pay:** `MtdfaProduct`
  wins every measured instance by 9×–3000×, and the fused product determinization
  never yields fewer states than `spot::ltlf_to_mtdfa` (exactly 2× more on the family
  where the Goal NFA is genuinely small). The mtdfa *representation* of Method 1 is
  still worth keeping — ~1.7× over explicit `NfaProduct` when the NFA is small, 12× at
  `game_solving` — but 16× **slower** than `NfaProduct` when it is not. Keep it as the
  paper's NFA route and as a differential oracle; not a default. Numbers, instance
  design and a re-run trap (the intuitive NFA-blowup family degenerates — its NFA is
  as big as its DFA, because `ltlf_to_nfa` is mirror-based) in
  `docs/prd/mtnfa-product.md` "Benchmark results, 2026-07-28".
- **Follow-ups it leaves open** (all recorded in that PRD): the F2 precondition is
  still `assert`-only; the generated corpus never reaches the multi-block
  `delta_edges` path (its transducers are all out-degree 1); `--minimize-mtdfa` is
  silently ignored for `--mtnfa-product`; `README.md`'s wired-flag list is stale.
- **History (as-planned):**
- **PRD:** `docs/prd/mtnfa-product.md` (draft, grilled 2026-07-27) — ready for
  `/glossary` then concurrent `/developer` + `/test-writer`. Both seeds below were
  settled in the grill: the product stays symbolic and the transducer states are
  **tracked alongside** the goal subset (the `(R,q_{in},q_{out})` key, `main.tex:253`),
  **not** folded into the terminal; `turn_order.hpp` is reused exactly as
  `MtdfaProduct` does.
- **Intent:** once the MTNFA representation lands (`docs/prd/mtnfa.md` — the data
  structure + construction + determinize-to-`mtdfa` + isolated `product_xor`
  oracle), build the full Method-1 mtdfa synthesis method: the **symbolic**
  NFA×transducer product (cons filter + transducer-state tracking), determinize the
  product MTNFA into a `spot::mtdfa`, wired to `solve_mtdfa` (landed). Ships as a
  `Synthesis` class `MtnfaProduct` + `--mtnfa-product` CLI + the three canonical
  benchmark stages. **Depends on:** the MTNFA PRD, explicit `NfaProduct` (reference
  oracle), `solve_mtdfa`.
- **Why split off:** the MTNFA PRD de-risks the bespoke set-terminal apply +
  determinizer in isolation on the goal NFA alone; this item then adds the
  well-understood product/cons layer (mirrors `MtdfaProduct`'s route) + method
  wiring on top.
- **Seeds for grilling:**
  - Product stays symbolic: cons = `emits_region(q_in) & emits_region(q_out)`
    restricting live letters; the reachability invariant makes determinized states
    $(R, q_{in}, q_{out})$. Fold transducer successors into the terminal, or track
    them alongside?
  - Comparisons this unlocks: vs `MtdfaProduct` (method axis — does NFA-product's
    determinize step beat DFA-product avoiding it?), vs `NfaProduct` (representation
    axis).
  - Reuse `turn_order.hpp` (`require_turn_order_aps`) as the `solve_mtdfa`
    precondition, exactly as `MtdfaProduct` does.


### Explicit `NfaProduct` — Method 1 (`alg:nfa_product`), the paper's NFA route
- **Intent:** build Method 1 explicitly (`NfaProduct`, glossary *the five
  methods*): the explicit NFA×transducer product over `ltlf_to_nfa` (landed),
  `NfaToDfa` subset determinization into a `twa_graph`, then `solve_dfa`. This is
  the paper's actual Method 1 and, downstream, the **reference oracle +
  representation baseline** the mtdfa route (`MtnfaProduct`) is graded against.
  Independent — needs only `ltlf_to_nfa` + the existing
  `build_product`/`solve_dfa` machinery; does **not** depend on the MTNFA PRD.
- **Outcome:** shipped 2026-07-17 (`41af246` implementation, gates closed in
  `df232a6`, merged `c2fb7a9`). PRD `docs/prd/nfa-product.md` **implemented** —
  `nfa_to_dfa` + `build_product_nondet` (Phase 1) then `NfaProduct` +
  `--nfa-product` + the canonical bench stages with `determinize` as a nested
  sub-span under `product_construction` (settling the stage-mapping seed). $N$ is
  completed before the product (`complete_here`), which is also what exposed the
  `main.tex` `\algname{NfaToDfa}` empty-subset gap now tracked under **Later**.
  Its domain review also found the `materialize_product` acceptance-mark bug
  (pre-existing, deferred to **Later**).
- **Seeds for grilling:** _(resolved in the PRD grill)_

### Replace the explicit-DFA scaffolding with MTDFA — `MtdfaProduct` (2nd impl of Method 2)
- **Intent:** `ltlf_to_dfa` already builds a `spot::mtdfa` via `ltlf_to_mtdfa`,
  then throws the symbolic form away (`as_twa` + `complete_here` path-enumeration
  blowup). Keep the MTDFA all the way through — goal, product, and game stay MTBDD
  arrays, solved with Spot's own MTDFA game solver the way `ltlfsynt` does — added
  as a **second implementation of Method 2** alongside `DfaProduct` (left untouched
  to preserve the differential).
- **Outcome:** shipped, both phases, PRD `docs/prd/mtdfa-product.md` **CLOSED
  2026-07-16**, all gates clean. **Phase 1** (`61b1ad0`): `emits_dfa` (the
  *Output-agreement automaton*, no rejecting sink — skip-not-sink, faithful to
  `alg:dfa_product`'s partial $\delta$), `turn_order.hpp`
  (`register_turn_order_aps` / `require_turn_order_aps` — turn order rides the BDD
  variable order, Ifree strictly above every controllable), `solve_mtdfa` (decision
  2: pinned $\Iknown,\Oknown$ made **controllable** then projected strategy-side,
  theory-reviewed equivalent to `solve_dfa`'s arena-side projection), `MtdfaProduct`,
  and the `--mtdfa-product` CLI flag. The `backprop_nodes=true` segfault was
  root-caused to **upstream Spot #639, fixed in 2.15** (not our bug) — `CMakeLists`
  now requires `libspot >= 2.15`. **Phase 2** (`d1b0355`, gate bookkeeping
  `0d2f93c`): the three canonical `BenchTimer` stages wired
  (`automaton_construction` = `ltlf_to_mtdfa` **alone** — the measured win) plus a
  `minimize_mtdfa` knob (own span, default off). Suite green **307/307**;
  `/code-reviewer` + `/code-review` + `/theory-review` all clean. **Benchmark
  validated** the core claim live vs `DfaProduct` on realizable + unrealizable
  instances: faster at every stage — `automaton_construction` ~1.2–1.7× (grows with
  goal-DFA size), `product_construction` / `game_solving` 5–46× by staying symbolic;
  verdicts never disagreed. `minimize_mtdfa` shows no downstream payoff at these
  sizes (adjacent free real estate, as predicted). Follow-ups still open under
  **Later**: size metrics + the other-four-methods stage-mapping (benchmarking item).

### Symbolic DFA-product construction (skip the minterm loop)
- **Intent:** replace Method-2 `DfaProduct`'s exponential minterm loop
  (enumerate $v\in2^{\mathcal I\cup\mathcal O}$, group into guarded edges —
  $2^{|\mathcal I\cup\mathcal O|}$ by design) with **symbolic BDD-guard algebra**:
  compute successors and the $\cons$ filter directly on edge-guard BDDs, never
  materialising a letter. Promoted to #1 (2026-07-10) because **benchmarking** is
  the tool's eventual purpose, so the letter loop stopped being an acceptable
  baseline.
- **Outcome:** shipped in two phases (PRD `docs/prd/symbolic-dfa-product.md`,
  `implemented (Phase 1 + Phase 2)`, all four gates clean). Phase 1 (`326136c`):
  the symbolic `Transducer` contract — `emits_region(q)` (region form of `emits`)
  + `delta_edges(q)` (edge-partition form of `delta`) on the base class,
  implemented in `OutputLabeledTransducer`, with the Phase-1 contract-equivalence
  oracle (`tests/symbolic_transducer_contract_test.cpp`). Phase 2 (`d88904c`):
  `build_product_symbolic` (guard = $g_{goal}\wedge\bigwedge_i(g_i\wedge
  \texttt{emits\_region}(q_i))$, cost = product of out-degrees) + `ProductGuards`
  / `to_guard_map` / `materialize_product`, `DfaProduct::synthesize` rewired off
  the minterm loop. The **build-equivalence metamorphic oracle**
  (`tests/product_build_equivalence_test.cpp` + generated-corpus body) asserts
  `build_product_symbolic == to_guard_map(build_product(...))` (BDD-equal game),
  bug-injection-verified non-vacuous. Suite green 226/226, verdicts byte-identical
  (invariant 4). `/code-reviewer` + `/code-review` clean (2 low-severity considers
  applied), `/theory-review` blessed the symbolic-$\cons$ region faithful to
  `\cref{def:consistency}` by minterm distributivity (with a `\cl` traceability
  note pushed to Overleaf, `43b15f4`). The per-letter core is **kept** (backs
  `verify_controller` + the oracle reference). Two follow-ups logged under
  **Later**: the symbolic `verify_controller` ν-fixpoint (deliberately out of
  scope) and benchmarking (the driver this rewrite serves).

### Intense "soak" mode for the generated corpus
- **Intent:** an opt-in, wall-clock-budgeted escalating soak over the generated
  corpus (nightly / pre-release), leaving the fast `ctest` gate byte-for-byte
  unchanged. Reframed in the grill from "env-overridable knobs + seed sweep" to a
  `LTLF_EK_SOAK=<secs>` runner that escalates complexity (wider $\Ifree$, deeper
  formulas, more $\Tin$ states) until the deadline.
- **Outcome:** shipped in two phases (PRD `docs/prd/generated-corpus-soak-mode.md`,
  `implemented`, all gates clean). Phase 1 (`c897c82`): `CorpusConfig` +
  `LTLF_EK_CORPUS_*` per-knob env overrides (loud-on-malformed) + the byte-identical
  golden guard. Phase 2 (this commit): the `LTLF_EK_SOAK` escalating `run_corpus`
  driver (fresh-seed levels, `ladder`, width ceilings, joint clamp, per-case
  `bad_alloc`→skip), the metamorphic `t_in` replay dump, and ladder/soak tests.
  Suite green 215/215; `/code-reviewer` + `/code-review` both clean. Two follow-ups
  logged under **Later** (metamorphic per-case time bound; `levels_reached`
  cosmetic). A deeper Spot finding surfaced and is captured in memory:
  `randltlgenerator` in-process rebuilds are **not** seed-reproducible (global RNG +
  AP apid recycling), so per-case replay is via the printed `phi`/partition/`t_in`,
  not corpus regeneration.

### Trace-level controller verifier oracle
- **Intent:** the internal linchpin correctness oracle — for a synthesized
  `Controller` $T_C$, check **every trace agreeing with $\Tin,\Tout,T_C$ satisfies
  $\varphi$** (reachability of $F_\varphi$ under adversarial env, built directly on
  `def:probDefTransducer`). Reusable by every method; unblocks `--model-check`.
- **Outcome:** shipped. `verify_controller` + `Role::t_c` +
  `controller_as_transducer` + `VerifyResult`/`Witness` landed (`81a4cf4`), then
  migrated onto the shared product core (`f5f53e7`) and cleaned post-merge
  (`6c2950b`). All four PRD gates clean — glossary, tests (`verify_controller_test.cpp`,
  12 cases across oracles #1–#6; suite green 186/186), `/code-reviewer` + generic
  `/code-review` both clean, `/theory-review` code↔math faithful. PRD
  `docs/prd/controller-verifier.md` `Status: implemented`. One non-blocking
  residue (Witness bdd lifetime) tracked separately under **Later**.

### Implement the `ltlfsynt` external oracle (known-**input** $\Tin$)
- **Intent:** an external, independent realizability oracle — cross-check the
  built `ltlf-ek-synth` against Spot's `ltlfsynt` on the equirealizable
  `psi_in -> phi` reduction.
- **Outcome:** shipped (`745b3d7`). `tests/ltlfsynt_oracle_test.cpp` drives the
  external `ltlfsynt` binary as a subprocess; CMake wires
  `find_program(LTLFSYNT_EXECUTABLE)` + `LTLFSYNT_BINARY` with `GTEST_SKIP` on a
  box without Spot's CLI (env override `LTLFSYNT_BIN`). PRD:
  `docs/prd/ltlfsynt-oracle.md`. A faithfulness guard (`6fc2b34`) also
  cross-checks each corpus $(\Tin,\psi_{in})$ pair against itself. The
  known-**output** $\Tout$ half **shipped 2026-08-03** (see Done).

### Sharpen the Transducer definition, signature & input API
- **Intent:** firm up the `Transducer` abstraction — its definition, C++
  signature, and how the CLI hands a transducer to the library as input.
- **Outcome:** both halves shipped. In-library C++ path — `concrete-transducer.md`
  (**implemented**, `2b45755`): `OutputLabeledTransducer` concretises the
  `Transducer` base, consumed by `Synthesis::synthesize` as `t_in`/`t_out`.
  External file format / CLI parser — `transducer-file-format.md`
  (**implemented**): the `%%LAMBDA` format + `parse_transducer`
  (`src/transducer_io.cpp`), wired into the CLI via `--known-input-transducer`.

### Git integration (Overleaf sync)
- **Intent:** sync `latex/main.tex` with Overleaf via its git bridge.
- **Outcome:** `latex/` submodule tracks Overleaf's `main` (`branch = main`,
  `update = rebase`); submodule pointer is committed in the parent (already was,
  via `784c296`); two-way sync workflow documented in `docs/overleaf-sync.md` and
  linked from the README.
