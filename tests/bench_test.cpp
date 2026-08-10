// Pre-2.13 op::strong_X opt-in (docs/prd/generated-corpus-oracle.md "Formula
// generation"): must precede the *first* transitive inclusion of
// <spot/tl/formula.hh> below, hence this file's very first lines.
#define SPOT_USES_STRONG_X 1

#include <cctype>
#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/misc/optionmap.hh>
#include <spot/tl/defaultenv.hh>
#include <spot/tl/parse.hh>
#include <spot/tl/randomltl.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/hoa.hh>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/dfa_product.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/variables.hpp"

#include "support/fixtures.hpp"

// Tests for docs/prd/benchmarking.md, bound to the PRD's frozen "Interfaces &
// types" (Stage / stage_name / BenchSpan / BenchReport / BenchScope /
// BenchTimer) and "Test oracles" section, written on the concurrent-workflow
// test-writer branch *before* include/ltlf_ek/bench.hpp / src/bench.cpp
// necessarily exist -- so this file may not compile/link until the developer
// branch lands them (this is expected; the launcher integrates the two).
//
// Timing is non-deterministic (PRD "Test oracles"): every assertion below is
// on structure / ordering / non-negativity / containment, never an absolute
// duration.
namespace {

using ltlf_ek::BenchReport;
using ltlf_ek::BenchScope;
using ltlf_ek::BenchSpan;
using ltlf_ek::BenchTimer;
using ltlf_ek::Controller;
using ltlf_ek::DfaProduct;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::Role;
using ltlf_ek::Stage;
using ltlf_ek::VariablePartition;
using ltlf_ek::stage_name;
using ltlf_ek::trivial_transducer;

using ltlf_ek::test_support::IoFreeVars;
using ltlf_ek::test_support::Phi;
using ltlf_ek::test_support::TinAlwaysI;

// ---------------------------------------------------------------------------
// Minimal schema-only JSON parser (PRD "Test oracles": "`to_json` schema ---
// ... Assert keys/structure, never values"). No production JSON library is
// linked in this project, so this is a small test-local utility, duplicated
// rather than shared across translation units (this project's one-file-per-
// suite norm, see tests/ltlfsynt_oracle_test.cpp's ShellQuote/CliResult
// comment). It supports exactly the JSON subset the frozen schema needs:
// objects, arrays, strings, numbers (tracking whether the literal looked
// integer-shaped), booleans, null.
// ---------------------------------------------------------------------------

struct JsonValue {
  enum class Kind { kObject, kArray, kString, kNumber, kBool, kNull };
  Kind kind = Kind::kNull;
  std::map<std::string, JsonValue> object_fields;
  std::vector<JsonValue> array_items;
  std::string string_value;
  bool bool_value = false;
  bool number_is_integer = false;  // no '.'/exponent in the literal.
};

class JsonParser {
 public:
  explicit JsonParser(std::string text) : text_(std::move(text)) {}

  JsonValue Parse() {
    SkipWhitespace();
    JsonValue v = ParseValue();
    SkipWhitespace();
    if (pos_ != text_.size())
      throw std::runtime_error(
          "trailing content after the JSON value at offset " +
          std::to_string(pos_));
    return v;
  }

 private:
  std::string text_;
  std::size_t pos_ = 0;

  char Peek() const {
    if (pos_ >= text_.size())
      throw std::runtime_error("unexpected end of JSON input");
    return text_[pos_];
  }
  void Expect(char c) {
    if (Peek() != c)
      throw std::runtime_error(std::string("expected '") + c +
                               "' at offset " + std::to_string(pos_));
    ++pos_;
  }
  bool Consume(const std::string& literal) {
    if (text_.compare(pos_, literal.size(), literal) == 0) {
      pos_ += literal.size();
      return true;
    }
    return false;
  }
  void SkipWhitespace() {
    while (pos_ < text_.size() &&
          std::isspace(static_cast<unsigned char>(text_[pos_])))
      ++pos_;
  }

  JsonValue ParseValue() {
    SkipWhitespace();
    const char c = Peek();
    if (c == '{') return ParseObject();
    if (c == '[') return ParseArray();
    if (c == '"') return ParseString();
    if (c == 't' || c == 'f') return ParseBool();
    if (c == 'n') return ParseNull();
    return ParseNumber();
  }

