// Self-contained reproducer for a SIGSEGV in Spot's MTDFA game solver.
// Pure Spot: no ltlf_ek code is involved.  Not part of the build --- see
// README.md in this directory for how to compile and what it shows.
//
// Reported against: libspot 2.14.4.dev (linked).  The same code path is
// present unchanged in the 2.15.1.dev source tree.
//
//   spot::mtdfa_winning_strategy(P, /*backprop=*/true) segfaults when P is a
//   spot::product() whose operand came from twadfa_to_mtdfa() on a DFA that
//   materialises a REJECTING SINK (a non-accepting state with a bddtrue
//   self-loop).  The same language expressed WITHOUT the sink --- as an
//   incomplete DFA, missing edge = implicit reject --- is fine, as is the
//   same language via ltlf_to_mtdfa().
//
// Crash site (2.14.4 line numbers; 2.15.1 differs by 2):
//   spot/twaalgos/ltlf2dfa.cc:1593  strategy_finalize
//     -> global_backprop->root_winner(term / 2)
//     -> spot/twaalgos/backprop.hh:102  winner(s) { return (*this)[s].winner; }
//   ...which indexes the adjlist WITHOUT a bounds check.  root_winner() itself
//   is guarded (returns -1 on a find() miss), so the entry exists but resolves
//   to an out-of-range backprop state.
//
// Trigger conditions (all three needed):
//   1. an operand with a materialised rejecting sink,
//   2. backprop=true (strategy_finalize is exclusive to that path),
//   3. a goal that is REALIZABLE and forces a next step (X[!]).
// (3) matters because strategy_finalize returns early on accepting terminals
// (`term & 1`) without touching global_backprop --- so goals whose strategy
// only ever finalizes accepting terminals (G(!k), F(!k), !k) never crash.
//
// Usage: ./mtdfa-sink-segfault [spot|nosink|sink] ["<goal formula>"]
//   spot   : filter = ltlf_to_mtdfa("G(!k)")          -> survives
//   nosink : filter = 1-state incomplete DFA          -> survives
//   sink   : filter = 2-state DFA + rejecting sink    -> SIGSEGV
// Default goal "X[!] !k" is realizable under the filter.

#include <iostream>
#include <string>

#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/ltlf2dfa.hh>

static spot::formula parse(const char* s)
{
  auto pf = spot::parse_infix_psl(s);
  if (pf.format_errors(std::cerr))
    exit(2);
  return pf.f;
}

int main(int argc, char** argv)
{
  const std::string mode = (argc > 1) ? argv[1] : "sink";
  const char* goal = (argc > 2) ? argv[2] : "X[!] !k";

  auto dict = spot::make_bdd_dict();

  // Variable order: the uncontrollable input `a` strictly above the
  // controllables {k, o} (Mealy semantics; cf. bin/ltlfsynt.cc).
  auto reg = spot::make_twa_graph(dict);
  reg->register_ap("a");
  reg->register_ap("k");
  reg->register_ap("o");

  spot::mtdfa_ptr A = spot::ltlf_to_mtdfa(parse(goal), dict);

  // The filter accepts exactly the words whose every letter has !k.  All
  // three encodings below denote that same language.
  spot::mtdfa_ptr B;
  if (mode == "spot")
    {
      B = spot::ltlf_to_mtdfa(parse("G(!k)"), dict);
    }
  else
    {
      auto g = spot::make_twa_graph(dict);
      g->register_ap("a");
      const int k = g->register_ap("k");
      g->register_ap("o");
      g->set_acceptance(spot::acc_cond::inf({0}));
      g->prop_state_acc(true);
      if (mode == "sink")
        {
          g->new_states(2);                        // 0 accepting, 1 rejecting sink
          g->set_init_state(0);
          // state-based acceptance: every out-edge of accepting q0 is marked
          g->new_edge(0, 0, bdd_nithvar(k), {0});
          g->new_edge(0, 1, bdd_ithvar(k), {0});   // the !covered letter -> sink
          g->new_edge(1, 1, bddtrue, {});          // sink self-loop, unmarked
        }
      else                                         // nosink: incomplete, no sink
        {
          g->new_states(1);
          g->set_init_state(0);
          g->new_edge(0, 0, bdd_nithvar(k), {0});
        }
      std::cerr << mode << ": states=" << g->num_states()
                << " deterministic=" << spot::is_deterministic(g) << '\n';
      B = spot::twadfa_to_mtdfa(g);                // throws if not deterministic
    }

  std::cerr << "goal roots=" << A->num_roots()
            << " filter roots=" << B->num_roots() << '\n';

  spot::mtdfa_ptr P = spot::product(A, B);
  std::cerr << "product roots=" << P->num_roots() << '\n';

  P->set_controllable_variables(bdd_ithvar(dict->varnum(spot::formula::ap("k")))
                                & bdd_ithvar(dict->varnum(spot::formula::ap("o"))));

  std::cerr << "calling mtdfa_winning_strategy(backprop=true)\n";
  spot::mtdfa_ptr S = spot::mtdfa_winning_strategy(P, true);
  std::cerr << "SURVIVED. roots=" << S->num_roots()
            << " states[0]==bddfalse? " << (S->states[0] == bddfalse) << '\n';
  return 0;
}
