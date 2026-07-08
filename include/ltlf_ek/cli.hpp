#pragma once

#include <istream>
#include <memory>
#include <string>

#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/variables.hpp"

// Library-level building blocks for the `ltlf-ek-synth` executable
// (docs/prd/cli-wrapper.md).  Split out of `main` so they are unit-testable
// without spawning a subprocess.  None of these are domain concepts (they are
// CLI plumbing over the existing Synthesis/Transducer interfaces), so none of
// them get a docs/GLOSSARY.md entry --- see the PRD's "Ubiquitous-language
// terms used" section.
namespace ltlf_ek {

// Read the CLI's part-file format into a VariablePartition
// (docs/prd/cli-wrapper.md "Part-file format"):
//
//   # comment; blank lines ignored
//   input_free:   a b
//   input_known:  c
//   output_free:  x
//   output_known: y
//
// Four keys (input_free / input_known / output_free / output_known), one per
// line; a value is a space-separated list of AP names; a missing key or an
// empty value is the empty set; `#` starts a comment (to end of line).
//
// Throws std::invalid_argument on: a line with no ':', an unrecognised key, a
// key repeated across lines, or an AP name listed in more than one of the four
// sets (the partition-disjointness invariant).
VariablePartition parse_partition_file(std::istream& in);

// Construct the Synthesis method named by a CLI method flag with its leading
// `--` stripped, e.g. "dfa-product" (docs/GLOSSARY.md "The five methods").
// Only "dfa-product" is wired today (-> DfaProduct); the other four recognised
// method names throw std::logic_error("... not yet implemented"); any other
// name throws std::invalid_argument("unrecognised method").
std::unique_ptr<Synthesis> make_synthesis_method(
    const std::string& method_flag);

}  // namespace ltlf_ek
