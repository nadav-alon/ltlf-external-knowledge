#include "ltlf_ek/bench_suite.hpp"

#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <spot/tl/parse.hh>
#include <spot/tl/print.hh>
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

// An n-state chain: delta advances one state per letter and absorbs in the
// last, lambda pins k false until the chain lands and true forever after ---
// "k turns on at step n-1". The point is the STATE COUNT: every other family
// carries one-state knowledge, so |product| <= |goal| by construction and the
// knowledge-size axis is never exercised. Here |T_in| = n.
//
// Deterministic, complete, and aperiodic (count-to-a-threshold then absorb),
// so a psi_in does exist --- which is why the family declares t2 and not t3.
// It is not supplied, so the ltlfsynt race skips the family rather than
// racing an encoding (docs/GLOSSARY.md "Comparability tier", T2).
OutputLabeledTransducer chain_transducer(const VariablePartition& vars,
                                         const spot::bdd_dict_ptr& dict,
                                         std::int64_t n) {
  std::ostringstream text;
  text << "HOA: v1\n"
          "States: " << n << "\n"
          "Start: 0\n"
          "AP: 0\n"
          "acc-name: all\n"
          "Acceptance: 0 t\n"
          "--BODY--\n";
  for (std::int64_t i = 0; i < n; ++i) {
    const std::int64_t next = (i + 1 < n) ? i + 1 : i;  // last state absorbs
    text << "State: " << i << "\n  [t] " << next << "\n";
  }
  text << "--END--\n%%LAMBDA\n";
  for (std::int64_t i = 0; i < n; ++i)
    text << "state " << i << ": " << (i + 1 < n ? "!k" : "k") << "\n";
  std::istringstream in(text.str());
  return parse_transducer(in, vars, Role::t_in, dict);
}

// An n-state saturating run-length counter over the free input `a`: state i
// advances on `a` and resets to 0 on `!a`, saturating at n-1; lambda pins k
// true exactly in the saturated state. Language: "k holds iff the last n-1
// letters were all a".
//
// Unlike chain_transducer this is NOT synchronised with the trace position ---
// the state depends on the input history, so the product genuinely multiplies
// rather than sharing the goal's own step counter.
//
// Still aperiodic (a counter that resets and saturates is star-free, unlike a
// mod-n counter, which is the parity witness generalised), so t2 remains the
// honest tier and Stop-list 1 is not being guessed at.
OutputLabeledTransducer run_length_transducer(const VariablePartition& vars,
                                              const spot::bdd_dict_ptr& dict,
                                              std::int64_t n) {
  std::ostringstream text;
  text << "HOA: v1\n"
          "States: " << n << "\n"
          "Start: 0\n"
          "AP: 1 \"a\"\n"
          "acc-name: all\n"
          "Acceptance: 0 t\n"
          "--BODY--\n";
  for (std::int64_t i = 0; i < n; ++i) {
    const std::int64_t on_a = (i + 1 < n) ? i + 1 : i;  // saturate at n-1
    text << "State: " << i << "\n"
         << "  [0] " << on_a << "\n"
         << "  [!0] 0\n";
  }
  text << "--END--\n%%LAMBDA\n";
  for (std::int64_t i = 0; i < n; ++i)
    text << "state " << i << ": " << (i + 1 < n ? "!k" : "k") << "\n";
  std::istringstream in(text.str());
  return parse_transducer(in, vars, Role::t_in, dict);
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

// The knowledge-size axis: `cons-prunes`'s phi_n exactly, with the one-state
// T_in swapped for the n-state chain. Same goal automaton, same partition,
// only |T_in| varies --- the matched-pair pattern of B4, applied to the one
// dimension the other four families hold fixed at 1.
class KnowledgeChainFamily final : public BenchFamily {
 public:
  std::string name() const override { return "knowledge-chain"; }

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
    OutputLabeledTransducer t_in = chain_transducer(vars, dict, n);
    OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);

    // t2, not t1: the chain is aperiodic so a psi_in exists, but this family
    // deliberately does not supply one, so it must not enter the ltlfsynt
    // table. Not t3 either --- that would assert non-aperiodicity, which is
    // false here (Stop-list 1 forbids guessing at that claim in either
    // direction).
    return BenchCase{name(),
                     params,
                     phi,
                     vars,
                     std::move(t_in),
                     std::move(t_out),
                     ComparabilityTier::t2,
                     std::nullopt,
                     realizable};
  }
};

// The other half of the knowledge-size pair: n-state knowledge that cons
// cannot prune, because phi never mentions the known variable it constrains.
// This is the family that shows |product| = |T_in| * |goal| --- the growth
// regime every other family hides by holding |T_in| at 1.
class KnowledgeChainInertFamily final : public BenchFamily {
 public:
  std::string name() const override { return "knowledge-chain-inert"; }

