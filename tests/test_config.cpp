#include "can_node_sim/Config.hpp"

#include <cassert>
#include <cstdint>
#include <string>

int main() {
  const std::string yaml_text = R"(
node_id: 7
od_defaults:
  "0x2000:0": 42
pdo_overrides:
  rpdo:
    1:
      transmission_type: 254
  tpdo:
    1:
      cob_id: 0x185
      transmission_type: 1
      inhibit_time: 10
      event_timer: 100
)";

  const auto config = can_node_sim::ConfigLoader::LoadText(yaml_text);
  assert(config.node_id.has_value());
  assert(*config.node_id == 7);
  assert(config.od_defaults.size() == 1);
  assert(config.rpdo_overrides.size() == 1);
  assert(config.tpdo_overrides.size() == 1);

  can_node_sim::ObjectDictionary dictionary;
  dictionary[{0x2000, 0}] = can_node_sim::OdEntry{{0x2000, 0},
                                                  "ControlWord",
                                                  can_node_sim::DataType::kUnsigned8,
                                                  can_node_sim::AccessType::kReadWrite,
                                                  uint64_t{0}};
  dictionary[{0x1400, 2}] = can_node_sim::OdEntry{{0x1400, 2},
                                                  "RPDO1 TxType",
                                                  can_node_sim::DataType::kUnsigned8,
                                                  can_node_sim::AccessType::kReadWrite,
                                                  uint64_t{255}};
  dictionary[{0x1800, 1}] = can_node_sim::OdEntry{{0x1800, 1},
                                                  "TPDO1 COB-ID",
                                                  can_node_sim::DataType::kUnsigned32,
                                                  can_node_sim::AccessType::kReadWrite,
                                                  uint64_t{0x181}};
  dictionary[{0x1800, 2}] = can_node_sim::OdEntry{{0x1800, 2},
                                                  "TPDO1 TxType",
                                                  can_node_sim::DataType::kUnsigned8,
                                                  can_node_sim::AccessType::kReadWrite,
                                                  uint64_t{255}};
  dictionary[{0x1800, 3}] = can_node_sim::OdEntry{{0x1800, 3},
                                                  "TPDO1 Inhibit",
                                                  can_node_sim::DataType::kUnsigned16,
                                                  can_node_sim::AccessType::kReadWrite,
                                                  uint64_t{0}};
  dictionary[{0x1800, 5}] = can_node_sim::OdEntry{{0x1800, 5},
                                                  "TPDO1 Event",
                                                  can_node_sim::DataType::kUnsigned16,
                                                  can_node_sim::AccessType::kReadWrite,
                                                  uint64_t{0}};

  can_node_sim::ConfigLoader::ApplyToDictionary(config, dictionary);

  assert(std::get<uint64_t>(dictionary[{0x2000, 0}].value) == 42ULL);
  assert(std::get<uint64_t>(dictionary[{0x1400, 2}].value) == 254ULL);
  assert(std::get<uint64_t>(dictionary[{0x1800, 1}].value) == 0x185ULL);
  assert(std::get<uint64_t>(dictionary[{0x1800, 2}].value) == 1ULL);
  assert(std::get<uint64_t>(dictionary[{0x1800, 3}].value) == 10ULL);
  assert(std::get<uint64_t>(dictionary[{0x1800, 5}].value) == 100ULL);

  return 0;
}
