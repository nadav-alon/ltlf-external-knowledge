// ltlf-ek-synth --- CLI front end for the Synthesis methods
// (docs/prd/cli-wrapper.md).  Thin orchestration only: parse argv, assemble a
// VariablePartition and the two external-knowledge transducers on one shared
// bdd_dict, dispatch to a Synthesis method, format the result.  No new
// synthesis semantics live here --- see include/ltlf_ek/cli.hpp for the
// library-level (unit-testable) pieces this file wires together.
//
// Exit codes: 0 realizable, 20 unrealizable, 2 usage error, 1 internal /
// not-yet-implemented (PRD "Behaviour" #6, "Edge cases").

#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <spot/tl/formula.hh>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/hoa.hh>

#include "ltlf_ek/cli.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/transducer_io.hpp"
#include "ltlf_ek/variables.hpp"

namespace {

using ltlf_ek::Controller;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::Role;
using ltlf_ek::Synthesis;
using ltlf_ek::VariablePartition;

// The five method flags this CLI recognises (docs/GLOSSARY.md "The five
// methods"); only "dfa-product" is wired (ltlf_ek::make_synthesis_method).
const std::vector<std::string> kMethodFlags = {
    "dfa-product", "nfa-product", "otf-dfa-product", "otf-agg-product",
    "otf-dyn-agg-product"};

// A usage mistake (bad/missing/conflicting flags, malformed input the user
// supplied) --- distinct from an internal/not-yet-implemented error.  Maps to
// exit code 2 (PRD "Edge cases").
class UsageError : public std::runtime_error {
 public:
  explicit UsageError(const std::string& msg) : std::runtime_error(msg) {}
};

struct CliArgs {
  std::vector<std::string> method_flags;
  std::optional<std::string> formula;
  std::optional<std::string> part_file;
  std::optional<std::string> inputs_csv;
  std::optional<std::string> outputs_csv;
  std::optional<std::string> known_input_transducer;
  std::optional<std::string> known_output_transducer;
  bool model_check = false;
  bool realizable = false;
};

struct Flag {
  std::string name;
  std::optional<std::string> value;
};

// Split a `--name` / `--name=value` argv entry.
Flag SplitFlag(const std::string& arg) {
  if (arg.size() < 3 || arg[0] != '-' || arg[1] != '-')
    throw UsageError("unrecognised argument (expected --flag): " + arg);
  const std::string body = arg.substr(2);
  const std::size_t eq = body.find('=');
  if (eq == std::string::npos) return {body, std::nullopt};
  return {body.substr(0, eq), body.substr(eq + 1)};
}

CliArgs ParseArgs(int argc, char** argv) {
  CliArgs args;
  const std::vector<std::string> raw(argv + 1, argv + argc);
  for (std::size_t i = 0; i < raw.size(); ++i) {
    const Flag f = SplitFlag(raw[i]);
    auto need_value = [&]() -> std::string {
      if (f.value) return *f.value;
      if (i + 1 >= raw.size())
        throw UsageError("--" + f.name + " requires a value");
      return raw[++i];
    };

    if (std::find(kMethodFlags.begin(), kMethodFlags.end(), f.name) !=
        kMethodFlags.end()) {
      args.method_flags.push_back(f.name);
    } else if (f.name == "formula") {
      args.formula = need_value();
    } else if (f.name == "part-file") {
      args.part_file = need_value();
    } else if (f.name == "inputs") {
      args.inputs_csv = need_value();
    } else if (f.name == "outputs") {
      args.outputs_csv = need_value();
    } else if (f.name == "known-input-transducer") {
      args.known_input_transducer = need_value();
    } else if (f.name == "known-output-transducer") {
      args.known_output_transducer = need_value();
    } else if (f.name == "model-check") {
      args.model_check = true;
    } else if (f.name == "realizable") {
      args.realizable = true;
    } else {
      throw UsageError("unrecognised flag: --" + f.name);
    }
  }
  return args;
}

// Comma-separated AP names -> a set (empty entries ignored, so both "" and a
// missing flag give the empty set).
std::set<std::string> SplitCsv(const std::string& csv) {
  std::set<std::string> out;
  std::istringstream ss(csv);
  std::string tok;
  while (std::getline(ss, tok, ',')) {
    const std::size_t a = tok.find_first_not_of(" \t");
    if (a == std::string::npos) continue;
    const std::size_t b = tok.find_last_not_of(" \t");
    out.insert(tok.substr(a, b - a + 1));
  }
  return out;
}

std::ifstream OpenOrThrow(const std::string& path, const char* what) {
  std::ifstream in(path);
  if (!in)
    throw UsageError(std::string("cannot open ") + what + ": " + path);
  return in;
}

// Assemble the VariablePartition from exactly one of the two mutually
// exclusive sources (PRD "Behaviour" #2, "Edge cases").
VariablePartition BuildPartition(const CliArgs& args) {
  const bool has_part_file = args.part_file.has_value();
  const bool has_inputs_outputs =
      args.inputs_csv.has_value() || args.outputs_csv.has_value();
  if (has_part_file == has_inputs_outputs)
    throw UsageError(
        "exactly one of --part-file or --inputs/--outputs is required");

  if (has_part_file) {
    std::ifstream in = OpenOrThrow(*args.part_file, "--part-file");
    return ltlf_ek::parse_partition_file(in);
  }

  const std::set<std::string> inputs = SplitCsv(args.inputs_csv.value_or(""));
  const std::set<std::string> outputs =
      SplitCsv(args.outputs_csv.value_or(""));
  std::vector<std::string> overlap;
  std::set_intersection(inputs.begin(), inputs.end(), outputs.begin(),
                        outputs.end(), std::back_inserter(overlap));
  if (!overlap.empty())
    throw UsageError("--inputs/--outputs overlap on '" + overlap.front() +
                     "' (I and O must be disjoint)");
  return VariablePartition::split(inputs, outputs, /*governed=*/{});
}

// Every known-set / transducer-flag combination other than "both empty" or
// "both given" is a usage error (PRD "Edge cases").
void ValidateKnownTransducerFlags(const VariablePartition& partition,
                                  const CliArgs& args) {
  if (!partition.input_known.empty() && !args.known_input_transducer)
    throw UsageError(
        "input_known is non-empty but --known-input-transducer is missing");
  if (partition.input_known.empty() && args.known_input_transducer)
    throw UsageError(
        "--known-input-transducer given but input_known is empty (ambiguous "
        "intent; use a part-file if a known input is intended)");
  if (!partition.output_known.empty() && !args.known_output_transducer)
    throw UsageError(
        "output_known is non-empty but --known-output-transducer is missing");
  if (partition.output_known.empty() && args.known_output_transducer)
    throw UsageError(
        "--known-output-transducer given but output_known is empty "
        "(ambiguous intent; use a part-file if a known output is intended)");
}

OutputLabeledTransducer BuildTransducer(
    const std::optional<std::string>& file, const VariablePartition& partition,
    Role role, const spot::bdd_dict_ptr& dict) {
  if (!file) return ltlf_ek::trivial_transducer(partition, role, dict);
  std::ifstream in = OpenOrThrow(
      *file, role == Role::t_in ? "--known-input-transducer"
                                : "--known-output-transducer");
  return ltlf_ek::parse_transducer(in, partition, role, dict);
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const CliArgs args = ParseArgs(argc, argv);

    if (args.method_flags.empty())
      throw UsageError("exactly one method flag is required (e.g. "
                       "--dfa-product)");
    if (args.method_flags.size() > 1)
      throw UsageError("more than one method flag given");
    if (!args.formula) throw UsageError("--formula is required");

    const VariablePartition partition = BuildPartition(args);
    ValidateKnownTransducerFlags(partition, args);

    if (args.model_check) {
      std::cerr << "model-check not yet implemented (see Verifier backlog "
                   "item)\n";
      return 1;
    }

    std::unique_ptr<Synthesis> method;
    try {
      method = ltlf_ek::make_synthesis_method(args.method_flags.front());
    } catch (const std::logic_error& e) {
      std::cerr << e.what() << "\n";
      return 1;
    }

    spot::formula phi;
    try {
      phi = spot::parse_formula(*args.formula);
    } catch (const std::runtime_error& e) {
      throw UsageError(std::string("could not parse --formula: ") + e.what());
    }

    // One shared bdd_dict for phi's DFA and both transducers (PRD "Behaviour"
    // #1).  Keep a scratch registrar graph alive for the whole run so every
    // I∪O AP is registered on the dict up front, even one absent from phi and
    // from both transducers (PRD "Edge cases": "Partition AP absent from phi
    // and from both transducers").
    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    const spot::twa_graph_ptr ap_registrar = spot::make_twa_graph(dict);
    for (const auto& ap : partition.inputs()) ap_registrar->register_ap(ap);
    for (const auto& ap : partition.outputs()) ap_registrar->register_ap(ap);

    const OutputLabeledTransducer t_in = BuildTransducer(
        args.known_input_transducer, partition, Role::t_in, dict);
    const OutputLabeledTransducer t_out = BuildTransducer(
        args.known_output_transducer, partition, Role::t_out, dict);

    const std::optional<Controller> result =
        method->synthesize(phi, partition, t_in, t_out);

    if (result) {
      if (args.realizable) {
        std::cout << "REALIZABLE\n";
      } else {
        spot::print_hoa(std::cout, result->strategy) << "\n";
      }
      return 0;
    }
    if (args.realizable) {
      std::cout << "UNREALIZABLE\n";
    } else {
      std::cerr << "UNREALIZABLE\n";
    }
    return 20;
  } catch (const UsageError& e) {
    std::cerr << "usage error: " << e.what() << "\n";
    return 2;
  } catch (const std::invalid_argument& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 2;
  } catch (const std::exception& e) {
    std::cerr << "internal error: " << e.what() << "\n";
    return 1;
  }
}
