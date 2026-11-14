#!/usr/bin/env python3
"""Unit tests for summary_paper_tables.py, on hand-built sweep JSON. Run by ctest."""
import unittest

import summary_paper_tables as spt
from test_sweep_report import lt, sweep, timing

K, NK = spt.SIMP, spt.SIMP_NK


def hpc_sweep(**kw):
    s = sweep(**kw)
    s["provenance"]["n_max"] = 2
    return s


def lt_row(family, n, real, med, internal):
    return dict(lt(family, n, real, med, med), ltlfsynt_internal_ns=internal)


class Rq2(unittest.TestCase):
    def test_rq2_row_pairs_the_simplified_solves_and_reads_ltlfsynt_full_and_stage_time(self):
        s = hpc_sweep(
            timings=[timing("zeta", 2, True, K, 2e6, 2e6), timing("zeta", 2, True, NK, 8e6, 8e6)],
            ltlfsynt=[lt_row("zeta", 2, True, 20e6, 5e6)])
        row = [l for l in spt.rq2(s).splitlines() if l.startswith("zeta ")][0]
        self.assertEqual(row, "zeta & 2 & 2.00 & 8.00 & 4.00$\\times$ & 20.00 & 5.00 \\\\")

    def test_rq2_ignores_every_n_but_the_sweeps_largest(self):
        s = hpc_sweep(timings=[timing("zeta", 1, True, K, 2e6, 2e6), timing("zeta", 1, True, NK, 8e6, 8e6)])
        self.assertNotIn("zeta", spt.rq2(s))


class Rq1(unittest.TestCase):
    def test_rq1_row_lists_every_variant_as_knowledge_over_no_knowledge_at_the_largest_n(self):
        s = hpc_sweep(timings=[
            timing("zeta", 2, True, "nfa-product", 3e6, 3e6), timing("zeta", 2, True, "nfa-product-nk", 6e6, 6e6),
            timing("zeta", 1, True, "nfa-product", 9e6, 9e6)])
        row = [l for l in spt.rq1(s).splitlines() if l.startswith("zeta ")][0]
        self.assertEqual(row.split(" & ")[2], "3.00 / 6.00")

    def test_rq1_reads_t_o_for_a_variant_with_no_finished_cell(self):
        s = hpc_sweep(timings=[timing("zeta", 2, True, "dfa-product", 0, None, stage="TIMEOUT")])
        row = [l for l in spt.rq1(s).splitlines() if l.startswith("zeta ")][0]
        self.assertEqual(row.split(" & ")[1], "t/o / n/a")

    def test_rq1_reads_n_a_for_a_variant_the_family_never_ran(self):
        s = hpc_sweep(timings=[timing("zeta", 2, True, "dfa-product", 3e6, 3e6)])
        row = [l for l in spt.rq1(s).splitlines() if l.startswith("zeta ")][0]
        self.assertEqual(row.split(" & ")[1], "3.00 / n/a")

    def test_rq1_counts_a_familys_verdict_mismatches(self):
        s = hpc_sweep(timings=[timing("zeta", 2, True, K, 1e6, 1e6)],
                      ltlfsynt=[dict(lt_row("zeta", 1, True, 1e6, 1e6), verdict_mismatch=True),
                                dict(lt_row("zeta", 2, True, 1e6, 1e6), verdict_mismatch=False)])
        row = [l for l in spt.rq1(s).splitlines() if l.startswith("zeta ")][0]
        self.assertTrue(row.endswith("& 1 \\\\"))


class Figures(unittest.TestCase):
    def test_speedup_series_is_ltlfsynt_full_synthesis_over_the_knowledge_subject_per_n(self):
        s = hpc_sweep(
            timings=[timing("zeta", 1, True, K, 2e6, 2e6), timing("zeta", 2, True, K, 1e6, 1e6)],
            ltlfsynt=[lt_row("zeta", 1, True, 10e6, 1e6), lt_row("zeta", 2, True, 10e6, 1e6)])
        self.assertEqual(spt.speedup(s).splitlines()[0], "\\addplot coordinates {(1,5) (2,10)};")

    def test_a_family_with_no_figure_has_no_series(self):
        s = hpc_sweep(timings=[timing("zeta", 1, True, K, 2e6, 2e6)])
        self.assertEqual(spt.speedup(s), "\n")


if __name__ == "__main__":
    unittest.main()
