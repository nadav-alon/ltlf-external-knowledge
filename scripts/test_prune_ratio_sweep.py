#!/usr/bin/env python3
"""Unit tests for prune_ratio_sweep.py's pure logic. Run by ctest."""
import unittest

import prune_ratio_sweep as ps


def row(k, u, f="a.tlsf", known=1, status="OK"):
    r = {"file": f, "tier": 1, "status": status, "known_outputs": known, "outputs": 5}
    if k is not None:
        r["k"] = {"product_states": k}
    if u is not None:
        r["u"] = {"product_states": u}
    return r


class PruneRatioSweep(unittest.TestCase):
    def test_ratio_is_k_over_u(self):
        self.assertEqual(ps.prune_ratio(row(258, 516)), 0.5)

    def test_a_row_missing_a_product_has_no_ratio_and_is_partial(self):
        self.assertIsNone(ps.prune_ratio(row(10, None)))
        self.assertEqual(ps.classify(row(10, None)), "PARTIAL")

    def test_no_known_output_is_its_own_outcome(self):
        self.assertEqual(ps.classify(row(None, None, known=0)), "NO_KNOWLEDGE")

    def test_non_ok_status_passes_through(self):
        self.assertEqual(ps.classify({"status": "MEMOUT"}), "MEMOUT")

    def test_candidates_are_strictly_below_half_and_sorted(self):
        rows = [row(5, 10, "half"), row(4, 10, "b"), row(1, 10, "a"), row(11, 10, "up")]
        self.assertEqual([r["file"] for _, r in ps.candidates(rows)], ["a", "b"])


if __name__ == "__main__":
    unittest.main()
