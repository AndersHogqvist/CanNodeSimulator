#pragma once

#include <cstdint>
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <string>
#include <vector>

namespace can_node_sim {

/** Runtime log levels used by simulator components. */
enum class LogLevel : std::uint8_t { kDebug, kInfo, kWarn, kError };

/** Returns true when a message should be emitted for the configured threshold. */
inline bool ShouldLog(LogLevel message_level, LogLevel configured_level) {
  return message_level >= configured_level;
}

/** Converts project log level into spdlog log level. */
inline spdlog::level::level_enum ToSpdlogLevel(LogLevel level) {
  if (level == LogLevel::kDebug) {
    return spdlog::level::debug;
  }
  if (level == LogLevel::kInfo) {
    return spdlog::level::info;
  }
  if (level == LogLevel::kWarn) {
    return spdlog::level::warn;
  }
  return spdlog::level::err;
}

/** Initializes global spdlog behavior for the process. */
inline void InitializeLogging(LogLevel configured_level, const std::string &log_file_path = "") {
  std::vector<spdlog::sink_ptr> sinks;
  sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

  if (!log_file_path.empty()) {
    sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path, true));
  }

  auto logger = std::make_shared<spdlog::logger>("CanNodeSim", sinks.begin(), sinks.end());
  logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e][%n][%l] %v");
  logger->set_level(ToSpdlogLevel(configured_level));
  logger->flush_on(spdlog::level::warn);

  spdlog::set_default_logger(logger);
  spdlog::set_level(ToSpdlogLevel(configured_level));
}

/** Emits one structured log line through spdlog. */
inline void LogMessage(const char *component,
                       LogLevel message_level,
                       LogLevel configured_level,
                       const std::string &message) {
  if (!ShouldLog(message_level, configured_level)) {
    return;
  }
  spdlog::log(ToSpdlogLevel(message_level), "[{}] {}", component, message);
}

}  // namespace can_node_sim