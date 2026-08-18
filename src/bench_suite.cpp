#include "ltlf_ek/bench_suite.hpp"

#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/dfa_product.hpp"
#include "ltlf_ek/mtdfa_product.hpp"
#include "ltlf_ek/mtnfa_product.hpp"
#include "ltlf_ek/nfa_product.hpp"
#include "ltlf_ek/otf_mtdfa_product.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/transducer_io.hpp"
#include "ltlf_ek/turn_order.hpp"

namespace ltlf_ek {

std::string_view comparability_tier_name(ComparabilityTier t) {
  switch (t) {
    case ComparabilityTier::t1: return "t1";
    case ComparabilityTier::t2: return "t2";
    case ComparabilityTier::t3: return "t3";
  }
  throw std::invalid_argument("comparability_tier_name: unknown ComparabilityTier");
}

namespace {

// ---------------------------------------------------------------------------
// Shared instance-construction helpers (PRD B4/B5: no randomness, every
// formula a template, every transducer hand-built).
// ---------------------------------------------------------------------------

// The sweep floor (PRD "Edge cases": X[!]^0 degenerates to phi itself; the
// sweep starts at n=2, and a family must reject a parameter below its own
// floor rather than emit a malformed formula). Uniform across all five
// families --- none of B4's phi_n shapes are malformed below 2, but the
// policy is declared once here rather than re-derived per family.
constexpr std::int64_t kNFloor = 2;

// n applications of the strong-next operator X[!] around `inner` (main.tex's
// X[!], SPOT_USES_STRONG_X): x_bang_wrap(0, s) == s.
std::string x_bang_wrap(std::int64_t n, const std::string& inner) {
  std::string wrapped = inner;
  for (std::int64_t i = 0; i < n; ++i) wrapped = "X[!](" + wrapped + ")";
  return wrapped;
}

std::int64_t require_param(const BenchParams& params, const std::string& key) {
  const auto it = params.find(key);
  if (it == params.end())
    throw std::invalid_argument("bench_suite: BenchParams missing key '" +
                                key + "'");
  return it->second;
}

void check_floor(const std::string& family, std::int64_t n) {
  if (n < kNFloor)
    throw std::invalid_argument(family + ": n=" + std::to_string(n) +
                                " is below the sweep floor (" +
                                std::to_string(kNFloor) +
                                "); a family must reject a parameter below "
                                "its own floor rather than emit a malformed "
                                "formula (PRD \"Edge cases\")");
}

// Common sweep: every n in [n_min, n_max], both polarities (PRD B4: "all
// with a single sweep axis n and a realizable flag"). n_min below the floor
// is rejected up front, same rule as instantiate().
std::vector<BenchParams> default_sweep(std::int64_t n_min, std::int64_t n_max) {
  if (n_min < kNFloor)
    throw std::invalid_argument(
        "bench_suite: sweep n_min=" + std::to_string(n_min) +
        " is below the sweep floor (" + std::to_string(kNFloor) + ")");
  std::vector<BenchParams> out;
  for (std::int64_t n = n_min; n <= n_max; ++n)
    for (std::int64_t realizable = 0; realizable <= 1; ++realizable)
      out.push_back(BenchParams{{"n", n}, {"realizable", realizable}});
  return out;
}

spot::formula parse_or_throw(const std::string& text) {
  try {
    return spot::parse_formula(text);
  } catch (const std::runtime_error& e) {
    throw std::invalid_argument("bench_suite: could not parse formula '" +
                                text + "': " + std::string(e.what()));
  }
}

// The historical unrealizable-forcing conjunct (docs/prd/otf-mtdfa-product.md
// "Benchmark results, 2026-07-29": "Unrealizable variants (phi_n & X[!] i, i
// in Ifree) reproduce both tables within noise, since the cost is
// construction, not solving") --- already-measured, not invented here (PRD
// B4). `i` is a fresh input_free AP no transducer commits to, so the
// adversarial environment can always choose i=false and falsify the whole
// conjunction, regardless of n or of what phi_base already says.
std::string add_unrealizable_conjunct(const std::string& phi_base,
                                      VariablePartition& vars) {
  vars.input_free.insert("i");
  return "(" + phi_base + ") & X[!](i)";
}

// A one-state T_in/T_out: delta self-loops on every letter (`[t] 0`, no AP
// needed), lambda commits `lambda_formula` at its single state. Used for
// cons-prunes' constant-true T_in (PRD B4's "one-state Tin whose lambda pins
// k true every step") --- the same shape as tests/ltlfsynt_oracle_test.cpp's
// kTinConstTrue fixture, hand-authored here rather than shared across
// translation units (this project's one-file-per-suite precedent extended to
// production code that plays the same role as a test fixture).
OutputLabeledTransducer one_state_const_transducer(
    const VariablePartition& vars, Role role, const spot::bdd_dict_ptr& dict,
    const std::string& lambda_formula) {
  std::ostringstream text;
  text << "HOA: v1\n"
         "States: 1\n"
         "Start: 0\n"
         "AP: 0\n"
         "acc-name: all\n"
         "Acceptance: 0 t\n"
         "--BODY--\n"
         "State: 0\n"
         "  [t] 0\n"
         "--END--\n"
         "%%LAMBDA\n"
         "state 0: "
      << lambda_formula << "\n";
  std::istringstream in(text.str());
  return parse_transducer(in, vars, role, dict);
}

// The T3 witness, fixed and hand-built (PRD B4 "The T3 witness, fixed and
// hand-built"): 2 states; delta toggles on input `a`, self-loops otherwise;
// lambda pins k true in the even state (0) and false in the odd state (1)
// --- "k holds iff an even number of a's have occurred so far". The toggle
// is input-triggered (not a G/X-chain of k itself), which is exactly what
// makes it a non-star-free (T3) witness rather than the star-free near-miss
// the PRD warns against.
OutputLabeledTransducer parity_transducer(const VariablePartition& vars,
                                          const spot::bdd_dict_ptr& dict) {
  static constexpr char kHoa[] =
      "HOA: v1\n"
      "States: 2\n"
      "Start: 0\n"
      "AP: 1 \"a\"\n"
      "acc-name: all\n"
      "Acceptance: 0 t\n"
      "--BODY--\n"
      "State: 0\n"
      "  [!0] 0\n"
      "  [0] 1\n"
      "State: 1\n"
      "  [!0] 1\n"
      "  [0] 0\n"
      "--END--\n"
      "%%LAMBDA\n"
      "state 0: k\n"
      "state 1: !k\n";
  std::istringstream in(kHoa);
  return parse_transducer(in, vars, Role::t_in, dict);
}

// ---------------------------------------------------------------------------
// The five families of B4.
// ---------------------------------------------------------------------------

// 3.1's Family A (docs/prd/otf-mtdfa-product.md): k in Iknown, T_in pins k
// true every step --- cons prunes hard, Goal is 2^n, product is n+1.
class ConsPrunesFamily final : public BenchFamily {
 public:
  std::string name() const override { return "cons-prunes"; }

