#pragma once

#include "can_node_sim/Logging.hpp"
#include "can_node_sim/Model.hpp"
#include "can_node_sim/Simulator.hpp"
#include "can_node_sim/SocketCanTransport.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace can_node_sim {

/** Keyword adapter used by Robot Remote handlers. */
class RobotKeywords {
public:
  RobotKeywords(CanNodeSimulator &simulator,
                const std::vector<PdoDefinition> &pdos,
                SocketCanTransport *transport = nullptr,
                LogLevel log_level            = LogLevel::kInfo);

  /** Keyword: Read OD */
  std::string ReadOd(uint16_t index, uint8_t sub_index) const;
  /** Keyword: Write OD */
  void WriteOd(uint16_t index, uint8_t sub_index, const std::string &value_text) const;

  /** Keyword: Read PDO Signal */
  std::string ReadPdoSignal(PdoDirection direction, uint16_t pdo_number, std::size_t ordinal) const;
  /** Keyword: Write PDO Signal */
  void WritePdoSignal(PdoDirection direction,
                      uint16_t pdo_number,
                      std::size_t ordinal,
                      const std::string &value_text) const;

  /** Keyword: List PDO Signals */
  std::vector<std::string> ListPdoSignals(PdoDirection direction, uint16_t pdo_number) const;

  /** Keyword: Start CAN */
  void StartCan(const std::string &interface_name) const;
  /** Keyword: Stop CAN */
  void StopCan() const;
  /** Keyword: Send TPDO */
  void SendTpdo(uint16_t pdo_number) const;
  /** Keyword: Send NMT command */
  void SendNmtCommand(uint8_t command, uint8_t target_node_id) const;
  /** Keyword: Send heartbeat */
  void SendHeartbeat(uint8_t nmt_state) const;

private:
  const PdoDefinition &RequirePdo_(PdoDirection direction, uint16_t pdo_number) const;
  static Value ParseValue_(const std::string &value_text, DataType data_type);
  static std::string FormatValue_(const Value &value);
  void Log_(LogLevel level, const std::string &message) const;

  CanNodeSimulator *simulator_;
  SocketCanTransport *transport_;
  std::vector<PdoDefinition> pdos_;
  LogLevel log_level_;
};

}  // namespace can_node_sim
