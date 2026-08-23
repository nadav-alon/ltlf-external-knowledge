// The Q3 certificate, standalone: L(T_in) == L(A_N) as a finite-word language
// equivalence (docs/plans/2026-08-17-week.md, Wednesday step 2).
//
// Deliberately NOT a CMake target: Wednesday's day-run writes only new files
// and touches neither the harness nor the build.  Compile out of tree:
//
//   c++ -std=c++20 -O2 -I include -isystem ~/opt/spot-2.15.1/include \
//       scripts/slippery_world_cert.cpp build/libltlf_ek.a \
//       -L ~/opt/spot-2.15.1/lib -lspot -lbddx \
//       -Wl,-rpath,$HOME/opt/spot-2.15.1/lib -o build/scratch/sw-cert
//
//   sw-cert --part-file P --transducer T --formula A
//
// Both sides become deterministic finite automata on ONE bdd_dict:
//   L(T_in)  = emits_dfa(T_in)     -- every state final, delta partial;
//   L(A_N)   = ltlf_to_dfa(A_N)    -- state-based final marks, completed.
// A synchronous product then hunts a reachable pair whose finality differs;
// a missing edge on either side is an implicit rejecting sink (kDead).
// Prints the two state counts, the verdict, and -- on a difference -- the
// shortest witness word as a sequence of letters.
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <bddx.h>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/bddprint.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/cli.hpp"
#include "ltlf_ek/emits_dfa.hpp"
#include "ltlf_ek/ltlf_to_dfa.hpp"
#include "ltlf_ek/transducer_io.hpp"
#include "ltlf_ek/turn_order.hpp"

namespace {

constexpr int kDead = -1;

// Finality per state, read off the state-based marks: a state is final iff
// some out-edge carries mark 0.  Every state of both inputs has a carrier
// (emits_dfa calls ensure_acceptance_readable; ltlf_to_dfa is complete), and
// a state whose out-edges disagree would mean the automaton is not really
// state-based -- checked, not assumed.
std::vector<bool> finality(const spot::const_twa_graph_ptr& g) {
  std::vector<bool> fin(g->num_states(), false);
  for (unsigned s = 0; s < g->num_states(); ++s) {
    bool first = true, val = false;
    for (const auto& e : g->out(s)) {
      const bool m = g->acc().accepting(e.acc);
      if (first) { val = m; first = false; }
      else if (m != val)
        throw std::runtime_error("not state-based: state " + std::to_string(s));
    }
    fin[s] = val;
  }
  return fin;
}

struct Dfa {
  spot::const_twa_graph_ptr g;
  std::vector<bool> fin;
  bool final_at(int s) const { return s != kDead && fin[s]; }
};

}  // namespace

