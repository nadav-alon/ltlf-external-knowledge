#include "ltlf_ek/cli.hpp"

#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

#include "ltlf_ek/detail/util.hpp"
#include "ltlf_ek/dfa_product.hpp"
#include "ltlf_ek/mtdfa_product.hpp"

namespace ltlf_ek {

VariablePartition parse_partition_file(std::istream& in) {
  // The four recognised keys, one line each; value = space-separated AP names.
  static const std::string kInputFree = "input_free";
  static const std::string kInputKnown = "input_known";
  static const std::string kOutputFree = "output_free";
  static const std::string kOutputKnown = "output_known";

  std::set<std::string> input_free, input_known, output_free, output_known;
  std::set<std::string> seen_keys;

  std::string line;
  while (std::getline(in, line)) {
    const std::size_t hash = line.find('#');
    const std::string content = detail::trim(
        hash == std::string::npos ? line : line.substr(0, hash));
    if (content.empty()) continue;

    const std::size_t colon = content.find(':');
    if (colon == std::string::npos)
      throw std::invalid_argument(
          "parse_partition_file: malformed line (expected 'key: values'): " +
          content);
    const std::string key = detail::trim(content.substr(0, colon));
    const std::string value = detail::trim(content.substr(colon + 1));

    std::set<std::string>* target = nullptr;
    if (key == kInputFree)
      target = &input_free;
    else if (key == kInputKnown)
      target = &input_known;
    else if (key == kOutputFree)
      target = &output_free;
    else if (key == kOutputKnown)
      target = &output_known;
    else
      throw std::invalid_argument("parse_partition_file: unknown key '" +
                                  key + "'");

    if (!seen_keys.insert(key).second)
      throw std::invalid_argument("parse_partition_file: duplicate key '" +
                                  key + "'");

    std::istringstream vs(value);
    std::string ap;
    while (vs >> ap) target->insert(ap);
  }

  VariablePartition p;
  p.input_free = std::move(input_free);
  p.input_known = std::move(input_known);
  p.output_free = std::move(output_free);
  p.output_known = std::move(output_known);

  // Partition disjointness: no AP may be listed in more than one set.
  std::set<std::string> seen_aps;
  for (const std::set<std::string>* s :
      {&p.input_free, &p.input_known, &p.output_free, &p.output_known})
    for (const auto& ap : *s)
      if (!seen_aps.insert(ap).second)
        throw std::invalid_argument(
            "parse_partition_file: AP '" + ap +
            "' listed in more than one set (partition must be disjoint)");

  return p;
}

std::unique_ptr<Synthesis> make_synthesis_method(
    const std::string& method_flag, bool minimize_mtdfa) {
  if (method_flag == "dfa-product") return std::make_unique<DfaProduct>();
  if (method_flag == "mtdfa-product")
    return std::make_unique<MtdfaProduct>(minimize_mtdfa);

  static const std::set<std::string> kRecognisedNotWired = {
      "nfa-product", "otf-dfa-product", "otf-agg-product",
      "otf-dyn-agg-product"};
  if (kRecognisedNotWired.count(method_flag))
    throw std::logic_error("make_synthesis_method: method '" + method_flag +
                           "' not yet implemented");

  throw std::invalid_argument("make_synthesis_method: unrecognised method '" +
                              method_flag + "'");
}

}  // namespace ltlf_ek
