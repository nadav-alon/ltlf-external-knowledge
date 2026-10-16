#!/usr/bin/env python3
"""Unit tests for merge_sweep.py, on hand-built part JSON. Run by ctest."""
import unittest

import merge_sweep as ms


def part(family, stopped=False, mismatches=0, wall=10, **prov):
    p = {"machine": "cn01", "cpu_model": "Xeon Gold 6130", "cmake_build_type": "Release", "spot_version": "2.15.1", "repo_commit": "abc",
         "repeat": 5, "timeout_seconds": 20, "n_min": 2, "n_max": 6,
         "subjects": ["s"], "budget_seconds": 4000000000, "spot_resolved_path": "/usr/local/lib",
         "families": [family],
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
        self.assertEqual(m["provenance"]["sweep_wall_seconds"], 14)

    def test_parts_of_different_sweeps_are_refused(self):
        with self.assertRaisesRegex(ValueError, "repeat"):
            ms.merge([part("a"), part("b", repeat=1)])

    def test_parts_with_different_grid_or_subjects_are_refused(self):
        with self.assertRaisesRegex(ValueError, "n_max"):
            ms.merge([part("a"), part("b", n_max=4)])
        with self.assertRaisesRegex(ValueError, "subjects"):
            ms.merge([part("a"), part("b", subjects=["t"])])

    def test_a_short_family_set_is_refused_when_expected(self):
        parts = [part("a"), part("b")]
        self.assertEqual(ms.merge(parts, ["a", "b"])["provenance"]["families"], ["a", "b"])
        with self.assertRaisesRegex(ValueError, "expected"):
            ms.merge(parts, ["a", "b", "c"])

    def test_parts_from_two_hostnames_of_one_cpu_model_merge_listing_both(self):
        m = ms.merge([part("a"), part("b", machine="cn02"), part("c", machine="cn01")])
        self.assertEqual(m["provenance"]["machines"], ["cn01", "cn02"])
        self.assertNotIn("machine", m["provenance"])

    def test_parts_from_two_cpu_models_are_refused(self):
        with self.assertRaisesRegex(ValueError, "cpu_model"):
            ms.merge([part("a"), part("b", machine="cn02", cpu_model="Xeon Gold 6230")])

    def test_no_parts_is_refused(self):
        with self.assertRaises(ValueError):
            ms.merge([])


if __name__ == "__main__":
    unittest.main()
