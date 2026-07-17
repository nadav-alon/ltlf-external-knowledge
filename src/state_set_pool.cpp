#include "ltlf_ek/detail/state_set_pool.hpp"

#include <algorithm>
#include <iterator>
#include <limits>

namespace ltlf_ek::detail {

namespace {

// Sorted merge with de-duplication --- keeps interned sets canonical
// ("Novel mechanisms (a)").
std::vector<unsigned> MergeSorted(const std::vector<unsigned>& a,
                                  const std::vector<unsigned>& b) {
  std::vector<unsigned> out;
  out.reserve(a.size() + b.size());
  std::set_union(a.begin(), a.end(), b.begin(), b.end(),
                std::back_inserter(out));
  return out;
}

// A terminal counts as below every real variable ("Novel mechanisms (b)"):
// its notional level is past the last real variable, so it never wins a
// "topmost variable" comparison against a real variable node.
int LevelOf(const bdd& x) {
  return bdd_is_terminal(x) ? std::numeric_limits<int>::max() : bdd_level(x);
}

// Order-normalized pair-of-ids key: {a.id(), b.id()} unordered ("Novel
// mechanisms (b)"), packed into a single integer for the memo map.
std::uint64_t NormalizedKey(int a_id, int b_id) {
  const auto lo = static_cast<std::uint32_t>(std::min(a_id, b_id));
  const auto hi = static_cast<std::uint32_t>(std::max(a_id, b_id));
  return (static_cast<std::uint64_t>(hi) << 32) | lo;
}

}  // namespace

std::size_t StateSetPool::VectorHash::operator()(
    const std::vector<unsigned>& v) const noexcept {
  std::size_t h = v.size();
  for (unsigned x : v)
    h ^= std::hash<unsigned>{}(x) + 0x9e3779b9U + (h << 6) + (h >> 2);
  return h;
}

StateSetPool::StateSetPool() { intern({}); }

unsigned StateSetPool::intern(std::vector<unsigned> sorted_states) {
  auto it = index_of_.find(sorted_states);
  if (it != index_of_.end()) return it->second;
  const unsigned idx = static_cast<unsigned>(sets_.size());
  index_of_.emplace(sorted_states, idx);
  sets_.push_back(std::move(sorted_states));
  return idx;
}

const std::vector<unsigned>& StateSetPool::set_of(int terminal_index) const {
  return sets_.at(static_cast<std::size_t>(terminal_index));
}

bdd StateSetPool::guarded_singleton(const bdd& guard, unsigned state) {
  const unsigned idx = intern({state});
  return bdd_ite(guard, bdd_terminalpp(static_cast<int>(idx)),
                 bdd_terminalpp(0));
}

bdd StateSetPool::set_union(const bdd& a, const bdd& b) {
  std::unordered_map<std::uint64_t, bdd> memo;
  return UnionRec(a, b, memo);
}

bdd StateSetPool::UnionRec(const bdd& a, const bdd& b,
                           std::unordered_map<std::uint64_t, bdd>& memo) {
  if (a == b) return a;  // idempotent short-circuit

  const std::uint64_t key = NormalizedKey(a.id(), b.id());
  if (auto it = memo.find(key); it != memo.end()) return it->second;

  bdd result;
  if (bdd_is_terminal(a) && bdd_is_terminal(b)) {
    result = bdd_terminalpp(static_cast<int>(intern(
        MergeSorted(set_of(bdd_get_terminal(a)), set_of(bdd_get_terminal(b))))));
  } else {
    const int v = (LevelOf(a) <= LevelOf(b)) ? bdd_var(a) : bdd_var(b);
    const bdd a_lo = (!bdd_is_terminal(a) && bdd_var(a) == v) ? bdd_low(a) : a;
    const bdd a_hi = (!bdd_is_terminal(a) && bdd_var(a) == v) ? bdd_high(a) : a;
    const bdd b_lo = (!bdd_is_terminal(b) && bdd_var(b) == v) ? bdd_low(b) : b;
    const bdd b_hi = (!bdd_is_terminal(b) && bdd_var(b) == v) ? bdd_high(b) : b;
    result = bdd_ite(bdd_ithvarpp(v), UnionRec(a_hi, b_hi, memo),
                     UnionRec(a_lo, b_lo, memo));
  }
  memo.emplace(key, result);
  return result;
}

}  // namespace ltlf_ek::detail
