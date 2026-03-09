#include "can_node_sim/Simulator.hpp"
#include "can_node_sim/SocketCanTransport.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

int main() {
  can_node_sim::ObjectDictionary dictionary;
  dictionary[{0x3000, 0}] = can_node_sim::OdEntry{{0x3000, 0},
                                                  "SignalA",
                                                  can_node_sim::DataType::kUnsigned16,
                                                  can_node_sim::AccessType::kReadWrite,
                                                  uint64_t{0x2211}};
  dictionary[{0x3001, 0}] = can_node_sim::OdEntry{{0x3001, 0},
                                                  "SignalB",
                                                  can_node_sim::DataType::kUnsigned8,
                                                  can_node_sim::AccessType::kReadWrite,
                                                  uint64_t{0x33}};

  can_node_sim::PdoDefinition tpdo;
  tpdo.direction            = can_node_sim::PdoDirection::kTransmit;
  tpdo.pdo_number           = 1;
  tpdo.communication.cob_id = 0x181;
  tpdo.signals.push_back(can_node_sim::MappingSignal{{0x3000, 0}, 16, "SignalA"});
  tpdo.signals.push_back(can_node_sim::MappingSignal{{0x3001, 0}, 8, "SignalB"});

  can_node_sim::PdoDefinition rpdo = tpdo;
  rpdo.direction                   = can_node_sim::PdoDirection::kReceive;

  can_node_sim::CanNodeSimulator simulator(dictionary, {tpdo, rpdo});

  const auto encoded = can_node_sim::SocketCanTransport::EncodePdoPayload(tpdo, simulator);
  assert(encoded.size() == 3);
  assert(encoded[0] == 0x11);
  assert(encoded[1] == 0x22);
  assert(encoded[2] == 0x33);

  const std::vector<uint8_t> incoming{0xAA, 0xBB, 0xCC};
  can_node_sim::SocketCanTransport::DecodePdoPayload(rpdo, incoming, simulator);

  assert(std::get<uint64_t>(simulator.ReadOd(0x3000, 0)) == 0xBBAAULL);
  assert(std::get<uint64_t>(simulator.ReadOd(0x3001, 0)) == 0xCCULL);

  return 0;
}
