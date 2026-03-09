#include "can_node_sim/PdoBuilder.hpp"

#include <sstream>

namespace can_node_sim {

namespace {

std::optional<uint64_t> read_u64(const ObjectDictionary &dictionary,
                                 uint16_t index,
                                 uint8_t sub_index) {
  const ObjectKey key{index, sub_index};
  const auto it = dictionary.find(key);
  if (it == dictionary.end()) {
    return std::nullopt;
  }
  return value_to_u64(it->second.value);
}

std::string key_text(uint16_t index, uint8_t sub_index) { return canonical_key(index, sub_index); }

PdoBuildResult build_range(const ObjectDictionary &dictionary,
                           PdoDirection direction,
                           uint16_t comm_start,
                           uint16_t map_start,
                           uint16_t count) {
  PdoBuildResult result;

  for (uint16_t offset = 0; offset < count; ++offset) {
    const auto map_index        = static_cast<uint16_t>(map_start + offset);
    const auto comm_index       = static_cast<uint16_t>(comm_start + offset);
    const auto signal_count_opt = read_u64(dictionary, map_index, 0);
    if (!signal_count_opt.has_value()) {
      continue;
    }

    PdoDefinition pdo;
    pdo.direction  = direction;
    pdo.pdo_number = static_cast<uint16_t>(offset + 1);

    if (const auto cob_id = read_u64(dictionary, comm_index, 1); cob_id.has_value()) {
      pdo.communication.cob_id = static_cast<uint32_t>(*cob_id);
    }
    else {
      result.warnings.push_back(BuildWarning{"Missing COB-ID at " + key_text(comm_index, 1)});
    }

    if (const auto tx_type = read_u64(dictionary, comm_index, 2); tx_type.has_value()) {
      pdo.communication.transmission_type = static_cast<uint8_t>(*tx_type);
    }
    if (const auto inhibit_time = read_u64(dictionary, comm_index, 3); inhibit_time.has_value()) {
      pdo.communication.inhibit_time = static_cast<uint16_t>(*inhibit_time);
    }
    if (const auto event_timer = read_u64(dictionary, comm_index, 5); event_timer.has_value()) {
      pdo.communication.event_timer = static_cast<uint16_t>(*event_timer);
    }

    const auto signal_count = static_cast<uint8_t>(*signal_count_opt);
    for (uint8_t sub = 1; sub <= signal_count; ++sub) {
      const auto mapping_word = read_u64(dictionary, map_index, sub);
      if (!mapping_word.has_value()) {
        result.warnings.push_back(BuildWarning{"Missing mapping at " + key_text(map_index, sub)});
        continue;
      }

      const auto packed = static_cast<uint32_t>(*mapping_word);
      MappingSignal signal;
      signal.key.index     = static_cast<uint16_t>((packed >> 16U) & 0xFFFFU);
      signal.key.sub_index = static_cast<uint8_t>((packed >> 8U) & 0xFFU);
      signal.bit_length    = static_cast<uint8_t>(packed & 0xFFU);

      const auto od_it = dictionary.find(signal.key);
      if (od_it != dictionary.end() && !od_it->second.parameter_name.empty()) {
        signal.name = od_it->second.parameter_name;
      }
      else {
        signal.name = signal.key.ToCanonicalString();
        std::ostringstream oss;
        oss << "Missing OD entry for mapped signal " << signal.key.ToCanonicalString();
        result.warnings.push_back(BuildWarning{oss.str()});
      }

      pdo.signals.push_back(std::move(signal));
    }

    result.pdos.push_back(std::move(pdo));
  }

  return result;
}

}  // namespace

PdoBuildResult PdoBuilder::Build(const ObjectDictionary &dictionary) {
  PdoBuildResult result;

  auto append = [&](const PdoBuildResult &partial) {
    result.pdos.insert(result.pdos.end(), partial.pdos.begin(), partial.pdos.end());
    result.warnings.insert(result.warnings.end(), partial.warnings.begin(), partial.warnings.end());
  };

  append(build_range(dictionary, PdoDirection::kReceive, 0x1400, 0x1600, 0x200));
  append(build_range(dictionary, PdoDirection::kTransmit, 0x1800, 0x1A00, 0x200));

  return result;
}

}  // namespace can_node_sim
