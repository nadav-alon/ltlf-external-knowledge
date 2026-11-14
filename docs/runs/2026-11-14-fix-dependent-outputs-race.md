# `--fix-dependent-outputs` race, 2026-11-14

Issue #250 (part of #243). Raw cells: `2026-11-14-fix-dependent-outputs-race.json`, produced by
`python3 scripts/hard_race.py --timeout 60 --n-max 20` over the smallest hard instances
(mux 16..20, shift 20, TwoCountersDisButAC), 8 GiB per cell, one repetition, one host.
Experimental flag; no soundness lemma exists (`def:outdep` is a placeholder), so none of this is paper material.

## Verdict and controller checks

- **0 verdict mismatches**: every finished arm, fixed or not, agrees with NK and `ltlfsynt` on every instance.
- **0 unsafe controllers.** `mux-16` (derived and compact Tout) verifies SAFE under `--model-check`.
- `shift-20` compact-fixed finishes REALIZABLE in 9.7 s but its controller is **unverifiable**: `verify_controller`
  runs out of memory (`std::bad_alloc`, 8 GiB) after about 42 s. It does the same on the controller of the plain
  compact arm, so this is the verifier's limit, not the flag's.

## Bar of #243: not met

The bar: >= 2x faster than both NK and `ltlfsynt` on at least one hard family, 0 mismatches.

| instance | NK | ltlfsynt | compact | compact-fixed | derived-fixed |
|---|---|---|---|---|---|
| mux-16 | 0.86 s | 0.01 s | 1.25 s | 0.12 s | 0.19 s |
| shift-20 | 19.3 s | timeout (60 s) | 28.5 s | 9.7 s | timeout |
| TwoCountersDisButAC | 0.64 s | 0.01 s | | | 16.5 s (UNREALIZABLE) |

- **mux**: the flag makes EK 7x faster than NK at n=16, but `ltlfsynt` answers in 0.01 s, so the speedup over the
  faster of the two is 0.05x. mux n=17..20 have no derived Tout and no compact one (compact exists only at powers of 2).
- **shift**: compact-fixed beats NK 2.0x (19.3 / 9.7 = 1.986x, just under) and `ltlfsynt` timed out, so the
  speedup over both is at least 1.99x counting the timeout as 60 s; short of the bar by a hair, and not verified.
- The derived arm gains nothing on shift-20 (timeout both ways: extraction alone takes 17 s).

Per #243's rule a miss is a negative result: the theory work (`def:outdep` soundness lemma) is not warranted by this prototype.
The range n>=21 for shift and n>=32 for mux was not raced (60 s cells); a longer run could move shift past 2x, since NK grows faster.