int main(int argc, char** argv) {
  std::string part_file, tin_file, formula_file;
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string f = argv[i];
    if (f == "--part-file") part_file = argv[i + 1];
    else if (f == "--transducer") tin_file = argv[i + 1];
    else if (f == "--formula") formula_file = argv[i + 1];
    else { std::cerr << "unrecognised flag: " << f << "\n"; return 2; }
  }
  if (part_file.empty() || tin_file.empty() || formula_file.empty()) {
    std::cerr << "usage: sw-cert --part-file P --transducer T --formula A\n";
    return 2;
  }

  std::ifstream pf(part_file);
  const ltlf_ek::VariablePartition vars = ltlf_ek::parse_partition_file(pf);
  auto dict = spot::make_bdd_dict();
  ltlf_ek::register_turn_order_aps(vars, dict);

  std::ifstream tf(tin_file);
  const ltlf_ek::OutputLabeledTransducer tin =
      ltlf_ek::parse_transducer(tf, vars, ltlf_ek::Role::t_in, dict);

  std::ifstream ff(formula_file);
  const std::string text((std::istreambuf_iterator<char>(ff)),
                         std::istreambuf_iterator<char>());
  auto pr = spot::parse_infix_psl(text);
  if (pr.format_errors(std::cerr)) return 2;

  const Dfa lhs{ltlf_ek::emits_dfa(tin, dict), {}};
  const Dfa rhs{ltlf_ek::ltlf_to_dfa(pr.f, dict), {}};
  Dfa a{lhs.g, finality(lhs.g)};
  Dfa b{rhs.g, finality(rhs.g)};

  std::cout << "states_emits_dfa_Tin " << a.g->num_states() << "\n"
            << "states_dfa_A " << b.g->num_states() << "\n"
            << "empty_word_Tin "
            << (a.final_at((int)a.g->get_init_state_number()) ? "accepts" : "rejects")
            << "\nempty_word_A "
            << (b.final_at((int)b.g->get_init_state_number()) ? "accepts" : "rejects")
            << "\n";

  // Synchronous product BFS.  Predecessor links give the shortest witness.
  using Pair = std::pair<int, int>;
  std::map<Pair, unsigned> seen;
  std::vector<Pair> order;
  std::vector<std::pair<int, bdd>> from;  // (predecessor index, letter guard)
  std::deque<unsigned> work;
  auto push = [&](Pair p, int pred, bdd letter) {
    auto [it, ins] = seen.emplace(p, order.size());
    if (!ins) return;
    order.push_back(p);
    from.push_back({pred, letter});
    work.push_back(it->second);
  };
  push({(int)a.g->get_init_state_number(), (int)b.g->get_init_state_number()},
       -1, bddtrue);

  int bad = -1;
  while (!work.empty() && bad < 0) {
    const unsigned cur = work.front();
    work.pop_front();
    const auto [sa, sb] = order[cur];
    // The EMPTY word (cur == 0) is excluded from the comparison and reported
    // on its own line: emits_dfa's initial state is final by construction (a
    // transducer's run of length 0 vacuously agrees with lambda), while the
    // repo's LTLf convention rejects the empty word, so the two disagree there
    // for EVERY formula.  That is a semantic mismatch of the two encodings of
    // "language", not a fact about this domain.
    if (cur != 0 && a.final_at(sa) != b.final_at(sb)) { bad = (int)cur; break; }

    // Out-edges of each side, plus the residual leading to kDead.
    std::vector<std::pair<bdd, int>> ea, eb;
    bdd cova = bddfalse, covb = bddfalse;
    if (sa != kDead)
      for (const auto& e : a.g->out(sa)) { ea.push_back({e.cond, (int)e.dst}); cova |= e.cond; }
    if (sb != kDead)
      for (const auto& e : b.g->out(sb)) { eb.push_back({e.cond, (int)e.dst}); covb |= e.cond; }
    ea.push_back({!cova, kDead});
    eb.push_back({!covb, kDead});

    for (const auto& [ga, da] : ea)
      for (const auto& [gb, db] : eb) {
        const bdd g = ga & gb;
        if (g == bddfalse) continue;
        if (da == kDead && db == kDead) continue;  // both stuck: absorbing.
        push({da, db}, (int)cur, g);
      }
  }

  if (bad < 0) {
    std::cout << "verdict EQUIVALENT_ON_NONEMPTY\nproduct_states " << order.size() << "\n";
    return 0;
  }

  std::vector<std::string> word;
  for (int i = bad; from[i].first >= 0; i = from[i].first) {
    std::ostringstream os;
    spot::bdd_print_formula(os, dict, from[i].second);
    word.push_back(os.str());
  }
  std::cout << "verdict DIFFERENT\nproduct_states " << order.size() << "\n"
            << "witness_length " << word.size() << "\nwitness";
  for (auto it = word.rbegin(); it != word.rend(); ++it)
    std::cout << " [" << *it << "]";
  std::cout << "\nside_Tin " << (a.final_at(order[bad].first) ? "accepts" : "rejects")
            << "\nside_A " << (b.final_at(order[bad].second) ? "accepts" : "rejects")
            << "\n";
  return 1;
}
