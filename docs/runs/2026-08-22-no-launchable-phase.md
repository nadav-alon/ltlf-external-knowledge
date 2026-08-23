# Day-run 2026-08-22 (wave 1, slot 1) — no launchable phase

**Verdict: `DONE`.** No phase was started. Every phase reachable by the
launcher's Step-0 rules is held on a decision only the user can make, and all of
those decisions were already raised in writing on 2026-08-20 / 2026-08-21 and
are still unanswered. Nothing was committed to a feature branch, nothing was
half-landed, `master` is untouched apart from this report.

Deadline check: started 10:08, `LTLF_EK_RUN_DEADLINE` 15:07 — time was not the
constraint.

## 1. What was surveyed

`master` is at `837dcc6` (2026-08-19 20:38). **No commit anywhere in the repo is
newer than `edf-phase3`'s `e7bbfb1` at 2026-08-21 13:31**, i.e. the tip of
yesterday's own day-run. There was no evening session on Friday 2026-08-21, so
the three questions that run left open are exactly as it left them.

### Step-0 rule 2 — the backlog's **Now / next**

| Item | PRD on `master`? | Launchable? |
| --- | --- | --- |
| `#1` untangle branch spaghetti | no (deliberately) | no — user's evening; code half already closed 2026-08-18 |
| `#2` re-grill the benchmark families | **yes, now**: `docs/prd/engineered-domain-families.md` (`b223684`, glossary gate closed `837dcc6`) | **phases 1–2 landed; 3 blocked; 4 depends on 3** — see §2 |
| `#3` parametric benchmark suite | yes: `docs/prd/benchmark-suite.md` | no — Phase 3 fails launch gate 5, see §3 |
| `#4` input-dependency gallery | no | no |
| Method 3.2 (`otfagg`) | no | no — blocked on a *proven* theory finding (state-keyed `F_P` over-accepts) |
| Method 3.1 Phase 2 (`otf_solve_fused`) | spec'd in `otf-mtdfa-product.md` | no — the backlog says in as many words it **needs its own grill** first |

**Backlog drift worth fixing tonight:** `#2` still reads *"No PRD, deliberately
— this is the grill that would produce one."* That grill **happened** on
2026-08-19 and produced `engineered-domain-families.md`, which is on `master`
and marked unattended-ready. Rule 2 read literally would skip `#2` and land on
`#3`; I followed the PRD's actual state instead, and both paths end in the same
place, so nothing turned on it. But a future run should not have to notice this.

### Step-0 rule 3 — a PRD that exists only on a branch

None. The scan over all nine unmerged branches returns nothing: no branch adds a
`docs/prd/` file that `master` lacks. `edf-phase1/2/3` carry *implementation* of
a PRD that is already on `master`, which is rule 2's business, not rule 3's.

## 2. `engineered-domain-families` — the live PRD, and why it cannot move

