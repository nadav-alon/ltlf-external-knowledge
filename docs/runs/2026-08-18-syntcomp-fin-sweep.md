# Day-run 2026-08-18 — SYNTCOMP-fin full-corpus sweep + `ltlfsynt` cost profile

**Verdict: all three steps ran to completion on their pre-registered
populations.** The hit rate is **6/388 = 1.5 %** for `--direction out` and
**0/405 = 0.0 %** for `--direction in`, on a tier-1 population analysed
end to end — **430/430, zero `NOT_REACHED`** — and the `ltlfsynt` profile
covers **1717/1717, also zero `NOT_REACHED`**. Every one of the 7 hits is an
*unrealizable* specification, so Q8's certificate had no controller to
model-check; the verdict was corroborated three ways instead and agrees 7/7.
**No theory finding, no new blocker.**

Plan: [`docs/plans/2026-08-17-week.md`](../plans/2026-08-17-week.md), *Tuesday
2026-08-18*. Independent of Monday night; uses only `ltlf-ek-deps`,
`ltlf-ek-synth` and `ltlfsynt` — all on `master` (`40c5d72`) — plus Monday's
committed ingestion adapter `scripts/tlsf_adapter.py`. Writes this report and
the raw JSON; touches no source file.

Predecessor: [`docs/runs/2026-08-17-syntcomp-fin-recon.md`](2026-08-17-syntcomp-fin-recon.md).
The three blockers it named were decided by the user on Monday evening (B1/B2/B3
in the week plan); this run executes those decisions rather than re-opening them.

## Pre-registration

Stated in `build/scratch/sweep/sweep.py`'s docstring **before** the run and
restated here in full, because that script is disposable (`build/` is
gitignored) and the pre-registration is the part that has to survive.

**Population.** The 1717 `ADAPTER_OK` instances of `tlsf-fin` at commit
`2d53c6b52af3cfca997b239b8c9d0347b2cf166d`, as classified by Monday's
`parse-split.json`. The 25 real-TLSF files (`Scutella` ×4, `chomp_game` ×21) are
out — whether to depend on `syfco` for them is Wednesday's grill question 4.

**Partition.** Exactly the TLSF split, unchanged from Monday: `input_free` =
INPUTS, `output_free` = OUTPUTS, both *known* sets empty. The question is what
the specification carries on its own.

**Tiers** (B2, decided by the user):

| tier | population | budget | purpose |
|---|---|---|---|
| 1 | `n_ap` ≤ 16 — **430** instances | 120 s, 8 GiB | produce the real hit-rate |
| 2 | `n_ap` > 16 — **1287** instances | 30 s, 4 GiB | census the wall per category |

Every result row carries its tier, so a tier-2 `TIMEOUT` is never read as a
tier-1 one. Memory is capped with a plain shell `ulimit -v`, never
`resource.setrlimit` in a `preexec_fn` — Monday showed the latter turns
`std::bad_alloc` into an unrecoverable `SIGSEGV` and corrupts the taxonomy.
Concurrency was capped so that Σ(memory caps) over all simultaneously running
measurement processes stayed ≤ 12 GiB on this 19 GiB / 24-core box.

**Order — and why it is load-bearing.** Within each tier, one seeded shuffle,
`random.Random(20260818)`. The unattended window closes at a hard wall-clock
deadline, and a randomised order means that if the deadline truncates a tier,
what *was* analysed is an **unbiased random subsample** of that tier rather than
its alphabetically-early or computationally-cheap end. Instances the deadline
never reached are their own `NOT_REACHED` bucket, excluded from every
denominator with the count stated.

**Outcomes**, each counted separately and never folded into "no dependency
found" (Q9's recommendation; B2's *the wall is a result, not an excuse*):
`HIT` / `NONE` / `TIMEOUT` / `MEMOUT` / `EMPTYLANG` (exit 3 — φ unsatisfiable for
`out`, valid for `in`) / `ERR` / `ARGV_TOO_LONG` / `NOT_REACHED`. **The
denominator of every hit-rate below is *analysed* = `HIT` + `NONE`, never
*corpus*.** No re-runs, no second variable grouping, no re-draw.

