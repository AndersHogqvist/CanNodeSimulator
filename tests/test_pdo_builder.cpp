#include "can_node_sim/PdoBuilder.hpp"

#include <cassert>

int main() {
  can_node_sim::ObjectDictionary dictionary;

  dictionary[{0x1400, 1}] = can_node_sim::OdEntry{{0x1400, 1},
                                                  "RPDO1 COB-ID",
                                                  can_node_sim::DataType::kUnsigned32,
                                                  can_node_sim::AccessType::kReadWrite,
                                                  uint64_t{0x200}};
  dictionary[{0x1400, 2}] = can_node_sim::OdEntry{{0x1400, 2},
                                                  "RPDO1 Type",
                                                  can_node_sim::DataType::kUnsigned8,
                                                  can_node_sim::AccessType::kReadWrite,
                                                  uint64_t{255}};
  dictionary[{0x1600, 0}] = can_node_sim::OdEntry{{0x1600, 0},
                                                  "RPDO1 map count",
                                                  can_node_sim::DataType::kUnsigned8,
                                                  can_node_sim::AccessType::kReadWrite,
                                                  uint64_t{1}};
  dictionary[{0x1600, 1}] = can_node_sim::OdEntry{{0x1600, 1},
                                                  "RPDO1 map 1",
                                                  can_node_sim::DataType::kUnsigned32,
                                                  can_node_sim::AccessType::kReadWrite,
                                                  uint64_t{0x20000008}};
  dictionary[{0x2000, 0}] = can_node_sim::OdEntry{{0x2000, 0},
                                                  "ControlWord",
                                                  can_node_sim::DataType::kUnsigned8,
                                                  can_node_sim::AccessType::kReadWrite,
                                                  uint64_t{0}};

  const auto result = can_node_sim::PdoBuilder::Build(dictionary);

  assert(result.pdos.size() == 1);
  const auto &pdo = result.pdos.front();
  assert(pdo.direction == can_node_sim::PdoDirection::kReceive);
  assert(pdo.pdo_number == 1);
  assert(pdo.communication.cob_id == 0x200U);
  assert(pdo.signals.size() == 1);
  assert(pdo.signals[0].key.index == 0x2000);
  assert(pdo.signals[0].key.sub_index == 0);
  assert(pdo.signals[0].bit_length == 8);

  return 0;
}
