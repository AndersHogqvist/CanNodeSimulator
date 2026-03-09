#include "can_node_sim/EdsParser.hpp"

#include <cassert>
#include <cstdint>
#include <string>

int main() {
  const std::string eds = R"(
[1018sub1]
ParameterName="Vendor ID"
DataType=0x0007
AccessType=ro
DefaultValue=0x11223344

[1800sub1]
ParameterName="TPDO1 COB-ID"
DataType=0x0007
AccessType=rw
DefaultValue=$NODEID+0x180

[2000sub0]
ParameterName="Mode"
DataType=0x0005
AccessType=rw
DefaultValue=2
)";

  auto parsed =
      can_node_sim::EdsParser::ParseText(eds, can_node_sim::EdsParseOptions{.node_id = 3});

  const can_node_sim::ObjectKey vendor_key{0x1018, 1};
  const auto vendor_it = parsed.object_dictionary.find(vendor_key);
  assert(vendor_it != parsed.object_dictionary.end());
  assert(std::get<uint64_t>(vendor_it->second.value) == 0x11223344ULL);

  const can_node_sim::ObjectKey cob_id_key{0x1800, 1};
  const auto cob_id_it = parsed.object_dictionary.find(cob_id_key);
  assert(cob_id_it != parsed.object_dictionary.end());
  assert(std::get<uint64_t>(cob_id_it->second.value) == 0x183ULL);

  const can_node_sim::ObjectKey mode_key{0x2000, 0};
  const auto mode_it = parsed.object_dictionary.find(mode_key);
  assert(mode_it != parsed.object_dictionary.end());
  assert(std::get<uint64_t>(mode_it->second.value) == 2ULL);

  return 0;
}
