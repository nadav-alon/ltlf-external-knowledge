#!/usr/bin/env python3
"""Unit tests for merge_sweep.py, on hand-built part JSON. Run by ctest."""
import unittest

import merge_sweep as ms


def part(family, stopped=False, mismatches=0, wall=10, **prov):
    p = {"cmake_build_type": "Release", "spot_version": "2.15.1", "repo_commit": "abc",
         "repeat": 5, "timeout_seconds": 20, "families": [family],
         "stopped_early": stopped, "verdict_mismatch_count": mismatches,
         "sweep_wall_seconds": wall}
    p.update(prov)
    return {"provenance": p, "timings": [{"family": family}],
            "structural": [{"family": family}], "ltlfsynt": []}


class MergeSweep(unittest.TestCase):
    def test_rows_concatenate_and_provenance_combines(self):
        m = ms.merge([part("a", wall=5), part("b", stopped=True, mismatches=2, wall=9)])
        self.assertEqual([r["family"] for r in m["timings"]], ["a", "b"])
        self.assertEqual(m["provenance"]["families"], ["a", "b"])
        self.assertTrue(m["provenance"]["stopped_early"])
        self.assertEqual(m["provenance"]["verdict_mismatch_count"], 2)
        self.assertEqual(m["provenance"]["sweep_wall_seconds"], 9)

    def test_parts_of_different_sweeps_are_refused(self):
        with self.assertRaisesRegex(ValueError, "repeat"):
            ms.merge([part("a"), part("b", repeat=1)])

    def test_no_parts_is_refused(self):
        with self.assertRaises(ValueError):
            ms.merge([])


if __name__ == "__main__":
    unittest.main()
