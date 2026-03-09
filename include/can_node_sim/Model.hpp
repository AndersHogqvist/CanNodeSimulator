#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace can_node_sim {

/** Supported CANopen object data types used by this simulator. */
enum class DataType : std::uint8_t {
  kUnknown,
  kBoolean,
  kInteger8,
  kInteger16,
  kInteger32,
  kInteger64,
  kUnsigned8,
  kUnsigned16,
  kUnsigned32,
  kUnsigned64,
  kReal32,
  kReal64,
  kVisibleString,
  kOctetString
};

/** Access permissions for object-dictionary entries. */
enum class AccessType : std::uint8_t { kReadOnly, kReadWrite, kWriteOnly, kConstant, kUnknown };

using Value = std::variant<std::monostate, bool, int64_t, uint64_t, double, std::string>;

struct ObjectKey {
  uint16_t index{0};
  uint8_t sub_index{0};

  bool operator==(const ObjectKey &other) const noexcept {
    return index == other.index && sub_index == other.sub_index;
  }

  /** Returns key in canonical text form: 0xXXXX:sub. */
  std::string ToCanonicalString() const;
};

struct ObjectKeyHash {
  std::size_t operator()(const ObjectKey &key) const noexcept;
};

struct OdEntry {
  ObjectKey key;
  std::string parameter_name;
  DataType data_type{DataType::kUnknown};
  AccessType access_type{AccessType::kUnknown};
  Value value;
};

using ObjectDictionary = std::unordered_map<ObjectKey, OdEntry, ObjectKeyHash>;

struct MappingSignal {
  ObjectKey key;
  uint8_t bit_length{0};
  std::string name;
};

/** Direction of a Process Data Object. */
enum class PdoDirection : std::uint8_t { kReceive, kTransmit };

struct PdoCommunication {
  uint32_t cob_id{0};
  uint8_t transmission_type{255};
  uint16_t inhibit_time{0};
  uint16_t event_timer{0};
};

struct PdoDefinition {
  PdoDirection direction{PdoDirection::kReceive};
  uint16_t pdo_number{1};
  PdoCommunication communication;
  std::vector<MappingSignal> signals;
};

struct BuildWarning {
  std::string message;
};

/** Builds a canonical object key string from index and sub-index. */
std::string canonical_key(uint16_t index, uint8_t sub_index);

/** Parses a canonical object key string into a structured key. */
std::optional<ObjectKey> parse_canonical_key(const std::string &key_text);

/** Converts an EDS data-type code to the internal enum value. */
DataType data_type_from_eds_code(uint32_t type_code);

/** Converts textual EDS access mode to the internal enum value. */
AccessType access_type_from_eds_text(const std::string &access_text);

bool is_numeric_type(DataType type);

bool value_is_compatible(DataType type, const Value &value);

std::optional<uint64_t> value_to_u64(const Value &value);

}  // namespace can_node_sim
