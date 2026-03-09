#pragma once

#include "can_node_sim/Logging.hpp"
#include "can_node_sim/Model.hpp"
#include "can_node_sim/Simulator.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace can_node_sim {

/** Linux SocketCAN transport that connects RPDO/TPDOs to simulator state. */
class SocketCanTransport {
public:
  SocketCanTransport(CanNodeSimulator &simulator,
                     const std::vector<PdoDefinition> &pdos,
                     uint8_t node_id,
                     LogLevel log_level = LogLevel::kInfo);
  ~SocketCanTransport();

  SocketCanTransport(const SocketCanTransport &)            = delete;
  SocketCanTransport &operator=(const SocketCanTransport &) = delete;
  SocketCanTransport(SocketCanTransport &&)                 = delete;
  SocketCanTransport &operator=(SocketCanTransport &&)      = delete;

  /** Opens and binds a CAN interface and starts RX processing. */
  void Start(const std::string &interface_name);
  /** Stops RX processing and closes the CAN socket. */
  void Stop();

  /** Returns true when the transport is actively receiving CAN frames. */
  bool IsRunning() const;

  /** Encodes and sends one TPDO frame from current simulator values. */
  void SendTpdo(uint16_t pdo_number);
  /** Sends an NMT command frame on CAN-ID 0x000. */
  void SendNmtCommand(uint8_t command, uint8_t target_node_id);
  /** Sends a heartbeat frame for this node. */
  void SendHeartbeat(uint8_t nmt_state);
  /** Sends an EMCY frame for this node. */
  void SendEmcy(uint16_t error_code,
                uint8_t error_register,
                const std::vector<uint8_t> &manufacturer_data);

  /** Encodes mapped values into a PDO payload (byte-aligned signals only). */
  static std::vector<uint8_t> EncodePdoPayload(const PdoDefinition &pdo,
                                               const CanNodeSimulator &simulator);
  /** Decodes a PDO payload into mapped simulator values (byte-aligned signals only). */
  static void DecodePdoPayload(const PdoDefinition &pdo,
                               const std::vector<uint8_t> &payload,
                               CanNodeSimulator &simulator);

private:
  void RxLoop_();
  void TxLoop_();
  void HandleFrame_(uint32_t can_id, const std::array<uint8_t, 8> &data, uint8_t dlc);
  void SendRawFrame_(uint32_t can_id, const std::vector<uint8_t> &payload) const;
  void SendSynchronousTpdos_();
  void Log_(LogLevel level, const std::string &message) const;

  static bool IsTransmissionTypeSynchronous_(uint8_t transmission_type);
  static bool IsTransmissionTypeAsynchronous_(uint8_t transmission_type);
  static uint8_t NmtCommandToState_(uint8_t command);

  CanNodeSimulator &simulator_;
  uint8_t node_id_;
  LogLevel log_level_;

  std::unordered_map<uint32_t, PdoDefinition> rpdo_by_cob_id_;
  std::unordered_map<uint16_t, PdoDefinition> tpdo_by_number_;

  std::atomic<bool> running_{false};
  std::atomic<uint8_t> nmt_state_{127};
  int socket_fd_{-1};
  mutable std::mutex send_mutex_;
  std::thread rx_thread_;
  std::thread tx_thread_;
  std::chrono::steady_clock::time_point next_heartbeat_due_;
  std::unordered_map<uint16_t, std::chrono::steady_clock::time_point> next_async_tpdo_due_;
};

}  // namespace can_node_sim
