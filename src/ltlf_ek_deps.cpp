// ltlf-ek-deps --- CLI front end for the maximally-dependent-output search
// (docs/prd/output-dependencies-tool.md, \cref{def:outdep}).  Thin
// orchestration only: parse argv, assemble a VariablePartition, call
// dependent_outputs, format the result and (optionally) emit an updated part
// file and a Tout transducer file --- mirrors src/ltlf_ek_synth.cpp's style.
//
// Exit codes: 0 success (including Xdep = empty), 2 usage error, 3 phi
// unsatisfiable, 1 internal (PRD "Phase 3 -- the binary").

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <spot/tl/formula.hh>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>

#include "ltlf_ek/cli.hpp"
#include "ltlf_ek/dependent_outputs.hpp"
#include "ltlf_ek/detail/util.hpp"
#include "ltlf_ek/transducer_io.hpp"
#include "ltlf_ek/variables.hpp"

namespace {

using ltlf_ek::DependentOutputs;
using ltlf_ek::VariablePartition;

// A usage mistake (bad/missing/conflicting flags) --- distinct from an
// internal error or an unsatisfiable phi.  Maps to exit code 2.
class UsageError : public std::runtime_error {
 public:
  explicit UsageError(const std::string& msg) : std::runtime_error(msg) {}
};

struct CliArgs {
  std::optional<std::string> formula;
  std::optional<std::string> part_file;
  std::optional<std::string> inputs_csv;
  std::optional<std::string> outputs_csv;
  std::optional<std::string> emit_part;
  std::optional<std::string> transducer;
  bool verbose = false;
};

struct Flag {
  std::string name;
  std::optional<std::string> value;
};

// Split a `--name` / `--name=value` argv entry (same shape as
// src/ltlf_ek_synth.cpp's SplitFlag).
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
  // Every flag here is single-valued, so a repeat is a mistake, not an
  // override --- silently taking the last `--formula` of two is how a
  // scripted caller ends up analysing a formula it did not mean.
  std::set<std::string> seen;
  const std::vector<std::string> raw(argv + 1, argv + argc);
  for (std::size_t i = 0; i < raw.size(); ++i) {
    const Flag f = SplitFlag(raw[i]);
    if (!seen.insert(f.name).second)
      throw UsageError("--" + f.name + " given more than once");
    auto need_value = [&]() -> std::string {
      if (f.value) return *f.value;
      if (i + 1 >= raw.size())
        throw UsageError("--" + f.name + " requires a value");
      return raw[++i];
    };
    // Reject `--verbose=false`, which otherwise reads as "off" and turns
    // narration ON (the value is simply discarded for a boolean flag).
    auto reject_value = [&]() {
      if (f.value)
        throw UsageError("--" + f.name + " takes no value (got '" + *f.value +
                         "')");
    };

    if (f.name == "formula") {
      args.formula = need_value();
    } else if (f.name == "part-file") {
      args.part_file = need_value();
    } else if (f.name == "inputs") {
      args.inputs_csv = need_value();
    } else if (f.name == "outputs") {
      args.outputs_csv = need_value();
    } else if (f.name == "emit-part") {
      args.emit_part = need_value();
    } else if (f.name == "transducer") {
      args.transducer = need_value();
    } else if (f.name == "verbose") {
      reject_value();
      args.verbose = true;
    } else {
      throw UsageError("unrecognised flag: --" + f.name);
    }
  }
  return args;
}

// Comma-separated AP names -> a set (empty entries ignored), same as
// src/ltlf_ek_synth.cpp's SplitCsv.
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

// Do two path spellings name the same file?  Compared on the RESOLVED path:
// weakly_canonical folds away `.`, `..`, symlinks and relative-vs-absolute
// spellings without requiring the file to exist yet (an --emit-part target
// usually does not).  A raw string compare does not, and that gap is what let
// `--part-file ./p --emit-part p` walk through the guard below and rewrite
// the very file it had just read.
bool SameFile(const std::string& a, const std::string& b) {
  namespace fs = std::filesystem;
  if (a == b) return true;
  std::error_code ec;
  const fs::path ca = fs::weakly_canonical(fs::absolute(a, ec), ec);
  if (ec) return false;
  std::error_code ec2;
  const fs::path cb = fs::weakly_canonical(fs::absolute(b, ec2), ec2);
  if (ec2) return false;
  return ca == cb;
}

// Assemble the VariablePartition from exactly one of the two mutually
// exclusive sources (mirrors src/ltlf_ek_synth.cpp's BuildPartition).  I9's
// "refuse a non-empty output_known on input" is NOT re-checked here ---
// dependent_outputs already refuses it (std::invalid_argument), and every
// std::invalid_argument other than UnsatisfiableFormula maps to exit 2 in
// main(), so the refusal still lands on the right exit code without
// duplicating the check.
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

// Join a set of AP names with ", ", for the one-line stdout summary.
std::string Join(const std::set<std::string>& names) {
  std::string s;
  bool first = true;
  for (const auto& n : names) {
    if (!first) s += ", ";
    s += n;
    first = false;
  }
  return s;
}

// One output file, composed in memory before anything touches the disk.
struct PendingArtifact {
  std::string path;
  std::string content;
  const char* what;
};

