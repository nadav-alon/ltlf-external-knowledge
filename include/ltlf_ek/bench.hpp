#pragma once

#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// Benchmarking / stage timing (docs/prd/benchmarking.md).  Observability
// only: a thread-local RAII span collector that reads a clock and appends
// records; it never alters synthesis logic, so a run's controller / verdict
// is byte-identical whether timing is on or off.  Infra, not a domain
// concept (docs/GLOSSARY.md "Canonical benchmarking stage" covers only
// `Stage`/`stage_name`) --- following the include/ltlf_ek/cli.hpp precedent,
// BenchScope/BenchTimer/BenchSpan/BenchReport get no glossary entry.
namespace ltlf_ek {

// Canonical comparable stages --- the soft registry (see PRD "Behaviour").
// Deliberate to add (enum value + name row + glossary line); that is the ONLY
// infra a new *comparable axis* needs. A non-canonical sub-phase needs none.
enum class Stage {
  automaton_construction,   // LtlfToDfa / LtlfToNfa (Goal automaton build)
  product_construction,     // Product (build_product_symbolic + materialize)
  game_solving,              // SolveDfa
  aggregation,               // Methods 3.2/3.3 aggregate (reserved; unused here)
};

// Stable key string for a canonical stage (the JSON "label").
std::string_view stage_name(Stage s);

// One recorded span: a tree node. Parent wall-duration contains its children.
struct BenchSpan {
  std::string label;                    // stage_name(Stage) or a free string
  bool canonical;                       // true => label is a registry key
  std::chrono::nanoseconds duration;
  std::vector<BenchSpan> children;
};

// Canonical comparable size axes --- the same soft registry as Stage (PRD
// docs/prd/benchmark-suite.md B2, "Canonical size metric" in
// docs/GLOSSARY.md). Which method charges which value is the charge table
// in that PRD, not restated here.
enum class SizeMetric {
  goal_dfa_states,
  goal_nfa_states,
  nfa_product_states,
  product_states,
  product_bdd_nodes,
  controller_states,
};

std::string_view size_metric_name(SizeMetric m);

// Record one measurement into the active BenchScope's collector.
// No-op (near-zero cost) if no BenchScope is active --- the BenchTimer rule.
void record_size_metric(SizeMetric m, std::uint64_t value);
void record_size_metric(std::string label, std::uint64_t value);  // free-form tier

// One recorded measurement.
struct BenchSizeMetric {
  std::string label;      // size_metric_name(SizeMetric) or a free string
  bool canonical;         // true => label is a registry key
  std::uint64_t value;
};

// The report for one BenchScope lifetime.
struct BenchReport {
  std::chrono::nanoseconds total;       // the BenchScope's own wall lifetime
  std::vector<BenchSpan> roots;         // spans with no open parent
  std::vector<BenchSizeMetric> metrics; // flat: a metric belongs to the run,
                                         // not to a span (B2).
  // Structured nested JSON (schema in PRD "Test oracles"); integer nanoseconds.
  void to_json(std::ostream& os) const;
};

// Opaque: the thread-local collector state (stack of open spans + roots).
// Defined in bench.cpp; only BenchScope/BenchTimer touch it.
class BenchCollector;

// RAII: installs a thread-local collector for the dynamic scope of this object,
// and measures its own wall lifetime as report().total. Nested install is
// forbidden (asserts) --- there is at most one active collector per thread.
class BenchScope {
 public:
  BenchScope();
  ~BenchScope();
  BenchReport report() const;           // callable while alive or from dtor path
  BenchScope(const BenchScope&) = delete;
  BenchScope& operator=(const BenchScope&) = delete;

 private:
  std::unique_ptr<BenchCollector> collector_;
  std::chrono::steady_clock::time_point start_;
};

// RAII span guard. Opens a span in the active collector on construction, closes
// it on destruction. No-op (near-zero cost) if no BenchScope is active.
class BenchTimer {
 public:
  explicit BenchTimer(Stage s);            // canonical span   (canonical == true)
  explicit BenchTimer(std::string label);  // free-form sub-span (canonical == false)
  ~BenchTimer();
  BenchTimer(const BenchTimer&) = delete;
  BenchTimer& operator=(const BenchTimer&) = delete;

 private:
  bool active_;
};

}  // namespace ltlf_ek
