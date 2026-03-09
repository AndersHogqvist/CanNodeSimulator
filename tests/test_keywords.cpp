#include "can_node_sim/RobotKeywords.hpp"

#include <cassert>
#include <cstdint>

int main() {
  can_node_sim::ObjectDictionary dictionary;
  dictionary[{0x2000, 0}] = can_node_sim::OdEntry{{0x2000, 0},
                                                  "ControlWord",
                                                  can_node_sim::DataType::kUnsigned8,
                                                  can_node_sim::AccessType::kReadWrite,
                                                  uint64_t{0}};
  dictionary[{0x2001, 0}] = can_node_sim::OdEntry{{0x2001, 0},
                                                  "StatusWord",
                                                  can_node_sim::DataType::kUnsigned16,
                                                  can_node_sim::AccessType::kReadWrite,
                                                  uint64_t{0x1234}};

  can_node_sim::PdoDefinition rpdo;
  rpdo.direction            = can_node_sim::PdoDirection::kReceive;
  rpdo.pdo_number           = 1;
  rpdo.communication.cob_id = 0x201;
  rpdo.signals.push_back(can_node_sim::MappingSignal{{0x2000, 0}, 8, "ControlWord"});

  can_node_sim::CanNodeSimulator simulator(dictionary, {rpdo});
  can_node_sim::RobotKeywords keywords(simulator, {rpdo});

  assert(keywords.ReadOd(0x2001, 0) == "4660");

  keywords.WriteOd(0x2000, 0, "17");
  assert(std::get<uint64_t>(simulator.ReadOd(0x2000, 0)) == 17ULL);

  keywords.WritePdoSignal(can_node_sim::PdoDirection::kReceive, 1, 0, "85");
  assert(keywords.ReadPdoSignal(can_node_sim::PdoDirection::kReceive, 1, 0) == "85");

  const auto signals = keywords.ListPdoSignals(can_node_sim::PdoDirection::kReceive, 1);
  assert(signals.size() == 1);

  bool bad_value_failed = false;
  try {
    keywords.WriteOd(0x2000, 0, "not-a-number");
  } catch (const std::exception &) {
    bad_value_failed = true;
  }
  assert(bad_value_failed);

  bool start_can_failed = false;
  try {
    keywords.StartCan("can0");
  } catch (const std::exception &) {
    start_can_failed = true;
  }
  assert(start_can_failed);

  bool nmt_failed = false;
  try {
    keywords.SendNmtCommand(1, 0);
  } catch (const std::exception &) {
    nmt_failed = true;
  }
  assert(nmt_failed);

  bool heartbeat_failed = false;
  try {
    keywords.SendHeartbeat(5);
  } catch (const std::exception &) {
    heartbeat_failed = true;
  }
  assert(heartbeat_failed);

  return 0;
}
