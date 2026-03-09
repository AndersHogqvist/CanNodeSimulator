#include "can_node_sim/Simulator.hpp"

#include <string>

namespace can_node_sim {

CanNodeSimulator::CanNodeSimulator(ObjectDictionary dictionary,
                                   const std::vector<PdoDefinition> &pdos,
                                   LogLevel log_level)
    : dictionary_(std::move(dictionary)), log_level_(log_level) {
  for (const auto &pdo : pdos) {
    pdo_by_key_[MakePdoKey_(pdo.direction, pdo.pdo_number)] = pdo;
  }
  Log_(LogLevel::kInfo,
       "Initialized with " + std::to_string(dictionary_.size()) + " OD entries and " +
           std::to_string(pdo_by_key_.size()) + " PDO definitions");
}

Value CanNodeSimulator::ReadOd(uint16_t index, uint8_t sub_index) const {
  std::lock_guard<std::mutex> lock(mutex_);
  Log_(LogLevel::kDebug,
       "Read OD request for " + ObjectKey{index, sub_index}.ToCanonicalString());
  const auto &entry = RequireEntry_(index, sub_index);
  if (entry.access_type == AccessType::kWriteOnly) {
    throw SimulatorError("OD entry is write-only: " + entry.key.ToCanonicalString());
  }
  return entry.value;
}

OdEntry CanNodeSimulator::ReadOdEntry(uint16_t index, uint8_t sub_index) const {
  std::lock_guard<std::mutex> lock(mutex_);
  Log_(LogLevel::kDebug,
       "Read OD entry metadata for " + ObjectKey{index, sub_index}.ToCanonicalString());
  return RequireEntry_(index, sub_index);
}

void CanNodeSimulator::WriteOd(uint16_t index, uint8_t sub_index, const Value &value) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto key = ObjectKey{index, sub_index};
  Log_(LogLevel::kDebug, "Write OD request for " + key.ToCanonicalString());
  auto &entry = RequireEntry_(index, sub_index);

  if (entry.access_type == AccessType::kReadOnly || entry.access_type == AccessType::kConstant) {
    throw SimulatorError("OD entry is not writable: " + entry.key.ToCanonicalString());
  }

  if (!value_is_compatible(entry.data_type, value)) {
    throw SimulatorError("Type mismatch for OD entry: " + entry.key.ToCanonicalString());
  }

  entry.value = value;
  Log_(LogLevel::kInfo, "Wrote OD value for " + key.ToCanonicalString());
}

Value CanNodeSimulator::ReadPdoSignal(PdoDirection direction,
                                      uint16_t pdo_number,
                                      std::size_t ordinal) const {
  std::lock_guard<std::mutex> lock(mutex_);
  Log_(LogLevel::kDebug,
       "Read PDO signal request for PDO " + std::to_string(pdo_number) +
           " ordinal " + std::to_string(ordinal));

  const auto &pdo = RequirePdo_(direction, pdo_number);
  if (ordinal >= pdo.signals.size()) {
    throw SimulatorError("Signal ordinal out of range");
  }

  const auto &signal = pdo.signals[ordinal];
  return RequireEntry_(signal.key.index, signal.key.sub_index).value;
}

void CanNodeSimulator::WritePdoSignal(PdoDirection direction,
                                      uint16_t pdo_number,
                                      std::size_t ordinal,
                                      const Value &value) {
  std::lock_guard<std::mutex> lock(mutex_);
  Log_(LogLevel::kDebug,
       "Write PDO signal request for PDO " + std::to_string(pdo_number) +
           " ordinal " + std::to_string(ordinal));

  const auto &pdo = RequirePdo_(direction, pdo_number);
  if (ordinal >= pdo.signals.size()) {
    throw SimulatorError("Signal ordinal out of range");
  }

  const auto &signal = pdo.signals[ordinal];
  auto &entry        = RequireEntry_(signal.key.index, signal.key.sub_index);

  if (entry.access_type == AccessType::kReadOnly || entry.access_type == AccessType::kConstant) {
    throw SimulatorError("Mapped OD entry is not writable: " + entry.key.ToCanonicalString());
  }

  if (!value_is_compatible(entry.data_type, value)) {
    throw SimulatorError("Type mismatch for mapped signal");
  }

  entry.value = value;
  Log_(LogLevel::kInfo,
       "Updated mapped PDO signal for " + signal.key.ToCanonicalString() + " (PDO " +
           std::to_string(pdo_number) + ")");
}

CanNodeSimulator::PdoKey CanNodeSimulator::MakePdoKey_(PdoDirection direction,
                                                       uint16_t pdo_number) {
  const uint32_t dir_part = direction == PdoDirection::kReceive ? 0U : 1U;
  return (dir_part << 16U) | pdo_number;
}

OdEntry &CanNodeSimulator::RequireEntry_(uint16_t index, uint8_t sub_index) {
  ObjectKey key{index, sub_index};
  const auto it = dictionary_.find(key);
  if (it == dictionary_.end()) {
    throw SimulatorError("OD entry not found: " + key.ToCanonicalString());
  }
  return it->second;
}

const OdEntry &CanNodeSimulator::RequireEntry_(uint16_t index, uint8_t sub_index) const {
  ObjectKey key{index, sub_index};
  const auto it = dictionary_.find(key);
  if (it == dictionary_.end()) {
    throw SimulatorError("OD entry not found: " + key.ToCanonicalString());
  }
  return it->second;
}

const PdoDefinition &CanNodeSimulator::RequirePdo_(PdoDirection direction,
                                                   uint16_t pdo_number) const {
  const auto it = pdo_by_key_.find(MakePdoKey_(direction, pdo_number));
  if (it == pdo_by_key_.end()) {
    throw SimulatorError("PDO not found");
  }
  return it->second;
}

void CanNodeSimulator::Log_(LogLevel level, const std::string &message) const {
  LogMessage("Simulator", level, log_level_, message);
}

}  // namespace can_node_sim
