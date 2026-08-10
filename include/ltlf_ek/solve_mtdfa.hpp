#pragma once

#include <optional>

#include <spot/twaalgos/ltlf2dfa.hh>

#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// SolveDfa(P) for the mtdfa Representation (docs/GLOSSARY.md "Game solving"),
// sibling of solve_dfa --- NOT a replacement.  `product` is the intersected
// mtdfa P = ltlf_to_mtdfa(phi) x emits_dfa(t_in) x emits_dfa(t_out)
// (docs/prd/mtdfa-product.md "Decision 1"); `vars` supplies the free/known
// split.  nullopt = unrealizable.
//
// Decision 2 (docs/prd/mtdfa-product.md): the mtdfa game solver has only one
// knob (set_controllable_variables), so Iknown/Oknown are made controllable
// alongside Ofree --- pinned as forced system moves rather than projected
// arena-side, the way solve_dfa does it (main.tex:315's projection \na,
// reached by a different route; flagged for /theory-review).  Every Spot
// argument below is pinned by Phase 0's probes against the actual linked
// libspot, not guessed:
//
//   0. require_turn_order_aps(vars, product->get_dict()) --- Phase 0/Q2: a bad
//      AP order does not crash, it silently returns a wrong "unrealizable".
//   1. set_controllable_variables(Ofree u Iknown u Oknown).
//   2. mtdfa_winning_strategy(product, /*backprop_nodes=*/true) --- the
//      linear-time route (Phase 0 pins this: this PRD's whole claim is cost).
//   3. Unrealizable test (Phase 0/Q3): num_roots() == 0 ||
//      states[0] == bddfalse --- NOT num_roots() == 0 alone, which never
//      happens.
//   4. mtdfa_strategy_to_mealy(strategy, /*labels=*/false, /*loop=*/true) ---
//      Phase 0/Q4: loop=false leaves a free-choice state making lambda_C a
//      relation, not a function.
//   5. Project Iknown, Oknown out of every edge (the solve_dfa.cpp:49 idiom,
//      one stage later): a guard that becomes bddfalse is left in place
//      (functionally absent, matching this codebase's bddfalse-is-undefined
//      idiom throughout).
//   6. set_synthesis_outputs(mealy, Ofree), return Controller{mealy}.  This
//      mealy has NO "state-player" property (unlike solve_dfa's split arena)
//      --- do not re-split it; controller_as_transducer only unsplits when
//      that property is present.
std::optional<Controller> solve_mtdfa(const spot::mtdfa_ptr& product,
                                      const VariablePartition& vars);

}  // namespace ltlf_ek
