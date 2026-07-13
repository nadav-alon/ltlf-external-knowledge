#include "ltlf_ek/bench.hpp"

#include <cassert>
#include <cstdio>
#include <ostream>
#include <utility>

// Collector mechanism (docs/prd/benchmarking.md "Interfaces & types",
// "Collector mechanism (pinned)"): a thread-local pointer to the live
// BenchScope's Collector, a stack of open spans, steady_clock nanoseconds.
namespace ltlf_ek {

std::string_view stage_name(Stage s) {
  switch (s) {
    case Stage::automaton_construction:
      return "automaton_construction";
    case Stage::product_construction:
      return "product_construction";
    case Stage::game_solving:
      return "game_solving";
    case Stage::aggregation:
      return "aggregation";
  }
  return "unknown_stage";
}

// The collector: a stack of open frames (spans still running) plus the
// finished top-level spans (`roots`). A BenchTimer pushes a frame on
// construction and pops it (appending the finished BenchSpan to the new
// top-of-stack frame's children, or to `roots` if the stack is now empty) on
// destruction.
class BenchCollector {
 public:
  void push(std::string label, bool canonical) {
    stack_.push_back(Frame{std::move(label), canonical,
                           std::chrono::steady_clock::now(), {}});
  }

  void pop() {
    Frame frame = std::move(stack_.back());
    stack_.pop_back();
    BenchSpan span{std::move(frame.label), frame.canonical,
                  std::chrono::steady_clock::now() - frame.start,
                  std::move(frame.children)};
    if (stack_.empty()) {
      roots_.push_back(std::move(span));
    } else {
      stack_.back().children.push_back(std::move(span));
    }
  }

  const std::vector<BenchSpan>& roots() const { return roots_; }

 private:
  struct Frame {
    std::string label;
    bool canonical;
    std::chrono::steady_clock::time_point start;
    std::vector<BenchSpan> children;
  };

  std::vector<Frame> stack_;
  std::vector<BenchSpan> roots_;
};

namespace {
// At most one active collector per thread (BenchCollector, owned by the live
// BenchScope). nullptr when no BenchScope is active --- the no-op path every
// BenchTimer checks.
thread_local BenchCollector* g_active_collector = nullptr;
}  // namespace

BenchScope::BenchScope()
    : collector_(std::make_unique<BenchCollector>()),
      start_(std::chrono::steady_clock::now()) {
  assert(g_active_collector == nullptr &&
        "BenchScope: nested install forbidden (one collector per thread)");
  g_active_collector = collector_.get();
}

BenchScope::~BenchScope() { g_active_collector = nullptr; }

BenchReport BenchScope::report() const {
  return BenchReport{std::chrono::steady_clock::now() - start_,
                     collector_->roots()};
}

BenchTimer::BenchTimer(Stage s) : active_(false) {
  if (g_active_collector == nullptr) return;
  active_ = true;
  g_active_collector->push(std::string(stage_name(s)), /*canonical=*/true);
}

BenchTimer::BenchTimer(std::string label) : active_(false) {
  if (g_active_collector == nullptr) return;
  active_ = true;
  g_active_collector->push(std::move(label), /*canonical=*/false);
}

BenchTimer::~BenchTimer() {
  if (!active_) return;
  g_active_collector->pop();
}

namespace {

// Minimal JSON string escaping --- labels are canonical stage names or
// caller-supplied free-form text, never attacker-controlled binary, but
// escape defensively so `to_json`'s output is always well-formed JSON.
void WriteJsonString(std::ostream& os, std::string_view s) {
  os << '"';
  for (const char c : s) {
    switch (c) {
      case '"':
        os << "\\\"";
        break;
      case '\\':
        os << "\\\\";
        break;
      case '\n':
        os << "\\n";
        break;
      case '\r':
        os << "\\r";
        break;
      case '\t':
        os << "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          os << buf;
        } else {
          os << c;
        }
    }
  }
  os << '"';
}

void WriteSpan(std::ostream& os, const BenchSpan& span) {
  os << "{\"label\":";
  WriteJsonString(os, span.label);
  os << ",\"canonical\":" << (span.canonical ? "true" : "false");
  os << ",\"duration_ns\":" << span.duration.count();
  os << ",\"children\":[";
  bool first = true;
  for (const BenchSpan& child : span.children) {
    if (!first) os << ",";
    first = false;
    WriteSpan(os, child);
  }
  os << "]}";
}

}  // namespace

void BenchReport::to_json(std::ostream& os) const {
  os << "{\"total_ns\":" << total.count() << ",\"roots\":[";
  bool first = true;
  for (const BenchSpan& span : roots) {
    if (!first) os << ",";
    first = false;
    WriteSpan(os, span);
  }
  os << "]}";
}

}  // namespace ltlf_ek