  JsonValue ParseObject() {
    Expect('{');
    JsonValue v;
    v.kind = JsonValue::Kind::kObject;
    SkipWhitespace();
    if (Peek() == '}') { ++pos_; return v; }
    while (true) {
      SkipWhitespace();
      const JsonValue key = ParseString();
      SkipWhitespace();
      Expect(':');
      JsonValue val = ParseValue();
      v.object_fields.emplace(key.string_value, std::move(val));
      SkipWhitespace();
      if (Peek() == ',') { ++pos_; continue; }
      Expect('}');
      break;
    }
    return v;
  }

  JsonValue ParseArray() {
    Expect('[');
    JsonValue v;
    v.kind = JsonValue::Kind::kArray;
    SkipWhitespace();
    if (Peek() == ']') { ++pos_; return v; }
    while (true) {
      v.array_items.push_back(ParseValue());
      SkipWhitespace();
      if (Peek() == ',') { ++pos_; continue; }
      Expect(']');
      break;
    }
    return v;
  }

  JsonValue ParseString() {
    Expect('"');
    JsonValue v;
    v.kind = JsonValue::Kind::kString;
    std::string out;
    while (true) {
      if (pos_ >= text_.size())
        throw std::runtime_error("unterminated JSON string");
      const char c = text_[pos_++];
      if (c == '"') break;
      if (c == '\\') {
        if (pos_ >= text_.size())
          throw std::runtime_error("unterminated JSON string escape");
        const char esc = text_[pos_++];
        switch (esc) {
          case '"': out += '"'; break;
          case '\\': out += '\\'; break;
          case '/': out += '/'; break;
          case 'n': out += '\n'; break;
          case 't': out += '\t'; break;
          case 'r': out += '\r'; break;
          case 'b': out += '\b'; break;
          case 'f': out += '\f'; break;
          case 'u': pos_ += 4; out += '?'; break;  // schema has no \u escapes.
          default:
            throw std::runtime_error("unsupported JSON string escape");
        }
      } else {
        out += c;
      }
    }
    v.string_value = out;
    return v;
  }

  JsonValue ParseBool() {
    JsonValue v;
    v.kind = JsonValue::Kind::kBool;
    if (Consume("true")) { v.bool_value = true; return v; }
    if (Consume("false")) { v.bool_value = false; return v; }
    throw std::runtime_error("expected true/false at offset " +
                             std::to_string(pos_));
  }

  JsonValue ParseNull() {
    if (!Consume("null"))
      throw std::runtime_error("expected null at offset " +
                               std::to_string(pos_));
    JsonValue v;
    v.kind = JsonValue::Kind::kNull;
    return v;
  }

  JsonValue ParseNumber() {
    const std::size_t start = pos_;
    if (pos_ < text_.size() && text_[pos_] == '-') ++pos_;
    bool is_integer = true;
    while (pos_ < text_.size() &&
          std::isdigit(static_cast<unsigned char>(text_[pos_])))
      ++pos_;
    if (pos_ < text_.size() && text_[pos_] == '.') {
      is_integer = false;
      ++pos_;
      while (pos_ < text_.size() &&
            std::isdigit(static_cast<unsigned char>(text_[pos_])))
        ++pos_;
    }
    if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
      is_integer = false;
      ++pos_;
      if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-'))
        ++pos_;
      while (pos_ < text_.size() &&
            std::isdigit(static_cast<unsigned char>(text_[pos_])))
        ++pos_;
    }
    if (pos_ == start)
      throw std::runtime_error("expected a JSON value at offset " +
                               std::to_string(pos_));
    JsonValue v;
    v.kind = JsonValue::Kind::kNumber;
    v.number_is_integer = is_integer;
    return v;
  }
};

// Recursively checks one BenchSpan-shaped JSON node against the frozen schema
// (PRD "JSON schema"): {label: string, canonical: bool, duration_ns: integer,
// children: array of the same shape}. Keys/structure only, never values.
void ExpectNodeMatchesFrozenSchema(const JsonValue& node) {
  ASSERT_EQ(node.kind, JsonValue::Kind::kObject);
  ASSERT_EQ(node.object_fields.size(), 4u)
      << "a span node must have exactly {label, canonical, duration_ns, "
        "children}";
  ASSERT_TRUE(node.object_fields.count("label"));
  EXPECT_EQ(node.object_fields.at("label").kind, JsonValue::Kind::kString);
  ASSERT_TRUE(node.object_fields.count("canonical"));
  EXPECT_EQ(node.object_fields.at("canonical").kind, JsonValue::Kind::kBool);
  ASSERT_TRUE(node.object_fields.count("duration_ns"));
  const JsonValue& duration_ns = node.object_fields.at("duration_ns");
  EXPECT_EQ(duration_ns.kind, JsonValue::Kind::kNumber);
  EXPECT_TRUE(duration_ns.number_is_integer)
      << "PRD schema: 'Integer nanoseconds'";
  ASSERT_TRUE(node.object_fields.count("children"));
  const JsonValue& children = node.object_fields.at("children");
  ASSERT_EQ(children.kind, JsonValue::Kind::kArray);
  for (const JsonValue& child : children.array_items)
    ExpectNodeMatchesFrozenSchema(child);
}

