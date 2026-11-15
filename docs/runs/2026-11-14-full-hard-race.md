# Full hard-instance race, 2026-11-14

Issue #283 (part of #243). Raw cells: `2026-11-14-full-hard-race.json`, produced by `scripts/hard_race.py` with its
defaults (120 s per cell, 8 GiB per process) over all 71 instances in `tests/data/hard-instances`: mux 16..64,
shift 20..40, TwoCountersDisButAC. All six arms, including #250's fixed arms. One repetition.

Provenance: HPC, SLURM array 1275437 (one instance per task, 2 CPUs, 16G), image job 1275436, Xeon Gold 6130 on
cn01, cn02, cn11, cn13–cn19. The image was built from `021a915`, which is master `e9b3c51` plus the image and
sbatch changes on this branch (`scripts/hpc/ltlf-ek.def` now builds `ltlf-ek-synth` and `ltlf-ek-deps`;
`scripts/hpc/hard-race.sbatch`). No other source differs from master. Spot 2.15.1.

Several tasks shared each node, so wall times are noisier and slower than the desktop smoke's. Example: NK on
shift-20 takes 48.9 s here and took 19.3 s in `2026-11-14-fix-dependent-outputs-race.md`.

## Checks

- **0 verdict mismatches**: every finished arm agrees with every finished baseline on every instance.
- **0 fixed-arm failures**, **0 unsafe controllers.** Both fixed arms on mux-16 verify SAFE.
- The shift controllers from compact-fixed (shift-20, shift-21) are **unverifiable**: `verify_controller` runs out
  of 8 GiB, as in the smoke.

## Bar of #243: not met

The bar: >= 2x faster than both NK and `ltlfsynt` on at least one hard family, 0 mismatches. Per the decision on
#250, an instance counts only if something checked it: a finished NK or `ltlfsynt` verdict that agrees, or a
verified controller. This split was computed by hand from the JSON (`bar_checked`), because #288 has not landed;
`bar` in the JSON is `hard_race.py`'s own unsplit figure, also not met.

| arm | family | checked (geomean) | unchecked (geomean) | fixed arm unfinished | no cell |
|---|---|---|---|---|---|
| compact-fixed | mux | 1 (0.07x) | 0 | 2 | 46 |
| derived-fixed | mux | 1 (0.03x) | 0 | 2 | 46 |
| compact-fixed | shift | 1 (1.74x) | 1 (>= 1.77x) | 19 | 0 |
| derived-fixed | shift | 0 | 0 | 1 | 20 |

Speedup is min(NK, ltlfsynt) / fixed arm wall time, with a timed-out baseline charged 120 s.

### mux: closed

`ltlfsynt` solves every mux size from 16 to 64 in at most 0.04 s. NK finishes only mux 16..19 (2.7 s to 49.5 s)
and times out from mux-20 up. Only mux 16, 32 and 64 have a knowledge arm (derived Tout at 16; compact at powers
of 2). The fixed arms finish only mux-16: compact-fixed in 0.33 s, derived-fixed in 0.68 s, both 8x and 4x faster
than NK but 16–34x slower than `ltlfsynt`. They time out at 32 and 64. Even with #289's compact arm at every size,
no arm can be 2x faster than a 0.04 s `ltlfsynt`, so this family cannot carry the bar.

### shift: not met, even as a lower bound

`ltlfsynt` times out on every shift instance (20..40). NK finishes only shift-20 (48.9 s). compact-fixed finishes
shift-20 (28.1 s, 1.74x over NK, checked by NK's verdict) and shift-21 (67.7 s, >= 1.77x counting both baselines'
timeouts, unchecked), and times out on shift 22..40. The plain compact arm finishes only shift-20 (72.3 s); the
derived arms finish nothing. No shift instance reaches 2x, checked or not.

### TwoCountersDisButAC

UNREALIZABLE. NK 1.31 s, `ltlfsynt` 0.03 s, derived 53.5 s, derived-fixed 53.4 s (no compact arm).

## Reading

On the full grid at 120 s, `--fix-dependent-outputs` speeds EK up against NK where it finishes (mux-16 8x, shift-20
1.7x), but beats the faster baseline nowhere by 2x. Mux is out of reach of any arm because `ltlfsynt` is near
instant; shift is the only family where the baselines struggle, and there the fixed arm stops finishing at n=22.
Whether that closes option 1 of #243 is the developer's call.
