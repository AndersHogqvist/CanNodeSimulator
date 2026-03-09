#pragma once

#include "can_node_sim/Model.hpp"

#include <vector>

namespace can_node_sim {

/** Result of deriving PDO definitions from the object dictionary. */
struct PdoBuildResult {
  std::vector<PdoDefinition> pdos;
  std::vector<BuildWarning> warnings;
};

/** Builds RPDO/TPDO definitions from standard CANopen OD ranges. */
class PdoBuilder {
public:
  static PdoBuildResult Build(const ObjectDictionary &dictionary);
};

}  // namespace can_node_sim