// ---------------------------------------------------------------------------
// Unit: no-op when inactive (PRD "Test oracles").
// ---------------------------------------------------------------------------

TEST(BenchTimer, NoOpWithoutActiveScopeRecordsNothingAndDoesNotCrash) {
  // No BenchScope is constructed anywhere in this test: every BenchTimer
  // below must take the no-op path (thread-local null check) and must not
  // crash, whether canonical, free-form, or nested.
  { BenchTimer canonical(Stage::game_solving); }
  { BenchTimer free_form(std::string("scratch")); }
  {
    BenchTimer outer(Stage::product_construction);
    BenchTimer inner(std::string("nested_without_a_scope"));
  }
  SUCCEED();
}

// ---------------------------------------------------------------------------
// Unit: tree shape (PRD "Test oracles").
// ---------------------------------------------------------------------------

TEST(BenchTimer, CanonicalCtorSetsLabelToStageNameAndCanonicalTrueForEveryStage) {
  const std::vector<Stage> stages = {Stage::automaton_construction,
                                     Stage::product_construction,
                                     Stage::game_solving, Stage::aggregation};
  for (Stage s : stages) {
    SCOPED_TRACE("stage=" + std::string(stage_name(s)));
    BenchScope scope;
    { BenchTimer t(s); }
    const BenchReport report = scope.report();
    ASSERT_EQ(report.roots.size(), 1u);
    EXPECT_EQ(report.roots[0].label, std::string(stage_name(s)));
    EXPECT_TRUE(report.roots[0].canonical);
  }
}

TEST(BenchTimer, FreeFormCtorSetsCanonicalFalse) {
  BenchScope scope;
  { BenchTimer t(std::string("custom_phase")); }
  const BenchReport report = scope.report();
  ASSERT_EQ(report.roots.size(), 1u);
  EXPECT_EQ(report.roots[0].label, "custom_phase");
  EXPECT_FALSE(report.roots[0].canonical);
  EXPECT_TRUE(report.roots[0].children.empty());
}

TEST(BenchScope, HandNestedTimersProduceExpectedLabelsCanonicalFlagsAndParentChildNesting) {
  BenchScope scope;
  {
    BenchTimer automaton(Stage::automaton_construction);
    { BenchTimer determinize(std::string("determinize")); }
  }
  { BenchTimer product(Stage::product_construction); }

  const BenchReport report = scope.report();
  ASSERT_EQ(report.roots.size(), 2u);

  const BenchSpan& first = report.roots[0];
  EXPECT_EQ(first.label, std::string(stage_name(Stage::automaton_construction)));
  EXPECT_TRUE(first.canonical);
  ASSERT_EQ(first.children.size(), 1u);
  EXPECT_EQ(first.children[0].label, "determinize");
  EXPECT_FALSE(first.children[0].canonical);
  EXPECT_TRUE(first.children[0].children.empty());

  const BenchSpan& second = report.roots[1];
  EXPECT_EQ(second.label, std::string(stage_name(Stage::product_construction)));
  EXPECT_TRUE(second.canonical);
  EXPECT_TRUE(second.children.empty());
}

TEST(BenchScope, EmptyReportWhenNoTimersFireIsValid) {
  BenchScope scope;
  const BenchReport report = scope.report();
  EXPECT_TRUE(report.roots.empty());
  EXPECT_GE(report.total.count(), 0);
}

// ---------------------------------------------------------------------------
// Unit: monotonic containment (PRD "Test oracles").
// ---------------------------------------------------------------------------

void ExpectSpanNonNegativeAndContainsItsChildren(const BenchSpan& span) {
  EXPECT_GE(span.duration.count(), 0) << "span '" << span.label << "'";
  for (const BenchSpan& child : span.children) {
    EXPECT_GE(span.duration.count(), child.duration.count())
        << "parent '" << span.label << "' duration must be >= child '"
        << child.label << "'";
    ExpectSpanNonNegativeAndContainsItsChildren(child);
  }
}