  std::vector<BenchParams> sweep(std::int64_t n_min,
                                 std::int64_t n_max) const override {
    return default_sweep(n_min, n_max);
  }

  BenchCase instantiate(const BenchParams& params) const override {
    const std::int64_t n = require_param(params, "n");
    const bool realizable = require_param(params, "realizable") != 0;
    check_floor(name(), n);

    // phi is over the free output v only; k is known and constrained by the
    // chain, but no conjunct of phi refers to it, so cons has nothing to cut.
    VariablePartition vars;
    vars.input_free = {"a"};
    vars.output_free = {"v"};
    vars.input_known = {"k"};
    std::string phi_text = "F(v & " + x_bang_wrap(n, "v") + ")";
    if (!realizable) phi_text = add_unrealizable_conjunct(phi_text, vars);

    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    register_turn_order_aps(vars, dict);

    const spot::formula phi = parse_or_throw(phi_text);
    OutputLabeledTransducer t_in = run_length_transducer(vars, dict, n);
    OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);

    return BenchCase{name(),
                     params,
                     phi,
                     vars,
                     std::move(t_in),
                     std::move(t_out),
                     ComparabilityTier::t2,
                     std::nullopt,
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
// The slippery-world domain families (docs/prd/engineered-domain-families.md
// D1-D4, D6): an N x N grid, N = 2^n.  All three arms below share this
// section's machinery (SlipperyClass/slippery_step/SlipperyEncoding/
// slippery_transducer_hoa/instantiate_slippery) and differ only in how
// psi_in is built: enumerated (D3's arms 1 vs 2, Phase 1) or compact (D5's
// arm 3, Phase 3).
// ---------------------------------------------------------------------------

// The five priority classes of a `mv` letter and their literal guards, fixed
// priority L > R > U > D, no direction = stay (D1). Exactly one holds for any
// of the 16 mv valuations, so together with `slip` (2 more) they partition
// the joint space into 10 mutually exclusive, jointly exhaustive predicates
// --- delta is total by construction, no separate totality argument needed.
struct SlipperyClass {
  const char* name;
  std::vector<std::string> guard;  // literal tokens, e.g. {"!mvl", "mvr"}
};
const std::vector<SlipperyClass>& slippery_classes() {
  static const std::vector<SlipperyClass> classes = {
      {"L", {"mvl"}},
      {"R", {"!mvl", "mvr"}},
      {"U", {"!mvl", "!mvr", "mvu"}},
      {"D", {"!mvl", "!mvr", "!mvu", "mvd"}},
      {"S", {"!mvl", "!mvr", "!mvu", "!mvd"}},
  };
  return classes;
}

// One cell's transition under a priority class and a slip bit: walls
// SATURATE (clamp), they do not block (D1, scripts/slippery_world.py:47 is
// normative over its own "no-ops" docstring, which is wrong). Step is 1 cell,
// or 2 when slip holds.
std::pair<std::int64_t, std::int64_t> slippery_step(std::int64_t x,
                                                     std::int64_t y,
                                                     const std::string& cls,
                                                     bool slip,
                                                     std::int64_t N) {
  const std::int64_t d = slip ? 2 : 1;
  if (cls == "L") return {std::max<std::int64_t>(x - d, 0), y};
  if (cls == "R") return {std::min<std::int64_t>(x + d, N - 1), y};
  if (cls == "U") return {x, std::max<std::int64_t>(y - d, 0)};
  if (cls == "D") return {x, std::min<std::int64_t>(y + d, N - 1)};
  return {x, y};  // S: no direction = stay
}

// Position encoding (D2/D3's arms 1 vs 2), mirroring
// scripts/slippery_world.py's Enc class exactly, specialised to N = 2^n so
// `bits` (binary) is exactly n --- no unrepresentable code, per D2.
struct SlipperyEncoding {
  bool one_hot;
  std::int64_t bits;  // == n, meaningful only when !one_hot
  std::int64_t N;

  std::vector<std::string> aps() const {
    std::vector<std::string> out;
    if (!one_hot) {
      for (std::int64_t i = 0; i < bits; ++i) out.push_back("bx" + std::to_string(i));
      for (std::int64_t i = 0; i < bits; ++i) out.push_back("by" + std::to_string(i));
    } else {
      for (std::int64_t j = 0; j < N; ++j) out.push_back("hx" + std::to_string(j));
      for (std::int64_t j = 0; j < N; ++j) out.push_back("hy" + std::to_string(j));
    }
    return out;
  }

  // "coordinate `axis` == k" as a list of literal tokens (a conjunction).
  std::vector<std::string> lits(char axis, std::int64_t k) const {
    std::vector<std::string> out;
    if (!one_hot) {
      for (std::int64_t i = 0; i < bits; ++i) {
        const bool bit = (k >> i) & 1;
        out.push_back((bit ? "" : "!") + std::string("b") + axis + std::to_string(i));
      }
    } else {
      for (std::int64_t j = 0; j < N; ++j)
        out.push_back((j == k ? "" : "!") + std::string("h") + axis + std::to_string(j));
    }
    return out;
  }

  std::string at(char axis, std::int64_t k) const {
    const auto ls = lits(axis, k);
    std::string s;
    for (std::size_t i = 0; i < ls.size(); ++i) {
      if (i) s += " & ";
      s += ls[i];
    }
    return s;
  }

  std::string cell(std::int64_t x, std::int64_t y) const {
    return "(" + at('x', x) + ") & (" + at('y', y) + ")";
  }
};

// The enumerated N^2-state Output-labeled transducer (D1, D8: |T_in| = N^2 =
// 4^n): state x*N+y, delta ten-way split per state (five classes x two slip
// values --- guards are pairwise disjoint by construction, so no merging of
// same-destination edges is needed for determinism), lambda commits the
// state's position literals and never reads `slip` (the Moore condition
// D1 requires by construction, not by argument).
std::string slippery_transducer_hoa(const SlipperyEncoding& enc, std::int64_t N) {
  std::vector<std::string> aps = enc.aps();
  aps.push_back("slip");
  for (const char* m : {"mvl", "mvr", "mvu", "mvd"}) aps.push_back(m);
  std::map<std::string, std::size_t> idx;
  for (std::size_t i = 0; i < aps.size(); ++i) idx[aps[i]] = i;

  auto guard_str = [&](const std::vector<std::string>& lits) {
    std::string s;
    for (std::size_t i = 0; i < lits.size(); ++i) {
      if (i) s += "&";
      const std::string& lit = lits[i];
      const bool neg = !lit.empty() && lit[0] == '!';
      const std::string bare = neg ? lit.substr(1) : lit;
      s += (neg ? "!" : "") + std::to_string(idx.at(bare));
    }
    return s;
  };

  std::ostringstream out;
  out << "HOA: v1\n"
      << "States: " << (N * N) << "\n"
      << "Start: 0\n"
      << "AP: " << aps.size();
  for (const auto& a : aps) out << " \"" << a << "\"";
  out << "\n"
      << "acc-name: all\nAcceptance: 0 t\n--BODY--\n";

  for (std::int64_t x = 0; x < N; ++x) {
    for (std::int64_t y = 0; y < N; ++y) {
      out << "State: " << (x * N + y) << "\n";
      for (const SlipperyClass& c : slippery_classes()) {
        for (int s = 0; s < 2; ++s) {
          const auto dst = slippery_step(x, y, c.name, s != 0, N);
          std::vector<std::string> lits = c.guard;
          lits.push_back(s ? "slip" : "!slip");
          out << "  [" << guard_str(lits) << "] " << (dst.first * N + dst.second)
              << "\n";
        }
      }
    }
  }
  out << "--END--\n%%LAMBDA\n";
  for (std::int64_t x = 0; x < N; ++x)
    for (std::int64_t y = 0; y < N; ++y)
      out << "state " << (x * N + y) << ": " << enc.cell(x, y) << "\n";
  return out.str();
}

// A_N, the enumerated environment assumption (D3 arms 1/2): the same
// transition table restated per-coordinate as an LTLf formula, weak X only
// (D6 --- X[!] would collapse A_N to false silently, see Stop-list 7).
// Exactly 14N+1 top-level conjuncts (D8, T5): 1 init + 2 axes x N cells x 7
// per-cell rules (2 movers x 2 slip values + 3 non-movers x 1).
std::string slippery_assumption(const SlipperyEncoding& enc, std::int64_t N) {
  std::vector<std::string> conj;
  conj.push_back("(" + enc.cell(0, 0) + ")");

  struct Axis {
    char axis;
    const char* mover_a;
    const char* mover_b;
  };
  const Axis axes[] = {{'x', "L", "R"}, {'y', "U", "D"}};

  for (const Axis& ax : axes) {
    for (std::int64_t k = 0; k < N; ++k) {
      const std::string here = ax.axis == 'x' ? enc.at('x', k) : enc.at('y', k);
      for (const SlipperyClass& c : slippery_classes()) {
        const bool is_mover = c.name == std::string(ax.mover_a) ||
                              c.name == std::string(ax.mover_b);
        const std::vector<int> slips = is_mover ? std::vector<int>{0, 1}
                                                : std::vector<int>{0};
        for (int s : slips) {
          const std::int64_t px = ax.axis == 'x' ? k : 0;
          const std::int64_t py = ax.axis == 'x' ? 0 : k;
          const auto dst = slippery_step(px, py, c.name, s != 0, N);
          const std::int64_t dst_k = ax.axis == 'x' ? dst.first : dst.second;

          std::string guard;
          for (std::size_t i = 0; i < c.guard.size(); ++i) {
            if (i) guard += " & ";
            guard += c.guard[i];
          }
          if (is_mover) guard += std::string(" & ") + (s ? "slip" : "!slip");

          conj.push_back("G(((" + here + ") & (" + guard + ")) -> X(" +
                         enc.at(ax.axis, dst_k) + "))");
        }
      }
    }
  }

  std::string out;
  for (std::size_t i = 0; i < conj.size(); ++i) {
    if (i) out += " & ";
    out += conj[i];
  }
  return out;
}

// A_N builder signature shared by the enumerated (`slippery_assumption`) and
// compact (`slippery_compact_assumption`, D5) styles, so `instantiate_slippery`
// below is the ONE code path that builds T_in for all three arms (D3: "Arms 1
// and 3 share a byte-identical T_in ... only psi_in differs" -- T8).
using AssumptionBuilder = std::string (*)(const SlipperyEncoding&, std::int64_t);

// Shared instantiate() for all three arms (D3: "one BenchFamily sweeping n
// with the landed realizable flag selecting the goal"). n < 2 is rejected by
// check_floor, same rule and same floor as every other family (D2's floor
// coincides with kNFloor). `psi_in` is the only thing that varies across
// arms; the phi/vars/t_in/t_out construction below is IDENTICAL for every
// caller, which is what makes T8's byte-identical-T_in claim true by
// construction rather than by coincidence.
BenchCase instantiate_slippery(const std::string& family_name, bool one_hot,
                               const BenchParams& params,
                               AssumptionBuilder psi_in_builder) {
  const std::int64_t n = require_param(params, "n");
  const bool realizable = require_param(params, "realizable") != 0;
  check_floor(family_name, n);

  const std::int64_t N = std::int64_t(1) << n;
  const SlipperyEncoding enc{one_hot, n, N};

  VariablePartition vars;
  vars.input_free = {"slip"};
  {
    const auto position_aps = enc.aps();
    vars.input_known = std::set<std::string>(position_aps.begin(), position_aps.end());
  }
  vars.output_free = {"mvl", "mvr", "mvu", "mvd"};
  // output_known left empty: A_rest = top (D1), no family here exercises T_out.

  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  register_turn_order_aps(vars, dict);

  const std::int64_t c = (N - 1) / 2;  // = 2^(n-1) - 1 under N = 2^n (D3)
  const std::string phi_text = realizable ? "F(" + enc.cell(N - 1, N - 1) + ")"
                                          : "F(" + enc.cell(c, c) + ")";
  const spot::formula phi = parse_or_throw(phi_text);

  std::istringstream tin_text(slippery_transducer_hoa(enc, N));
  OutputLabeledTransducer t_in = parse_transducer(tin_text, vars, Role::t_in, dict);
  OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);

  return BenchCase{family_name,
                   params,
                   phi,
                   vars,
                   std::move(t_in),
                   std::move(t_out),
                   ComparabilityTier::t1,
                   std::optional<std::string>(psi_in_builder(enc, N)),
                   realizable};
}

// Arm 1 (D3): binary position encoding, 2n APs.
class SlipperyBinaryFamily final : public BenchFamily {
 public:
  std::string name() const override { return "slippery-binary"; }

