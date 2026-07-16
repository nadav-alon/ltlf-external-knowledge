#include "ltlf_ek/detail/past_ltlf_to_dfa.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>

#include <bddx.h>
#include <spot/tl/apcollect.hh>

#include "ltlf_ek/detail/mona_dfa.hpp"

namespace ltlf_ek::detail {

namespace {

// Reserved prefix for the encoder's own bound position variables (the fresh
// "j"/"k"/"__last" MONA quantifier names introduced per temporal operator).
// Must never collide with an AP name declared in `var2 ...;` -- checked in
// encode_mirror before emitting any source.
constexpr char kFreshPrefix[] = "_ekpos";

// Recursive spot::formula -> M2L-Str walk (see past_ltlf_to_dfa.hpp's
// doc-comment for the per-operator table).  One MirrorWalker per
// encode_mirror call; `next_` hands out strictly-increasing fresh position
// names so nested quantifiers (e.g. inside `f1 U f2`'s two bound variables)
// never collide.
class MirrorWalker {
 public:
  std::string Fresh() { return kFreshPrefix + std::to_string(next_++); }

  // Trans(f, pos): the M2L-Str clause for f's *mirror* dual, holding at
  // rev(w) position `pos` iff f (in phi's ordinary future reading) holds at
  // the corresponding original position.  `pos` is always either the
  // top-level "last position" variable or a variable this walker itself
  // freshly bound -- never a compound expression -- so arithmetic like
  // "pos-1" below is always applied to a single MONA var1 name.
  std::string Trans(const spot::formula& f, const std::string& pos) {
    switch (f.kind()) {
      case spot::op::ap:
        return "(" + pos + " in " + f.ap_name() + ")";
      case spot::op::tt:
        return "(true)";
      case spot::op::ff:
        return "(false)";
      case spot::op::Not:
        return "(~" + Trans(f[0], pos) + ")";
      case spot::op::And: {
        std::string s = "(true";
        for (unsigned i = 0; i < f.size(); ++i)
          s += " & " + Trans(f[i], pos);
        return s + ")";
      }
      case spot::op::Or: {
        std::string s = "(false";
        for (unsigned i = 0; i < f.size(); ++i)
          s += " | " + Trans(f[i], pos);
        return s + ")";
      }
      case spot::op::Xor:
        return "(~(" + Trans(f[0], pos) + " <=> " + Trans(f[1], pos) + "))";
      case spot::op::Implies:
        return "(" + Trans(f[0], pos) + " => " + Trans(f[1], pos) + ")";
      case spot::op::Equiv:
        return "(" + Trans(f[0], pos) + " <=> " + Trans(f[1], pos) + ")";
      case spot::op::X:
        return TransWeakYesterday(f[0], pos);
      case spot::op::strong_X:
        return TransStrongYesterday(f[0], pos);
      case spot::op::F:
        return TransOnce(f[0], pos);
      case spot::op::G:
        return TransHistorically(f[0], pos);
      case spot::op::U:
        return TransSince(f[0], f[1], pos);
      case spot::op::R:
        return TransTrigger(f[0], f[1], pos);
      case spot::op::W:
        return "(" + TransSince(f[0], f[1], pos) + " | " +
              TransHistorically(f[0], pos) + ")";
      case spot::op::M:
        return TransStrongTrigger(f[0], f[1], pos);
      default:
        throw std::runtime_error(
            "encode_mirror: LTLf operator '" + f.kindstr() +
            "' has no M2L-Str clause (past_ltlf_to_dfa.hpp's table covers "
            "ap/tt/ff/boolean/X/strong_X/F/G/U/R/W/M only)");
    }
  }

 private:
  // X f (weak next), mirrored: dual is a *weak* Yesterday.  Holds at pos if
  // there is no predecessor (pos=0, vacuously true, matching Spot's weak X
  // being satisfied at the last original position) or the predecessor
  // satisfies f's clause.
  std::string TransWeakYesterday(const spot::formula& f,
                                 const std::string& pos) {
    const std::string j = Fresh();
    return "((" + pos + "=0) | (ex1 " + j + ": " + j + "=" + pos + "-1 & " +
          Trans(f, j) + "))";
  }

  // strong_X f, mirrored: dual is a *strong* Yesterday -- a predecessor must
  // exist (pos!=0) and satisfy f's clause.
  std::string TransStrongYesterday(const spot::formula& f,
                                   const std::string& pos) {
    const std::string j = Fresh();
    return "(~(" + pos + "=0) & (ex1 " + j + ": " + j + "=" + pos + "-1 & " +
          Trans(f, j) + "))";
  }

  // F f, mirrored: dual is Once -- some j at or before pos satisfies f.
  std::string TransOnce(const spot::formula& f, const std::string& pos) {
    const std::string j = Fresh();
    return "(ex1 " + j + ": " + j + "<=" + pos + " & " + Trans(f, j) + ")";
  }

  // G f, mirrored: dual is Historically -- every j at or before pos
  // satisfies f.
  std::string TransHistorically(const spot::formula& f,
                                const std::string& pos) {
    const std::string j = Fresh();
    return "(all1 " + j + ": " + j + "<=" + pos + " => " + Trans(f, j) + ")";
  }