TEST(BenchReport, MonotonicContainmentHoldsAcrossANestedTree) {
  BenchScope scope;
  {
    BenchTimer automaton(Stage::automaton_construction);
    { BenchTimer sub(std::string("determinize")); }
    { BenchTimer sub2(std::string("minimize")); }
  }
  { BenchTimer product(Stage::product_construction); }
  {
    BenchTimer game(Stage::game_solving);
    {
      BenchTimer inner(std::string("backward_pass"));
      { BenchTimer leaf(std::string("fixpoint_iterate")); }
    }
  }

  const BenchReport report = scope.report();
  ASSERT_EQ(report.roots.size(), 3u);
  for (const BenchSpan& root : report.roots)
    ExpectSpanNonNegativeAndContainsItsChildren(root);

  EXPECT_GE(report.total.count(), 0);
  std::chrono::nanoseconds roots_sum{0};
  for (const BenchSpan& root : report.roots) roots_sum += root.duration;
  EXPECT_GE(report.total.count(), roots_sum.count())
      << "total (BenchScope's own wall lifetime) must contain the roots' "
        "durations";
}

// ---------------------------------------------------------------------------
// Unit: nested BenchScope asserts (death test, PRD "Test oracles" /
// "Edge cases"). Relies on assert() being active, i.e. a non-NDEBUG build --
// this project's CMakeLists.txt defaults CMAKE_BUILD_TYPE to Debug.
// ---------------------------------------------------------------------------

TEST(BenchScopeDeathTest, NestedBenchScopeInstallAsserts) {
  BenchScope outer;
  EXPECT_DEATH({ BenchScope inner; }, "");
}

// ---------------------------------------------------------------------------
// Unit: to_json schema (PRD "Test oracles" + "JSON schema"). Assert
// keys/structure only, never values.
// ---------------------------------------------------------------------------

TEST(BenchReportToJson, NonEmptyReportParsesAsOneObjectMatchingTheFrozenSchema) {
  BenchScope scope;
  {
    BenchTimer automaton(Stage::automaton_construction);
    { BenchTimer determinize(std::string("determinize")); }
  }
  { BenchTimer product(Stage::product_construction); }
  { BenchTimer solve(Stage::game_solving); }
  const BenchReport report = scope.report();

  std::ostringstream os;
  report.to_json(os);
  const JsonValue root = JsonParser(os.str()).Parse();

  ASSERT_EQ(root.kind, JsonValue::Kind::kObject);
  // Knowingly-changed schema case (docs/prd/benchmark-suite.md "Interfaces &
  // types -> Phase 1": "BenchReport::to_json always emits a 'metrics' array,
  // empty when nothing was recorded; that is an additive but VISIBLE schema
  // change"): the object now carries {total_ns, roots, metrics}, not just
  // {total_ns, roots}.
  ASSERT_EQ(root.object_fields.size(), 3u)
      << "schema is exactly {total_ns, roots, metrics}";
  ASSERT_TRUE(root.object_fields.count("total_ns"));
  const JsonValue& total_ns = root.object_fields.at("total_ns");
  EXPECT_EQ(total_ns.kind, JsonValue::Kind::kNumber);
  EXPECT_TRUE(total_ns.number_is_integer)
      << "PRD schema: 'Integer nanoseconds'";

  ASSERT_TRUE(root.object_fields.count("roots"));
  const JsonValue& roots = root.object_fields.at("roots");
  ASSERT_EQ(roots.kind, JsonValue::Kind::kArray);
  EXPECT_EQ(roots.array_items.size(), 3u);
  for (const JsonValue& node : roots.array_items)
    ExpectNodeMatchesFrozenSchema(node);

  // No record_size_metric call was made anywhere in this test -- only
  // BenchTimer spans -- so "metrics" must still be present but empty, even
  // though "roots" itself is non-empty here (the two arrays are independent;
  // see EmptyReportStillParsesAsOneObjectWithAnEmptyRootsAndMetricsArray and
  // MetricsArrayIsEmptyWhenNoSizeMetricIsRecordedEvenWhenRootsIsNonEmpty
  // below for the two ends of that independence).
  ASSERT_TRUE(root.object_fields.count("metrics"));
  const JsonValue& metrics = root.object_fields.at("metrics");
  ASSERT_EQ(metrics.kind, JsonValue::Kind::kArray);
  EXPECT_TRUE(metrics.array_items.empty())
      << "no record_size_metric call was made in this test; the metrics "
        "array is empty when nothing was recorded, independent of roots";
}

