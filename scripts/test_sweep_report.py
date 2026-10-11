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
        self.assertIn("(N=3): 7.00x", out)

    def test_a_family_ltlfsynt_cannot_encode_says_so_instead_of_a_speedup(self):
        out = sr.render(sweep(timings=[timing("knowledge-chain", 2, True, EK, 1, 1)]))
        self.assertIn("No speedup over ltlfsynt", out)


if __name__ == "__main__":
    unittest.main()
