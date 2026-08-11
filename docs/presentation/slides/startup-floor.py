#!/usr/bin/env python3
"""Measure the per-process initialisation floor behind the flat `ltlfsynt` line.

The benchmark suite times our methods *in-process* (`method.synthesize(...)`,
src/bench_suite.cpp) but times `ltlfsynt` around a whole subprocess
(src/ltlf_ek_bench.cpp).  That makes `ltlfsynt_ns` look like a ~5 ms constant.
This script shows the constant is neither spawn cost nor specific to
`ltlfsynt`: our own CLI pays the same floor.

Not part of `make` -- it needs `ltlf-ek-synth` built, and it is a one-off
measurement quoted on a slide rather than an input to a figure.  Run:

    cmake --build build-release -j --target ltlf-ek-synth
    python3 docs/presentation/slides/startup-floor.py      # from the repo root

Each row is the MINIMUM over N spawns, matching the statistic
`ltlf-ek-bench` keeps for `ltlfsynt` (best of `--repeat`).
"""

import subprocess
import time

# Absolute, deliberately: the bare name resolves through PATH to a 2.14.4.dev
# install -- see docs/presentation/benchmark-numbers.md.
LTLFSYNT = "/home/cowclaw/opt/spot-2.15.1/bin/ltlfsynt"
SYNTH = "build-release/ltlf-ek-synth"
N = 15

CASES = [
    ("/bin/true (bare fork+exec)", ["/bin/true"]),
    ("ltlfsynt --version (no synthesis at all)", [LTLFSYNT, "--version"]),
    ("ltlfsynt, trivial instance",
     [LTLFSYNT, "--ins=a", "--outs=o", "--semantics=Mealy", "--realizability",
      "-f", "o"]),
    ("ltlf-ek-synth, trivial instance (ours)",
     [SYNTH, "--mtdfa-product", "--formula=o", "--inputs", "a", "--outputs",
      "o", "--realizable"]),
]


def best(argv, n=N):
    times = []
    for _ in range(n):
        t0 = time.perf_counter_ns()
        subprocess.run(argv, capture_output=True)
        times.append(time.perf_counter_ns() - t0)
    return min(times) / 1e6, sum(times) / len(times) / 1e6


def main():
    print(f"{'case':48} {'min ms':>9} {'mean ms':>9}")
    for label, argv in CASES:
        try:
            lo, mean = best(argv)
            print(f"{label:48} {lo:9.2f} {mean:9.2f}")
        except OSError as exc:
            print(f"{label:48} FAILED: {exc}")


if __name__ == "__main__":
    main()