TEST(BenchReportToJson, EmptyReportStillParsesAsOneObjectWithAnEmptyRootsAndMetricsArray) {
  BenchScope scope;
  const BenchReport report = scope.report();
  std::ostringstream os;
  report.to_json(os);
  const JsonValue root = JsonParser(os.str()).Parse();
  ASSERT_EQ(root.kind, JsonValue::Kind::kObject);
  ASSERT_TRUE(root.object_fields.count("total_ns"));
  ASSERT_TRUE(root.object_fields.count("roots"));
  EXPECT_EQ(root.object_fields.at("roots").kind, JsonValue::Kind::kArray);
  EXPECT_TRUE(root.object_fields.at("roots").array_items.empty());
  ASSERT_TRUE(root.object_fields.count("metrics"));
  EXPECT_EQ(root.object_fields.at("metrics").kind, JsonValue::Kind::kArray);
  EXPECT_TRUE(root.object_fields.at("metrics").array_items.empty());
}

// Sanctioned schema change, isolated case (docs/prd/benchmark-suite.md
// "Interfaces & types -> Phase 1"): "metrics" is empty whenever nothing was
// recorded via record_size_metric -- even when "roots" itself is non-empty
// (i.e. the emptiness of the two arrays is independent, not one flag).  This
// PRD only wires record_size_metric into the five methods' .cpp files, not
// into this file's hand-built BenchTimer-only fixtures, so no test in this
// file ever exercises record_size_metric directly; the size-metric sink's
// own unit fixtures (including a non-empty "metrics" array) live in
// tests/bench_size_metric_test.cpp, disjoint from this file's Stage/span
// territory.
TEST(BenchReportToJson, MetricsArrayIsEmptyWhenNoSizeMetricIsRecordedEvenWhenRootsIsNonEmpty) {
  BenchScope scope;
  { BenchTimer automaton(Stage::automaton_construction); }
  const BenchReport report = scope.report();

  std::ostringstream os;
  report.to_json(os);
  const JsonValue root = JsonParser(os.str()).Parse();

  ASSERT_EQ(root.kind, JsonValue::Kind::kObject);
  ASSERT_TRUE(root.object_fields.count("roots"));
  EXPECT_FALSE(root.object_fields.at("roots").array_items.empty())
      << "one BenchTimer fired; roots must stay non-empty so this test "
        "isolates the metrics-emptiness claim from the "
        "roots-emptiness one already covered above";
  ASSERT_TRUE(root.object_fields.count("metrics"));
  const JsonValue& metrics = root.object_fields.at("metrics");
  ASSERT_EQ(metrics.kind, JsonValue::Kind::kArray);
  EXPECT_TRUE(metrics.array_items.empty())
      << "PRD 'Interfaces & types' Phase 1: BenchReport::to_json always "
        "emits a \"metrics\" array, empty when nothing was recorded -- "
        "independent of whether roots itself is empty";
}

// ---------------------------------------------------------------------------
// Integration: DfaProduct under a scope (PRD "Test oracles"). Requires
// DfaProduct to emit the three canonical stages -- not yet true until the
// developer branch instruments src/dfa_product.cpp, so this test is expected
// to fail (empty report.roots) until the two branches are integrated.
// ---------------------------------------------------------------------------

TEST(BenchScopeIntegration, DfaProductEmitsCanonicalStagesOnceEachInOrder) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  DfaProduct method;

  BenchReport report;
  {
    BenchScope scope;
    const std::optional<Controller> controller =
        method.synthesize(Phi("G(i -> o)"), vars, t_in, t_out);
    ASSERT_TRUE(controller.has_value())
        << "fixture must stay realizable for the integration oracle to mean "
          "anything";
    report = scope.report();
  }

  ASSERT_EQ(report.roots.size(), 3u)
      << "expected exactly automaton_construction, product_construction, "
        "game_solving";
  const std::vector<Stage> expected_order = {
      Stage::automaton_construction, Stage::product_construction,
      Stage::game_solving};
  for (std::size_t i = 0; i < expected_order.size(); ++i) {
    SCOPED_TRACE("root index " + std::to_string(i));
    EXPECT_EQ(report.roots[i].label,
             std::string(stage_name(expected_order[i])));
    EXPECT_TRUE(report.roots[i].canonical);
  }
  EXPECT_GT(report.total.count(), 0);
}

