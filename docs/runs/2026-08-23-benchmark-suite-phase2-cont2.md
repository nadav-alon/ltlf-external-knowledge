# Day-run 2026-08-23 — `benchmark-suite` Phase 2 (cont. II): per-case process isolation

**Step 1 of `docs/plans/2026-08-23-sunday.md`.** The day's blocking dependency:
steps 2 and 3 are both downstream of this runner working.

**Status: landed.** Branch `bench-runner-isolation`, draft PR against `master`.
`master` untouched, per the plan's Stop-list 4 — the merge is the user's.

## What the phase repairs

`RunCaseWithTimeout` bounded a case by **detaching** its worker thread on a
deadline miss. The detached worker is still inside Spot/MONA at process exit, so
**any** timing-out case killed the run with SIGSEGV *before* `--out` was written
— not a degraded report, *no report*. That contradicts the PRD's own Stop-list-8
contract that a timing-out row is recorded and the other families continue.

It is now a **forked child per case**: `fork()`, the child runs the case and
serialises its `(key, value)` rows over a pipe and always ends in `_exit()`; the
parent `poll()`s the read end against the deadline, reads to EOF, and reaps on
every path — `waitpid` on a clean EOF, `SIGKILL` then a blocking `waitpid` on a
miss. A blocking Spot call cannot be cancelled in-thread, so the deadline is now
enforced on something the OS can actually kill.

`TimedRunResult` carries a three-way `RunOutcome` (`kSuccess` / `kTimedOut` /
`kFailed`) with a `failure_detail`; the sweep emits `FAILED_<detail>` and
`PARTIAL_FAILED_<k>_of_<K>` marker rows beside the pre-existing `TIMEOUT` /
`PARTIAL_TIMEOUT_<k>_of_<K>`. Same `TimingRow` shape, so the JSON schema,
`scripts/bench_xlsx_export.py` and every other consumer are unchanged.

## What landed

| File | Change |
| --- | --- |
| `src/ltlf_ek_bench.cpp` | the fork/pipe/poll/reap rewrite (+319/−52), the three-way outcome, the marker rows, the fault-injection hook |
| `src/bench_suite.cpp` | one stale comment (`mona_available()` recomputes per case now, not per sweep) |
| `tests/bench_runner_isolation_test.cpp` | new, 6 cases |
| `CMakeLists.txt` | wires the new test file |
| `docs/prd/benchmark-suite.md` | phase entry, as-built developer comments, gate evidence, the open-findings list |

