#pragma once

#include <istream>
#include <memory>
#include <ostream>
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

// The exact inverse of parse_partition_file above (docs/prd/
// output-dependencies-tool.md "Phase 3 -- the binary"): write the four keys,
// one per line, in the same "key: value" shape, so
// parse_partition_file(print_partition_file(p)) == p.  A key with an empty
// set is still written (as `key:` with nothing after the colon), matching the
// reader's "a missing key or an empty value is the empty set" rule --- either
// spelling round-trips, so the writer always uses the explicit form.
void print_partition_file(std::ostream& out, const VariablePartition& p);

// Construct the Synthesis method named by a CLI method flag with its leading
// `--` stripped, e.g. "dfa-product" (docs/GLOSSARY.md "The five methods").
// Eight flags over five methods: a flag names a method x Representation CELL,
// never a method.  "mtdfa-product" and "mtnfa-product" are SECOND
// implementations of Methods 2 and 1 (the mtdfa Representation,
// docs/prd/mtdfa-product.md / docs/prd/mtnfa-product.md), not sixth/seventh
// methods --- DfaProduct / NfaProduct stay the explicit-Representation cells
// of those same methods.  "otf-mtdfa-product" is NOT a second implementation:
// Method 3.1's explicit cell (OtfDfaProduct) is unbuilt and "otf-dfa-product"
// is still in kRecognisedNotWired (src/cli.cpp), so OtfMtdfaProduct is that
// method's ONLY implementation (docs/prd/otf-mtdfa-product.md).
// "dfa-product", "mtdfa-product", "nfa-product", "mtnfa-product" and
// "otf-mtdfa-product" are wired today (-> DfaProduct, MtdfaProduct,
// NfaProduct, MtnfaProduct, OtfMtdfaProduct respectively,
// docs/prd/nfa-product.md / docs/prd/mtnfa-product.md /
// docs/prd/otf-mtdfa-product.md); the other three recognised method names
// throw std::logic_error("... not yet implemented"); any other name throws
// std::invalid_argument("unrecognised method").
//
// `minimize_mtdfa` (Phase 2, docs/prd/mtdfa-product.md "Benchmarking") is the
// MtdfaProduct-only knob wired from the CLI's `--minimize-mtdfa` flag; it is
// forwarded to MtdfaProduct's constructor and ignored by every other method.
std::unique_ptr<Synthesis> make_synthesis_method(
    const std::string& method_flag, bool minimize_mtdfa = false);

}  // namespace ltlf_ek