// ---------------------------------------------------------------------------
// Zero-perturbation oracle (PRD "Test oracles", load-bearing): over a set of
// generated-corpus cases, the synthesized controller / realizability verdict
// is byte-identical with vs without an active BenchScope. Ties into the
// project's existing generated-corpus differential (docs/prd/generated-
// corpus-oracle.md) methodologically -- same generation technique
// (spot::randltlgenerator, partition-first, an in-memory Case-A-total random
// T_in) -- kept self-contained in this file (this project's one-file-per-
// suite duplication norm) rather than reaching into
// tests/ltlfsynt_oracle_test.cpp's file-local anonymous-namespace corpus, to
// keep this PRD's test territory disjoint from that file while its own
// concurrent work is in flight.
// ---------------------------------------------------------------------------

constexpr unsigned kZpSeed = 20260713u;         // fixed seed (deterministic).
constexpr std::size_t kZpCaseCount = 24;        // corpus size (kept small: a
                                                 // synthesize() call runs
                                                 // twice per case).
constexpr int kZpTreeSizeMin = 1;
constexpr int kZpTreeSizeMax = 8;
constexpr double kZpStrongXProbability = 0.30;  // see kCorpusStrongXProbability
                                                 // in tests/ltlfsynt_oracle_
                                                 // test.cpp for the rationale.
constexpr int kZpInputMax = 3;
constexpr int kZpOutputMax = 3;
constexpr double kZpIknownProbability = 0.5;
constexpr int kZpTinStatesMax = 2;

std::string DescribeZpPartition(const VariablePartition& p) {
  auto join = [](const std::set<std::string>& s) {
    std::string out;
    for (const std::string& x : s) {
      if (!out.empty()) out += ",";
      out += x;
    }
    return out;
  };
  std::ostringstream os;
  os << "Ifree={" << join(p.input_free) << "} Iknown={" << join(p.input_known)
     << "} Ofree={" << join(p.output_free) << "} Oknown={"
     << join(p.output_known) << "}";
  return os.str();
}

// spot::randltlgenerator wrapper (same technique as generated-corpus-oracle.md
// "Formula generation"): APs come from the partition's exact I union O set
// (partition-first), operator palette excludes xor/M via priorities.
spot::formula ZpRandomFormula(const VariablePartition& partition,
                              std::mt19937& rng) {
  std::set<std::string> ap_names = partition.inputs();
  for (const std::string& name : partition.outputs()) ap_names.insert(name);

  spot::atomic_prop_set aprops;
  for (const std::string& name : ap_names)
    aprops.insert(spot::default_environment::instance().require(name));

  spot::option_map opts;
  opts.set("output", spot::randltlgenerator::LTL);
  opts.set("tree_size_min", kZpTreeSizeMin);
  opts.set("tree_size_max", kZpTreeSizeMax);
  opts.set("seed", static_cast<int>(rng()));

  std::string priorities_str = "xor=0,M=0";
  std::vector<char> priorities(priorities_str.begin(), priorities_str.end());
  priorities.push_back('\0');

  spot::randltlgenerator rg(aprops, opts, priorities.data());
  const spot::formula phi = rg.next();
  if (!phi)
    throw std::runtime_error(
        "ZpRandomFormula: randltlgenerator produced no formula");
  return phi;
}

// Rewrites weak X -> X[!] with probability kZpStrongXProbability (memory
// ltlf-weak-x-and-termination-semantics: X[!] is the operator that stresses
// real hardness, not Spot's default weak X).
spot::formula ZpStrengthenNext(spot::formula f, std::mt19937& rng) {
  spot::formula mapped = f.map(
      [&](spot::formula child) { return ZpStrengthenNext(child, rng); });
  std::bernoulli_distribution flip(kZpStrongXProbability);
  if (mapped.is(spot::op::X) && flip(rng))
    return spot::formula::unop(spot::op::strong_X, mapped[0]);
  return mapped;
}

