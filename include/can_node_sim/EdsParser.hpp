#pragma once

#include "can_node_sim/Model.hpp"

#include <string>
#include <vector>

namespace can_node_sim {

/** Parse options that influence EDS value interpretation. */
struct EdsParseOptions {
  uint8_t node_id{1};
};

/** Parsed EDS output including dictionary and non-fatal warnings. */
struct ParsedEds {
  ObjectDictionary object_dictionary;
  std::vector<std::string> warnings;
};

/** Parser for a CANopen EDS subset required by the simulator. */
class EdsParser {
public:
  /** Parses an EDS file from disk. */
  static ParsedEds ParseFile(const std::string &file_path, const EdsParseOptions &options = {});
  /** Parses an EDS payload that is already loaded in memory. */
  static ParsedEds ParseText(const std::string &text, const EdsParseOptions &options = {});
};

}  // namespace can_node_sim