  std::vector<BenchParams> sweep(std::int64_t n_min,
                                 std::int64_t n_max) const override {
    return default_sweep(n_min, n_max);
  }

  BenchCase instantiate(const BenchParams& params) const override {
    const std::int64_t n = require_param(params, "n");
    const bool realizable = require_param(params, "realizable") != 0;
    check_floor(name(), n);

    VariablePartition vars;
    vars.input_known = {"k"};
    std::string phi_text = "F(k & " + x_bang_wrap(n, "k") + ")";
    if (!realizable) phi_text = add_unrealizable_conjunct(phi_text, vars);

    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    register_turn_order_aps(vars, dict);

    const spot::formula phi = parse_or_throw(phi_text);
    OutputLabeledTransducer t_in =
        one_state_const_transducer(vars, Role::t_in, dict, "k");
    OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);

    return BenchCase{name(),
                     params,
                     phi,
                     vars,
                     std::move(t_in),
                     std::move(t_out),
                     ComparabilityTier::t1,
                     std::optional<std::string>("G(k)"),
                     realizable};
  }
};

// 3.1's Family B (docs/prd/otf-mtdfa-product.md): k in Ofree, trivial
// transducers --- the control, where cons prunes nothing.
class ConsInertFamily final : public BenchFamily {
 public:
  std::string name() const override { return "cons-inert"; }

  std::vector<BenchParams> sweep(std::int64_t n_min,
                                 std::int64_t n_max) const override {
    return default_sweep(n_min, n_max);
  }

  BenchCase instantiate(const BenchParams& params) const override {
    const std::int64_t n = require_param(params, "n");
    const bool realizable = require_param(params, "realizable") != 0;
    check_floor(name(), n);

    VariablePartition vars;
    vars.output_free = {"k"};
    std::string phi_text = "F(k & " + x_bang_wrap(n, "k") + ")";
    if (!realizable) phi_text = add_unrealizable_conjunct(phi_text, vars);

    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    register_turn_order_aps(vars, dict);

    const spot::formula phi = parse_or_throw(phi_text);
    OutputLabeledTransducer t_in = trivial_transducer(vars, Role::t_in, dict);
    OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);

    // No Iknown/Oknown at all, so there is nothing for an external psi_in to
    // encode; "1" (true) is the vacuous-constraint encoding, not an omission
    // (PRD B3: a T1 family must carry psi_in as data).
    return BenchCase{name(),
                     params,
                     phi,
                     vars,
                     std::move(t_in),
                     std::move(t_out),
                     ComparabilityTier::t1,
                     std::optional<std::string>("1"),
                     realizable};
  }
};