**This run resumed in-flight work.** The rewrite itself was written and verified
earlier the same day (see the PRD's as-built comment) but was left uncommitted
when that session hit a spend limit at 07:46. This run verified it independently,
took it through both review halves, and landed it.

## Gates

- **tests** — `/test-writer`, two rounds. 6 cases in
  `tests/bench_runner_isolation_test.cpp`. Beyond the phase's green checkpoint,
  the `kFailed` paths are now driven end-to-end through the fault-injection hook,
  each asserting **B2 rule 1** (a crashed cell yields a *row* with metrics
  **absent**, never zeros) while the other cells in the same sweep stay real. The
  orphan test was verified to actually fail when the kill-and-reap path is
  removed.
- **code-review** — domain (`/code-reviewer`) ran twice: round 1 on the rewrite
  (2 must-fix, 8 consider), round 2 on the fix — **clean**. Generic half via
  `/review` on the PR; see below.
- **theory-review** — **not spawned, deliberately.** Process-isolation plumbing
  around an already-frozen `run_bench_case`: no method, no `cons`, no
  progression/product/final-state logic, no `Synthesis`/`Transducer` contract.
  Both review rounds agreed. The gate stays open for Phase 3, where the
  comparability tier lands.
- **glossary** — nothing owed. Every new identifier (`RunOutcome`,
  `SerializeRows`, `WriteAll`, `failure_detail`, `MaybeInjectFault`) lives in the
  anonymous namespace of `src/ltlf_ek_bench.cpp`; none is public and none names a
  domain concept.

## Green checkpoint

`cmake --build build -j` clean. `ctest --test-dir build -j8`: **699/700**, run
independently by this session after the fix round, not merely taken on report.

The single red is `BenchSuiteCrossMethodAgreement.AllFiveMethodsAgreeWithEachOtherAndTheDeclaredVerdict`
on `parity-t3` at $n = 2, 3$ — known-open, all five methods agree with *each
other* and only the family's declared `expected_realizable = true` dissents, so
it is a wrong **declaration**, not a method divergence. Untouched, per the plan's
Stop-list 1 and `benchmark-suite.md` Stop-list 4. **It is expected to be red all
day and is not a regression from this work.**

## Review findings acted on

Round 1's two must-fixes:

1. **The `kFailed` path was entirely untested** — the phase's own motivating
   path. Closed with three new cases through the hook.
2. **The "no zombies" test was vacuous**: it grepped `ps` *after* the bench
   process had exited, by which point any unreaped child had been reparented to
   init and reaped by it — it could not fail even with `waitpid` deleted. Replaced
   with an orphaned-**running**-descendant check sampled while the parent is
   alive.

Three findings were **promoted from *consider* to must-fix by the launcher**,
because each silently corrupts the sweep this phase exists to make survivable:

3. `pipe()` had no `O_CLOEXEC`, so a `mona` grandchild inherited the write end,
   the parent never saw EOF, and a **successful** case was reported `TIMEOUT`
   with its rows discarded. → `pipe2(…, O_CLOEXEC)`.
4. `kill(-pid, …)` relied on a process group whose creation was never checked; if
   both `setpgid` calls failed, nothing was signalled and the blocking `waitpid`
   hung the sweep unbounded — reopening this phase's own failure class. → a
   direct `kill(pid, SIGKILL)` alongside the group kill.
5. The `poll` timeout was `static_cast<int>` with no clamp, so a large
   `--timeout` narrowed to a negative int and `poll` waited **forever**. → clamped
   to `INT_MAX`.

Round 2 re-reviewed only the fix and returned **clean**.

## Findings deferred — recorded, not fixed

All *consider*-level, listed in full in the PRD's *Developer comments*. The ones
worth the user's eye:

- **Ctrl-C on an interactive sweep no longer reaches the child.** `setpgid` moves
  it out of the terminal's foreground group, and the parent installs no
  `SIGINT`/`SIGTERM` handler, so the child and its MONA grandchild survive and
  keep burning a core. Harmless for scripted runs; promote it if sweeps are ever
  driven by hand.
- **`waitpid` is not `EINTR`-retried and `status` starts at 0**, which
  `WIFEXITED`/`WEXITSTATUS` read as a clean exit 0 — an interrupted wait would
  report a *killed* child as a success. Unreachable today (no handler exists
  anywhere in the repo), but it is the one silent-wrong-answer path in the diff,
  and installing the handler above makes it reachable.
- **The fault-injection hook is silent when live.** A stale exported
  `LTLF_EK_BENCH_FAULT_INJECT` would turn a real sweep into `FAILED_` markers
  indistinguishable from genuine crashes, and the JSON records neither.
- **`ParseInt` accepts negatives**, so `--timeout=-1` wraps to ~136 years. The
  clamp removes the hang; the deadline is still effectively unbounded.
- Cosmetic: marker-row detail strings carry spaces and parens
  (`FAILED_pipe() failed`) where every other stage value is identifier-shaped;
  `_exit()` discards the child's buffered `std::cout`; two stale test comments.

## Measurement-semantics change, stated because it is systematic

Repeats are now **cold processes**, so a min-of-K over repeats is a *cold-start*
minimum where it previously benefited from warm Spot/BuDDy caches across repeats
inside one process. This does **not** touch the structural (count) rows — those
were checked byte-identical between the pre-change and post-fix binaries — but it
does mean Phase 2 (cont. II) timings are not directly comparable to timings taken
before it. Anything that compares against pre-2026-08-23 numbers should say so.

## Questions for the evening grill

1. **Is the interactive-sweep signal path worth closing?** Installing a
   `SIGINT`/`SIGTERM` handler that tears down the live child group would also make
   the `EINTR`/`status` item above reachable, so the two are one decision, not two.
   If every sweep is script-driven, both stay closed as documented.
2. **Should the fault-injection hook ship at all, or be compiled out of Release?**
   It is off by default and cannot fire accidentally, but it is production surface
   that exists only for a test.
3. **Do the pre-2026-08-23 timings in `docs/runs/2026-08-11-benchmarks.xlsx` need
   a re-run** under the cold-process runner before anything is compared across
   that boundary, or is the structural axis the only one that carries?

## Budget

Phases run: 1. Build/test repair rounds: 0 (green on arrival, green after the
fix). Review rounds: 2 of a cap of 2 — round 2 was clean, so the cap was reached
but not exceeded. Agents spawned: 2 review, 1 `developer`, 1 `test-writer`.
