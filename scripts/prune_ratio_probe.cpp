// Prune-ratio probe for the SYNTCOMP-fin re-mine (docs/research/
// 2026-11-22-syntcomp-fin-prune-ratio.md).  One instance per process, so the
// driver (scripts/prune_ratio_sweep.py) can cap memory with `ulimit -v` and
// time with its own timeout.
//
// Deliberately NOT a CMake target (precedent: scripts/otf_game_solving_gap_probe.cpp).
// Compile out of tree against a Release build/libltlf_ek.a:
//
//   c++ -std=c++20 -O2 -I include -isystem /usr/local/include \
//       scripts/prune_ratio_probe.cpp build/libltlf_ek.a \
//       -L /usr/local/lib -lspot -lbddx -Wl,-rpath,/usr/local/lib \
//       -o build/prune-ratio-probe
//
//   prune-ratio-probe FORMULA_FILE PART_FILE
//
// Extracts the maximally dependent outputs of the formula (dependent_outputs),
// round-trips the resulting T_out through its file format (as ltlf-ek-deps
// --transducer followed by ltlf-ek-synth --known-output-transducer would),
// then runs DfaProduct twice on the same formula: with the extracted
// knowledge (K) and with the knowledge-free partition (U, the
// `dfa-product-unpruned` baseline).  Prints one JSON object on stdout:
// `product_states` is the reachable explicit product, the prune ratio is K/U.
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/cli.hpp"
#include "ltlf_ek/dependent_outputs.hpp"
#include "ltlf_ek/dfa_product.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/transducer_io.hpp"
#include "ltlf_ek/turn_order.hpp"
#include "ltlf_ek/variables.hpp"

using namespace ltlf_ek;
using Clock = std::chrono::steady_clock;

namespace {

double Since(Clock::time_point t) {
  return std::chrono::duration<double>(Clock::now() - t).count();
}

std::string Slurp(const char* path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error(std::string("cannot open ") + path);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

struct Measured {
  std::optional<std::uint64_t> product_states;
  std::optional<std::uint64_t> goal_dfa_states;
  bool realizable = false;
  double seconds = 0;
};

Measured RunProduct(const spot::formula& phi, const VariablePartition& vars,
                    const OutputLabeledTransducer& t_in,
                    const OutputLabeledTransducer& t_out) {
  Measured m;
  const auto t0 = Clock::now();
  BenchScope scope;
  DfaProduct method;
  m.realizable = method.synthesize(phi, vars, t_in, t_out).has_value();
  for (const auto& x : scope.report().metrics) {
    if (x.label == "product_states") m.product_states = x.value;
    if (x.label == "goal_dfa_states") m.goal_dfa_states = x.value;
  }
  m.seconds = Since(t0);
  return m;
}

void Emit(const char* key, const Measured& m) {
  std::cout << ",\"" << key << "\":{\"product_states\":";
  if (m.product_states) std::cout << *m.product_states; else std::cout << "null";
  std::cout << ",\"goal_dfa_states\":";
  if (m.goal_dfa_states) std::cout << *m.goal_dfa_states; else std::cout << "null";
  std::cout << ",\"realizable\":" << (m.realizable ? "true" : "false")
            << ",\"seconds\":" << m.seconds << "}";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: prune-ratio-probe FORMULA_FILE PART_FILE\n";
    return 2;
  }
  try {
    const spot::formula phi = spot::parse_formula(Slurp(argv[1]));
    std::istringstream part_in(Slurp(argv[2]));
    const VariablePartition partition = parse_partition_file(part_in);

    const auto t0 = Clock::now();
    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    DependentOutputs dep;
    try {
      dep = dependent_outputs(phi, partition, dict);
    } catch (const UnsatisfiableFormula&) {
      std::cout << "{\"status\":\"EMPTYLANG\"}\n";
      return 0;
    }
    const double extract_s = Since(t0);
    std::cout << "{\"status\":\"OK\",\"known_outputs\":" << dep.dependent.size()
              << ",\"outputs\":" << partition.outputs().size()
              << ",\"extract_seconds\":" << extract_s;
    std::cout.flush();
    if (dep.dependent.empty()) {
      std::cout << ",\"note\":\"no dependent output: K == U by construction\"}\n";
      return 0;
    }

    std::ostringstream tfile;
    print_transducer(tfile, *dep.t_out);

    const spot::bdd_dict_ptr kdict = spot::make_bdd_dict();
    register_turn_order_aps(dep.partition, kdict);
    std::istringstream tin(tfile.str());
    const OutputLabeledTransducer k_out =
        parse_transducer(tin, dep.partition, Role::t_out, kdict);
    const OutputLabeledTransducer k_in =
        trivial_transducer(dep.partition, Role::t_in, kdict);
    Emit("k", RunProduct(phi, dep.partition, k_in, k_out));
    std::cout.flush();

    const spot::bdd_dict_ptr udict = spot::make_bdd_dict();
    register_turn_order_aps(partition, udict);
    const OutputLabeledTransducer u_in =
        trivial_transducer(partition, Role::t_in, udict);
    const OutputLabeledTransducer u_out =
        trivial_transducer(partition, Role::t_out, udict);
    Emit("u", RunProduct(phi, partition, u_in, u_out));
    std::cout << "}\n";
    return 0;
  } catch (const std::exception& e) {
    std::cout << "{\"status\":\"ERR\",\"what\":\"" << e.what() << "\"}\n";
    return 1;
  }
}
