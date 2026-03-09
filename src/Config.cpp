#include "can_node_sim/Config.hpp"

#include <stdexcept>
#include <yaml-cpp/yaml.h>

namespace can_node_sim {

namespace {

PdoCommOverride parse_pdo_override(const YAML::Node &node) {
  PdoCommOverride result;
  if (!node.IsMap()) {
    throw std::runtime_error("PDO override entry must be a map");
  }

  if (node["cob_id"]) {
    result.cob_id = node["cob_id"].as<uint32_t>();
  }
  if (node["transmission_type"]) {
    result.transmission_type = node["transmission_type"].as<uint8_t>();
  }
  if (node["inhibit_time"]) {
    result.inhibit_time = node["inhibit_time"].as<uint16_t>();
  }
  if (node["event_timer"]) {
    result.event_timer = node["event_timer"].as<uint16_t>();
  }

  return result;
}

void parse_pdo_override_section(const YAML::Node &section,
                                std::unordered_map<uint16_t, PdoCommOverride> &target) {
  if (!section) {
    return;
  }
  if (!section.IsMap()) {
    throw std::runtime_error("PDO override section must be a map");
  }

  for (const auto &item : section) {
    const auto pdo_number = item.first.as<uint16_t>();
    if (pdo_number == 0) {
      throw std::runtime_error("PDO number must be >= 1");
    }
    target[pdo_number] = parse_pdo_override(item.second);
  }
}

Value parse_value_for_type(const std::string &text, DataType data_type) {
  try {
    switch (data_type) {
      case DataType::kBoolean:
        if (text == "1" || text == "true" || text == "TRUE") {
          return true;
        }
        if (text == "0" || text == "false" || text == "FALSE") {
          return false;
        }
        throw std::runtime_error("Invalid boolean value");
      case DataType::kInteger8:
      case DataType::kInteger16:
      case DataType::kInteger32:
      case DataType::kInteger64:
        return static_cast<int64_t>(std::stoll(text, nullptr, 0));
      case DataType::kUnsigned8:
      case DataType::kUnsigned16:
      case DataType::kUnsigned32:
      case DataType::kUnsigned64:
        return static_cast<uint64_t>(std::stoull(text, nullptr, 0));
      case DataType::kReal32:
      case DataType::kReal64:
        return std::stod(text);
      case DataType::kVisibleString:
      case DataType::kOctetString:
      case DataType::kUnknown:
        return text;
    }
  } catch (const std::exception &ex) {
    throw std::runtime_error(std::string("Invalid OD default override value: ") + ex.what());
  }

  throw std::runtime_error("Unsupported data type");
}

void apply_single_pdo_override(uint16_t base_index,
                               const std::unordered_map<uint16_t, PdoCommOverride> &overrides,
                               ObjectDictionary &dictionary) {
  for (const auto &[pdo_number, override_value] : overrides) {
    const auto comm_index = static_cast<uint16_t>(base_index + pdo_number - 1U);

    if (override_value.cob_id.has_value()) {
      dictionary[{comm_index, 1}].key   = {comm_index, 1};
      dictionary[{comm_index, 1}].value = static_cast<uint64_t>(*override_value.cob_id);
    }
    if (override_value.transmission_type.has_value()) {
      dictionary[{comm_index, 2}].key   = {comm_index, 2};
      dictionary[{comm_index, 2}].value = static_cast<uint64_t>(*override_value.transmission_type);
    }
    if (override_value.inhibit_time.has_value()) {
      dictionary[{comm_index, 3}].key   = {comm_index, 3};
      dictionary[{comm_index, 3}].value = static_cast<uint64_t>(*override_value.inhibit_time);
    }
    if (override_value.event_timer.has_value()) {
      dictionary[{comm_index, 5}].key   = {comm_index, 5};
      dictionary[{comm_index, 5}].value = static_cast<uint64_t>(*override_value.event_timer);
    }
  }
}

}  // namespace

SimulatorConfig ConfigLoader::LoadFile(const std::string &file_path) {
  const auto root = YAML::LoadFile(file_path);
  return LoadText(YAML::Dump(root));
}

SimulatorConfig ConfigLoader::LoadText(const std::string &yaml_text) {
  const auto root = YAML::Load(yaml_text);
  SimulatorConfig config;

  if (root["node_id"]) {
    config.node_id = root["node_id"].as<uint8_t>();
  }

  if (const auto od_defaults = root["od_defaults"]; od_defaults) {
    if (!od_defaults.IsMap()) {
      throw std::runtime_error("od_defaults must be a map");
    }
    for (const auto &item : od_defaults) {
      config.od_defaults[item.first.as<std::string>()] = item.second.as<std::string>();
    }
  }

  if (const auto pdo_overrides = root["pdo_overrides"]; pdo_overrides) {
    if (!pdo_overrides.IsMap()) {
      throw std::runtime_error("pdo_overrides must be a map");
    }
    parse_pdo_override_section(pdo_overrides["rpdo"], config.rpdo_overrides);
    parse_pdo_override_section(pdo_overrides["tpdo"], config.tpdo_overrides);
  }

  return config;
}

void ConfigLoader::ApplyToDictionary(const SimulatorConfig &config, ObjectDictionary &dictionary) {
  for (const auto &[key_text, value_text] : config.od_defaults) {
    const auto key = parse_canonical_key(key_text);
    if (!key.has_value()) {
      throw std::runtime_error("Invalid od_defaults key: " + key_text);
    }

    const auto it = dictionary.find(*key);
    if (it == dictionary.end()) {
      throw std::runtime_error("OD key from config not found in dictionary: " + key_text);
    }

    it->second.value = parse_value_for_type(value_text, it->second.data_type);
  }

  apply_single_pdo_override(0x1400, config.rpdo_overrides, dictionary);
  apply_single_pdo_override(0x1800, config.tpdo_overrides, dictionary);
}

}  // namespace can_node_sim
