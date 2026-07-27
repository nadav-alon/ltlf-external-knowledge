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
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <bddx.h>
#include <spot/tl/formula.hh>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/hoa.hh>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/cli.hpp"
#include "ltlf_ek/detail/util.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/transducer_io.hpp"
#include "ltlf_ek/turn_order.hpp"
#include "ltlf_ek/variables.hpp"
#include "ltlf_ek/verify_controller.hpp"

namespace {

using ltlf_ek::Controller;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::Role;
using ltlf_ek::Synthesis;
using ltlf_ek::VariablePartition;
using ltlf_ek::Witness;

// The method flags this CLI recognises: the five methods (docs/GLOSSARY.md
// "The five methods") plus "mtdfa-product" and "mtnfa-product", second
// implementations of Methods 2 and 1 over the mtdfa Representation
// (docs/prd/mtdfa-product.md / docs/prd/mtnfa-product.md) --- SEVEN flags over
// five methods.  "dfa-product", "mtdfa-product", "nfa-product" and
// "mtnfa-product" are wired (ltlf_ek::make_synthesis_method); the other three
// are not yet implemented.  This list is the SECOND site a new flag must be
// added to --- make_synthesis_method (src/cli.cpp) alone is not enough, since
// ParseArgs rejects an unlisted flag before ever consulting the factory (the
// domain-review D1 finding in docs/prd/mtnfa-product.md).
const std::vector<std::string> kMethodFlags = {
    "dfa-product",     "mtdfa-product",   "nfa-product",     "mtnfa-product",
    "otf-dfa-product", "otf-agg-product", "otf-dyn-agg-product"};

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
  std::optional<std::string> controller;  // --controller F (Role::t_c file).
  bool realizable = false;
  std::optional<std::string> benchmark_file;  // --benchmark=FILE (docs/prd/
                                              // benchmarking.md).
  bool minimize_mtdfa = false;  // --minimize-mtdfa (docs/prd/mtdfa-product.md
                                // Phase 2); MtdfaProduct-only, ignored by
                                // every other method.
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
    } else if (f.name == "controller") {
      args.controller = need_value();
    } else if (f.name == "realizable") {
      args.realizable = true;
    } else if (f.name == "benchmark") {
      args.benchmark_file = need_value();
    } else if (f.name == "minimize-mtdfa") {
      args.minimize_mtdfa = true;
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
    const std::string trimmed = ltlf_ek::detail::trim(tok);
    if (!trimmed.empty()) out.insert(trimmed);
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

// One agreeing letter, printed as its positive-literal AP set (e.g. "{a,c}")
// --- a --model-check-only presentation choice, not a domain format (PRD
// "CLI --model-check wiring" leaves the exact witness text unspecified).
std::string FormatLetter(bdd v, const std::vector<std::string>& aps,
                         const spot::twa_graph_ptr& registrar) {
  std::string s = "{";
  bool first = true;
  for (const auto& ap : aps) {
    if ((v & bdd_ithvar(registrar->register_ap(ap))) == bddfalse) continue;
    if (!first) s += ",";
    s += ap;
    first = false;
  }
  s += "}";
  return s;
}

// Print a counterexample lasso (docs/GLOSSARY.md "Controller verifier"):
// the prefix letters, then the repeating cycle letters (empty => dead-end).
void PrintWitness(std::ostream& os, const Witness& w,
                  const VariablePartition& partition,
                  const spot::twa_graph_ptr& registrar) {
  // Materialize inputs() into a local first: it returns a fresh set by value,
  // so `inputs().begin()`/`inputs().end()` would be iterators into two
  // distinct temporaries (a mismatched range -> UB / infinite loop).
  const std::set<std::string> ins = partition.inputs();
  const std::set<std::string> outs = partition.outputs();
  std::vector<std::string> aps(ins.begin(), ins.end());
  aps.insert(aps.end(), outs.begin(), outs.end());
  std::sort(aps.begin(), aps.end());

  os << "prefix:";
  for (const bdd& v : w.prefix) os << " " << FormatLetter(v, aps, registrar);
  os << "\n";
  os << "cycle:";
  for (const bdd& v : w.cycle) os << " " << FormatLetter(v, aps, registrar);
  os << "\n";
}

// Construct the method named by `flag`, printing and swallowing an unwired
// method's std::logic_error so both call sites in main() can react to a null
// return with the shared exit-1 "not yet implemented" handling.
// `minimize_mtdfa` (docs/prd/mtdfa-product.md Phase 2) is forwarded to
// make_synthesis_method; only MtdfaProduct reads it.
std::unique_ptr<Synthesis> BuildMethodOrNull(const std::string& flag,
                                             bool minimize_mtdfa) {
  try {
    return ltlf_ek::make_synthesis_method(flag, minimize_mtdfa);
  } catch (const std::logic_error& e) {
    std::cerr << e.what() << "\n";
    return nullptr;
  }
}

// RAII emit-guard (docs/prd/benchmarking.md "CLI" section, "Recommended
// implementation"): writes the accumulated report to --benchmark=FILE on
// every completion path uniformly --- realizable, unrealizable, or a thrown
// internal error (best-effort/partial, PRD "Edge cases"). Must be declared
// *after* the BenchScope it reads so it destructs first (while the scope is
// still alive, per BenchScope::report()'s "while alive or from dtor path"
// contract) --- ordinary reverse-construction-order RAII. A write failure is
// a stderr warning only; it never touches main()'s exit code.
class BenchmarkEmitGuard {
 public:
  BenchmarkEmitGuard(const std::optional<std::string>& file,
                     const std::optional<ltlf_ek::BenchScope>& scope)
      : file_(file), scope_(scope) {}

  ~BenchmarkEmitGuard() {
    if (!file_ || !scope_) return;
    std::ofstream out(*file_);
    bool ok = static_cast<bool>(out);
    if (ok) {
      scope_->report().to_json(out);
      ok = static_cast<bool>(out);
    }
    if (!ok) {
      std::cerr << "warning: could not write --benchmark report to " << *file_
                << "\n";
    }
  }

  BenchmarkEmitGuard(const BenchmarkEmitGuard&) = delete;
  BenchmarkEmitGuard& operator=(const BenchmarkEmitGuard&) = delete;

 private:
  const std::optional<std::string>& file_;
  const std::optional<ltlf_ek::BenchScope>& scope_;
};

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

    // --benchmark=FILE (docs/prd/benchmarking.md): one whole-run BenchScope
    // (parse -> build transducers -> synthesize). Constructed only when
    // requested, so a non-benchmarked run installs no collector at all.
    // bench_emit must be declared after bench_scope so it destructs first
    // (LIFO), reading the report while the scope is still alive.
    std::optional<ltlf_ek::BenchScope> bench_scope;
    if (args.benchmark_file) bench_scope.emplace();
    const BenchmarkEmitGuard bench_emit(args.benchmark_file, bench_scope);

    const VariablePartition partition = BuildPartition(args);
    ValidateKnownTransducerFlags(partition, args);

    // Free-form input_parsing span (PRD "Behaviour": "around formula +
    // transducer parsing"); a no-op when no BenchScope is active, so this
    // stays unconditional. Closed explicitly (input_timer.reset()) once the
    // transducers are built, below.
    std::optional<ltlf_ek::BenchTimer> input_timer;
    input_timer.emplace(std::string("input_parsing"));

    spot::formula phi;
    try {
      phi = spot::parse_formula(*args.formula);
    } catch (const std::runtime_error& e) {
      throw UsageError(std::string("could not parse --formula: ") + e.what());
    }

    // One shared bdd_dict for phi's DFA, both transducers, and (--model-check)
    // T_C (PRD "Behaviour" #1, docs/prd/controller-verifier.md "bdd_dict
    // discipline").  Keep a scratch registrar graph alive for the whole run so
    // every I∪O AP is registered on the dict up front, even one absent from
    // phi and from both transducers (PRD "Edge cases": "Partition AP absent
    // from phi and from both transducers").
    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    const spot::twa_graph_ptr ap_registrar = spot::make_twa_graph(dict);
    // Phase 0/Q2 (docs/prd/mtdfa-product.md "Turn order"): register in the
    // MTDFA-game-correct order --- Ifree strictly above every controllable
    // (Ofree u Iknown u Oknown).  register_ap is idempotent, so this is also
    // correct for DfaProduct: with V = {} (Iknown = Oknown = {}) the order
    // collapses to exactly Ifree-then-Ofree, identical to the loops this
    // replaces.
    ltlf_ek::register_turn_order_aps(partition, dict);

    const OutputLabeledTransducer t_in = BuildTransducer(
        args.known_input_transducer, partition, Role::t_in, dict);
    const OutputLabeledTransducer t_out = BuildTransducer(
        args.known_output_transducer, partition, Role::t_out, dict);

    input_timer.reset();  // input_parsing span ends here.

    if (args.model_check) {
      // --controller F: check a given artifact --- short-circuits before
      // method dispatch (docs/prd/controller-verifier.md "CLI --model-check
      // wiring"), so an unwired --<method> is never even constructed here.
      std::optional<OutputLabeledTransducer> file_t_c;
      const ltlf_ek::Transducer* t_c = nullptr;
      if (args.controller) {
        std::ifstream in = OpenOrThrow(*args.controller, "--controller");
        file_t_c = ltlf_ek::parse_transducer(in, partition, Role::t_c, dict);
        t_c = &*file_t_c;
      } else {
        // Self-check: --controller omitted, so the method must synthesize a
        // controller first.
        std::unique_ptr<Synthesis> method = BuildMethodOrNull(
            args.method_flags.front(), args.minimize_mtdfa);
        if (!method) return 1;
        const std::optional<Controller> result =
            method->synthesize(phi, partition, t_in, t_out);
        // No controller to model-check when the spec is unrealizable: report
        // it with the standard unrealizable verdict (stderr / exit 20) rather
        // than dereferencing an empty optional.  (The PRD's CLI sketch used
        // `.value()` and left this path unspecified.)
        if (!result) {
          std::cerr << "UNREALIZABLE\n";
          return 20;
        }
        file_t_c = ltlf_ek::controller_as_transducer(*result, partition);
        t_c = &*file_t_c;
      }

      const ltlf_ek::VerifyResult r =
          ltlf_ek::verify_controller(phi, partition, t_in, t_out, *t_c);
      if (r.ok) {
        std::cout << "SAFE\n";
        return 0;
      }
      std::cout << "UNSAFE\n";
      PrintWitness(std::cout, *r.counterexample, partition, ap_registrar);
      return 20;
    }

    std::unique_ptr<Synthesis> method =
        BuildMethodOrNull(args.method_flags.front(), args.minimize_mtdfa);
    if (!method) return 1;

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