  std::vector<BenchParams> sweep(std::int64_t n_min,
                                 std::int64_t n_max) const override {
    return default_sweep(n_min, n_max);
  }

  BenchCase instantiate(const BenchParams& params) const override {
    return instantiate_slippery(name(), /*one_hot=*/false, params,
                                &slippery_assumption);
  }
};

// Arm 2 (D3): one-hot position encoding, 2N APs --- the negative control
// already measured null against arm 1 (Wednesday's probe); ships as a
// confirmed contrast, not an untested assumption.
class SlipperyOnehotFamily final : public BenchFamily {
 public:
  std::string name() const override { return "slippery-onehot"; }

  std::vector<BenchParams> sweep(std::int64_t n_min,
                                 std::int64_t n_max) const override {
    return default_sweep(n_min, n_max);
  }

  BenchCase instantiate(const BenchParams& params) const override {
    return instantiate_slippery(name(), /*one_hot=*/true, params,
                                &slippery_assumption);
  }
};

// ---------------------------------------------------------------------------
// Arm 3 (D3, D5): the compact A_N, bit-level arithmetic over the SAME binary
// encoding and the SAME T_in as arm 1 (instantiate_slippery is one code path
// for all three arms --- see AssumptionBuilder above). Phase 3 only; nothing
// here touches produced_trace_equivalence.{hpp,cpp}, which stays as landed by
// Phase 2 (PRD "Out of scope").
// ---------------------------------------------------------------------------

// One bit's name, LSB first, the probe's spelling (D5): "bx0".."bx{n-1}" /
// "by0".."by{n-1}". Matches SlipperyEncoding::lits/at exactly for !one_hot.
std::string compact_bit(char axis, std::int64_t i) {
  return std::string("b") + axis + std::to_string(i);
}

// A parenthesised n-ary conjunction, "1" (Spot's true) for the empty case ---
// defensive only; every call site below has >= 1 term for n >= 2 (D2's
// floor).
std::string compact_conj(const std::vector<std::string>& terms) {
  if (terms.empty()) return "1";
  std::string out;
  for (std::size_t i = 0; i < terms.size(); ++i) {
    if (i) out += " & ";
    out += "(" + terms[i] + ")";
  }
  return out;
}

// D5's four predicates over one axis's bit vector.
std::string compact_max(char axis, std::int64_t n) {
  std::vector<std::string> terms;
  for (std::int64_t i = 0; i < n; ++i) terms.push_back(compact_bit(axis, i));
  return compact_conj(terms);
}
std::string compact_min(char axis, std::int64_t n) {
  std::vector<std::string> terms;
  for (std::int64_t i = 0; i < n; ++i) terms.push_back("!" + compact_bit(axis, i));
  return compact_conj(terms);
}
std::string compact_max1(char axis, std::int64_t n) {
  std::vector<std::string> terms = {"!" + compact_bit(axis, 0)};
  for (std::int64_t i = 1; i < n; ++i) terms.push_back(compact_bit(axis, i));
  return compact_conj(terms);
}
std::string compact_min1(char axis, std::int64_t n) {
  std::vector<std::string> terms = {compact_bit(axis, 0)};
  for (std::int64_t i = 1; i < n; ++i) terms.push_back("!" + compact_bit(axis, i));
  return compact_conj(terms);
}

// D5's Keep: every bit unchanged, weak X only (D6 --- X[!] here would make
// the rule's guard-total antecedent unsatisfiable at the last position and
// collapse A_N to false, silently, per Stop-list 7; T7 guards this). Weak X
// alone is not enough at the boundary either; kNextExists below carries the
// other half of the fix.
std::string compact_keep(char axis, std::int64_t n) {
  std::vector<std::string> terms;
  for (std::int64_t i = 0; i < n; ++i) {
    const std::string b = compact_bit(axis, i);
    terms.push_back("X(" + b + ") <-> " + b);
  }
  return compact_conj(terms);
}

// D5's Inc/Dec: bit 0 always flips; bit i>=1 flips iff every strictly-lower
// bit is set (Inc) / clear (Dec) --- ripple-carry, written as
// "X b_i <-> (b_i <-> !AND_{j<i} guard_j)" (b_i XOR AND_j is "b_i <-> !c",
// this repo's spelling for xor, see tests/ltlf_ek_deps_test.cpp's "F(a xor
// b)" == "F(a <-> !b)" fixture).
std::string compact_inc_or_dec(char axis, std::int64_t n, bool increment) {
  std::vector<std::string> terms;
  const std::string b0 = compact_bit(axis, 0);
  terms.push_back("X(" + b0 + ") <-> !" + b0);
  for (std::int64_t i = 1; i < n; ++i) {
    std::vector<std::string> guard_bits;
    for (std::int64_t j = 0; j < i; ++j) {
      const std::string bj = compact_bit(axis, j);
      guard_bits.push_back(increment ? bj : ("!" + bj));
    }
    const std::string bi = compact_bit(axis, i);
    terms.push_back("X(" + bi + ") <-> (" + bi + " <-> !(" +
                    compact_conj(guard_bits) + "))");
  }
  return compact_conj(terms);
}

// D5's Inc2/Dec2: bit 0 unchanged, bit 1 always flips, bit i>=2 flips iff
// every guard bit strictly between 1 and i is set (Inc2) / clear (Dec2) ---
// "add/subtract 2 starting at bit 1".
std::string compact_inc2_or_dec2(char axis, std::int64_t n, bool increment) {
  std::vector<std::string> terms;
  const std::string b0 = compact_bit(axis, 0);
  const std::string b1 = compact_bit(axis, 1);
  terms.push_back("X(" + b0 + ") <-> " + b0);
  terms.push_back("X(" + b1 + ") <-> !" + b1);
  for (std::int64_t i = 2; i < n; ++i) {
    std::vector<std::string> guard_bits;
    for (std::int64_t j = 1; j < i; ++j) {
      const std::string bj = compact_bit(axis, j);
      guard_bits.push_back(increment ? bj : ("!" + bj));
    }
    const std::string bi = compact_bit(axis, i);
    terms.push_back("X(" + bi + ") <-> (" + bi + " <-> !(" +
                    compact_conj(guard_bits) + "))");
  }
  return compact_conj(terms);
}

std::string compact_set_max(char axis, std::int64_t n) {
  std::vector<std::string> terms;
  for (std::int64_t i = 0; i < n; ++i) terms.push_back("X(" + compact_bit(axis, i) + ")");
  return compact_conj(terms);
}
// SetMin: "X(!b_i)", NOT "!X(b_i)". The two agree everywhere except at the
// last position, where weak X makes "X(!b_i)" true for free but "!X(b_i)"
// FALSE --- the same trace-boundary defect the update rules had, in its
// worst form: an unsatisfiable consequent under a satisfiable guard, which
// forbids the last position outright instead of merely over-constraining it.
// kNextExists below already makes this unreachable; keeping the negation
// inside X keeps every rule body boundary-neutral on its own (D6's "weak X
// only" read as a property of the body, not just of the operator spelling).
std::string compact_set_min(char axis, std::int64_t n) {
  std::vector<std::string> terms;
  for (std::int64_t i = 0; i < n; ++i) terms.push_back("X(!" + compact_bit(axis, i) + ")");
  return compact_conj(terms);
}

// "a next position exists" (main.tex's X[!], SPOT_USES_STRONG_X), conjoined
// into the GUARD of every D5 rule below.
//
// Why it is needed. Each update rule relates an X-term to a non-X-term inside
// one biconditional, "X b_i <-> rhs_i". Under weak X the left side is true for
// free at the LAST position of a produced trace, so the biconditional
// collapses to the bare "rhs_i" --- a constraint on the CURRENT cell that
// T_in does not impose there (T_in simply stops; it commits no successor). For
// Keep that reads "every bit of the last cell is set"; for Inc it reads "every
// bit is clear"; either way A_N rejects produced traces that T_in accepts, and
// the T1 certificate is red with a length-1 witness at (0,0) --- position 0 is
// simultaneously the first and the last, so Min(bx) & Min(by) from the init
// conjunct meets Keep's "all bits set" and nothing survives.
//
// Why in the guard and not the body. Writing the body with X[!] instead
// (X[!] b_i <-> rhs_i) is Stop-list 7's trap: it does not go vacuous, it flips
// the left side to FALSE at the last position and collapses A_N to "!rhs_i"
// there. Guarding the implication is what makes the whole rule vacuously true
// at the boundary --- which is exactly the shape the enumerated arms already
// have for free (slippery_assumption puts its ENTIRE consequent under one weak
// X, so the rule is satisfied at the last position without a guard). The
// compact arm cannot do that, because its consequent mentions the current cell
// too, so it must say "a next position exists" explicitly.
const char kNextExists[] = "X[!]1";

// A slippery_classes() literal guard, rendered as one conjunction string ---
// the same rendering slippery_assumption already uses per-class.
std::string compact_class_guard(const SlipperyClass& c) {
  std::string g;
  for (std::size_t i = 0; i < c.guard.size(); ++i) {
    if (i) g += " & ";
    g += c.guard[i];
  }
  return g;
}

// D5's seven-rule case-split for one axis, as 7 SEPARATE "G(guard -> rule)"
// strings joined by " & " --- Spot's And is n-ary and flattens nested Ands,
// but not G-rooted children, so the result is 7 distinct top-level And
// children when this axis's string is itself conjoined into the whole
// formula (matches the enumerated arms' T5 test methodology: G-rooted
// children counted directly, exactly as tests/slippery_world_test.cpp's
// ANHasExactlyFourteenNPlusOneTopLevelConjuncts already does for arms 1/2).
// `inc_class`/`dec_class` name which of the five priority classes increments
// / decrements this axis: x is R/L; y is D/U (D5: "the y axis is the same
// table with U playing L's role ... and D playing R's").
std::string compact_axis_rules(char axis, std::int64_t n,
                               const char* inc_class, const char* dec_class) {
  const std::string max_p = compact_max(axis, n);
  const std::string min_p = compact_min(axis, n);
  const std::string max1_p = compact_max1(axis, n);
  const std::string min1_p = compact_min1(axis, n);
  const std::string keep_r = compact_keep(axis, n);
  const std::string inc_r = compact_inc_or_dec(axis, n, /*increment=*/true);
  const std::string dec_r = compact_inc_or_dec(axis, n, /*increment=*/false);
  const std::string inc2_r = compact_inc2_or_dec2(axis, n, /*increment=*/true);
  const std::string dec2_r = compact_inc2_or_dec2(axis, n, /*increment=*/false);
  const std::string set_max_r = compact_set_max(axis, n);
  const std::string set_min_r = compact_set_min(axis, n);

  const SlipperyClass* inc_c = nullptr;
  const SlipperyClass* dec_c = nullptr;
  for (const SlipperyClass& c : slippery_classes()) {
    if (c.name == std::string(inc_class)) inc_c = &c;
    if (c.name == std::string(dec_class)) dec_c = &c;
  }
  const std::string g_inc = compact_class_guard(*inc_c);
  const std::string g_dec = compact_class_guard(*dec_c);

  // Every rule's guard carries kNextExists, so all 14 go vacuous at the last
  // position instead of collapsing onto the current cell (see kNextExists).
  const std::string nx = std::string(" & ") + kNextExists;

  std::vector<std::string> rules;
  // The two mover classes, each split on slip -- 4 of the 7 rows (D5's
  // table).
  rules.push_back("G((" + g_inc + " & !slip" + nx + ") -> ((" + max_p +
                  ") -> (" + keep_r + ")) & (!(" + max_p + ") -> (" + inc_r +
                  ")))");
  rules.push_back("G((" + g_inc + " & slip" + nx + ") -> ((" + max_p + " | " +
                  max1_p + ") -> (" + set_max_r + ")) & (!(" + max_p + " | " +
                  max1_p + ") -> (" + inc2_r + ")))");
  rules.push_back("G((" + g_dec + " & !slip" + nx + ") -> ((" + min_p +
                  ") -> (" + keep_r + ")) & (!(" + min_p + ") -> (" + dec_r +
                  ")))");
  rules.push_back("G((" + g_dec + " & slip" + nx + ") -> ((" + min_p + " | " +
                  min1_p + ") -> (" + set_min_r + ")) & (!(" + min_p + " | " +
                  min1_p + ") -> (" + dec2_r + ")))");
  // The three non-mover classes: Keep, no slip split (D5's "-- | Keep" rows).
  for (const SlipperyClass& c : slippery_classes()) {
    if (&c == inc_c || &c == dec_c) continue;
    rules.push_back("G((" + compact_class_guard(c) + nx + ") -> (" + keep_r +
                    "))");
  }

  std::string out;
  for (std::size_t i = 0; i < rules.size(); ++i) {
    if (i) out += " & ";
    out += rules[i];
  }
  return out;
}

// D5's compact A_N: 1 init conjunct ("Min(bx) & Min(by)", same start-cell
// literal set as the enumerated arms' cell(0,0)) + 2 axes x 7 rules = 14
// G-rooted rules for every n (D8, T5), against the enumerated arms' 14N.
// Spot's And is n-ary and flattens, so the init conjunct contributes its own
// 2n literals as top-level children and the whole formula parses to 14 + 2n
// conjuncts -- NOT the constant 15 D8 first claimed; see T5 and the PRD's
// 2026-08-22 entry. Matches AssumptionBuilder's signature so
// instantiate_slippery's ONE code path (T_in construction included) serves
// this arm too.
std::string slippery_compact_assumption(const SlipperyEncoding& enc,
                                        std::int64_t /*N*/) {
  const std::int64_t n = enc.bits;
  std::string out = "(" + enc.cell(0, 0) + ")";
  out += " & " + compact_axis_rules('x', n, /*inc_class=*/"R", /*dec_class=*/"L");
  out += " & " + compact_axis_rules('y', n, /*inc_class=*/"D", /*dec_class=*/"U");
  return out;
}

// Arm 3 (D3, D5): binary position encoding (same 2n APs as arm 1), compact
// A_N. sweep()/instantiate() shape identical to arms 1/2 --- n >= 2 x
// realizable in {0,1}.
class SlipperyBinaryCompactFamily final : public BenchFamily {
 public:
  std::string name() const override { return "slippery-binary-compact"; }

