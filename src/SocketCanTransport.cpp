#include "can_node_sim/SocketCanTransport.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iterator>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

namespace can_node_sim {

namespace {

constexpr uint32_t kCanSffMask         = 0x7FFU;
constexpr uint32_t kSyncCobId          = 0x80U;
constexpr uint32_t kEmcyBaseCobId      = 0x80U;
constexpr uint32_t kHeartbeatBaseCobId = 0x700U;
constexpr uint32_t kNmtCobId           = 0x000U;
constexpr std::size_t kCanMaxDataLen   = 8;
constexpr auto kHeartbeatPeriod        = std::chrono::seconds(1);
constexpr auto kTxLoopPeriod           = std::chrono::milliseconds(10);

uint64_t read_value_as_u64(const Value &value) {
  if (const auto maybe = value_to_u64(value); maybe.has_value()) {
    return *maybe;
  }
  throw SimulatorError("Unable to encode non-integer value into PDO payload");
}

}  // namespace

SocketCanTransport::SocketCanTransport(CanNodeSimulator &simulator,
                                       const std::vector<PdoDefinition> &pdos,
                                       uint8_t node_id,
                                       LogLevel log_level)
    : simulator_(simulator), node_id_(node_id), log_level_(log_level) {
  const auto now      = std::chrono::steady_clock::now();
  next_heartbeat_due_ = now + kHeartbeatPeriod;

  for (const auto &pdo : pdos) {
    if (pdo.direction == PdoDirection::kReceive) {
      rpdo_by_cob_id_[pdo.communication.cob_id & kCanSffMask] = pdo;
    }
    else {
      tpdo_by_number_[pdo.pdo_number] = pdo;
      if (IsTransmissionTypeAsynchronous_(pdo.communication.transmission_type) &&
          pdo.communication.event_timer > 0U) {
        next_async_tpdo_due_[pdo.pdo_number] =
            now + std::chrono::milliseconds(pdo.communication.event_timer);
      }
    }
  }
}

SocketCanTransport::~SocketCanTransport() { Stop(); }

void SocketCanTransport::Start(const std::string &interface_name) {
  if (running_.load()) {
    throw std::runtime_error("SocketCAN transport is already running");
  }

  socket_fd_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);
  if (socket_fd_ < 0) {
    throw std::runtime_error("socket(PF_CAN) failed: " + std::string(std::strerror(errno)));
  }

  const unsigned interface_index = if_nametoindex(interface_name.c_str());
  if (interface_index == 0U) {
    const auto err = std::string(std::strerror(errno));
    close(socket_fd_);
    socket_fd_ = -1;
    throw std::runtime_error("if_nametoindex failed: " + err);
  }

  sockaddr_can addr{};
  addr.can_family  = AF_CAN;
  addr.can_ifindex = static_cast<int>(interface_index);

