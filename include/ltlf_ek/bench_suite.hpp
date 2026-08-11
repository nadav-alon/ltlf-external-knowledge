#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <spot/tl/formula.hh>

#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/variables.hpp"

// The Phase 2 benchmark registry (docs/prd/benchmark-suite.md "Interfaces &
// types", "Phase 2 -- the registry"): families that instantiate a scaling
// series of (phi, VariablePartition, T_in, T_out) instances, subjects that
// run the five synthesis methods against a case, and a harvester that turns
// one BenchScope's report into rows. Infra, not a domain concept --- like
// bench.hpp's BenchScope/BenchTimer/BenchReport, this header's plumbing types
// (BenchParams, BenchCase, BenchFamily, BenchSubject, BenchRow) get no
// glossary entry (docs/GLOSSARY.md "Canonical size metric" C++ note).
// ComparabilityTier is the one domain type here --- see docs/GLOSSARY.md
// "Comparability tier".
namespace ltlf_ek {

// The declared expressibility class of a family's T_in (docs/GLOSSARY.md
// "Comparability tier", PRD B3). Declared as data by the family; nothing
// inspects a transducer at run time to decide one.
enum class ComparabilityTier { t1, t2, t3 };

std::string_view comparability_tier_name(ComparabilityTier t);

// One case's sweep parameters, e.g. {{"n", 6}, {"realizable", 1}}. A
// std::map (not unordered_map) so a row key is stable across runs (PRD
// "Interfaces & types").
using BenchParams = std::map<std::string, std::int64_t>;

// One fully-instantiated benchmark instance: a Goal formula, its variable
// partition, both knowledge transducers, and the declared comparability
// metadata a family commits to. t_in / t_out are the concrete
// OutputLabeledTransducer, not the abstract Transducer interface --- Transducer
// is pure-virtual (PRD "Interfaces & types" says implementation may sharpen
// the tentative Phase 2 shapes; see this PRD's "Developer comments" for the
// record of that sharpening), and a BenchCase must own its transducers by
// value to outlive instantiate()'s local dict/parse machinery.
struct BenchCase {
  std::string family;
  BenchParams params;
  spot::formula phi;
  VariablePartition vars;
  OutputLabeledTransducer t_in;
  OutputLabeledTransducer t_out;
  ComparabilityTier tier;              // DECLARED by the family (B3)
  std::optional<std::string> psi_in;   // required iff tier == ComparabilityTier::t1
  bool expected_realizable;
};

// A scaling family (PRD B4): a single sweep axis n plus a realizable flag,
// both prior harnesses' pattern. instantiate() must reject an n below the
// family's own floor (PRD "Edge cases": n=0/1 degenerate, the sweep starts at
// n=2) rather than emit a malformed formula.
class BenchFamily {
 public:
  virtual ~BenchFamily() = default;
  virtual std::string name() const = 0;
  virtual std::vector<BenchParams> sweep(std::int64_t n_min,
                                         std::int64_t n_max) const = 0;
  virtual BenchCase instantiate(const BenchParams& params) const = 0;
};

// What is being measured. "Run the five synthesis methods" is one
// implementation; a future "time ltlf-ek-deps" or "time LtlfToDfa alone" is
// another, added without touching the runner.
class BenchSubject {
 public:
  virtual ~BenchSubject() = default;
  virtual std::string name() const = 0;            // e.g. "dfa-product"
  virtual void run(const BenchCase& c) const = 0;   // called under a live BenchScope
};

const std::vector<std::unique_ptr<BenchFamily>>& bench_families();
const std::vector<std::unique_ptr<BenchSubject>>& bench_subjects();

// One measured row. Timings and metrics share the key shape --- key is
// either a canonical size_metric_name(...)/stage_name(...) string or a
// free-form label, exactly mirroring BenchSizeMetric::label / BenchSpan::label
// (bench.hpp).
struct BenchRow {
  std::string family;
  BenchParams params;
  std::string subject;
  std::string key;          // size_metric_name(...) or stage_name(...)
  std::uint64_t value;      // count, or nanoseconds
};

// Run one case under one subject and harvest its report into rows. Opens
// exactly one BenchScope (nesting asserts, bench.hpp) so a BenchSubject::run
// implementation must not open its own. A subject that skips cleanly (e.g. a
// MONA-dependent subject with mona absent, PRD "Edge cases") records nothing,
// so this returns an empty vector rather than a zero row --- consistent with
// B2 rule 1, "an absent metric is absent, never zero", extended to an absent
// method run.
std::vector<BenchRow> run_bench_case(const BenchCase& c, const BenchSubject& s);

}  // namespace ltlf_ek