Phases 1 and 2 are **landed** on the feature branch `edf-phase1` (`f8e4ec0`,
pushed, draft **PR #13**). Phase 2's headline result stands: the certificate is
green on every landed T1 family's declared `psi_in`, closing `benchmark-suite`'s
B3 hole.

**Phase 3 stopped on Stop-list 1** yesterday and is unrepairable without the
user. The compact arithmetic `A_N` is implemented and compiles, but the Phase 2
certificate reports it **not equivalent** to `T_in` on non-empty words — witness
a length-1 stutter at `(0,0)`, at n = 2,3,4, both goals. The cause is in **D5's
pinned math, not the implementation**: under weak `X`, `(X b_i <-> rhs_i)`
collapses to `rhs_i` rather than to true at the final position, so `Keep` demands
every bit set at the end. Fixing it means editing D5 — the user's own bespoke
construction — and Stop-list 1 explicitly forbids repairing `A_N`. A fresh
session hits this same wall in the same place.

**Phase 4 depends on Phase 3.** Its green checkpoint requires a committed report
covering **three arms**, and arm 3 is the compact family. Running the sweep now
would publish a headline separation claim over a construction known to be wrong,
which is worse than not running it.

So the PRD has no launchable phase, not because it is finished, but because its
next phase is parked on the user.

## 3. `benchmark-suite` Phase 3 — fails launch gate 5

Gate 5 is *"the PRD's stop-list does not name a condition that already holds."*
`parity-t3`'s declared `expected_realizable = true` is known-wrong — all five
methods agree on `false`, so only the declaration dissents — and the PRD's own
header says it verbatim: **"Phase 3 must not bake this cell into the structural
baseline while it stands."** Flipping the declaration is Stop-list 4, an
O5-class theory finding, explicitly not a benchmark repair.

Second, independent hold: Phase 3 commits a **cell-exact** structural baseline,
and the family set is in active flux right now — `engineered-domain-families` is
mid-flight adding `slippery-binary`, `slippery-onehot` and (blocked)
`slippery-binary-compact`. A baseline cut today is stale by construction.

Per the skill, a failed launch gate is not a reason to go find different work,
so I did not fall through to another candidate; I recorded it here.

## 4. What this run deliberately did **not** do

- **Did not touch D5 or `A_N`.** Stop-list 1, and it is the user's math.
- **Did not run Phase 4 on two arms.** The checkpoint says three; shipping a
  separation claim with a known-broken arm is the failure mode the PRD exists
  to avoid.
- **Did not pick up the two open Phase-2 review findings** in
  `src/produced_trace_equivalence.cpp` (eager `2^|AP|` letter enumeration —
  `1 << k` is UB for k >= 64 and it, not `ltlf_to_dfa`, causes one-hot's
  `bad_alloc` at n = 4 and the new ~220 s `ctest` cost; plus the dead
  `tau_dfa->ap()` loop). They are real, and they are *cheap* now that no Phase 3
  work is in flight — but yesterday's report asked the user to **sequence** them
  (§5.3) and got no answer, and they are not a PRD phase. Picking them up
  unattended would be answering the question that was asked.
- **Did not flip `parity-t3`'s declaration**, did not merge `edf-phase3` (red),
  did not tick any gate, did not open or edit a PR, did not write a PRD for
  `#1`/`#2`/`#4`, did not start the ungrilled Method 3.1 Phase 2.
- **Did not update `docs/BACKLOG.md`** beyond flagging the `#2` drift here —
  the backlog is the user's intention log, not a run's to rewrite.

## 5. Questions for the evening grill

Ranked by what unblocks the most. **All four are carried forward from
2026-08-21 unchanged** — this run added no new ones, which is itself the signal
that the day had nothing to do.

1. **How should D5 handle the trace boundary?** *(unblocks Phase 3, then 4 —
   the whole PRD)* Two shapes are on the table: **(1)** wrap each axis's update
   body under one outer `X(...)`, mirroring the enumerated arms — proven
   precedent in this very PRD, but not a bracket move, since the RHS refers to
   current bit values and folding those into the guard by enumeration is exactly
   the compactness the arm exists to avoid; **(2)** an explicit boundary escape
   hatch making each rule vacuous at the last position, keeping the ripple shape
   verbatim at one extra literal per rule. Yesterday's run leaned (2) on blast
   radius. This is bespoke math you wrote; a run must not invent option (3).
2. **Is the conjunct count `15`, or `14 + 2n`?** Measured `14 + 2n`
   (18/20/22/24/26 for n = 2..6) — the init conjunct is `2n` literals Spot
   flattens. Purely a PRD-text correction, but T9 gates the **separation
   claim's** DFA-size measurement behind the asserted-exact number, so the
   PRD's headline result is currently *unmeasured, not disproven*.
3. **Sequence the two open `produced_trace_equivalence.cpp` findings.** Now
   strictly cheaper to say yes to than yesterday: there is no in-flight Phase 3
   work for a fix to collide with, and one of the two is a genuine UB.
4. **Does the boundary bug have a sibling in the enumerated arms?** Probably
   not — they wrap one `X` around a whole cube, the pattern that works, and
   their certificate is green over 24 cases. But the two arms were argued
   correct by *different* reasoning and only one has now been tested to
   destruction. Worth a look before Phase 4 leans on either.

Also worth a minute tonight, cheap and not blocking: **repoint `#2` in the
backlog at its PRD** (§1), and **decide whether `parity-t3`'s declaration gets
flipped** — it is the sole blocker left on `benchmark-suite` Phase 3 now that
`#1`'s code half is closed, and it has been failing one `ctest` cell since
2026-08-18.

## 6. Budget used

Phases run: **0**. Repair rounds: 0 of 2. Review rounds: 0 of 2. No agent was
spawned, no build or `ctest` was run — there was nothing to build. Cost was the
Step-0 survey only.

## 7. Status line

```
DONE no launchable phase: engineered-domain-families Ph3 held on Stop-list 1 (D5 boundary math, user's), Ph4 depends on it; benchmark-suite Ph3 fails launch gate 5 (parity-t3 declaration); #1/#2/#4 and Method 3.1 Ph2 have no grilled PRD
```