VariablePartition ZpRandomPartition(std::mt19937& rng) {
  std::uniform_int_distribution<int> input_count(1, kZpInputMax);
  const int n_inputs = input_count(rng);
  std::uniform_int_distribution<int> output_count(0, kZpOutputMax);
  const int n_outputs = output_count(rng);

  std::set<std::string> inputs, outputs;
  int next_id = 0;
  for (int i = 0; i < n_inputs; ++i)
    inputs.insert("z" + std::to_string(next_id++));
  for (int i = 0; i < n_outputs; ++i)
    outputs.insert("z" + std::to_string(next_id++));

  std::bernoulli_distribution is_known(kZpIknownProbability);
  std::set<std::string> governed;
  for (const std::string& name : inputs)
    if (is_known(rng)) governed.insert(name);

  return VariablePartition::split(inputs, outputs, governed);
}

std::vector<bdd> ZpAllLettersOver(const std::vector<int>& vars) {
  const std::size_t n = vars.size();
  std::vector<bdd> letters;
  letters.reserve(std::size_t{1} << n);
  for (std::size_t k = 0; k < (std::size_t{1} << n); ++k) {
    bdd v = bddtrue;
    for (std::size_t i = 0; i < n; ++i)
      v &= (k >> i & 1) ? bdd_ithvar(vars[i]) : bdd_nithvar(vars[i]);
    letters.push_back(v);
  }
  return letters;
}

// In-memory Case-A-total random T_in (same construction as generated-corpus-
// oracle.md "Random Tin generation"): delta is guarded on mutually
// exclusive/exhaustive Ifree-cubes (deterministic + total over Ifree),
// lambda commits a full Iknown assignment per Ifree-cube (a total function
// Ifree -> 2^Iknown) -- valid by construction, no post-hoc check needed.
OutputLabeledTransducer ZpRandomTin(const VariablePartition& partition,
                                    std::mt19937& rng,
                                    const spot::bdd_dict_ptr& dict) {
  if (partition.input_known.empty())
    return trivial_transducer(partition, Role::t_in, dict);

  auto g = spot::make_twa_graph(dict);
  std::vector<int> ifree_vars, iknown_vars;
  for (const std::string& n : partition.input_free)
    ifree_vars.push_back(g->register_ap(n));
  for (const std::string& n : partition.input_known)
    iknown_vars.push_back(g->register_ap(n));
  for (const std::string& n : partition.output_free) g->register_ap(n);
  for (const std::string& n : partition.output_known) g->register_ap(n);

  std::uniform_int_distribution<int> state_count(1, kZpTinStatesMax);
  const int n = state_count(rng);
  g->new_states(n);
  g->set_init_state(0);

  const std::vector<bdd> ifree_letters = ZpAllLettersOver(ifree_vars);
  std::uniform_int_distribution<int> dst_dist(0, n - 1);
  std::bernoulli_distribution bit(0.5);
  std::vector<bdd> lambda_by_state(n, bddfalse);
  for (int q = 0; q < n; ++q) {
    bdd lambda_q = bddfalse;
    for (const bdd& ifree_cube : ifree_letters) {
      g->new_edge(q, dst_dist(rng), ifree_cube);
      bdd iknown_cube = bddtrue;
      for (int x : iknown_vars)
        iknown_cube &= bit(rng) ? bdd_ithvar(x) : bdd_nithvar(x);
      lambda_q |= ifree_cube & iknown_cube;
    }
    lambda_by_state[q] = lambda_q;
  }

  bdd sigma0_cube = bddtrue;
  for (int x : ifree_vars) sigma0_cube &= bdd_ithvar(x);
  bdd sigma1_cube = bddtrue;
  for (int x : iknown_vars) sigma1_cube &= bdd_ithvar(x);
  return OutputLabeledTransducer(g, std::move(lambda_by_state), sigma0_cube,
                                 sigma1_cube);
}

struct ZpCase {
  spot::formula phi;
  VariablePartition partition;
  OutputLabeledTransducer t_in;
};

std::vector<ZpCase> BuildZpCorpus() {
  std::mt19937 rng(kZpSeed);
  std::vector<ZpCase> corpus;
  corpus.reserve(kZpCaseCount);
  for (std::size_t i = 0; i < kZpCaseCount; ++i) {
    VariablePartition partition = ZpRandomPartition(rng);
    spot::formula phi =
        ZpStrengthenNext(ZpRandomFormula(partition, rng), rng);
    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    OutputLabeledTransducer t_in = ZpRandomTin(partition, rng, dict);
    corpus.push_back({phi, std::move(partition), std::move(t_in)});
  }
  return corpus;
}

