// Second reproducer for the MTDFA backprop segfault --- the SINK-FREE trigger.
// See README.md.  Unlike mtdfa-backprop-segfault.cc this one needs ltlf_ek (it
// uses the real emits_dfa); it is kept because it proves the bug is NOT about
// the rejecting sink.
//
// Exact replay of generated-corpus case i=48, which still segfaults AFTER
// emits_dfa stopped materialising its sink:
//   phi   = p1 | p6 | F(Gp1 & Fp7)
//   Ifree = {p1,p3}  Iknown = {p0,p2,p4}  Ofree = {p5,p6,p7}
// t_in here is TOTAL (every (state, Ifree-letter) has a lambda), so emits_dfa
// builds NO sink --- 3 states, 10 product roots, and it still crashes at the
// same site.  Passing "false" as argv[2] selects backprop=false, which
// survives.
//
//   $ ./mtdfa-backprop-segfault-sinkfree                 # -> Segmentation fault
//   $ ./mtdfa-backprop-segfault-sinkfree "..." false     # -> SURVIVED

#include <iostream>
#include <vector>
#include <string>

#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twaalgos/ltlf2dfa.hh>

#include "ltlf_ek/emits_dfa.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/solve_mtdfa.hpp"
#include "ltlf_ek/turn_order.hpp"
#include "ltlf_ek/variables.hpp"

using namespace ltlf_ek;

static spot::formula parse(const char* s) {
  auto pf = spot::parse_infix_psl(s);
  if (pf.format_errors(std::cerr)) exit(2);
  return pf.f;
}

static bdd V(const spot::bdd_dict_ptr& d, const char* n, bool pos = true) {
  int v = d->varnum(spot::formula::ap(n));
  return pos ? bdd_ithvar(v) : bdd_nithvar(v);
}

int main(int argc, char** argv) {
  const char* goal = (argc > 1) ? argv[1] : "p1 | p6 | F(Gp1 & Fp7)";

  auto dict = spot::make_bdd_dict();
  auto vars = VariablePartition::split({"p0", "p1", "p2", "p3", "p4"},
                                       {"p5", "p6", "p7"},
                                       /*governed=*/{"p0", "p2", "p4"});
  register_turn_order_aps(vars, dict);

  // --- the exact t_in from the harness dump -------------------------------
  auto g = spot::make_twa_graph(dict);
  for (const char* n : {"p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7"})
    g->register_ap(n);
  g->new_states(3);
  g->set_init_state(0);

  auto IF = [&](bool p1, bool p3) { return V(dict, "p1", p1) & V(dict, "p3", p3); };
  auto LAM = [&](bool p0, bool p2, bool p4) {
    return V(dict, "p0", p0) & V(dict, "p2", p2) & V(dict, "p4", p4);
  };

  // state 0
  g->new_edge(0, 1, IF(0, 0));
  g->new_edge(0, 1, IF(1, 0));
  g->new_edge(0, 0, IF(0, 1));
  g->new_edge(0, 0, IF(1, 1));
  // state 1
  g->new_edge(1, 0, IF(0, 0));
  g->new_edge(1, 2, IF(1, 0));
  g->new_edge(1, 2, IF(0, 1));
  g->new_edge(1, 2, IF(1, 1));
  // state 2
  g->new_edge(2, 1, IF(0, 0));
  g->new_edge(2, 0, IF(1, 0));
  g->new_edge(2, 1, IF(0, 1));
  g->new_edge(2, 0, IF(1, 1));

  std::vector<bdd> lambda_by_state = {
      (IF(0, 0) & LAM(0, 1, 1)) | (IF(1, 0) & LAM(0, 0, 1))
          | (IF(0, 1) & LAM(0, 1, 1)) | (IF(1, 1) & LAM(0, 0, 0)),
      (IF(0, 0) & LAM(1, 1, 1)) | (IF(1, 0) & LAM(1, 1, 1))
          | (IF(0, 1) & LAM(0, 1, 0)) | (IF(1, 1) & LAM(1, 1, 1)),
      (IF(0, 0) & LAM(1, 0, 1)) | (IF(1, 0) & LAM(0, 1, 1))
          | (IF(0, 1) & LAM(0, 0, 0)) | (IF(1, 1) & LAM(1, 0, 0)),
  };

  bdd sigma0 = V(dict, "p1") & V(dict, "p3");                       // Ifree
  bdd sigma1 = V(dict, "p0") & V(dict, "p2") & V(dict, "p4");       // Iknown
  OutputLabeledTransducer t_in(g, lambda_by_state, sigma0, sigma1);

  spot::twa_graph_ptr e = emits_dfa(t_in, dict);
  std::cerr << "emits_dfa(t_in): states=" << e->num_states() << "\n";

  spot::mtdfa_ptr A = spot::ltlf_to_mtdfa(parse(goal), dict);
  spot::mtdfa_ptr B = spot::twadfa_to_mtdfa(e);
  std::cerr << "goal roots=" << A->num_roots()
            << " emits roots=" << B->num_roots() << "\n";

  spot::mtdfa_ptr P = spot::product(A, B);
  std::cerr << "product roots=" << P->num_roots() << "\n";

  bdd ctrl = bddtrue;
  for (const char* n : {"p0", "p2", "p4", "p5", "p6", "p7"})
    ctrl &= V(dict, n);
  P->set_controllable_variables(ctrl);

  const bool bp = !(argc > 2 && std::string(argv[2]) == "false");
  std::cerr << "calling mtdfa_winning_strategy(backprop=" << bp << ")\n";
  spot::mtdfa_ptr S = spot::mtdfa_winning_strategy(P, bp);
  std::cerr << "SURVIVED. roots=" << S->num_roots() << "\n";
  return 0;
}
