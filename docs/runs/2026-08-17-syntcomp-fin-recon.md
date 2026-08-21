# Day-run 2026-08-17 — SYNTCOMP-fin reconnaissance + hit-rate pilot

**Verdict: all four steps ran; the corpus is ingestible and the pilot's answer is
a null.** Zero exploitable dependencies in 100 analysed instances, both
directions. Three blockers named, none of them the corpus.

Plan: [`docs/plans/2026-08-17-week.md`](../plans/2026-08-17-week.md), *Monday
2026-08-17*. Independent of everything; reads `master` (`25d2456`), writes this
report plus one new script.

## The headline

**The predicted null arrived, but so did the reason the full-corpus sweep will
not simply scale.** Week-plan finding 6 predicted arm (a) would find few
dependencies; the pilot found **none at all** — 0/47 for `--direction out`, 0/53
for `--direction in`. That is the motivation result, and it is the cheap half.

The expensive half is that **`ltlf-ek-deps` — not the corpus, not Spot — is what
the sweep runs out of.** The `Patterns/Uright` family is a clean scaling ladder
and it prices the extractor exactly:

| APs | 1–11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 |
|---|---|---|---|---|---|---|---|---|---|---|
| `--direction out` | <0.05 s | 0.1 s | 0.3 s | 1.0 s | 3.6 s | 13.0 s | MEMOUT | MEMOUT | MEMOUT | timeout |

≈3.5× per added atomic proposition, and a wall at 16–17 APs on a 2 GiB budget.
Raising the budget does not move the wall much: at **300 s and 8 GiB**,
`uright_pb_20` (a **144-character** formula, `p1 U (p2 U (… p20))`) dies with
`std::bad_alloc` after 52 s — a formula `ltlfsynt` decides **REALIZABLE in 5 ms**.
The failure mode is memory, not time (B2).

The corpus's median instance has **22 APs**, and only **25 %** of it sits at or
below 16:

| category | n | AP count min / median / max | ≤16 APs |
|---|---|---|---|
| Patterns | 40 | 1 / 11 / 20 | 32 (80 %) |
| Random | 1237 | 6 / 20 / 49 | 385 (31 %) |
| Two-player-Game | 440 | 4 / 124 / 502 | 13 (3 %) |
| **total** | **1717** | 1 / 22 / 502 | **430 (25 %)** |

So Tuesday's "whole corpus" sweep is, as things stand, a sweep of about a
quarter of the corpus — and the un-analysed three quarters are the *large*
instances, which is exactly the population where a dependency would have been
worth finding. That is a resource-policy question the sweep has to answer
out loud rather than absorb into a denominator.

## Corpus provenance

| | |
|---|---|
| repo | `https://github.com/SYNTCOMP/benchmarks`, subtree `tlsf-fin/` |
| commit | **`2d53c6b52af3cfca997b239b8c9d0347b2cf166d`** (2026-06-18, *"Remove chomp clone"*) |
| licence | `LICENSE-CC-BY` at the repo root |
| fetched as | `git clone --depth 1 --filter=blob:none --sparse` + `sparse-checkout set tlsf-fin` |
| scratch location | `build/scratch/syntcomp/` — **not vendored**, per the stop-list |
| size | 1742 `.tlsf` files |

Per-category totals: `Random` 1237, `Two-player-Game` 440 (Nim 400,
Double-Counter 20, Single-Counter 20), `Patterns` 40 (GFand 20, Uright 20),
`chomp_game` 21, `Scutella` 4.

## 1. Ingestion — the parse split (week-plan step 2, finding 4)

Finding 4's guess holds, and by a wide margin.

| category | total | adapter alone | parse failure | needs real TLSF |
|---|---|---|---|---|
| Patterns | 40 | **40** | 0 | 0 |
| Random | 1237 | **1237** | 0 | 0 |
| Two-player-Game | 440 | **440** | 0 | 0 |
| Scutella | 4 | 0 | 0 | **4** |
| chomp_game | 21 | 0 | 0 | **21** |
| **total** | **1742** | **1717 (98.6 %)** | **0** | **25 (1.4 %)** |

