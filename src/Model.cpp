#include "can_node_sim/Model.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace can_node_sim {

namespace {
std::string to_lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}
}  // namespace

std::string ObjectKey::ToCanonicalString() const { return canonical_key(index, sub_index); }

std::size_t ObjectKeyHash::operator()(const ObjectKey &key) const noexcept {
  return static_cast<std::size_t>((static_cast<uint32_t>(key.index) << 8U) | key.sub_index);
}

std::string canonical_key(uint16_t index, uint8_t sub_index) {
  std::ostringstream oss;
  oss << "0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << index << ":"
      << std::dec << static_cast<unsigned>(sub_index);
  return oss.str();
}

std::optional<ObjectKey> parse_canonical_key(const std::string &key_text) {
  const auto colon = key_text.find(':');
  if (colon == std::string::npos) {
    return std::nullopt;
  }

  try {
    const auto index_text = key_text.substr(0, colon);
    const auto sub_text   = key_text.substr(colon + 1);
    const auto index      = static_cast<uint16_t>(std::stoul(index_text, nullptr, 0));
    const auto sub        = static_cast<uint8_t>(std::stoul(sub_text, nullptr, 0));
    return ObjectKey{index, sub};
  } catch (...) {
    return std::nullopt;
  }
}

DataType data_type_from_eds_code(uint32_t type_code) {
  switch (type_code) {
    case 0x0001:
      return DataType::kBoolean;
    case 0x0002:
      return DataType::kInteger8;
    case 0x0003:
      return DataType::kInteger16;
    case 0x0004:
      return DataType::kInteger32;
    case 0x0015:
      return DataType::kInteger64;
    case 0x0005:
      return DataType::kUnsigned8;
    case 0x0006:
      return DataType::kUnsigned16;
    case 0x0007:
      return DataType::kUnsigned32;
    case 0x001B:
      return DataType::kUnsigned64;
    case 0x0008:
      return DataType::kReal32;
    case 0x0011:
      return DataType::kReal64;
    case 0x0009:
      return DataType::kVisibleString;
    case 0x000A:
      return DataType::kOctetString;
    default:
      return DataType::kUnknown;
  }
}

AccessType access_type_from_eds_text(const std::string &access_text) {
  const auto normalized = to_lower(access_text);
  if (normalized == "ro") {
    return AccessType::kReadOnly;
  }
  if (normalized == "rw") {
    return AccessType::kReadWrite;
  }
  if (normalized == "wo") {
    return AccessType::kWriteOnly;
  }
  if (normalized == "const") {
    return AccessType::kConstant;
  }
  return AccessType::kUnknown;
}

bool is_numeric_type(DataType type) {
  switch (type) {
    case DataType::kBoolean:
    case DataType::kInteger8:
    case DataType::kInteger16:
    case DataType::kInteger32:
    case DataType::kInteger64:
    case DataType::kUnsigned8:
    case DataType::kUnsigned16:
    case DataType::kUnsigned32:
    case DataType::kUnsigned64:
    case DataType::kReal32:
    case DataType::kReal64:
      return true;
    default:
      return false;
  }
}

bool value_is_compatible(DataType type, const Value &value) {
  if (std::holds_alternative<std::monostate>(value)) {
    return true;
  }

  switch (type) {
    case DataType::kBoolean:
      return std::holds_alternative<bool>(value);
    case DataType::kInteger8:
    case DataType::kInteger16:
    case DataType::kInteger32:
    case DataType::kInteger64:
      return std::holds_alternative<int64_t>(value);
    case DataType::kUnsigned8:
    case DataType::kUnsigned16:
    case DataType::kUnsigned32:
    case DataType::kUnsigned64:
      return std::holds_alternative<uint64_t>(value);
    case DataType::kReal32:
    case DataType::kReal64:
      return std::holds_alternative<double>(value);
    case DataType::kVisibleString:
    case DataType::kOctetString:
      return std::holds_alternative<std::string>(value);
    case DataType::kUnknown:
      return true;
  }

  return false;
}

std::optional<uint64_t> value_to_u64(const Value &value) {
  if (const auto *v = std::get_if<uint64_t>(&value)) {
    return *v;
  }
  if (const auto *v = std::get_if<int64_t>(&value)) {
    if (*v < 0) {
      return std::nullopt;
    }
    return static_cast<uint64_t>(*v);
  }
  if (const auto *v = std::get_if<bool>(&value)) {
    return *v ? 1U : 0U;
  }
  return std::nullopt;
}

}  // namespace can_node_sim
