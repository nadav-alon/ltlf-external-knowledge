#!/usr/bin/env python3
"""Unit tests for hard_instances.py. The binary-backed ones are skipped without build/ltlf-ek-*."""
import os
import subprocess
import tempfile
import unittest

import hard_instances as hi
import hard_race as hr

BUILD = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build")
SYNTH, DEPS = os.path.join(BUILD, "ltlf-ek-synth"), os.path.join(BUILD, "ltlf-ek-deps")
HAVE_BINARIES = os.path.exists(SYNTH) and os.path.exists(DEPS)


def verdict(*args):
    return subprocess.run([SYNTH, *hr.SYNTH_FLAGS, *args], capture_output=True, text=True).stdout.strip()


class HardInstances(unittest.TestCase):
    def test_the_committed_files_are_what_the_generator_writes(self):
        self.assertEqual(hi.main(["--check"]), 0)

    def test_the_committed_two_counters_spec_is_present_with_its_partition(self):
        for ext in (".ltlf", ".part"):
            self.assertTrue(os.path.exists(os.path.join(hi.DEFAULT_OUT, "TwoCountersDisButAC" + ext)))

    def test_mux_needs_ceil_log2_n_select_bits(self):
        self.assertEqual([hi.nbits(n) for n in (1, 2, 3, 4, 16, 17, 64)], [1, 1, 2, 2, 4, 5, 6])

    def test_mux_has_a_compact_tout_only_at_powers_of_two(self):
        self.assertIsNotNone(hi.mux(16)["tout"])
        self.assertIsNone(hi.mux(17)["tout"])

    def test_shift_tout_is_one_state(self):
        self.assertIn("States: 1\n", hi.tout_text(hi.shift(3)))

    def test_a_part_file_names_the_known_outputs_when_given(self):
        self.assertEqual(hi.part_text(["a"], ["x", "y"], ["y"]),
                         "input_free: a\ninput_known:\noutput_free: x\noutput_known: y\n")

    @unittest.skipUnless(HAVE_BINARIES, "needs build/ltlf-ek-synth and build/ltlf-ek-deps")
    def test_the_compact_tout_leaves_the_verdict_of_a_small_instance_unchanged(self):
        with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as d:
            for inst in (hi.mux(4), hi.shift(3)):
                base = hi.write(d, inst)
                formula = inst["formula"]
                known = base + ".known.part"
                with open(known, "w") as f:
                    f.write(hi.part_text(inst["inputs"], inst["outputs"], inst["outputs"]))
                self.assertEqual(
                    verdict("--formula", formula, "--part-file", known, "--known-output-transducer", base + ".tout"),
                    verdict("--formula", formula, "--part-file", base + ".part"), inst["name"])

    @unittest.skipUnless(HAVE_BINARIES, "needs build/ltlf-ek-synth and build/ltlf-ek-deps")
    def test_deps_finds_the_output_of_mux_exactly_at_a_power_of_two(self):
        with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as d:
            for n, hit in ((4, True), (6, False)):
                base = hi.write(d, hi.mux(n))
                out = subprocess.run([DEPS, "--direction", "out", "--part-file", base + ".part", "--formula",
                                      hi.mux(n)["formula"]], capture_output=True, text=True).stdout
                self.assertEqual("dependent outputs: out" in out, hit, "mux-%d: %s" % (n, out))


if __name__ == "__main__":
    unittest.main()
