#pragma once

#include "can_node_sim/Model.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace can_node_sim {

/** Communication-parameter overrides for one PDO. */
struct PdoCommOverride {
  std::optional<uint32_t> cob_id;
  std::optional<uint8_t> transmission_type;
  std::optional<uint16_t> inhibit_time;
  std::optional<uint16_t> event_timer;
};

/** Parsed simulator configuration from YAML. */
struct SimulatorConfig {
  std::optional<uint8_t> node_id;
  std::unordered_map<std::string, std::string> od_defaults;
  std::unordered_map<uint16_t, PdoCommOverride> rpdo_overrides;
  std::unordered_map<uint16_t, PdoCommOverride> tpdo_overrides;
};

/** Loader and applier for simulator YAML configuration. */
class ConfigLoader {
public:
  /** Loads configuration from a YAML file path. */
  static SimulatorConfig LoadFile(const std::string &file_path);
  /** Loads configuration from YAML text content. */
  static SimulatorConfig LoadText(const std::string &yaml_text);

  /** Applies OD and PDO overrides to a parsed object dictionary. */
  static void ApplyToDictionary(const SimulatorConfig &config, ObjectDictionary &dictionary);
};

}  // namespace can_node_sim
