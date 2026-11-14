# `--fix-dependent-outputs` race, 2026-11-14

Issue #250 (part of #243). Raw cells: `2026-11-14-fix-dependent-outputs-race.json`, produced by
`python3 scripts/hard_race.py --timeout 60 --n-max 20` over the smallest hard instances
(mux 16..20, shift 20, TwoCountersDisButAC), 8 GiB per cell, one repetition, one host.
Experimental flag; `def:outdep` is defined in `main.tex`, but no lemma says that substituting `X_dep := lambda` preserves realizability, so none of this is paper material.

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
| mux-16 | 0.84 s | 0.01 s | 1.22 s | 0.12 s | 0.19 s |
| shift-20 | 19.3 s | timeout (60 s) | 28.6 s | 9.7 s | timeout |
| TwoCountersDisButAC | 0.65 s | 0.01 s | | | 16.24 s (UNREALIZABLE) |

- **mux**: the flag makes EK 7x faster than NK at n=16, but `ltlfsynt` answers in 0.01 s, so the speedup over the
  faster of the two is 0.05x. mux n=17..20 have no derived Tout and no compact one (compact exists only at powers of 2).
- **shift**: compact-fixed beats NK 1.99x (19.3 / 9.7 = 1.986x) and `ltlfsynt` timed out, so the
  speedup over both is at least 1.99x counting the timeout as 60 s; short of the bar by a hair. This is the only
  evidence close to the bar, and its controller is unverifiable (above).
- The derived arm gains nothing on shift-20 (timeout both ways: extraction alone takes 17 s).

Not met on the n<=20 smoke: one instance per family (mux-16, shift-20), one repetition, a 60 s cap, and an
unverified controller for the one near-miss. Mux 17..64 and shift 21..40 were not raced, so whether this closes
option 1 of #243 is the developer's call, deferred to the full grid of #283, which will include the fixed arms.