  // f1 U f2, mirrored: dual is Since -- some j<=pos satisfies f2, and f1
  // holds on every position strictly after j up to and including pos.
  std::string TransSince(const spot::formula& f1, const spot::formula& f2,
                         const std::string& pos) {
    const std::string j = Fresh();
    const std::string k = Fresh();
    return "(ex1 " + j + ": " + j + "<=" + pos + " & " + Trans(f2, j) +
          " & (all1 " + k + ": (" + k + ">" + j + " & " + k + "<=" + pos +
          ") => " + Trans(f1, k) + "))";
  }

  // f1 R f2, mirrored: R is U's De Morgan dual, so its mirror is Since's De
  // Morgan dual (a "Trigger"): every j<=pos satisfies f2, unless f1 already
  // held at some position strictly after j (up to and including pos), which
  // releases f2 from that point on.
  std::string TransTrigger(const spot::formula& f1, const spot::formula& f2,
                           const std::string& pos) {
    const std::string j = Fresh();
    const std::string k = Fresh();
    return "(all1 " + j + ": " + j + "<=" + pos + " => (" + Trans(f2, j) +
          " | (ex1 " + k + ": (" + k + ">" + j + " & " + k + "<=" + pos +
          ") & " + Trans(f1, k) + ")))";
  }

  // f1 M f2 (strong release, dual of W), mirrored directly (equivalent to
  // "f2 U (f1 & f2)" mirrored): some j<=pos satisfies f1, and f2 holds on
  // every position from j through pos, inclusive.
  std::string TransStrongTrigger(const spot::formula& f1,
                                 const spot::formula& f2,
                                 const std::string& pos) {
    const std::string j = Fresh();
    const std::string k = Fresh();
    return "(ex1 " + j + ": " + j + "<=" + pos + " & " + Trans(f1, j) +
          " & (all1 " + k + ": (" + k + ">=" + j + " & " + k + "<=" + pos +
          ") => " + Trans(f2, k) + "))";
  }

  unsigned next_ = 0;
};

}  // namespace

MirrorEncoding encode_mirror(const spot::formula& phi) {
  // phi's AP support (as ltlf_to_dfa), sorted by name for a deterministic
  // "var2 ...;" declaration order shared with mona_output_to_dfa's
  // var_order.
  std::unique_ptr<spot::atomic_prop_set> aps(spot::atomic_prop_collect(phi));
  std::vector<std::string> var_order;
  for (const spot::formula& ap : *aps) var_order.push_back(ap.ap_name());
  std::sort(var_order.begin(), var_order.end());

  for (const std::string& name : var_order)
    if (name.rfind(kFreshPrefix, 0) == 0)
      throw std::runtime_error(
          "encode_mirror: AP name '" + name +
          "' collides with the encoder's reserved fresh-position-variable "
          "prefix '" + std::string(kFreshPrefix) + "'");

  MirrorWalker walker;
  // Wrap: the mirror's evaluation point is rev(w)'s *last* position
  // (def:mirror: "rev(w), |w|-1 |= mirror(phi)").  "ex1 __last: (all1 __q:
  // __q<=__last) & ..." both selects that unique position and has no
  // witness on the empty string, so the non-empty-trace exclusion is a
  // consequence of the encoding, not a special case.
  const std::string last = walker.Fresh();
  const std::string q = walker.Fresh();
  const std::string body = walker.Trans(phi, last);
  const std::string formula_line = "ex1 " + last + ": (all1 " + q + ": " +
                                   q + "<=" + last + ") & " + body + ";\n";

  std::string source = "m2l-str;\n";
  if (!var_order.empty()) {
    source += "var2 ";
    for (std::size_t i = 0; i < var_order.size(); ++i) {
      if (i) source += ", ";
      source += var_order[i];
    }
    source += ";\n";
  }
  source += formula_line;

  return MirrorEncoding{std::move(source), std::move(var_order)};
}

namespace {

// MONA compiles every M2L-Str formula with one extra leading state (see
// past_ltlf_to_dfa.hpp's doc-comment): the reported "Initial state" has
// exactly one out-edge, unconditional (guard=true), to the automaton's real
// initial state.  Re-points `d`'s init at that successor and purges the now-
// unreachable original init.  Throws if the assumed shape doesn't hold.
void StripMonaPreludeState(const spot::twa_graph_ptr& d) {
  const unsigned s0 = d->get_init_state_number();
  unsigned dst = 0;
  unsigned edge_count = 0;
  for (const auto& e : d->out(s0)) {
    ++edge_count;
    dst = e.dst;
    if (e.cond != bddtrue)
      throw std::runtime_error(
          "past_ltlf_to_dfa: MONA's leading state (the reported \"Initial "
          "state\") has a guard != true; the assumed MONA M2L-Str "
          "unconditional-prelude-edge artifact does not hold for this DFA");
  }
  if (edge_count != 1)
    throw std::runtime_error(
        "past_ltlf_to_dfa: MONA's leading state (the reported \"Initial "
        "state\") has " + std::to_string(edge_count) +
        " outgoing edge(s), expected exactly 1; the assumed MONA M2L-Str "
        "unconditional-prelude-edge artifact does not hold for this DFA");
  d->set_init_state(dst);
  d->purge_unreachable_states();
}

}  // namespace

spot::twa_graph_ptr past_ltlf_to_dfa(const spot::formula& phi,
                                     const spot::bdd_dict_ptr& dict) {
  const MirrorEncoding encoding = encode_mirror(phi);
  const std::string mona_out = run_mona(encoding.source);
  spot::twa_graph_ptr d =
      mona_output_to_dfa(mona_out, encoding.var_order, dict);
  StripMonaPreludeState(d);
  return d;
}

}  // namespace ltlf_ek::detail