// MtnfaProduct's corrected family (docs/prd/mtnfa-product.md): reverse
// language ("the (n+1)-th letter is v") is deterministic in O(n), so the Goal
// NFA is n+3 while the mtdfa route is 2^n.
class MirrorSmallFamily final : public BenchFamily {
 public:
  std::string name() const override { return "mirror-small"; }

  std::vector<BenchParams> sweep(std::int64_t n_min,
                                 std::int64_t n_max) const override {
    return default_sweep(n_min, n_max);
  }

  BenchCase instantiate(const BenchParams& params) const override {
    const std::int64_t n = require_param(params, "n");
    const bool realizable = require_param(params, "realizable") != 0;
    check_floor(name(), n);

    VariablePartition vars;
    vars.output_free = {"v"};
    std::string phi_text = "F(v & " + x_bang_wrap(n, "!X[!](1)") + ")";
    if (!realizable) phi_text = add_unrealizable_conjunct(phi_text, vars);

    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    register_turn_order_aps(vars, dict);

    const spot::formula phi = parse_or_throw(phi_text);
    OutputLabeledTransducer t_in = trivial_transducer(vars, Role::t_in, dict);
    OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);

    return BenchCase{name(),
                     params,
                     phi,
                     vars,
                     std::move(t_in),
                     std::move(t_out),
                     ComparabilityTier::t1,
                     std::optional<std::string>("1"),
                     realizable};
  }
};

// The documented trap (docs/prd/mtnfa-product.md): ltlf_to_nfa is
// mirror-based, so this family's NFA is ~1027 states at n=10, the same order
// as its DFA --- committed so nobody re-derives it.
class MirrorDegenerateFamily final : public BenchFamily {
 public:
  std::string name() const override { return "mirror-degenerate"; }

  std::vector<BenchParams> sweep(std::int64_t n_min,
                                 std::int64_t n_max) const override {
    return default_sweep(n_min, n_max);
  }

  BenchCase instantiate(const BenchParams& params) const override {
    const std::int64_t n = require_param(params, "n");
    const bool realizable = require_param(params, "realizable") != 0;
    check_floor(name(), n);

    VariablePartition vars;
    vars.output_free = {"v"};
    std::string phi_text = "F(v & " + x_bang_wrap(n, "v") + ")";
    if (!realizable) phi_text = add_unrealizable_conjunct(phi_text, vars);

    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    register_turn_order_aps(vars, dict);

    const spot::formula phi = parse_or_throw(phi_text);
    OutputLabeledTransducer t_in = trivial_transducer(vars, Role::t_in, dict);
    OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);

    return BenchCase{name(),
                     params,
                     phi,
                     vars,
                     std::move(t_in),
                     std::move(t_out),
                     ComparabilityTier::t1,
                     std::optional<std::string>("1"),
                     realizable};
  }
};

// The capability separation (PRD B4): same phi_n as cons-prunes, same game,
// swap T_in for the non-aperiodic parity witness --- the competing tool
// leaves the table (Stop-list 1: the tier is declared data, never proved).
class ParityT3Family final : public BenchFamily {
 public:
  std::string name() const override { return "parity-t3"; }

  std::vector<BenchParams> sweep(std::int64_t n_min,
                                 std::int64_t n_max) const override {
    return default_sweep(n_min, n_max);
  }

  BenchCase instantiate(const BenchParams& params) const override {
    const std::int64_t n = require_param(params, "n");
    const bool realizable = require_param(params, "realizable") != 0;
    check_floor(name(), n);

    VariablePartition vars;
    vars.input_free = {"a"};
    vars.input_known = {"k"};
    std::string phi_text = "F(k & " + x_bang_wrap(n, "k") + ")";
    if (!realizable) phi_text = add_unrealizable_conjunct(phi_text, vars);

    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    register_turn_order_aps(vars, dict);

    const spot::formula phi = parse_or_throw(phi_text);
    OutputLabeledTransducer t_in = parity_transducer(vars, dict);
    OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);

    // T3: no psi_in exists at any size (Stop-list 1), so none is declared
    // (PRD "The five methods": "T1 families must carry a non-empty psi_in
    // string; T3 must not").
    return BenchCase{name(),
                     params,
                     phi,
                     vars,
                     std::move(t_in),
                     std::move(t_out),
                     ComparabilityTier::t3,
                     std::nullopt,
                     realizable};
  }
};

// ---------------------------------------------------------------------------
// The five-method BenchSubjects (PRD "Phase 2 -- the registry"): one per
// Synthesis implementation, dispatched exactly like cli.cpp's
// make_synthesis_method (same names, so a future runner can reuse either
// registry against the same --families=/--subjects= flag spelling).
// ---------------------------------------------------------------------------

