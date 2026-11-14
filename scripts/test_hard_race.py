#!/usr/bin/env python3
"""Tests for hard_race.py. The ones that run the binaries are skipped without build/ltlf-ek-*."""
import json
import os
import shutil
import tempfile
import unittest

import hard_instances as hi
import hard_race as hr

HAVE_BINARIES = all(os.path.exists(os.path.join(hr.REPO, "build", b)) for b in ("ltlf-ek-synth", "ltlf-ek-deps"))
HAVE_LTLFSYNT = shutil.which("ltlfsynt") is not None


def cell(instance, arm, verdict, status="ok"):
    return {"instance": instance, "arm": arm, "status": status, "verdict": verdict}


class HardRace(unittest.TestCase):
    def test_instance_key_reads_family_and_size(self):
        self.assertEqual(hr.instance_key("mux-16"), ("mux", 16))
        self.assertEqual(hr.instance_key("TwoCountersDisButAC"), ("TwoCountersDisButAC", None))

    def test_two_finished_arms_that_disagree_are_a_verdict_mismatch(self):
        cells = [cell("a", "nk", "REALIZABLE"), cell("a", "ltlfsynt", "UNREALIZABLE"),
                 cell("b", "nk", "REALIZABLE"), cell("b", "ek-derived", "REALIZABLE")]
        self.assertEqual(hr.verdict_mismatches(cells), {"a": {"nk": "REALIZABLE", "ltlfsynt": "UNREALIZABLE"}})

    def test_an_unfinished_arm_is_no_mismatch(self):
        cells = [cell("a", "nk", "REALIZABLE"), cell("a", "ltlfsynt", None, "timeout")]
        self.assertEqual(hr.verdict_mismatches(cells), {})

    def test_instances_filter_by_family_and_size(self):
        with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as d:
            for name in ("mux-16", "mux-32", "shift-20", "Two"):
                open(os.path.join(d, name + ".ltlf"), "w").close()
            self.assertEqual(hr.instances(d, "mux", 20, None), ["mux-32"])
            self.assertEqual(hr.instances(d), ["Two", "mux-16", "mux-32", "shift-20"])

    def test_committed_specs_cover_the_issue_ranges(self):
        names = set(hr.instances(hr.DEFAULT_SPECS))
        self.assertTrue({"mux-%d" % n for n in range(16, 65)} <= names)
        self.assertTrue({"shift-%d" % n for n in range(20, 41)} <= names)
        self.assertIn("TwoCountersDisButAC", names)

    def test_the_bar_is_met_when_a_fixed_arm_is_twice_as_fast_as_both_baselines(self):
        def cells(fixed_wall, nk_wall=10.0, nk_status="ok"):
            return [dict(cell("mux-16", "nk", "REALIZABLE", nk_status), wall_s=nk_wall),
                    dict(cell("mux-16", "ltlfsynt", "REALIZABLE"), wall_s=8.0),
                    dict(cell("mux-16", "ek-compact-fixed", "REALIZABLE"), wall_s=fixed_wall)]
        self.assertTrue(hr.bar(cells(4.0), 60, {}, [])["met"])
        self.assertFalse(hr.bar(cells(4.1), 60, {}, [])["met"])
        self.assertFalse(hr.bar(cells(1.0), 60, {"mux-16": {}}, [])["met"])
        self.assertTrue(hr.bar(cells(3.9, nk_wall=99.0, nk_status="timeout"), 60, {}, [])["met"])
        self.assertFalse(hr.bar(cells(1.0), 60, {}, ["mux-16:ek-compact-fixed"])["met"])
        self.assertFalse(hr.bar(cells(1.0, nk_wall=99.0, nk_status="error"), 60, {}, [])["met"])
        self.assertEqual(hr.bar(cells(1.0, nk_status="error"), 60, {}, [])["evidence"], [])

    def test_the_bar_ignores_instances_below_the_hard_range(self):
        cells = [dict(cell("mux-8", "nk", "REALIZABLE"), wall_s=10.0), dict(cell("mux-8", "ltlfsynt", "REALIZABLE"), wall_s=10.0),
                 dict(cell("mux-8", "ek-compact-fixed", "REALIZABLE"), wall_s=0.1)]
        self.assertEqual(hr.bar(cells, 60, {}, [])["evidence"], [])

    def test_a_verifier_that_runs_out_of_memory_leaves_the_controller_unverifiable_not_unsafe(self):
        self.assertEqual(hr.verification(0, "SAFE\n"), "verified")
        self.assertEqual(hr.verification(20, "UNSAFE\nprefix: {i}\n"), "unsafe")
        self.assertEqual(hr.verification(1, ""), "unverifiable")
        self.assertEqual(hr.verification(None, ""), "unverifiable")

    def test_a_fixed_cell_that_errors_is_a_fixed_failure(self):
        cells = [cell("a", "ek-compact-fixed", None, "error"), cell("b", "ek-compact", None, "error"),
                 cell("c", "ek-derived-fixed", "REALIZABLE")]
        self.assertEqual(hr.fixed_failures(cells), ["a:ek-compact-fixed"])

    def small_race(self):
        with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as d:
            for inst in (hi.mux(4), hi.mux(6), hi.shift(3)):
                hi.write(d, inst)
            out = os.path.join(d, "run.json")
            self.assertEqual(hr.main(["--specs", d, "--out", out, "--timeout", "60"]), 0)
            with open(out) as f:
                run = json.load(f)
        return run, {(c["instance"], c["arm"]): c for c in run["cells"]}

    @unittest.skipUnless(HAVE_BINARIES and HAVE_LTLFSYNT, "needs build/ltlf-ek-* and ltlfsynt")
    def test_every_cell_of_a_small_race_names_its_arm(self):
        _, arms = self.small_race()
        self.assertEqual({a for (i, a) in arms if i == "shift-3"}, set(hr.ARMS))
        self.assertEqual({a for (i, a) in arms if i == "mux-6"}, {"nk", "ltlfsynt", "ek-derived"})
        self.assertEqual(arms[("mux-6", "ek-derived")]["status"], "none")
        self.assertTrue(all(c["status"] == "ok" for k, c in arms.items() if k != ("mux-6", "ek-derived")))

    @unittest.skipUnless(HAVE_BINARIES and HAVE_LTLFSYNT, "needs build/ltlf-ek-* and ltlfsynt")
    def test_a_realizable_fixed_cell_has_its_controller_verified(self):
        run, arms = self.small_race()
        for arm in ("ek-compact-fixed", "ek-derived-fixed"):
            got = arms[("shift-3", arm)]
            self.assertEqual((got["verdict"], got.get("controller")), ("REALIZABLE", "verified"))
        self.assertEqual(run["fixed_failures"], [])
        self.assertIn("met", run["bar"])

    @unittest.skipUnless(HAVE_BINARIES and HAVE_LTLFSYNT, "needs build/ltlf-ek-* and ltlfsynt")
    def test_the_arms_of_a_small_race_agree_on_every_verdict(self):
        run, _ = self.small_race()
        self.assertEqual(run["verdict_mismatches"], {})


if __name__ == "__main__":
    unittest.main()
