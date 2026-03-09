#pragma once

#include "can_node_sim/Logging.hpp"
#include "can_node_sim/Model.hpp"

#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace can_node_sim {

/** Exception thrown for simulator API contract violations. */
class SimulatorError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

/** Thread-safe facade for OD/PDO read/write access. */
class CanNodeSimulator {
public:
  CanNodeSimulator(ObjectDictionary dictionary,
                   const std::vector<PdoDefinition> &pdos,
                   LogLevel log_level = LogLevel::kInfo);

  /** Reads an object-dictionary value by index/sub-index. */
  Value ReadOd(uint16_t index, uint8_t sub_index) const;
  /** Returns OD entry metadata/value by index/sub-index. */
  OdEntry ReadOdEntry(uint16_t index, uint8_t sub_index) const;
  /** Writes an object-dictionary value by index/sub-index. */
  void WriteOd(uint16_t index, uint8_t sub_index, const Value &value);

  /** Reads a mapped PDO signal by direction, PDO number, and signal ordinal. */
  Value ReadPdoSignal(PdoDirection direction, uint16_t pdo_number, std::size_t ordinal) const;
  /** Writes a mapped PDO signal by direction, PDO number, and signal ordinal. */
  void WritePdoSignal(PdoDirection direction,
                      uint16_t pdo_number,
                      std::size_t ordinal,
                      const Value &value);

private:
  using PdoKey = uint32_t;

  static PdoKey MakePdoKey_(PdoDirection direction, uint16_t pdo_number);

  OdEntry &RequireEntry_(uint16_t index, uint8_t sub_index);
  const OdEntry &RequireEntry_(uint16_t index, uint8_t sub_index) const;

  const PdoDefinition &RequirePdo_(PdoDirection direction, uint16_t pdo_number) const;
  void Log_(LogLevel level, const std::string &message) const;

  mutable std::mutex mutex_;
  ObjectDictionary dictionary_;
  std::unordered_map<PdoKey, PdoDefinition> pdo_by_key_;
  LogLevel log_level_;
};

}  // namespace can_node_sim
