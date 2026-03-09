#include "can_node_sim/RobotKeywords.hpp"

#include <sstream>
#include <stdexcept>

namespace can_node_sim {

RobotKeywords::RobotKeywords(CanNodeSimulator &simulator,
                             const std::vector<PdoDefinition> &pdos,
                             SocketCanTransport *transport,
                             LogLevel log_level)
    : simulator_(&simulator), transport_(transport), pdos_(pdos), log_level_(log_level) {}

std::string RobotKeywords::ReadOd(uint16_t index, uint8_t sub_index) const {
  Log_(LogLevel::kDebug,
       "Read OD keyword for " + ObjectKey{index, sub_index}.ToCanonicalString());
  return FormatValue_(simulator_->ReadOd(index, sub_index));
}

void RobotKeywords::WriteOd(uint16_t index,
                            uint8_t sub_index,
                            const std::string &value_text) const {
  Log_(LogLevel::kDebug,
    "Write OD keyword for " + ObjectKey{index, sub_index}.ToCanonicalString());
  const auto entry = simulator_->ReadOdEntry(index, sub_index);
  simulator_->WriteOd(index, sub_index, ParseValue_(value_text, entry.data_type));
  Log_(LogLevel::kInfo,
    "Write OD completed for " + ObjectKey{index, sub_index}.ToCanonicalString());
}

std::string RobotKeywords::ReadPdoSignal(PdoDirection direction,
                                         uint16_t pdo_number,
                                         std::size_t ordinal) const {
  Log_(LogLevel::kDebug,
       "Read PDO signal keyword for PDO " + std::to_string(pdo_number) +
           " ordinal " + std::to_string(ordinal));
  return FormatValue_(simulator_->ReadPdoSignal(direction, pdo_number, ordinal));
}

void RobotKeywords::WritePdoSignal(PdoDirection direction,
                                   uint16_t pdo_number,
                                   std::size_t ordinal,
                                   const std::string &value_text) const {
  Log_(LogLevel::kDebug,
       "Write PDO signal keyword for PDO " + std::to_string(pdo_number) +
           " ordinal " + std::to_string(ordinal));
  const auto &pdo = RequirePdo_(direction, pdo_number);
  if (ordinal >= pdo.signals.size()) {
    throw std::runtime_error("Signal ordinal out of range");
  }

  const auto &signal = pdo.signals[ordinal];
  const auto entry   = simulator_->ReadOdEntry(signal.key.index, signal.key.sub_index);
  simulator_->WritePdoSignal(
      direction, pdo_number, ordinal, ParseValue_(value_text, entry.data_type));
  Log_(LogLevel::kInfo,
       "Write PDO signal completed for " + signal.key.ToCanonicalString() + " (PDO " +
           std::to_string(pdo_number) + ")");
}

std::vector<std::string> RobotKeywords::ListPdoSignals(PdoDirection direction,
                                                       uint16_t pdo_number) const {
  Log_(LogLevel::kDebug, "List PDO signals keyword for PDO " + std::to_string(pdo_number));
  const auto &pdo = RequirePdo_(direction, pdo_number);
  std::vector<std::string> names;
  names.reserve(pdo.signals.size());
  for (const auto &signal : pdo.signals) {
    names.push_back(signal.name + " (" + signal.key.ToCanonicalString() + ")");
  }
  return names;
}

void RobotKeywords::StartCan(const std::string &interface_name) const {
  if (transport_ == nullptr) {
    throw std::runtime_error("CAN transport is not configured");
  }
  Log_(LogLevel::kInfo, "Start CAN keyword on interface " + interface_name);
  transport_->Start(interface_name);
}

void RobotKeywords::StopCan() const {
  if (transport_ == nullptr) {
    throw std::runtime_error("CAN transport is not configured");
  }
  Log_(LogLevel::kInfo, "Stop CAN keyword invoked");
  transport_->Stop();
}

void RobotKeywords::SendTpdo(uint16_t pdo_number) const {
  if (transport_ == nullptr) {
    throw std::runtime_error("CAN transport is not configured");
  }
  Log_(LogLevel::kInfo, "Send TPDO keyword for PDO " + std::to_string(pdo_number));
  transport_->SendTpdo(pdo_number);
}

void RobotKeywords::SendNmtCommand(uint8_t command, uint8_t target_node_id) const {
  if (transport_ == nullptr) {
    throw std::runtime_error("CAN transport is not configured");
  }
  Log_(LogLevel::kInfo,
       "Send NMT command keyword (command=" + std::to_string(command) +
           ", target=" + std::to_string(target_node_id) + ")");
  transport_->SendNmtCommand(command, target_node_id);
}

void RobotKeywords::SendHeartbeat(uint8_t nmt_state) const {
  if (transport_ == nullptr) {
    throw std::runtime_error("CAN transport is not configured");
  }
  Log_(LogLevel::kInfo,
       "Send heartbeat keyword (state=" + std::to_string(nmt_state) + ")");
  transport_->SendHeartbeat(nmt_state);
}

const PdoDefinition &RobotKeywords::RequirePdo_(PdoDirection direction, uint16_t pdo_number) const {
  for (const auto &pdo : pdos_) {
    if (pdo.direction == direction && pdo.pdo_number == pdo_number) {
      return pdo;
    }
  }
  throw std::runtime_error("PDO not found");
}

Value RobotKeywords::ParseValue_(const std::string &value_text, DataType data_type) {
  try {
    switch (data_type) {
      case DataType::kBoolean:
        if (value_text == "1" || value_text == "true" || value_text == "TRUE") {
          return true;
        }
        if (value_text == "0" || value_text == "false" || value_text == "FALSE") {
          return false;
        }
        throw std::runtime_error("Invalid boolean value");
      case DataType::kInteger8:
      case DataType::kInteger16:
      case DataType::kInteger32:
      case DataType::kInteger64:
        return static_cast<int64_t>(std::stoll(value_text, nullptr, 0));
      case DataType::kUnsigned8:
      case DataType::kUnsigned16:
      case DataType::kUnsigned32:
      case DataType::kUnsigned64:
        return static_cast<uint64_t>(std::stoull(value_text, nullptr, 0));
      case DataType::kReal32:
      case DataType::kReal64:
        return std::stod(value_text);
      case DataType::kVisibleString:
      case DataType::kOctetString:
      case DataType::kUnknown:
        return value_text;
    }
  } catch (const std::exception &ex) {
    throw std::runtime_error(std::string("Invalid value for target type: ") + ex.what());
  }

  throw std::runtime_error("Unsupported data type");
}

std::string RobotKeywords::FormatValue_(const Value &value) {
  if (const auto *v = std::get_if<std::monostate>(&value)) {
    (void)v;
    return "";
  }
  if (const auto *v = std::get_if<bool>(&value)) {
    return *v ? "true" : "false";
  }
  if (const auto *v = std::get_if<int64_t>(&value)) {
    return std::to_string(*v);
  }
  if (const auto *v = std::get_if<uint64_t>(&value)) {
    return std::to_string(*v);
  }
  if (const auto *v = std::get_if<double>(&value)) {
    std::ostringstream oss;
    oss << *v;
    return oss.str();
  }
  if (const auto *v = std::get_if<std::string>(&value)) {
    return *v;
  }
  return "";
}

void RobotKeywords::Log_(LogLevel level, const std::string &message) const {
  LogMessage("Keywords", level, log_level_, message);
}

}  // namespace can_node_sim
