#include "ltlf_ek/turn_order.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <bddx.h>
#include <spot/tl/formula.hh>
#include <spot/twa/twagraph.hh>

namespace ltlf_ek {

namespace {

// Every registrar register_turn_order_aps ever creates is kept alive for the
// remainder of the process (Developer comments / PRD disagreements,
// docs/prd/mtdfa-product.md): spot::bdd_dict::register_proposition is
// ref-counted per owner, and unregistering the LAST owner of an AP erases its
// var_map entry --- silently undoing the fixed order the moment a fresh
// owner (t_in, t_out, the Goal automaton, ...) re-registers the same name,
// since a freed slot can be reallocated to a DIFFERENT formula.  A
// spot::twa_graph is used as the owner (not a bare pointer) because its
// destructor unregisters through the SAME dict it holds a strong reference
// to, so it can only ever unregister once every OTHER owner of that dict has
// already gone --- this container just defers that moment to process exit,
// trading a small, bounded footprint (one empty twa_graph per
// register_turn_order_aps call) for the correctness guarantee register_ap's
// idempotence depends on.
std::vector<spot::twa_graph_ptr>& persistent_registrars() {
  static std::vector<spot::twa_graph_ptr> registrars;
  return registrars;
}

int level_of(const spot::bdd_dict_ptr& dict, const std::string& name) {
  try {
    return bdd_var2level(dict->varnum(spot::formula::ap(name)));
  } catch (const std::out_of_range&) {
    throw std::invalid_argument(
        "require_turn_order_aps: AP '" + name +
        "' is not registered on dict (register_turn_order_aps must run "
        "first)");
  }
}

}  // namespace

void register_turn_order_aps(const VariablePartition& vars,
                             const spot::bdd_dict_ptr& dict) {
  spot::twa_graph_ptr registrar = spot::make_twa_graph(dict);
  for (const auto& n : vars.input_free) registrar->register_ap(n);
  for (const auto& n : vars.input_known) registrar->register_ap(n);
  for (const auto& n : vars.output_free) registrar->register_ap(n);
  for (const auto& n : vars.output_known) registrar->register_ap(n);
  persistent_registrars().push_back(std::move(registrar));
}

void require_turn_order_aps(const VariablePartition& vars,
                            const spot::bdd_dict_ptr& dict) {
  if (vars.input_free.empty()) return;  // vacuously satisfied.

  int max_ifree_level = -1;
  for (const auto& n : vars.input_free)
    max_ifree_level = std::max(max_ifree_level, level_of(dict, n));

  bool any_controllable = false;
  int min_controllable_level = -1;
  for (const std::set<std::string>* s :
      {&vars.input_known, &vars.output_free, &vars.output_known})
    for (const auto& n : *s) {
      const int lvl = level_of(dict, n);
      if (!any_controllable || lvl < min_controllable_level)
        min_controllable_level = lvl;
      any_controllable = true;
    }
  if (!any_controllable) return;  // vacuously satisfied.

  if (max_ifree_level >= min_controllable_level)
    throw std::invalid_argument(
        "require_turn_order_aps: an input_free variable does not sit "
        "strictly above every controllable variable (Ofree u Iknown u "
        "Oknown) in the BDD variable order --- the mtdfa game would "
        "silently read this as Moore semantics (main.tex Turn order, §83)");
}

}  // namespace ltlf_ek
