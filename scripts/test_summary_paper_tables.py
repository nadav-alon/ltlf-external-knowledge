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


if __name__ == "__main__":
    unittest.main()