// Runs DfaProduct::synthesize(phi, partition, t_in, t_out) twice -- once with
// no active BenchScope, once inside one -- and asserts the realizability
// verdict and (when realizable) the synthesized controller's HOA
// serialization are byte-identical (PRD "Behaviour": "a run's controller /
// verdict is byte-identical whether timing is on or off").
void ExpectZeroPerturbation(const spot::formula& phi,
                            const VariablePartition& partition,
                            const OutputLabeledTransducer& t_in,
                            const OutputLabeledTransducer& t_out,
                            bool* realizable_out = nullptr) {
  std::optional<Controller> without_scope;
  {
    DfaProduct method;
    without_scope = method.synthesize(phi, partition, t_in, t_out);
  }
  if (realizable_out) *realizable_out = without_scope.has_value();

  std::optional<Controller> with_scope;
  {
    BenchScope scope;
    DfaProduct method;
    with_scope = method.synthesize(phi, partition, t_in, t_out);
  }

  ASSERT_EQ(without_scope.has_value(), with_scope.has_value())
      << "an active BenchScope changed the realizability verdict";
  if (!without_scope) return;  // unrealizable: nothing further to compare.

  std::ostringstream a, b;
  spot::print_hoa(a, without_scope->strategy) << "\n";
  spot::print_hoa(b, with_scope->strategy) << "\n";
  EXPECT_EQ(a.str(), b.str())
      << "an active BenchScope changed the synthesized controller's HOA "
        "serialization (byte-identical invariant violated)";
}

TEST(BenchScopeZeroPerturbation, ActiveScopeNeverChangesGeneratedCorpusVerdictOrController) {
  const std::vector<ZpCase> corpus = BuildZpCorpus();
  std::size_t realizable_cases = 0;
  for (std::size_t i = 0; i < corpus.size(); ++i) {
    const ZpCase& c = corpus[i];
    std::ostringstream phi_os;
    phi_os << c.phi;
    SCOPED_TRACE("generated case " + std::to_string(i) +
                ": phi=" + phi_os.str() +
                ", partition=" + DescribeZpPartition(c.partition));

    const OutputLabeledTransducer t_out =
        trivial_transducer(c.partition, Role::t_out, c.t_in.dict());
    bool realizable = false;
    ExpectZeroPerturbation(c.phi, c.partition, c.t_in, t_out, &realizable);
    if (realizable) ++realizable_cases;
  }
  RecordProperty("zero_perturbation_generated_cases",
                 static_cast<int>(corpus.size()));
  RecordProperty("zero_perturbation_generated_realizable_cases",
                 static_cast<int>(realizable_cases));
}

// A handful of curated, load-bearing fixtures on top of the generated corpus
// (skill guidance "push oracles past trivial inputs"): a Mealy-sensitive
// formula the Moore monolithic baseline cannot cover, and the knowledge-flip
// formula from tests/dfa_product_test.cpp's KnowledgeTurnsUnrealizableInto
// Realizable, in both its unrealizable (free) and realizable (known) shapes.
struct ZpCuratedFixture {
  std::string phi;
  VariablePartition vars;
  bool known_i;  // true => t_in = TinAlwaysI (requires "i" in vars.input_known).
};

TEST(BenchScopeZeroPerturbation, CuratedLoadBearingFixturesAgreeWithAndWithoutScope) {
  const std::vector<ZpCuratedFixture> fixtures = {
      {"G(i -> o)", VariablePartition::split({"i"}, {"o"}, {}), false},
      {"o <-> i", VariablePartition::split({"i"}, {"o"}, {}), false},
      {"0", VariablePartition::split({"i"}, {"o"}, {}), false},
      {"X[!] 1 & (o <-> X i)", VariablePartition::split({"i"}, {"o"}, {}),
       false},
      {"X[!] 1 & (o <-> X i)",
       VariablePartition::split({"i"}, {"o"}, /*governed=*/{"i"}), true},
  };
  for (std::size_t i = 0; i < fixtures.size(); ++i) {
    const ZpCuratedFixture& f = fixtures[i];
    SCOPED_TRACE("curated fixture " + std::to_string(i) + ": phi=" + f.phi);
    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    const OutputLabeledTransducer t_in =
        f.known_i ? TinAlwaysI(dict) : trivial_transducer(f.vars, Role::t_in, dict);
    const OutputLabeledTransducer t_out =
        trivial_transducer(f.vars, Role::t_out, dict);
    ExpectZeroPerturbation(Phi(f.phi), f.vars, t_in, t_out);
  }
}

}  // namespace