The split is not a gradient: it is **per category, all-or-nothing**. Three
categories are pure basic TLSF; two are pure real TLSF.

"Adapter alone" means the file is `INFO` + `MAIN { INPUTS OUTPUTS GUARANTEES }`,
every signal is atomic, and the guarantee body becomes a Spot formula by
replacing `&&`→`&` and `||`→`|`. Nothing else differs — `->`, `<->`, `X`,
`X[!]`, `G`, `F`, `U` are already common spelling. The adapter is committed as
[`scripts/tlsf_adapter.py`](../../scripts/tlsf_adapter.py), which is what
Tuesday's sweep is specified to reuse.

### The adapter was checked against an independent oracle, not just against "it parses"

`syfco` **1.2.1.2** is installed at `~/.local/bin/syfco`, and
`syfco -f ltlxba-fin -m fully` is a full second implementation of the same
conversion — the one Spot's own `ltlfsynt --tlsf=` shells out to (confirmed via
`ltlfsynt --verbose`). It preserves the strong/weak next distinction (`X[!]` vs
`X`) that this corpus leans on heavily.

Every one of the **1717** conversions was compared against it, on Spot's
canonical rendering of both formulas (via `ltlfilt`) plus set equality of the
input and output signals: **1717/1717 match, 0 mismatches.** The 1717 is
therefore a correctness claim, not just a no-crash claim.

### Three concrete files that need real TLSF machinery

1. **`Scutella/scutella_pb_1_pe_.tlsf`** — `GLOBAL { DEFINITIONS { … } }` with a
   macro `ExactlyOne(x) = (||[0 <= i < (SIZEOF x)] x[i]) && …`; bit-vector output
   `s[5]`; `PRESET` / `ASSERT` / `GUARANTEE` instead of a single `GUARANTEES`.
   Needs indexed-conjunction expansion and `SIZEOF`.
2. **`chomp_game/parametric/generated/chomp_pb_2_2_pe_.tlsf`** — everything
   above **plus** `PARAMETERS { N = 2; M = 2; }`, parameter-sized bit vectors
   (`ix[N]`, `os[N*M]`), macros taking bit vectors as arguments
   (`Pos(grid,i,j) = grid[i + j*N]`), and a `REQUIRE` section (an environment
   assumption — the only `ASSUME`-shaped construct in the whole corpus).
3. **`chomp_game/parametric/generated/chomp_pb_4_8_pe_.tlsf`** — the same shape
   at `N=4, M=8`, i.e. 32 grid cells expanded from the macros. Included because
   it shows the expansion is a *code generator*, not a textual substitution.

All 25 divide into exactly two shapes: 4 with `DEFINITIONS` only (Scutella), 21
with `PARAMETERS` + `DEFINITIONS` (chomp).

### These 25 are reachable today — but taking that path is a decision

`syfco -f ltlxba-fin -m fully` converts **25/25**, and Spot parses **25/25** of
the results (checked). End to end, `ltlfsynt --tlsf=<file>` answers both
categories directly — `Scutella/scutella_pb_1` is REALIZABLE under Moore, and
`chomp_pb_2_2` is REALIZABLE, matching its own `//STATUS : realizable` footer
(chomp is the only sub-corpus carrying published verdicts: 21 files, all
"realizable").

What does **not** exist is a way to get the formula and partition *out* for the
repo's own tools without depending on `syfco` as a build/run prerequisite —
`ltlfilt` has no `--tlsf`, and syfco's LTL output formats other than
`ltlxba-fin` reject weak next outright. Whether to depend on syfco, vendor a
pre-converted set, or drop the 25 is left for Wednesday. At 1.4 % of the corpus
and two categories that are each internally uniform, dropping them costs little
except the `chomp_game` published verdicts.

## 2. Turn order (week-plan step 3, finding 5) — settled, and it is a real hazard

