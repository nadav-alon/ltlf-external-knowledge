#!/usr/bin/env python3
"""Unit tests for sweep_report.py, on hand-built sweep JSON. Run by ctest."""
import unittest

import sweep_report as sr


def timing(family, n, real, subject, ns, med, stage="wall_total"):
    return {"family": family, "n": n, "realizable": real, "subject": subject,
            "stage": stage, "ns": ns, "median_ns": med, "timed_out": False}


def structural(family, n, real, subject, value):
    return {"family": family, "n": n, "realizable": real, "subject": subject,
            "metric": "product_states", "value": value}


def lt(family, n, real, ns, med, status="ok"):
    return {"family": family, "n": n, "realizable": real, "status": status,
            "ltlfsynt_ns": ns, "ltlfsynt_median_ns": med}


def sweep(timings=(), structural_rows=(), ltlfsynt=()):
    return {"provenance": {"cmake_build_type": "Release", "spot_version": "2.15.1",
                           "repo_commit": "abcdef0123", "repeat": 5,
                           "timeout_seconds": 20, "stopped_early": False,
                           "verdict_mismatch_count": 0},
            "timings": list(timings), "structural": list(structural_rows),
            "ltlfsynt": list(ltlfsynt)}


EK = sr.EK_SUBJECT


class SweepReport(unittest.TestCase):
    def test_every_slippery_arm_is_one_family(self):
        rows = sr.build_rows(sweep(
            timings=[timing("slippery-binary", 2, True, EK, 10, 10),
                     timing("slippery-onehot", 2, True, EK, 10, 10),
                     timing("slippery-binary-compact", 2, True, EK, 10, 10)]))
        self.assertEqual(list(rows), ["slippery"])
        self.assertEqual(rows["slippery"][2]["cells"], 3)

    def test_speedup_is_the_ltlfsynt_median_over_the_ek_median(self):
        rows = sr.build_rows(sweep(
            timings=[timing("cons-prunes", 2, True, EK, 90, 100)],
            ltlfsynt=[lt("cons-prunes", 2, True, 400, 500)]))
        self.assertEqual(rows["cons-prunes"][2]["speedup_med"], 5.0)
        self.assertAlmostEqual(rows["cons-prunes"][2]["speedup_min"], 400 / 90)

    def test_a_family_figure_is_the_median_over_its_cells(self):
        rows = sr.build_rows(sweep(
            timings=[timing("slippery-binary", 2, True, EK, 1, 100),
                     timing("slippery-onehot", 2, True, EK, 1, 100),
                     timing("slippery-binary-compact", 2, True, EK, 1, 100)],
            ltlfsynt=[lt("slippery-binary", 2, True, 1, 200),
                      lt("slippery-onehot", 2, True, 1, 400),
                      lt("slippery-binary-compact", 2, True, 1, 1000)]))
        self.assertEqual(rows["slippery"][2]["speedup_med"], 4.0)

    def test_prune_ratio_is_knowledge_product_over_the_unpruned_product(self):
        rows = sr.build_rows(sweep(
            timings=[timing("cons-prunes", 3, True, EK, 1, 1)],
            structural_rows=[structural("cons-prunes", 3, True, sr.EK_PRODUCT, 5),
                             structural("cons-prunes", 3, True, sr.EK_UNPRUNED, 10),
                             structural("cons-prunes", 3, True, sr.EK_NK, 4)]))
        self.assertEqual(rows["cons-prunes"][3]["prune"], 0.5)
        self.assertEqual(rows["cons-prunes"][3]["prune_nk"], 1.25)

    def test_a_timed_out_ek_cell_is_not_a_finished_cell(self):
        rows = sr.build_rows(sweep(
            timings=[timing("f", 2, True, EK, 1, 1),
                     timing("f", 3, True, EK, 20 * 10**9, None, stage="TIMEOUT")],
            ltlfsynt=[lt("f", 2, True, 1, 1), lt("f", 3, True, 1, 1)]))
        self.assertIsNone(rows["f"][3].get("speedup_med"))

    def test_headline_is_the_largest_n_that_finished_on_both_sides(self):
        out = sr.render(sweep(
            timings=[timing("f", 2, True, EK, 1, 100), timing("f", 3, True, EK, 1, 100),
                     timing("f", 4, True, EK, 1, 100)],
            ltlfsynt=[lt("f", 2, True, 1, 300), lt("f", 3, True, 1, 700),
                      lt("f", 4, True, 1, 0, status="timeout")]))
        self.assertIn("(N=3, 1 of 1 cells): 7.00x", out)
        self.assertIn("ltlfsynt timed out (20 s) at N=4 in 1 of 1 cells", out)
        self.assertIn("speedup at least 200000000.00x", out)

    def test_a_partially_timed_out_ek_cell_is_left_out(self):
        rows = sr.build_rows(sweep(
            timings=[timing("f", 2, True, EK, 1, 1),
                     timing("f", 2, True, EK, 0, None, stage="PARTIAL_TIMEOUT_4_of_5"),
                     timing("f", 2, False, EK, 1, 100)],
            ltlfsynt=[lt("f", 2, True, 1, 1), lt("f", 2, False, 1, 500)]))
        self.assertEqual(rows["f"][2]["cells"], 1)
        self.assertEqual(rows["f"][2]["speedup_med"], 5.0)

    def test_a_partially_timed_out_ltlfsynt_cell_is_left_out(self):
        rows = sr.build_rows(sweep(
            timings=[timing("f", 2, True, EK, 1, 100)],
            ltlfsynt=[lt("f", 2, True, 1, 1, status="partial_timeout")]))
        self.assertIsNone(rows["f"][2].get("speedup_med"))

    def test_headline_prefers_the_largest_n_with_every_cell_finished(self):
        out = sr.render(sweep(
            timings=[timing("f", 2, True, EK, 1, 100), timing("f", 2, False, EK, 1, 100),
                     timing("f", 3, True, EK, 1, 100), timing("f", 3, False, EK, 1, 100)],
            ltlfsynt=[lt("f", 2, True, 1, 300), lt("f", 2, False, 1, 300),
                      lt("f", 3, True, 1, 900)]))
        self.assertIn("(N=2, 2 of 2 cells): 3.00x", out)

    def test_headline_names_the_cell_count_when_no_n_is_complete(self):
        out = sr.render(sweep(
            timings=[timing("f", 3, True, EK, 1, 100), timing("f", 3, False, EK, 1, 100)],
            ltlfsynt=[lt("f", 3, True, 1, 900)]))
        self.assertIn("(N=3, 1 of 2 cells): 9.00x", out)

    def test_an_ltlfsynt_error_cell_gets_a_note_naming_n_and_the_count(self):
        out = sr.render(sweep(
            timings=[timing("f", 2, True, EK, 1, 100), timing("f", 2, False, EK, 1, 100)],
            ltlfsynt=[lt("f", 2, True, 1, 300), lt("f", 2, False, 0, None, status="error")]))
        self.assertIn("ltlfsynt errored at N=2 in 1 of 2 cells", out)

    def test_a_stopped_early_sweep_is_flagged_at_the_top(self):
        s = sweep(timings=[timing("f", 2, True, EK, 1, 1)])
        s["provenance"]["stopped_early"] = True
        self.assertIn("STOPPED EARLY", sr.render(s).splitlines()[2])

    def test_a_family_ltlfsynt_cannot_encode_says_so_instead_of_a_speedup(self):
        out = sr.render(sweep(timings=[timing("knowledge-chain", 2, True, EK, 1, 1)]))
        self.assertIn("No speedup over ltlfsynt", out)


if __name__ == "__main__":
    unittest.main()