  if (bind(socket_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    const auto err = std::string(std::strerror(errno));
    close(socket_fd_);
    socket_fd_ = -1;
    throw std::runtime_error("bind(AF_CAN) failed: " + err);
  }

  // Keep blocking syscalls interruptible so Stop() can complete promptly.
  timeval timeout{};
  timeout.tv_sec  = 0;
  timeout.tv_usec = 100000;
  if (setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    const auto err = std::string(std::strerror(errno));
    close(socket_fd_);
    socket_fd_ = -1;
    throw std::runtime_error("setsockopt(SO_RCVTIMEO) failed: " + err);
  }
  if (setsockopt(socket_fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
    const auto err = std::string(std::strerror(errno));
    close(socket_fd_);
    socket_fd_ = -1;
    throw std::runtime_error("setsockopt(SO_SNDTIMEO) failed: " + err);
  }

  running_.store(true);
  rx_thread_ = std::thread(&SocketCanTransport::RxLoop_, this);
  tx_thread_ = std::thread(&SocketCanTransport::TxLoop_, this);
  Log_(LogLevel::kInfo, "SocketCAN transport started on " + interface_name);
}

void SocketCanTransport::Stop() {
  const bool was_running = running_.exchange(false);
  if (!was_running) {
    return;
  }

  if (socket_fd_ >= 0) {
    shutdown(socket_fd_, SHUT_RDWR);
  }

  if (rx_thread_.joinable()) {
    rx_thread_.join();
  }
  if (tx_thread_.joinable()) {
    tx_thread_.join();
  }

  if (socket_fd_ >= 0) {
    close(socket_fd_);
    socket_fd_ = -1;
  }

  Log_(LogLevel::kInfo, "SocketCAN transport stopped");
}

bool SocketCanTransport::IsRunning() const { return running_.load(); }

void SocketCanTransport::SendTpdo(uint16_t pdo_number) {
  const auto it = tpdo_by_number_.find(pdo_number);
  if (it == tpdo_by_number_.end()) {
    throw std::runtime_error("TPDO not found: " + std::to_string(pdo_number));
  }

  const auto payload = EncodePdoPayload(it->second, simulator_);
  SendRawFrame_(it->second.communication.cob_id, payload);
}

void SocketCanTransport::SendNmtCommand(uint8_t command, uint8_t target_node_id) {
  std::vector<uint8_t> payload{command, target_node_id};
  SendRawFrame_(kNmtCobId, payload);

  if (target_node_id == 0U || target_node_id == node_id_) {
    nmt_state_.store(NmtCommandToState_(command));
  }
}

void SocketCanTransport::SendHeartbeat(uint8_t nmt_state) {
  nmt_state_.store(nmt_state);
  SendRawFrame_(kHeartbeatBaseCobId + node_id_, {nmt_state});
}

void SocketCanTransport::SendEmcy(uint16_t error_code,
                                  uint8_t error_register,
                                  const std::vector<uint8_t> &manufacturer_data) {
  if (manufacturer_data.size() > 5) {
    throw std::runtime_error("EMCY manufacturer data must contain at most 5 bytes");
  }

  std::vector<uint8_t> payload;
  payload.reserve(kCanMaxDataLen);
  payload.push_back(static_cast<uint8_t>(error_code & 0xFFU));
  payload.push_back(static_cast<uint8_t>((error_code >> 8U) & 0xFFU));
  payload.push_back(error_register);
  payload.insert(payload.end(), manufacturer_data.begin(), manufacturer_data.end());
  while (payload.size() < kCanMaxDataLen) {
    payload.push_back(0);
  }

  const uint32_t cob_id = kEmcyBaseCobId + node_id_;
  SendRawFrame_(cob_id, payload);
}

std::vector<uint8_t> SocketCanTransport::EncodePdoPayload(const PdoDefinition &pdo,
                                                          const CanNodeSimulator &simulator) {
  std::vector<uint8_t> payload;
  payload.reserve(kCanMaxDataLen);

  for (std::size_t ordinal = 0; ordinal < pdo.signals.size(); ++ordinal) {
    const auto &signal = pdo.signals[ordinal];
    if ((signal.bit_length % 8U) != 0U) {
      throw std::runtime_error("Only byte-aligned PDO signals are currently supported");
    }

    const auto signal_bytes = static_cast<std::size_t>(signal.bit_length / 8U);
    const auto raw          = read_value_as_u64(
        simulator.ReadPdoSignal(PdoDirection::kTransmit, pdo.pdo_number, ordinal));

    for (std::size_t i = 0; i < signal_bytes; ++i) {
      payload.push_back(static_cast<uint8_t>((raw >> (8U * i)) & 0xFFU));
    }
  }

  if (payload.size() > kCanMaxDataLen) {
    throw std::runtime_error("PDO payload larger than 8 bytes");
  }

  return payload;
}

void SocketCanTransport::DecodePdoPayload(const PdoDefinition &pdo,
                                          const std::vector<uint8_t> &payload,
                                          CanNodeSimulator &simulator) {
  std::size_t cursor = 0;
  for (std::size_t ordinal = 0; ordinal < pdo.signals.size(); ++ordinal) {
    const auto &signal = pdo.signals[ordinal];
    if ((signal.bit_length % 8U) != 0U) {
      throw std::runtime_error("Only byte-aligned PDO signals are currently supported");
    }

    const auto signal_bytes = static_cast<std::size_t>(signal.bit_length / 8U);
    if ((cursor + signal_bytes) > payload.size()) {
      throw std::runtime_error("PDO payload shorter than mapped signal layout");
    }

    uint64_t raw = 0;
    for (std::size_t i = 0; i < signal_bytes; ++i) {
      raw |= (static_cast<uint64_t>(payload[cursor + i]) << (8U * i));
    }
    simulator.WritePdoSignal(PdoDirection::kReceive, pdo.pdo_number, ordinal, raw);

    cursor += signal_bytes;
  }
}

void SocketCanTransport::RxLoop_() {
  while (running_.load()) {
    can_frame frame{};
    const auto bytes_read = read(socket_fd_, &frame, sizeof(frame));
    if (bytes_read < 0) {
      if (!running_.load()) {
        break;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        continue;
      }
      Log_(LogLevel::kWarn, "SocketCAN read error: " + std::string(std::strerror(errno)));
      continue;
    }

    if (bytes_read != static_cast<ssize_t>(sizeof(frame))) {
      continue;
    }

    std::array<uint8_t, 8> frame_data{};
    std::copy_n(std::begin(frame.data), 8, frame_data.begin());
    HandleFrame_(frame.can_id & kCanSffMask, frame_data, frame.can_dlc);
  }
}

void SocketCanTransport::TxLoop_() {
  while (running_.load()) {
    const auto now = std::chrono::steady_clock::now();

    if (now >= next_heartbeat_due_) {
      try {
        SendHeartbeat(nmt_state_.load());
      } catch (const std::exception &ex) {
        Log_(LogLevel::kWarn, std::string("Heartbeat send failed: ") + ex.what());
      }
      next_heartbeat_due_ = now + kHeartbeatPeriod;
    }

    for (const auto &[pdo_number, pdo] : tpdo_by_number_) {
      if (!IsTransmissionTypeAsynchronous_(pdo.communication.transmission_type) ||
          pdo.communication.event_timer == 0U) {
        continue;
      }

      const auto due_it = next_async_tpdo_due_.find(pdo_number);
      if (due_it == next_async_tpdo_due_.end() || now < due_it->second) {
        continue;
      }

      try {
        SendTpdo(pdo_number);
      } catch (const std::exception &ex) {
        Log_(LogLevel::kWarn,
             "Asynchronous TPDO send failed for PDO " + std::to_string(pdo_number) + ": " +
                 ex.what());
      }

      due_it->second = now + std::chrono::milliseconds(pdo.communication.event_timer);
    }

    std::this_thread::sleep_for(kTxLoopPeriod);
  }
}

void SocketCanTransport::HandleFrame_(uint32_t can_id,
                                      const std::array<uint8_t, 8> &data,
                                      uint8_t dlc) {
  if (can_id == kSyncCobId) {
    SendSynchronousTpdos_();
    return;
  }

  if ((can_id & 0x780U) == kHeartbeatBaseCobId) {
    const auto heartbeat_node = static_cast<uint8_t>(can_id - kHeartbeatBaseCobId);
    Log_(LogLevel::kDebug, "Heartbeat received from node " + std::to_string(heartbeat_node));
    return;
  }

  if ((can_id & 0x780U) == kEmcyBaseCobId) {
    const auto emcy_node = static_cast<uint8_t>(can_id - kEmcyBaseCobId);
    Log_(LogLevel::kWarn, "EMCY received from node " + std::to_string(emcy_node));
    return;
  }

  const auto rpdo_it = rpdo_by_cob_id_.find(can_id);
  if (rpdo_it == rpdo_by_cob_id_.end()) {
    return;
  }

  std::vector<uint8_t> payload;
  payload.reserve(dlc);
  std::copy_n(data.begin(), dlc, std::back_inserter(payload));

  try {
    DecodePdoPayload(rpdo_it->second, payload, simulator_);
  } catch (const std::exception &ex) {
    Log_(LogLevel::kError, std::string("RPDO decode failed: ") + ex.what());
  }
}

void SocketCanTransport::SendRawFrame_(uint32_t can_id, const std::vector<uint8_t> &payload) const {
  const std::lock_guard<std::mutex> lock(send_mutex_);

  if (socket_fd_ < 0) {
    throw std::runtime_error("SocketCAN transport is not started");
  }
  if (payload.size() > kCanMaxDataLen) {
    throw std::runtime_error("CAN payload cannot exceed 8 bytes");
  }

  can_frame frame{};
  frame.can_id  = can_id & kCanSffMask;
  frame.can_dlc = static_cast<__u8>(payload.size());
  std::copy(payload.begin(), payload.end(), std::begin(frame.data));

  const auto bytes_written = write(socket_fd_, &frame, sizeof(frame));
  if (bytes_written != static_cast<ssize_t>(sizeof(frame))) {
    throw std::runtime_error("Failed to write CAN frame");
  }
}

void SocketCanTransport::SendSynchronousTpdos_() {
  for (const auto &[pdo_number, pdo] : tpdo_by_number_) {
    if (!IsTransmissionTypeSynchronous_(pdo.communication.transmission_type)) {
      continue;
    }

    try {
      SendTpdo(pdo_number);
    } catch (const std::exception &ex) {
      Log_(LogLevel::kError,
           "Failed sending synchronous TPDO " + std::to_string(pdo_number) + ": " + ex.what());
    }
  }
}

void SocketCanTransport::Log_(LogLevel level, const std::string &message) const {
  LogMessage("SocketCAN", level, log_level_, message);
}

bool SocketCanTransport::IsTransmissionTypeSynchronous_(uint8_t transmission_type) {
  return transmission_type >= 1U && transmission_type <= 240U;
}

bool SocketCanTransport::IsTransmissionTypeAsynchronous_(uint8_t transmission_type) {
  return transmission_type == 254U || transmission_type == 255U;
}

uint8_t SocketCanTransport::NmtCommandToState_(uint8_t command) {
  if (command == 1U) {
    return 5U;
  }
  if (command == 2U) {
    return 4U;
  }
  if (command == 128U) {
    return 127U;
  }
  return 0U;
}

}  // namespace can_node_sim
