#!/usr/bin/env python3
"""Unit tests for fond_encode.py, on a two-room domain small enough to count by hand. Run by ctest."""
import unittest

import fond_encode as fe

DOMAIN = """
(define (domain tiny)
  (:requirements :typing :non-deterministic)
  (:types room)
  (:predicates (at ?r - room) (door ?a ?b - room) (lit))
  (:action go :parameters (?a ?b - room)
    :precondition (and (at ?a) (door ?a ?b))
    :effect (and (at ?b) (not (at ?a))))
  (:action flip :parameters ()
    :precondition (not (lit))
    :effect (oneof (and) (lit))))
"""
PROBLEM = """
(define (problem p) (:domain tiny)
  (:objects r0 r1 - room)
  (:init (at r0) (door r0 r1) (door r1 r0))
  (:goal (at r1)))
"""


def model_with_action(action_pddl):
    """DOMAIN with one more action appended."""
    return fe.Model(DOMAIN.rstrip()[:-1] + action_pddl + ")", PROBLEM)


def model():
    return fe.Model(DOMAIN, PROBLEM)


def action(m, name):
    return [i for i, g in enumerate(m.ground) if g[0] == name][0]


class FondEncode(unittest.TestCase):
    def test_only_the_actions_whose_static_preconditions_hold_are_grounded(self):
        # go r0 r1 and go r1 r0; go r0 r0 has no door; flip has no parameters.
        self.assertEqual(sorted(g[0] for g in model().ground), ["flip_", "go_r0_r1", "go_r1_r0"])

    def test_domain_constants_are_objects(self):
        dom = DOMAIN.replace("(:predicates", "(:constants r0 r1 - room)\n  (:predicates")
        prob = PROBLEM.replace("(:objects r0 r1 - room)", "")
        self.assertEqual(len(fe.Model(dom, prob).reachable()[0]), 4)

    def test_a_problem_may_omit_its_objects_section(self):
        dom = DOMAIN.replace("(:predicates", "(:constants r0 r1 - room)\n  (:predicates")
        prob = PROBLEM.replace("(:objects r0 r1 - room)", "")
        self.assertEqual(len(fe.Model(dom, prob).ground), 3)

    def test_reachable_states_are_position_times_lit(self):
        order, _ = model().reachable()
        self.assertEqual(len(order), 4)

    def test_an_inapplicable_action_leaves_the_state_unchanged(self):
        m = model()
        s0 = m.reachable()[0][0]
        self.assertEqual(m.step(s0, action(m, "go_r1_r0"), 0), s0)

    def test_the_nature_bit_picks_the_oneof_branch(self):
        m = model()
        s0 = m.reachable()[0][0]
        flip = action(m, "flip_")
        self.assertEqual(m.step(s0, flip, 0), s0)
        self.assertIn(("lit",), m.step(s0, flip, 1))

    def test_a_precondition_on_a_dynamic_fluent_blocks_the_action_once_it_holds(self):
        m = model()
        lit = m.step(m.reachable()[0][0], action(m, "flip_"), 1)
        self.assertEqual(m.step(lit, action(m, "flip_"), 0), lit)

    def test_a_positive_equality_precondition_holds_only_for_the_same_object(self):
        m = model_with_action("""(:action stay :parameters (?a ?b - room)
            :precondition (= ?a ?b) :effect (lit))""")
        self.assertEqual(sorted(g[0] for g in m.ground if g[0].startswith("stay")),
                         ["stay_r0_r0", "stay_r1_r1"])
        s0 = m.reachable()[0][0]
        self.assertNotEqual(m.step(s0, action(m, "stay_r0_r0"), 0), s0)

    def test_a_negated_equality_precondition_is_not_grounded_for_the_same_object(self):
        m = model_with_action("""(:action move :parameters (?a ?b - room)
            :precondition (not (= ?a ?b)) :effect (lit))""")
        self.assertEqual(sorted(g[0] for g in m.ground if g[0].startswith("move")),
                         ["move_r0_r1", "move_r1_r0"])
        s0 = m.reachable()[0][0]
        self.assertNotEqual(m.step(s0, action(m, "move_r0_r1"), 0), s0)

    def test_an_equality_under_forall_is_evaluated_by_object_name(self):
        # (not (= ?x ?a)) fails for x = a, so no object satisfies the forall.
        m = model_with_action("""(:action alone :parameters (?a - room)
            :precondition (forall (?x - room) (not (= ?x ?a))) :effect (lit))""")
        s0 = m.reachable()[0][0]
        self.assertEqual(m.step(s0, action(m, "alone_r0"), 0), s0)

    def test_quotient_merges_states_the_observed_label_cannot_separate(self):
        # states 0,1 differ only by an unobserved bit and move alike; 2 is observed differently.
        block, n = fe.quotient([(0,), (0,), (1,)], [(2, 2), (2, 2), (2, 2)])
        self.assertEqual((n, block[0] == block[1], block[0] == block[2]), (2, True, False))

    def test_the_moore_quotient_drops_the_unobserved_lit_bit(self):
        full = fe.encode(DOMAIN, PROBLEM, "F(at_r1)")
        mini = fe.encode(DOMAIN, PROBLEM, "F(at_r1)", moore_quotient=True)
        self.assertEqual((full.t_in_states, mini.t_in_states), (4, 2))

    def test_the_hoa_header_and_lambda_follow_the_reachable_states(self):
        part, hoa, ins, outs, n, n_act = fe.encode(DOMAIN, PROBLEM, "F(at_r1)")
        self.assertIn("States: 4", hoa)
        self.assertEqual(hoa.count("\nstate "), 4)
        self.assertEqual((ins, outs, n_act), ("ev,at_r1", "a0,a1", 3))
        self.assertIn("input_known: at_r1", part)

    def test_a_goal_naming_a_fluent_that_never_holds_is_rejected(self):
        with self.assertRaises(ValueError):
            fe.encode(DOMAIN, PROBLEM, "F(at_r9)")


if __name__ == "__main__":
    unittest.main()