// Commit every artifact or none.  Each payload goes to a sibling temp file
// first; only once ALL of them are written and checked are they renamed into
// place.
//
// This is not tidiness.  The emitted part file declares output_known = Xdep,
// and ltlf-ek-synth REFUSES that without a companion
// --known-output-transducer, so a run that wrote the part file and then
// failed on the transducer leaves behind a pair that breaks the pipeline this
// tool exists to feed.  Worse, plain `ofstream out(path)` truncates on open,
// so the old sequential writer destroyed a co-managed file before it knew
// whether it could fill it --- exactly what I9's edge case forbids.
//
// The residual window is between two renames rather than across two whole
// writes; a same-directory rename of a file we just created is about as close
// to atomic as the standard library gets.
void CommitArtifacts(const std::vector<PendingArtifact>& artifacts) {
  namespace fs = std::filesystem;
  std::vector<fs::path> temps;
  auto discard_temps = [&temps]() {
    std::error_code ec;
    for (const auto& t : temps) fs::remove(t, ec);
  };

  for (const auto& a : artifacts) {
    const fs::path temp = a.path + ".ltlf-ek-deps.tmp";
    std::ofstream out(temp);
    if (!out) {
      discard_temps();
      throw UsageError(std::string("cannot open ") + a.what +
                       " for writing: " + a.path);
    }
    out << a.content;
    out.close();
    // Checked AFTER close: a write can fail late, on the final flush, and an
    // unchecked stream reports success on a file left truncated by ENOSPC.
    if (!out) {
      discard_temps();
      throw UsageError(std::string("failed writing ") + a.what + ": " +
                       a.path);
    }
    temps.push_back(temp);
  }

  for (std::size_t i = 0; i < artifacts.size(); ++i) {
    std::error_code ec;
    fs::rename(temps[i], artifacts[i].path, ec);
    if (ec) {
      discard_temps();
      throw UsageError(std::string("cannot install ") + artifacts[i].what +
                       ": " + artifacts[i].path + " (" + ec.message() + ")");
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const CliArgs args = ParseArgs(argc, argv);

    if (!args.formula) throw UsageError("--formula is required");
    // I9 edge case: a crash mid-write must not destroy the co-managed part
    // file, so refuse writing --emit-part onto the path we read --part-file
    // from.  Resolved-path comparison, not string equality (see SameFile).
    if (args.part_file && args.emit_part &&
        SameFile(*args.part_file, *args.emit_part))
      throw UsageError(
          "--emit-part must differ from --part-file (refusing to overwrite "
          "the file being read)");
    // The two outputs would otherwise silently clobber each other.
    if (args.emit_part && args.transducer &&
        SameFile(*args.emit_part, *args.transducer))
      throw UsageError("--emit-part and --transducer must differ");

    const VariablePartition partition = BuildPartition(args);

    spot::formula phi;
    try {
      phi = spot::parse_formula(*args.formula);
    } catch (const std::runtime_error& e) {
      throw UsageError(std::string("could not parse --formula: ") + e.what());
    }

    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();

    // --verbose narrates the REAL greedy search, through dependent_outputs'
    // observer hook rather than a local re-derivation of it, so the narration
    // cannot drift from the verdict printed below.  It goes to stderr: stdout
    // carries the one machine-readable line the PRD specifies, nothing else.
    ltlf_ek::CandidateObserver on_candidate;
    if (args.verbose)
      on_candidate = [](const std::string& z, bool accepted,
                        const std::optional<std::string>& undetermined) {
        if (accepted)
          std::cerr << "candidate " << z << ": accepted\n";
        else
          std::cerr << "candidate " << z
                    << ": rejected (undetermined: " << *undetermined << ")\n";
      };

    DependentOutputs result;
    try {
      result = ltlf_ek::dependent_outputs(phi, partition, dict, on_candidate);
    } catch (const ltlf_ek::UnsatisfiableFormula& e) {
      // The one dependent_outputs failure with its own exit code (PRD "Edge
      // cases"); its own exception type, so no other invalid_argument can be
      // mistaken for it.  Thrown before the greedy loop runs, so --verbose
      // has narrated nothing yet.
      std::cerr << "error: " << e.what() << "\n";
      return 3;
    }

    // Compose both artifacts before writing either (see CommitArtifacts).
    std::vector<PendingArtifact> artifacts;
    if (args.emit_part) {
      std::ostringstream part;
      ltlf_ek::print_partition_file(part, result.partition);
      artifacts.push_back({*args.emit_part, part.str(), "--emit-part"});
    }
    if (args.transducer) {
      if (result.t_out) {
        std::ostringstream t_out;
        ltlf_ek::print_transducer(t_out, *result.t_out);
        artifacts.push_back({*args.transducer, t_out.str(), "--transducer"});
      } else {
        // Xdep = empty (Edge cases): no Tout to build --- write no file
        // rather than a trivial one, matching --known-output-transducer's
        // rejection of an empty output_known in ltlf-ek-synth.
        std::cerr << "note: dependent outputs is empty; no --transducer file "
                     "written\n";
      }
    }
    CommitArtifacts(artifacts);

    // Printed only once every artifact is on disk, so the success line never
    // outlives a failed run.
    const std::set<std::string> outputs = partition.outputs();
    if (result.dependent.empty()) {
      std::cout << "dependent outputs: none\n";
    } else {
      std::cout << "dependent outputs: " << Join(result.dependent) << "   (of "
                << Join(outputs) << ")\n";
    }

    return 0;
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