**Verdict oracle (B3).** `ltlfsynt` in **Mealy** mode, not the corpus's
published `Finite,Moore` verdicts. Both semantics are run on every analysed
instance so that every divergence can be counted and listed. **Every number in
this report is under Mealy semantics; the corpus declares `Finite,Moore`.** No
Moore mode was built — that is a construction change to a settled part of the
repo and is not a day-run's call.

## Binary pinning — asserted, not assumed

The week plan's warning is real: in a **non-interactive** shell `ltlfsynt`
resolves to `/usr/local/bin/ltlfsynt`, which is Spot 2.14.4.dev and carries the
mtdfa backprop bug (upstream #639, fixed in 2.15). Since B3 makes `ltlfsynt` the
verdict oracle for the whole corpus, a bare call would have adjudicated
everything on the wrong version.

`sweep.py` therefore invokes `~/opt/spot-2.15.1/bin/ltlfsynt` by absolute path
and **aborts before the first instance** unless `--version` reports 2.15.1.
Checked at launch:

```
$ ~/opt/spot-2.15.1/bin/ltlfsynt --version
/home/cowclaw/opt/spot-2.15.1/bin/ltlfsynt (spot) 2.15.1

$ ldd build/ltlf-ek-deps  | grep spot
	libspot.so.0 => /home/cowclaw/opt/spot-2.15.1/lib/libspot.so.0
$ ldd build/ltlf-ek-synth | grep spot
	libspot.so.0 => /home/cowclaw/opt/spot-2.15.1/lib/libspot.so.0
```

Both repo binaries link the 2.15.1 tree, so the shadowed-install trap is closed
on all three tools this run uses.

## The headline

**Monday's null survived contact with the whole of tier 1, and it is now a null
with a number on it rather than an absence of one.** The pilot saw 0/100; the
complete tier-1 sweep sees **6 hits in 388 analysed instances (1.5 %)** for
output dependencies and **0 in 405 (0.0 %)** for input dependencies. The
direction asymmetry is the substantive part: *not one instance in the analysed
corpus carries an input dependency*, and the few output dependencies all sit in
a single family.

This is week-plan finding 6 arriving exactly as the user predicted, and it is
the motivation experiment rather than a disappointment: if real
finite-semantics specifications carry essentially no exploitable dependency,
external knowledge cannot be recovered *from $\varphi$* and must come from
outside it — which is what `main.tex`'s Introduction already asserts about PDDL
domains, and what nobody had measured.

Three qualifications, all of which the pre-registration required be stated
rather than absorbed into a denominator:

1. **The hits are concentrated.** All 7 — 6 in tier 1, 1 in tier 2 —
   sit in `Random/Lydia`. None in `Patterns`, none in `Two-player-Game`, in
   either tier.
2. **Every hit is unrealizable** (Step 3), so not one is a race candidate. The
   count of dependency-carrying instances that could actually carry a
   benchmark is **0**.
3. **Tier 2 is a census of a wall, not a rate.** 239 of its 800 rows completed;
   the rest are `TIMEOUT`, `MEMOUT` or `ARGV_TOO_LONG`, and 487 instances were
   never reached before the deadline.

## Step 1 — the extraction sweep

Tier 1 is complete: all 430 instances, both directions, no `NOT_REACHED` and no
harness errors. Tier 2 ran until the wall-clock deadline and is reported as the
partial, unbiased random subsample the seeded shuffle guarantees.


## deps tier 1 — 430 rows

### tier 1, `--direction out`

| category | in tier | analysed | HIT | NONE | TIMEOUT | MEMOUT | EMPTYLANG | ERR | ARGV_TOO_LONG | NOT_REACHED | hit rate |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Patterns | 32 | 32 | 0 | 32 | 0 | 0 | 0 | 0 | 0 | 0 | 0/32 = **0.0 %** |
| Random | 385 | 343 | 6 | 337 | 0 | 6 | 36 | 0 | 0 | 0 | 6/343 = **1.7 %** |
| Two-player-Game | 13 | 13 | 0 | 13 | 0 | 0 | 0 | 0 | 0 | 0 | 0/13 = **0.0 %** |
| TOTAL | 430 | 388 | 6 | 382 | 0 | 6 | 36 | 0 | 0 | 0 | 6/388 = **1.5 %** |

### tier 1, `--direction in`

| category | in tier | analysed | HIT | NONE | TIMEOUT | MEMOUT | EMPTYLANG | ERR | ARGV_TOO_LONG | NOT_REACHED | hit rate |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Patterns | 32 | 32 | 0 | 32 | 0 | 0 | 0 | 0 | 0 | 0 | 0/32 = **0.0 %** |
| Random | 385 | 360 | 0 | 360 | 0 | 6 | 19 | 0 | 0 | 0 | 0/360 = **0.0 %** |
| Two-player-Game | 13 | 13 | 0 | 13 | 0 | 0 | 0 | 0 | 0 | 0 | 0/13 = **0.0 %** |
| TOTAL | 430 | 405 | 0 | 405 | 0 | 6 | 19 | 0 | 0 | 0 | 0/405 = **0.0 %** |

HITS: 6
   Random/Lydia/case_05_50/09.tlsf {'t': 0.013, 'outcome': 'HIT', 'xdep': ['p159   (of p112', 'p119', 'p122', 'p151', 'p159)']} {'t': 0.016, 'outcome': 'NONE', 'xdep': []}
   Random/Lydia/case_06_50/38.tlsf {'t': 0.192, 'outcome': 'HIT', 'xdep': ['p158   (of p152', 'p155', 'p158', 'p176', 'p187', 'p189)']} {'t': 0.273, 'outcome': 'NONE', 'xdep': []}
   Random/Lydia/case_06_50/06.tlsf {'t': 0.168, 'outcome': 'HIT', 'xdep': ['p182', 'p200   (of p125', 'p161', 'p182', 'p193', 'p198', 'p200)']} {'t': 0.25, 'outcome': 'NONE', 'xdep': []}
   Random/Lydia/case_03_50/04.tlsf {'t': 0.007, 'outcome': 'HIT', 'xdep': ['p149   (of p149', 'p170', 'p172)']} {'t': 0.007, 'outcome': 'NONE', 'xdep': []}
   Random/Lydia/case_07_50/18.tlsf {'t': 0.057, 'outcome': 'HIT', 'xdep': ['p191   (of p121', 'p139', 'p145', 'p148', 'p162', 'p171', 'p191)']} {'t': 0.101, 'outcome': 'NONE', 'xdep': []}
   Random/Lydia/case_04_50/11.tlsf {'t': 0.007, 'outcome': 'HIT', 'xdep': ['p166   (of p101', 'p113', 'p150', 'p166', 'p175', 'p178', 'p181)']} {'t': 0.008, 'outcome': 'NONE', 'xdep': []}

## deps tier 2 — 800 rows

### tier 2, `--direction out`

| category | in tier | analysed | HIT | NONE | TIMEOUT | MEMOUT | EMPTYLANG | ERR | ARGV_TOO_LONG | NOT_REACHED | hit rate |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Patterns | 8 | 0 | 0 | 0 | 6 | 1 | 0 | 0 | 0 | 1 | 0/0 — *not a rate* |
| Random | 852 | 214 | 1 | 213 | 228 | 3 | 73 | 0 | 0 | 334 | 1/214 = **0.5 %** |
| Two-player-Game | 427 | 25 | 0 | 25 | 138 | 0 | 0 | 0 | 112 | 152 | 0/25 = **0.0 %** |
| TOTAL | 1287 | 239 | 1 | 238 | 372 | 4 | 73 | 0 | 112 | 487 | 1/239 = **0.4 %** |

### tier 2, `--direction in`

| category | in tier | analysed | HIT | NONE | TIMEOUT | MEMOUT | EMPTYLANG | ERR | ARGV_TOO_LONG | NOT_REACHED | hit rate |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Patterns | 8 | 1 | 0 | 1 | 6 | 0 | 0 | 0 | 0 | 1 | 0/1 = **0.0 %** |
| Random | 852 | 251 | 0 | 251 | 245 | 5 | 17 | 0 | 0 | 334 | 0/251 = **0.0 %** |
| Two-player-Game | 427 | 26 | 0 | 26 | 137 | 0 | 0 | 0 | 112 | 152 | 0/26 = **0.0 %** |
| TOTAL | 1287 | 278 | 0 | 278 | 388 | 5 | 17 | 0 | 112 | 487 | 0/278 = **0.0 %** |

HITS: 1
   Random/Lydia/case_07_50/20.tlsf {'t': 0.01, 'outcome': 'HIT', 'xdep': ['p182   (of p103', 'p106', 'p115', 'p123', 'p124', 'p128', 'p131', 'p159', 'p170', 'p182', 'p185', 'p192)']} {'t': 0.011, 'outcome': 'NONE', 'xdep': []}

## ltlfsynt cost profile — 1717 instances analysed

| category | analysed | REALIZABLE | UNREALIZABLE | TIMEOUT | MEMOUT | ERR |
|---|---|---|---|---|---|---|
| Patterns | 40 | 19 | 21 | 0 | 0 | 0 |
| Random | 1237 | 273 | 874 | 90 | 0 | 0 |
| Two-player-Game | 440 | 79 | 30 | 259 | 72 | 0 |
| TOTAL | 1717 | 371 | 925 | 349 | 72 | 0 |

### Mealy solve-time buckets (solved instances only)

| category | <0.01 s | 0.01-0.1 s | 0.1-1 s | 1-10 s | 10-20 s |
|---|---|---|---|---|---|
| Patterns | 40 | 0 | 0 | 0 | 0 |
| Random | 855 | 176 | 53 | 50 | 13 |
| Two-player-Game | 20 | 44 | 24 | 16 | 5 |
| TOTAL | 915 | 220 | 77 | 66 | 18 |

### B3 — Moore/Mealy divergence

| category | analysed | comparable | agree | differ |
|---|---|---|---|---|
| Patterns | 40 | 40 | 40 | 0 |
| Random | 1237 | 1147 | 1147 | 0 |
| Two-player-Game | 440 | 90 | 79 | 11 |
| TOTAL | 1717 | 1277 | 1266 | 11 |

DIVERGENCES (11):
  Two-player-Game/Nim/nim_03/System-first/nim_pb_03_02_pe_.tlsf (21 APs) — Moore: UNREALIZABLE, Mealy: REALIZABLE
  Two-player-Game/Nim/nim_03/System-first/nim_pb_03_03_pe_.tlsf (26 APs) — Moore: UNREALIZABLE, Mealy: REALIZABLE
  Two-player-Game/Nim/nim_03/System-first/nim_pb_03_04_pe_.tlsf (31 APs) — Moore: UNREALIZABLE, Mealy: REALIZABLE
  Two-player-Game/Nim/nim_03/System-first/nim_pb_03_05_pe_.tlsf (36 APs) — Moore: UNREALIZABLE, Mealy: REALIZABLE
  Two-player-Game/Nim/nim_03/System-first/nim_pb_03_06_pe_.tlsf (41 APs) — Moore: UNREALIZABLE, Mealy: REALIZABLE
  Two-player-Game/Nim/nim_03/System-first/nim_pb_03_07_pe_.tlsf (46 APs) — Moore: UNREALIZABLE, Mealy: REALIZABLE
  Two-player-Game/Nim/nim_03/System-first/nim_pb_03_08_pe_.tlsf (51 APs) — Moore: UNREALIZABLE, Mealy: REALIZABLE
  Two-player-Game/Nim/nim_04/System-first/nim_pb_04_01_pe_.tlsf (20 APs) — Moore: UNREALIZABLE, Mealy: REALIZABLE
  Two-player-Game/Nim/nim_05/System-first/nim_pb_05_02_pe_.tlsf (31 APs) — Moore: UNREALIZABLE, Mealy: REALIZABLE
  Two-player-Game/Nim/nim_06/System-first/nim_pb_06_01_pe_.tlsf (28 APs) — Moore: UNREALIZABLE, Mealy: REALIZABLE
  Two-player-Game/Nim/nim_08/System-first/nim_pb_08_01_pe_.tlsf (36 APs) — Moore: UNREALIZABLE, Mealy: REALIZABLE

### raceable band (Mealy, solved in 0.1-20 s)

| category | solved | of which >= 0.1 s | >= 1 s |
|---|---|---|---|
| Patterns | 40 | 0 | 0 |
| Random | 1147 | 116 | 63 |
| Two-player-Game | 109 | 45 | 21 |
| TOTAL | 1296 | 161 | 84 |

top-20 slowest solved (Mealy):
    19.98 s  Random/Syft/syft_4/057.tlsf                                             24 APs  UNREALIZABLE
    18.91 s  Random/Syft/syft_2/084.tlsf                                             23 APs  UNREALIZABLE
    17.94 s  Random/Syft/syft_2/060.tlsf                                             24 APs  UNREALIZABLE
    16.67 s  Two-player-Game/Nim/nim_05/System-first/nim_pb_05_05_pe_.tlsf           52 APs  REALIZABLE
    15.60 s  Two-player-Game/Single-Counter/System-first/counter_pb_16_pe_.tlsf      49 APs  REALIZABLE
    14.52 s  Random/Lydia/case_08_50/24.tlsf                                         16 APs  REALIZABLE
    13.91 s  Random/Syft/syft_2/200.tlsf                                             23 APs  UNREALIZABLE
    13.25 s  Random/Syft/syft_2/177.tlsf                                             24 APs  UNREALIZABLE
    13.03 s  Random/Lydia/case_08_50/08.tlsf                                         16 APs  REALIZABLE
    12.93 s  Two-player-Game/Double-Counter/System-first/countersDouble_pb_17_pe_.tlsf  87 APs  REALIZABLE
    12.28 s  Two-player-Game/Nim/nim_03/System-first/nim_pb_03_20_pe_.tlsf          111 APs  REALIZABLE
    12.06 s  Random/Lydia/case_05_50/34.tlsf                                         19 APs  UNREALIZABLE
    11.43 s  Random/Lydia/case_05_50/23.tlsf                                         20 APs  UNREALIZABLE
    11.14 s  Random/Syft/syft_2/019.tlsf                                             22 APs  UNREALIZABLE
    10.95 s  Two-player-Game/Nim/nim_04/System-first/nim_pb_04_07_pe_.tlsf           56 APs  UNREALIZABLE
    10.92 s  Random/Lydia/case_05_50/37.tlsf                                         19 APs  UNREALIZABLE
    10.76 s  Random/Syft/syft_4/142.tlsf                                             20 APs  UNREALIZABLE
    10.25 s  Random/Syft/syft_2/115.tlsf                                             22 APs  UNREALIZABLE
     9.81 s  Random/Syft/syft_2/049.tlsf                                             23 APs  UNREALIZABLE
     9.36 s  Two-player-Game/Nim/nim_03/System-first/nim_pb_03_19_pe_.tlsf          106 APs  REALIZABLE

## Step 3 — Q8's certificate, exercised for the first time

For each of the 7 hits — both tiers — the extractor was re-run to
materialise the transducer and the refined partition, the EK method
(`--otf-mtdfa-product`) was run on the **original** $\varphi$ with it, and the result handed to
`verify_controller` (`ltlf-ek-synth --model-check`).

**The certificate could not be exercised as a safety check: 7/7 came back
`UNREALIZABLE`**, so no controller was produced and there was nothing to
model-check — 0 `SAFE`, 0 `UNSAFE`.

That is not a failure of the certificate; it is a property of the population.
But it means the *retention rate* Q8 asks for is, on this corpus,
**0 certified controllers out of 7 hits**, and the certificate's first real
exercise still lies ahead.

Because the model-check was vacuous, the `UNREALIZABLE` verdict itself became
the thing to corroborate — an EK-side bug could manufacture a spurious
`UNREALIZABLE` and hide behind it. All 7 were therefore re-run three ways:

| instance | APs | $\Xdep$ | EK | `ltlfsynt` Mealy | `ltlfsynt` Moore | ours-no-knowledge | agree |
|---|---|---|---|---|---|---|---|
| `Random/Lydia/case_05_50/09.tlsf` | 10 | `p159` | UNREALIZABLE | UNREALIZABLE | UNREALIZABLE | UNREALIZABLE | yes |
| `Random/Lydia/case_06_50/38.tlsf` | 12 | `p158` | UNREALIZABLE | UNREALIZABLE | UNREALIZABLE | UNREALIZABLE | yes |
| `Random/Lydia/case_06_50/06.tlsf` | 12 | `p182, p200` | UNREALIZABLE | UNREALIZABLE | UNREALIZABLE | UNREALIZABLE | yes |
| `Random/Lydia/case_03_50/04.tlsf` | 6 | `p149` | UNREALIZABLE | UNREALIZABLE | UNREALIZABLE | UNREALIZABLE | yes |
| `Random/Lydia/case_07_50/18.tlsf` | 14 | `p191` | UNREALIZABLE | UNREALIZABLE | UNREALIZABLE | UNREALIZABLE | yes |
| `Random/Lydia/case_04_50/11.tlsf` | 14 | `p166` | UNREALIZABLE | UNREALIZABLE | UNREALIZABLE | UNREALIZABLE | yes |
| `Random/Lydia/case_07_50/20.tlsf` | 26 | `p182` | UNREALIZABLE | UNREALIZABLE | UNREALIZABLE | UNREALIZABLE | yes |

**7/7 agree three ways**, Moore agreeing with Mealy on all 7 as well. The
`UNREALIZABLE` is a property of the specifications, not an artifact of the EK
path — no O5-class theory finding, nothing to stop on.

The $\Xdep$ column is the authoritative parse, cross-checked against the
partition file the extractor itself emits (`xdep_from_part` in the raw JSON;
identical on all 7). The `xdep` field on the tier-1/tier-2 rows is the sweep's
inline parse, which swallows the extractor's `(of ...)` roster — read
`certificates`, not `deps_tier1`, for variable names.

## Step 2 — what the `ltlfsynt` profile says about raceability

**The profile is complete: all 1717 instances, both semantics, zero
`NOT_REACHED`.** 1296 were solved by `ltlfsynt` (Mealy) inside the 20 s / 2 GiB
budget; **421 were not** (349 `TIMEOUT`, 72 `MEMOUT`).

The number that matters for idea 2 is the **raceable band** — instances slow
enough that a speed-up could be visible at all, but not so fast that timing
noise swamps it. Taking 0.1 s as that floor:

- **161 of 1296 solved instances (12.4 %) take ≥ 0.1 s**;
- **84 (6.5 %) take ≥ 1 s**;
- the slowest solved instance takes 20.0 s, and **421 instances are not solved at
  all** inside the budget.

The corpus is therefore sharply bimodal: 88 % of everything `ltlfsynt` can
solve, it solves in under 0.1 s, and most of the remainder it cannot solve at
all. The usable middle is **161 instances**.

### The two steps, crossed — and this is the finding

Steps 1 and 2 answer separately, but the useful statement is their
intersection, which was computed rather than argued:

> **hits ∩ raceable band = ∅**

All 7 dependency-carrying instances are decided by `ltlfsynt` in **≤ 9 ms**
(slowest 0.009 s); not one falls in the 161-instance raceable band, and every
instance in that band has $\Xdep = \emptyset$.

That is stronger than the hit rate alone, and it is the sentence Wednesday
should carry into Q9: **among the 627 instances whose $\Xdep$ is known, there is
none on which an extracted transducer could demonstrate a speed-up** — because
where the dependencies are the problem is already trivial, and where the problem
is hard there are no dependencies.

The scope of that claim is exactly the analysed set (388 in tier 1 plus 239 in
tier 2, `--direction out`), not all of `tlsf-fin`: the 372 tier-2 `TIMEOUT`s,
4 `MEMOUT`s and 487 `NOT_REACHED` are instances whose $\Xdep$ nobody knows. What
can be said about those is that they are exactly the instances the *extractor*
cannot handle, so they are not reachable by arm (a) either way.

## B3 — the Moore/Mealy divergence is real, and it is not noise

Every number in this report is under **Mealy** semantics; every `tlsf-fin` file
declares `SEMANTICS: Finite,Moore`. Per B3 the `ltlfsynt`-Mealy verdict is the
oracle, and both semantics were run on every instance so the divergence could be
counted rather than assumed away.

**11 of 1277 comparable instances diverge** (0.9 %), and the divergence is
perfectly structured rather than scattered:

- every one is in `Two-player-Game/Nim/*/System-first/*_pe_`;
- every one is **Moore `UNREALIZABLE`, Mealy `REALIZABLE`** — never the reverse.

That is the expected direction — a Mealy system sees the current input before
committing its output, so it is strictly the more powerful player — which is
what makes it credible rather than alarming. It does mean the turn-order hazard
`include/ltlf_ek/turn_order.hpp` warns about is **not hypothetical on this
corpus**: on these 11 instances, reading a Mealy verdict as a Moore one flips
realizability. They are listed in full above, with both verdicts on every row of
the raw JSON.

No Moore mode was built — that is a construction change to a settled part of the
repo, and not a day-run's call (B3).

## Blockers and deviations

- **No new blockers, and nothing guessed.** B1, B2 and B3 were executed as the
  user decided them; none was re-opened.
- **Tier 2 is truncated by the deadline, by design.** 800 of 1287 instances were
  reached; the seeded shuffle makes those an unbiased random subsample, and the
  remaining 487 are the `NOT_REACHED` bucket, excluded from every denominator.
  Resuming appends to the same JSONL.
- **Tier-2 concurrency was raised from 2 workers to 3** once the `ltlfsynt`
  profile finished and released its 4 GiB, holding Σ(memory caps) at 12 GiB per
  B2. Per-instance budgets (30 s, 4 GiB) were unchanged, and tier 2 is a census
  of outcomes rather than a timing measurement, so this does not mix
  populations.
- **One fix inside the run:** the three-way cross-check first invoked
  `ltlf-ek-synth --realizability`, which does not exist (the flag is
  `--realizable`); it returned a 2 ms usage error that read as a verdict
  disagreement. Corrected and re-run — the table above is the corrected run.
  **No sweep data was affected**, as the sweep itself never calls that flag.
- **Restarted after the morning session ended.** Wave 1 launched both sweeps and
  lost them when its session closed: detached processes do not survive a session
  here, so a sweep has to run as a tracked background task. Both were restarted
  from their JSONL, with no data lost and no completed instance re-measured.

## What Wednesday's grill (Q9) now has

- Arm (a)'s deliverable question is answerable with a **pre-registered,
  corpus-wide, per-category null**: 1.5 % out and 0.0 % in on **430/430** of
  tier 1, with `MEMOUT`/`TIMEOUT`/`EMPTYLANG` reported beside it rather than
  folded in. Q9's recommendation — commit in advance that a low number ships
  as-is — is discharged.
- **The stronger claim is available and verified:** hits and raceable instances
  are disjoint on this corpus, so arm (a) cannot produce a race here however the
  extractor is tuned. That is a positive argument for the engineered-domain
  route (idea 1), not merely an absence of evidence.
- **Q8's certificate is built and runs, but has never been exercised** — no hit
  ever produced a controller. Wednesday's slippery-world probe is its first real
  test, which raises the value of probe item 2.
- **A concrete turn-order datum:** 11 `Nim/System-first` instances flip verdict
  between Moore and Mealy, all in the same direction.
- **The `syfco` question (the 25 real-TLSF files) remains open and untouched**,
  as the plan said it should.
