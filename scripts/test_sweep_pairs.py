#!/usr/bin/env python3
"""Unit tests for sweep_pairs.py, on hand-built sweep JSON. Run by ctest."""
import unittest

import sweep_pairs as sp
from test_sweep_report import lt, structural, sweep, timing

EK, F = "ek-subject", "formula-subject"


def internal(row, ns):
    return dict(row, ltlfsynt_internal_ns=ns)


class SweepPairs(unittest.TestCase):
    def test_speedup_counts_only_cells_both_sides_finished(self):
        rows = sp.build_rows(sweep(timings=[
            timing("fam", 2, True, EK, 10, 10), timing("fam", 2, True, F, 40, 40),
            timing("fam", 2, False, EK, 10, 10), timing("fam", 2, False, F, 0, None, stage="TIMEOUT")]), EK, F)
        r = rows[("fam", 2)]
        self.assertEqual((r["cells"], r["both"], r["unfinished"]), (2, 1, 1))
        self.assertEqual(r["speedup"], 4.0)

    def test_an_unfinished_formula_cell_enters_the_lower_bound_at_the_timeout(self):
        rows = sp.build_rows(sweep(timings=[
            timing("fam", 2, True, EK, 10, 10), timing("fam", 2, True, F, 40, 40),
            timing("fam", 2, False, EK, 1e9, 1e9), timing("fam", 2, False, F, 0, None, stage="TIMEOUT")]), EK, F)
        self.assertEqual(rows[("fam", 2)]["bound"], (4.0 + 20.0) / 2)

    def test_a_partial_timeout_counts_as_unfinished(self):
        rows = sp.build_rows(sweep(timings=[
            timing("fam", 2, True, EK, 10, 10), timing("fam", 2, True, F, 40, 40),
            timing("fam", 2, True, F, 0, None, stage="PARTIAL_TIMEOUT_3_of_5")]), EK, F)
        self.assertEqual(rows[("fam", 2)]["unfinished"], 1)

    def test_no_bound_is_printed_when_every_cell_finished(self):
        rows = sp.build_rows(sweep(timings=[
            timing("fam", 2, True, EK, 10, 10), timing("fam", 2, True, F, 40, 40)]), EK, F)
        self.assertIsNone(rows[("fam", 2)]["bound"])

    def test_an_ltlfsynt_timeout_enters_its_lower_bound_and_not_its_speedup(self):
        rows = sp.build_rows(sweep(
            timings=[timing("fam", 2, True, EK, 10, 10), timing("fam", 2, False, EK, 1e9, 1e9)],
            ltlfsynt=[internal(lt("fam", 2, True, 0, 0), 30), lt("fam", 2, False, 0, 0, status="timeout")]), EK, F)
        r = rows[("fam", 2)]
        self.assertEqual((r["lt"], r["lt_n"], r["lt_timeouts"]), (3.0, 1, 1))
        self.assertEqual(r["lt_bound"], (3.0 + 20.0) / 2)

    def test_arms_splits_slippery_into_its_arms(self):
        t = [timing(a, 2, True, EK, 10, 10) for a in ("slippery-binary", "slippery-onehot")]
        self.assertEqual(set(sp.build_rows(sweep(timings=t), EK, F)), {("slippery", 2)})
        self.assertEqual(set(sp.build_rows(sweep(timings=t), EK, F, arms=True)),
                         {("slippery-binary", 2), ("slippery-onehot", 2)})

    def test_the_roots_ratio_is_ek_over_formula(self):
        rows = sp.build_rows(sweep(
            timings=[timing("fam", 2, True, EK, 10, 10)],
            structural_rows=[structural("fam", 2, True, EK, 3, "product_mtdfa_roots"),
                             structural("fam", 2, True, F, 6, "product_mtdfa_roots")]), EK, F)
        self.assertEqual(rows[("fam", 2)]["roots"], 0.5)


if __name__ == "__main__":
    unittest.main()