// True iff `mona` resolves on PATH (the MONA_FOUND ctest gate's runtime
// analogue: this file lives in the ltlf_ek library, not the unit_tests
// target, so the compile-time MONA_FOUND define is not available here).
// Computed once and cached --- a subprocess probe on every case would be
// wasteful and mona's presence cannot change mid-run.
bool mona_available() {
  static const bool available =
      (std::system("command -v mona >/dev/null 2>&1") == 0);
  return available;
}

class DfaProductSubject final : public BenchSubject {
 public:
  std::string name() const override { return "dfa-product"; }
  void run(const BenchCase& c) const override {
    DfaProduct method;
    method.synthesize(c.phi, c.vars, c.t_in, c.t_out);
  }
};

class NfaProductSubject final : public BenchSubject {
 public:
  std::string name() const override { return "nfa-product"; }
  void run(const BenchCase& c) const override {
    // MONA absent: skip cleanly, never fail, never emit zeros (PRD "Edge
    // cases" "MONA absent") --- recording nothing leaves run_bench_case's
    // harvest empty for this (case, subject), read as absent, not zero.
    if (!mona_available()) return;
    NfaProduct method;
    method.synthesize(c.phi, c.vars, c.t_in, c.t_out);
  }
};

class MtdfaProductSubject final : public BenchSubject {
 public:
  std::string name() const override { return "mtdfa-product"; }
  void run(const BenchCase& c) const override {
    MtdfaProduct method;
    method.synthesize(c.phi, c.vars, c.t_in, c.t_out);
  }
};

class MtnfaProductSubject final : public BenchSubject {
 public:
  std::string name() const override { return "mtnfa-product"; }
  void run(const BenchCase& c) const override {
    if (!mona_available()) return;  // same MONA gate as NfaProductSubject.
    MtnfaProduct method;
    method.synthesize(c.phi, c.vars, c.t_in, c.t_out);
  }
};

class OtfMtdfaProductSubject final : public BenchSubject {
 public:
  std::string name() const override { return "otf-mtdfa-product"; }
  void run(const BenchCase& c) const override {
    OtfMtdfaProduct method;
    method.synthesize(c.phi, c.vars, c.t_in, c.t_out);
  }
};

// Depth-first flatten of a BenchReport's span tree into rows: one row per
// BenchSpan encountered, root or nested (PRD "the row key is generic", B6
// "per-*stage* nanoseconds"). A span's own `label` is already either a
// canonical stage_name(...) or a free-form string (bench.hpp), so it is used
// verbatim as the BenchRow key.
void flatten_spans(const std::vector<BenchSpan>& spans, const std::string& family,
                   const BenchParams& params, const std::string& subject,
                   std::vector<BenchRow>& out) {
  for (const auto& span : spans) {
    out.push_back(BenchRow{family, params, subject, span.label,
                           static_cast<std::uint64_t>(span.duration.count())});
    flatten_spans(span.children, family, params, subject, out);
  }
}

}  // namespace

const std::vector<std::unique_ptr<BenchFamily>>& bench_families() {
  static const std::vector<std::unique_ptr<BenchFamily>> families = [] {
    std::vector<std::unique_ptr<BenchFamily>> v;
    v.push_back(std::make_unique<ConsPrunesFamily>());
    v.push_back(std::make_unique<ConsInertFamily>());
    v.push_back(std::make_unique<MirrorSmallFamily>());
    v.push_back(std::make_unique<MirrorDegenerateFamily>());
    v.push_back(std::make_unique<ParityT3Family>());
    return v;
  }();
  return families;
}

const std::vector<std::unique_ptr<BenchSubject>>& bench_subjects() {
  static const std::vector<std::unique_ptr<BenchSubject>> subjects = [] {
    std::vector<std::unique_ptr<BenchSubject>> v;
    v.push_back(std::make_unique<DfaProductSubject>());
    v.push_back(std::make_unique<NfaProductSubject>());
    v.push_back(std::make_unique<MtdfaProductSubject>());
    v.push_back(std::make_unique<MtnfaProductSubject>());
    v.push_back(std::make_unique<OtfMtdfaProductSubject>());
    return v;
  }();
  return subjects;
}

std::vector<BenchRow> run_bench_case(const BenchCase& c, const BenchSubject& s) {
  BenchScope scope;  // the one BenchScope this case's run opens (nesting asserts).
  s.run(c);
  const BenchReport report = scope.report();

  std::vector<BenchRow> rows;
  for (const auto& m : report.metrics)
    rows.push_back(
        BenchRow{c.family, c.params, s.name(), m.label, m.value});
  flatten_spans(report.roots, c.family, c.params, s.name(), rows);
  return rows;
}

}  // namespace ltlf_ek
