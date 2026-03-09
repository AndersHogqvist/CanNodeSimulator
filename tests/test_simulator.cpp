#include "can_node_sim/Simulator.hpp"

#include <cassert>

int main() {
  can_node_sim::ObjectDictionary dictionary;
  dictionary[{0x2000, 0}] = can_node_sim::OdEntry{{0x2000, 0},
                                                  "ControlWord",
                                                  can_node_sim::DataType::kUnsigned8,
                                                  can_node_sim::AccessType::kReadWrite,
                                                  uint64_t{0}};
  dictionary[{0x2001, 0}] = can_node_sim::OdEntry{{0x2001, 0},
                                                  "StatusWord",
                                                  can_node_sim::DataType::kUnsigned8,
                                                  can_node_sim::AccessType::kReadOnly,
                                                  uint64_t{5}};

  can_node_sim::PdoDefinition rpdo;
  rpdo.direction  = can_node_sim::PdoDirection::kReceive;
  rpdo.pdo_number = 1;
  rpdo.signals.push_back(can_node_sim::MappingSignal{{0x2000, 0}, 8, "ControlWord"});

  can_node_sim::CanNodeSimulator simulator(dictionary, {rpdo});

  simulator.WriteOd(0x2000, 0, uint64_t{7});
  assert(std::get<uint64_t>(simulator.ReadOd(0x2000, 0)) == 7ULL);

  simulator.WritePdoSignal(can_node_sim::PdoDirection::kReceive, 1, 0, uint64_t{9});
  assert(std::get<uint64_t>(simulator.ReadOd(0x2000, 0)) == 9ULL);

  bool write_protected = false;
  try {
    simulator.WriteOd(0x2001, 0, uint64_t{3});
  } catch (const can_node_sim::SimulatorError &) {
    write_protected = true;
  }

  assert(write_protected);

  return 0;
}