  std::vector<BenchParams> sweep(std::int64_t n_min,
                                 std::int64_t n_max) const override {
    return default_sweep(n_min, n_max);
  }

  BenchCase instantiate(const BenchParams& params) const override {
    return instantiate_slippery(name(), /*one_hot=*/false, params,
                                &slippery_compact_assumption);
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

// ---------------------------------------------------------------------------
// The no-knowledge column (PRD "engineered-domain-families.md" D7): five
// `<method>-nk` subjects that run the same five methods on the SAME case but
// with the domain knowledge collapsed into the formula (psi_in -> phi) and
// Iknown/Oknown demoted to free, so the pairing with the EK row is exactly
// (family, params) with no new pairing convention.
// ---------------------------------------------------------------------------

// One no-knowledge instance built from a BenchCase's declared psi_in. nullopt
// iff `c.psi_in` is absent --- a case without it is skipped cleanly,
// recording nothing (D7, "Edge cases": the five landed non-T1 families are
// unaffected).
struct NkCase {
  spot::formula reduced;   // psi_in -> phi
  VariablePartition vars;  // Iknown -> Ifree, Oknown -> Ofree
  OutputLabeledTransducer t_in;
  OutputLabeledTransducer t_out;
};

std::optional<NkCase> build_nk_case(const BenchCase& c) {
  if (!c.psi_in.has_value()) return std::nullopt;

  // Demote FIRST (D7): trivial_transducer only accepts a role whose produced
  // slice is empty, and it is the demotion below that empties Iknown/Oknown
  // --- building against the domain partition would throw.
  VariablePartition nk_vars = c.vars;
  nk_vars.input_free.insert(nk_vars.input_known.begin(), nk_vars.input_known.end());
  nk_vars.input_known.clear();
  nk_vars.output_free.insert(nk_vars.output_known.begin(), nk_vars.output_known.end());
  nk_vars.output_known.clear();

  // A fresh dict: the demoted partition's turn-order requirement (every
  // Ifree var, now including what was Iknown, strictly above every
  // controllable var) generally differs from the domain dict's already-fixed
  // levels, and register_ap is idempotent --- reusing c.t_in's dict could not
  // repair a level a prior registration already set.
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  register_turn_order_aps(nk_vars, dict);

  const std::string phi_str = spot::str_psl(c.phi);
  const spot::formula reduced =
      parse_or_throw("(" + *c.psi_in + ") -> (" + phi_str + ")");

  OutputLabeledTransducer t_in = trivial_transducer(nk_vars, Role::t_in, dict);
  OutputLabeledTransducer t_out = trivial_transducer(nk_vars, Role::t_out, dict);

  return NkCase{reduced, nk_vars, std::move(t_in), std::move(t_out)};
}

class DfaProductNkSubject final : public BenchSubject {
 public:
  std::string name() const override { return "dfa-product-nk"; }
  void run(const BenchCase& c) const override {
    const auto nk = build_nk_case(c);
    if (!nk) return;
    DfaProduct method;
    method.synthesize(nk->reduced, nk->vars, nk->t_in, nk->t_out);
  }
};

class NfaProductNkSubject final : public BenchSubject {
 public:
  std::string name() const override { return "nfa-product-nk"; }
  void run(const BenchCase& c) const override {
    if (!c.psi_in.has_value()) return;
    if (!mona_available()) return;  // same MONA gate as NfaProductSubject.
    const auto nk = build_nk_case(c);
    if (!nk) return;
    NfaProduct method;
    method.synthesize(nk->reduced, nk->vars, nk->t_in, nk->t_out);
  }
};

class MtdfaProductNkSubject final : public BenchSubject {
 public:
  std::string name() const override { return "mtdfa-product-nk"; }
  void run(const BenchCase& c) const override {
    const auto nk = build_nk_case(c);
    if (!nk) return;
    MtdfaProduct method;
    method.synthesize(nk->reduced, nk->vars, nk->t_in, nk->t_out);
  }
};

class MtnfaProductNkSubject final : public BenchSubject {
 public:
  std::string name() const override { return "mtnfa-product-nk"; }
  void run(const BenchCase& c) const override {
    if (!c.psi_in.has_value()) return;
    if (!mona_available()) return;  // same MONA gate as MtnfaProductSubject.
    const auto nk = build_nk_case(c);
    if (!nk) return;
    MtnfaProduct method;
    method.synthesize(nk->reduced, nk->vars, nk->t_in, nk->t_out);
  }
};

class OtfMtdfaProductNkSubject final : public BenchSubject {
 public:
  std::string name() const override { return "otf-mtdfa-product-nk"; }
  void run(const BenchCase& c) const override {
    const auto nk = build_nk_case(c);
    if (!nk) return;
    OtfMtdfaProduct method;
    method.synthesize(nk->reduced, nk->vars, nk->t_in, nk->t_out);
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
    v.push_back(std::make_unique<KnowledgeChainFamily>());
    v.push_back(std::make_unique<KnowledgeChainInertFamily>());
    v.push_back(std::make_unique<ParityT3Family>());
    v.push_back(std::make_unique<SlipperyBinaryFamily>());
    v.push_back(std::make_unique<SlipperyOnehotFamily>());
    v.push_back(std::make_unique<SlipperyBinaryCompactFamily>());
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
    v.push_back(std::make_unique<DfaProductNkSubject>());
    v.push_back(std::make_unique<NfaProductNkSubject>());
    v.push_back(std::make_unique<MtdfaProductNkSubject>());
    v.push_back(std::make_unique<MtnfaProductNkSubject>());
    v.push_back(std::make_unique<OtfMtdfaProductNkSubject>());
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