**Step 3's own check passes.** Three `Two-player-Game/Single-Counter`
instances, documented realizable in the corpus README ("the system wins if the
counter eventually overflows… the specification assumes the environment will
send the increment signal at least once every two timesteps"):

| instance | declared | `ltlfsynt --semantics=Moore` | `ltlfsynt --semantics=Mealy` | ours, no knowledge |
|---|---|---|---|---|
| `counter_pb_01_pe_` | Finite,Moore | REALIZABLE | REALIZABLE | REALIZABLE |
| `counter_pb_02_pe_` | Finite,Moore | REALIZABLE | REALIZABLE | REALIZABLE |
| `counter_pb_03_pe_` | Finite,Moore | REALIZABLE | REALIZABLE | REALIZABLE |

(`ltlfsynt` = `~/opt/spot-2.15.1/bin/ltlfsynt`, absolute path per the standing
rule. Ours = `build/ltlf-ek-synth --otf-mtdfa-product`; its verdict is the
**exit code**, 0 realizable / 20 unrealizable, not a word on stdout.)

**But three agreeing instances do not license a corpus-wide number**, so the
same two-semantics comparison was run over the whole step-4 sample. It diverges:

| category | n | comparable | agree | differ |
|---|---|---|---|---|
| Patterns | 40 | 40 | 40 | 0 |
| Random | 40 | 33 | 33 | 0 |
| Two-player-Game | 40 | 3 | 2 | **1** |
| **total** | **120** | **76** | **75** | **1** |

The divergence:

> `Two-player-Game/Nim/nim_03/System-first/nim_pb_03_05_pe_.tlsf`
> (36 APs) — **Moore: UNREALIZABLE, Mealy: REALIZABLE.**

Reproduced through two independent ingestion paths (this report's adapter, and
`ltlfsynt --tlsf=` via syfco), and `ltlf-ek-synth` answers **REALIZABLE**,
i.e. it agrees with Mealy. `ldd build/ltlf-ek-deps` confirms
`libspot.so.0 => ~/opt/spot-2.15.1/lib/libspot.so.0`, so this is not the
shadowed-install trap and not upstream Spot #639.

**This is a finding, not a bug, and nothing was changed.** `turn_order.hpp`
untouched, no verdict "fixed" (stop-list). The content of the finding:

- every `tlsf-fin` file declares `SEMANTICS: Finite,Moore` (1721 of 1742 also
  `TARGET: Moore`; the 21 chomp files are `Mealy,Finite`);
- the repo's mtdfa game is **Mealy-only by construction** —
  `turn_order.hpp` registers `input_free` strictly above every controllable and
  has no Moore mode at all;
- the two readings **do** disagree on this corpus, at a measured rate of
  **1/76 comparable sample instances**, in the `Nim` family — which is where
  Tuesday's raceable instances are most likely to live.

Consequence for every number downstream: a corpus verdict has to be stamped with
its semantics, and the repo can currently only produce the Mealy one. Comparing
a repo verdict against a published `tlsf-fin` verdict is comparing two different
games unless the instance is one where they coincide.

## 3. Hit-rate pilot (week-plan step 4)

### Pre-registration

Stated in `build/scratch/pilot.py`'s docstring before the run, so it is not
retrofitted:

- **sample** — `random.Random(20260817).sample(...)`, `min(40, n)` per category,
  drawn from that category's adapter-parsable instances. One draw, seeded by the
  date. No re-draw, no second variable grouping.
- **partition** — exactly the TLSF split: `input_free` = INPUTS,
  `output_free` = OUTPUTS, both known sets empty. The question is what the
  specification carries *on its own*.
- **budget** — 20 s wall, 2 GiB address space, per process.
- **outcomes** — `HIT` / `NONE` / `TIMEOUT` / `MEMOUT` / `EMPTYLANG` (exit 3) /
  `ERR` / `ARGV_TOO_LONG`, each counted separately. A resource failure is never
  folded into "no dependency found" (Q9's recommendation).

`Scutella` and `chomp_game` contribute **0** instances — both are 100 % real
TLSF, so there is nothing for the adapter to hand the tool. Sampled: 120
instances over 3 categories.

### `--direction out`

| category | n | HIT | NONE | TIMEOUT | MEMOUT | EMPTYLANG | ERR | ARGV_TOO_LONG | hit rate |
|---|---|---|---|---|---|---|---|---|---|
| Patterns | 40 | 0 | 31 | 6 | 3 | 0 | 0 | 0 | 0/31 = **0.0 %** |
| Random | 40 | 0 | 15 | 18 | 2 | 5 | 0 | 0 | 0/15 = **0.0 %** |
| Two-player-Game | 40 | 0 | 1 | 23 | 0 | 0 | 0 | 16 | 0/1 — *not a rate* |
| **total** | **120** | **0** | **47** | **47** | **5** | **5** | 0 | **16** | 0/47 = **0.0 %** |

### `--direction in`

| category | n | HIT | NONE | TIMEOUT | MEMOUT | EMPTYLANG | ERR | ARGV_TOO_LONG | hit rate |
|---|---|---|---|---|---|---|---|---|---|
| Patterns | 40 | 0 | 32 | 7 | 1 | 0 | 0 | 0 | 0/32 = **0.0 %** |
| Random | 40 | 0 | 20 | 20 | 0 | 0 | 0 | 0 | 0/20 = **0.0 %** |
| Two-player-Game | 40 | 0 | 1 | 23 | 0 | 0 | 0 | 16 | 0/1 — *not a rate* |
| **total** | **120** | **0** | **53** | **50** | **1** | 0 | 0 | **16** | 0/53 = **0.0 %** |

**Read this honestly.** The Two-player-Game row has an analysed denominator of
**one**; it is reported for completeness and carries no information. The real
result is Patterns 0/31 and Random 0/15 for outputs, 0/32 and 0/20 for inputs.
And the analysed instances are the *small* ones: a completed run has median
0.02 s (max 17.6 s), while every unfinished one is on the far side of the AP
cliff in the headline. So the null is currently established on the easy quarter
of the corpus.

Five `Random/Syft` instances exit 3 (`EMPTYLANG`) under `--direction out`,
i.e. φ is unsatisfiable: `syft_1/039`, `syft_2/147`, `syft_5/012`,
`syft_5/030`, `syft_5/058`. Those are not failures — they are trivially
unrealizable instances that no race should include.

### Free side-observation: the `ltlfsynt` cost profile Tuesday needs

Collected because the same runs produced it. Under Mealy, 20 s:

- `Patterns` — 19 realizable, 21 unrealizable, **0** unfinished;
- `Random` — 12 / 21, **7** unfinished;
- `Two-player-Game` — 6 / 1, **33** unfinished.

Tuesday's step 2 exists to find instances that are hard for `ltlfsynt` but not
impossible. On this sample that band is almost empty at 20 s: Patterns and most
of Random are milliseconds, and Nim is a wall. Budget Tuesday's profiling run
accordingly — a longer per-instance timeout is what will populate the band.

## 4. Three named blockers

**B1 — the repo's CLIs cannot accept 164 of the corpus formulas at all.**
`ltlf-ek-deps` and `ltlf-ek-synth` take the formula as `--formula <string>`, and
Linux caps a single `argv` entry at `MAX_ARG_STRLEN` = 131072 bytes (measured on
this machine: `execve` succeeds at 131069 characters, fails at 131077). **164 of
440 `Two-player-Game` instances (37 % of the category, 9.5 % of the corpus)**
exceed it — 16 of the 40 sampled, showing up as `ARGV_TOO_LONG` above.
`ltlfsynt` is unaffected because it reads `-F -`. The remedy is a small one — a
`--formula-file` flag, or reading `--formula -` from stdin — but it is a change
to a frozen CLI surface, so this run did not make it. **Until it exists, "the
whole corpus" excludes 164 instances.**

**B2 — the extractor, not the corpus, is the scaling limit.** The AP ladder in
the headline: ≈3.5× per atomic proposition, wall at 16–17 APs on 2 GiB, and
raising to 300 s / 8 GiB does not rescue it. Of three re-run timeouts, one
succeeded at 81 s (`Random/Syft/syft_2/084`, verdict `none`); one died with
`std::bad_alloc` at 52 s on a **144-character** formula (`uright_pb_20`) that
`ltlfsynt` solves in 5 ms; and `Nim/nim_12/…/nim_pb_12_09` (164 APs) exhausted
a 10 GiB budget — peak RSS 10.0 GB, then `std::bad_alloc`. It is **memory, not
a crash**: the earlier `SIGSEGV` seen for that instance was an artefact of
capping with `RLIMIT_AS`, which turns an allocation failure into a fault the
runtime cannot unwind; with a plain `ulimit -v` and the same work it reports
`bad_alloc` cleanly. `ldd` confirms Spot 2.15.1, so this is neither the
shadowed-install trap nor upstream #639. Tuesday needs a stated resource policy
— budget, memory cap, and how a resource failure is reported — before it can
call anything a corpus-wide rate.

**B3 — the corpus is Moore, the repo is Mealy, and they genuinely differ.**
Section 2. One divergence in 76 comparable sample instances, reproduced two
ways. Not a bug to fix; a qualification every downstream number has to carry,
and an open question about whether the corpus can serve as an oracle at all for
a Mealy-only implementation.

## Stop-list compliance

| rule | status |
|---|---|
| do not modify `turn_order.hpp` | untouched |
| do not "fix" a verdict disagreement | recorded in §2, nothing changed |
| do not vendor the corpus | scratch clone under `build/scratch/` (gitignored); only the SHA is recorded |
| do not tune the sample | one seeded draw, reported as drawn; the 300 s re-run of three timeouts is labelled a probe and changes no pilot number |

## Questions this leaves for Wednesday's grill

1. **Q9 is now answerable with a number, and the number is zero.** Does a 0 %
   hit rate on the analysed quarter ship as the motivation result, or does the
   claim need the other three quarters first — which means paying for B1 and B2?
2. **Does arm (a) survive B2 at all?** If the extractor cannot pass 16–17 APs
   and the corpus median is 22, "no dependency found" and "could not look" are
   nearly the same set. Options: cap the corpus at ≤16 APs and say so; invest in
   the extractor; or reframe arm (a) as a *scaling* result about the extractor.
3. **What semantics do the corpus numbers get stamped with** (B3), and is a
   Moore mode wanted, or is the answer "report Mealy, and note the corpus's
   declared semantics differs"?
4. **The 25 real-TLSF files** — depend on syfco, vendor a converted set, or drop
   them? Dropping loses the only sub-corpus with published verdicts.
5. **B1's fix is cheap and blocks Tuesday's largest category.** Is a
   `--formula-file` / stdin flag on the two CLIs in scope?

## Reproduction

```sh
git clone --depth 1 --filter=blob:none --sparse \
    https://github.com/SYNTCOMP/benchmarks.git build/scratch/syntcomp
git -C build/scratch/syntcomp sparse-checkout set tlsf-fin   # 2d53c6b5

python3 scripts/tlsf_adapter.py <file>.tlsf --emit-part /tmp/p.part
```

The one-off harness that produced the tables (`classify.py`,
`validate_adapter.py`, `pilot.py`, `timeout_probe.py`, `summarize.py`, plus
`parse-split.json`, `adapter-validation.json`, `pilot.json`) lives in
`build/scratch/` and is **not committed** — `build/` is gitignored and the
corpus it points at is not vendored. `scripts/tlsf_adapter.py` is the piece
Tuesday depends on, and it is committed. The pre-registration in §3 is restated
here in full precisely because `pilot.py` is disposable.
