#include "can_node_sim/Config.hpp"
#include "can_node_sim/EdsParser.hpp"
#include "can_node_sim/Logging.hpp"
#include "can_node_sim/PdoBuilder.hpp"
#include "can_node_sim/RobotKeywords.hpp"
#include "can_node_sim/Simulator.hpp"
#include "can_node_sim/SocketCanTransport.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#if CANNODESIM_HAS_ROBOT_REMOTE
#include "robot_remote/RemoteServer.h"
#endif

namespace {

std::atomic<bool> g_stop_requested{false};

void on_signal(int signal_value) {
  if (signal_value == SIGINT) {
    g_stop_requested.store(true);
  }
}

can_node_sim::LogLevel parse_log_level(const std::string &text) {
  if (text == "debug") {
    return can_node_sim::LogLevel::kDebug;
  }
  if (text == "info") {
    return can_node_sim::LogLevel::kInfo;
  }
  if (text == "warn") {
    return can_node_sim::LogLevel::kWarn;
  }
  if (text == "error") {
    return can_node_sim::LogLevel::kError;
  }
  throw std::runtime_error("Invalid --log-level value. Use debug|info|warn|error");
}

#if CANNODESIM_HAS_ROBOT_REMOTE

std::optional<int64_t> xmlrpc_to_i64(const robot_remote::XmlRpcValue &value) {
  if (const auto *v = std::get_if<int>(&value.value)) {
    return static_cast<int64_t>(*v);
  }
  if (const auto *v = std::get_if<double>(&value.value)) {
    return static_cast<int64_t>(*v);
  }
  if (const auto *v = std::get_if<std::string>(&value.value)) {
    return static_cast<int64_t>(std::stoll(*v, nullptr, 0));
  }
  return std::nullopt;
}

std::optional<std::string> xmlrpc_to_string(const robot_remote::XmlRpcValue &value) {
  if (const auto *v = std::get_if<std::string>(&value.value)) {
    return *v;
  }
  if (const auto *v = std::get_if<int>(&value.value)) {
    return std::to_string(*v);
  }
  if (const auto *v = std::get_if<double>(&value.value)) {
    return std::to_string(*v);
  }
  if (const auto *v = std::get_if<bool>(&value.value)) {
    return *v ? "true" : "false";
  }
  return std::nullopt;
}

std::optional<can_node_sim::PdoDirection> parse_direction(const std::string &text) {
  if (text == "rx" || text == "rpdo" || text == "receive") {
    return can_node_sim::PdoDirection::kReceive;
  }
  if (text == "tx" || text == "tpdo" || text == "transmit") {
    return can_node_sim::PdoDirection::kTransmit;
  }
  return std::nullopt;
}

robot_remote::KeywordResult keyword_error(const std::string &message) {
  robot_remote::KeywordResult result;
  result.success = false;
  result.error   = message;
  return result;
}

std::vector<robot_remote::KeywordSpec> build_keyword_specs(can_node_sim::RobotKeywords &keywords) {
  using robot_remote::KeywordResult;
  using robot_remote::KeywordSpec;
  using robot_remote::XmlRpcArray;
  using robot_remote::XmlRpcValue;

  std::vector<KeywordSpec> specs;

  specs.push_back(KeywordSpec{
      "Read OD",
      "Read OD value by index and sub-index.",
      {"index", "subindex"},
      [&keywords](const std::vector<XmlRpcValue> &args,
                  const std::map<std::string, XmlRpcValue> &) {
        try {
          if (args.size() != 2) {
            return keyword_error("Read OD expects 2 arguments: index, subindex");
          }
          const auto index     = xmlrpc_to_i64(args[0]);
          const auto sub_index = xmlrpc_to_i64(args[1]);
          if (!index.has_value() || !sub_index.has_value()) {
            return keyword_error("Read OD arguments must be numeric");
          }

          KeywordResult result;
          result.return_value =
              keywords.ReadOd(static_cast<uint16_t>(*index), static_cast<uint8_t>(*sub_index));
          return result;
        } catch (const std::exception &ex) {
          return keyword_error(ex.what());
        }
      }});

  specs.push_back(KeywordSpec{
      "Write OD",
      "Write OD value by index and sub-index.",
      {"index", "subindex", "value"},
      [&keywords](const std::vector<XmlRpcValue> &args,
                  const std::map<std::string, XmlRpcValue> &) {
        try {
          if (args.size() != 3) {
            return keyword_error("Write OD expects 3 arguments: index, subindex, value");
          }
          const auto index     = xmlrpc_to_i64(args[0]);
          const auto sub_index = xmlrpc_to_i64(args[1]);
          const auto value     = xmlrpc_to_string(args[2]);
          if (!index.has_value() || !sub_index.has_value() || !value.has_value()) {
            return keyword_error("Write OD argument types are invalid");
          }

          keywords.WriteOd(static_cast<uint16_t>(*index), static_cast<uint8_t>(*sub_index), *value);
          return KeywordResult{};
        } catch (const std::exception &ex) {
          return keyword_error(ex.what());
        }
      }});

  specs.push_back(KeywordSpec{
      "Read PDO Signal",
      "Read mapped PDO signal by direction, PDO number and ordinal.",
      {"direction", "pdo_number", "ordinal"},
      [&keywords](const std::vector<XmlRpcValue> &args,
                  const std::map<std::string, XmlRpcValue> &) {
        try {
          if (args.size() != 3) {
            return keyword_error("Read PDO Signal expects 3 arguments");
          }

          const auto direction_text = xmlrpc_to_string(args[0]);
          const auto pdo_number     = xmlrpc_to_i64(args[1]);
          const auto ordinal        = xmlrpc_to_i64(args[2]);
          if (!direction_text.has_value() || !pdo_number.has_value() || !ordinal.has_value()) {
            return keyword_error("Read PDO Signal argument types are invalid");
          }

          const auto direction = parse_direction(*direction_text);
          if (!direction.has_value()) {
            return keyword_error("Direction must be receive/rpdo or transmit/tpdo");
          }

          KeywordResult result;
          result.return_value = keywords.ReadPdoSignal(
              *direction, static_cast<uint16_t>(*pdo_number), static_cast<std::size_t>(*ordinal));
          return result;
        } catch (const std::exception &ex) {
          return keyword_error(ex.what());
        }
      }});

  specs.push_back(
      KeywordSpec{"Write PDO Signal",
                  "Write mapped PDO signal by direction, PDO number and ordinal.",
                  {"direction", "pdo_number", "ordinal", "value"},
                  [&keywords](const std::vector<XmlRpcValue> &args,
                              const std::map<std::string, XmlRpcValue> &) {
                    try {
                      if (args.size() != 4) {
                        return keyword_error("Write PDO Signal expects 4 arguments");
                      }

                      const auto direction_text = xmlrpc_to_string(args[0]);
                      const auto pdo_number     = xmlrpc_to_i64(args[1]);
                      const auto ordinal        = xmlrpc_to_i64(args[2]);
                      const auto value          = xmlrpc_to_string(args[3]);
                      if (!direction_text.has_value() || !pdo_number.has_value() ||
                          !ordinal.has_value() || !value.has_value()) {
                        return keyword_error("Write PDO Signal argument types are invalid");
                      }

                      const auto direction = parse_direction(*direction_text);
                      if (!direction.has_value()) {
                        return keyword_error("Direction must be receive/rpdo or transmit/tpdo");
                      }

                      keywords.WritePdoSignal(*direction,
                                              static_cast<uint16_t>(*pdo_number),
                                              static_cast<std::size_t>(*ordinal),
                                              *value);
                      return KeywordResult{};
                    } catch (const std::exception &ex) {
                      return keyword_error(ex.what());
                    }
                  }});

  specs.push_back(
      KeywordSpec{"List PDO Signals",
                  "List mapped signals for a given PDO.",
                  {"direction", "pdo_number"},
                  [&keywords](const std::vector<XmlRpcValue> &args,
                              const std::map<std::string, XmlRpcValue> &) {
                    try {
                      if (args.size() != 2) {
                        return keyword_error("List PDO Signals expects 2 arguments");
                      }

                      const auto direction_text = xmlrpc_to_string(args[0]);
                      const auto pdo_number     = xmlrpc_to_i64(args[1]);
                      if (!direction_text.has_value() || !pdo_number.has_value()) {
                        return keyword_error("List PDO Signals argument types are invalid");
                      }

                      const auto direction = parse_direction(*direction_text);
                      if (!direction.has_value()) {
                        return keyword_error("Direction must be receive/rpdo or transmit/tpdo");
                      }

                      const auto items =
                          keywords.ListPdoSignals(*direction, static_cast<uint16_t>(*pdo_number));
                      XmlRpcArray array;
                      array.reserve(items.size());
                      for (const auto &item : items) {
                        array.emplace_back(item);
                      }

                      KeywordResult result;
                      result.return_value = XmlRpcValue(array);
                      return result;
                    } catch (const std::exception &ex) {
                      return keyword_error(ex.what());
                    }
                  }});

  specs.push_back(KeywordSpec{"Start CAN",
                              "Start SocketCAN transport on interface.",
                              {"interface"},
                              [&keywords](const std::vector<XmlRpcValue> &args,
                                          const std::map<std::string, XmlRpcValue> &) {
                                try {
                                  if (args.size() != 1) {
                                    return keyword_error("Start CAN expects 1 argument: interface");
                                  }
                                  const auto iface = xmlrpc_to_string(args[0]);
                                  if (!iface.has_value()) {
                                    return keyword_error("Start CAN interface must be string");
                                  }
                                  keywords.StartCan(*iface);
                                  return KeywordResult{};
                                } catch (const std::exception &ex) {
                                  return keyword_error(ex.what());
                                }
                              }});

  specs.push_back(KeywordSpec{
      "Stop CAN",
      "Stop SocketCAN transport.",
      {},
      [&keywords](const std::vector<XmlRpcValue> &, const std::map<std::string, XmlRpcValue> &) {
        try {
          keywords.StopCan();
          return KeywordResult{};
        } catch (const std::exception &ex) {
          return keyword_error(ex.what());
        }
      }});

  specs.push_back(KeywordSpec{"Send TPDO",
                              "Send one TPDO frame by PDO number.",
                              {"pdo_number"},
                              [&keywords](const std::vector<XmlRpcValue> &args,
                                          const std::map<std::string, XmlRpcValue> &) {
                                try {
                                  if (args.size() != 1) {
                                    return keyword_error(
                                        "Send TPDO expects 1 argument: pdo_number");
                                  }

                                  const auto pdo_number = xmlrpc_to_i64(args[0]);
                                  if (!pdo_number.has_value()) {
                                    return keyword_error("Send TPDO pdo_number must be numeric");
                                  }

                                  keywords.SendTpdo(static_cast<uint16_t>(*pdo_number));
                                  return KeywordResult{};
                                } catch (const std::exception &ex) {
                                  return keyword_error(ex.what());
                                }
                              }});

  specs.push_back(KeywordSpec{
      "Send NMT Command",
      "Send an NMT command (command byte, target node-id).",
      {"command", "target_node_id"},
      [&keywords](const std::vector<XmlRpcValue> &args,
                  const std::map<std::string, XmlRpcValue> &) {
        try {
          if (args.size() != 2) {
            return keyword_error("Send NMT Command expects 2 arguments: command, target_node_id");
          }

          const auto command        = xmlrpc_to_i64(args[0]);
          const auto target_node_id = xmlrpc_to_i64(args[1]);
          if (!command.has_value() || !target_node_id.has_value()) {
            return keyword_error("Send NMT Command arguments must be numeric");
          }

          keywords.SendNmtCommand(static_cast<uint8_t>(*command),
                                  static_cast<uint8_t>(*target_node_id));
          return KeywordResult{};
        } catch (const std::exception &ex) {
          return keyword_error(ex.what());
        }
      }});

  specs.push_back(
      KeywordSpec{"Send Heartbeat",
                  "Send heartbeat frame for simulated node (NMT state byte).",
                  {"nmt_state"},
                  [&keywords](const std::vector<XmlRpcValue> &args,
                              const std::map<std::string, XmlRpcValue> &) {
                    try {
                      if (args.size() != 1) {
                        return keyword_error("Send Heartbeat expects 1 argument: nmt_state");
                      }

                      const auto nmt_state = xmlrpc_to_i64(args[0]);
                      if (!nmt_state.has_value()) {
                        return keyword_error("Send Heartbeat nmt_state must be numeric");
                      }

                      keywords.SendHeartbeat(static_cast<uint8_t>(*nmt_state));
                      return KeywordResult{};
                    } catch (const std::exception &ex) {
                      return keyword_error(ex.what());
                    }
                  }});

  return specs;
}

#endif

}  // namespace

int main(int argc, char **argv) {
  try {
    if (argc < 2) {
      std::cerr << "Usage: " << argv[0]
                << " --eds <path> [--node-id <1-127>] [--can-iface <canX>] "
               "[--log-level <debug|info|warn|error>] [--log-file <path>] [--rf-port <port>] "
                   "[--config <path.yaml>]\n";
      return 1;
    }

    std::string eds_path;
    std::string can_iface;
    std::string config_path;
    std::string log_file_path;
    uint8_t node_id                  = 1;
    bool node_id_from_cli            = false;
    uint16_t rf_port                 = 8270;
    can_node_sim::LogLevel log_level = can_node_sim::LogLevel::kInfo;

    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--eds" && (i + 1) < argc) {
        eds_path = argv[++i];
      }
      else if (arg == "--node-id" && (i + 1) < argc) {
        node_id          = static_cast<uint8_t>(std::stoi(argv[++i]));
        node_id_from_cli = true;
      }
      else if (arg == "--can-iface" && (i + 1) < argc) {
        can_iface = argv[++i];
      }
      else if (arg == "--log-level" && (i + 1) < argc) {
        log_level = parse_log_level(argv[++i]);
      }
      else if (arg == "--log-file" && (i + 1) < argc) {
        log_file_path = argv[++i];
      }
      else if (arg == "--rf-port" && (i + 1) < argc) {
        rf_port = static_cast<uint16_t>(std::stoul(argv[++i]));
      }
      else if (arg == "--config" && (i + 1) < argc) {
        config_path = argv[++i];
      }
    }

    if (eds_path.empty()) {
      std::cerr << "Missing required --eds option\n";
      return 1;
    }

    can_node_sim::InitializeLogging(log_level, log_file_path);

    std::optional<can_node_sim::SimulatorConfig> config;
    if (!config_path.empty()) {
      config = can_node_sim::ConfigLoader::LoadFile(config_path);
      if (config->node_id.has_value() && !node_id_from_cli) {
        node_id = *config->node_id;
      }
    }

    auto parsed = can_node_sim::EdsParser::ParseFile(
        eds_path, can_node_sim::EdsParseOptions{.node_id = node_id});

    if (config.has_value()) {
      can_node_sim::ConfigLoader::ApplyToDictionary(*config, parsed.object_dictionary);
    }

    auto pdo_result = can_node_sim::PdoBuilder::Build(parsed.object_dictionary);

    can_node_sim::CanNodeSimulator simulator(parsed.object_dictionary, pdo_result.pdos, log_level);
    can_node_sim::SocketCanTransport transport(simulator, pdo_result.pdos, node_id, log_level);
    can_node_sim::RobotKeywords keywords(simulator, pdo_result.pdos, &transport, log_level);

    robot_remote::RemoteServer server(static_cast<int>(rf_port));
    server.set_keywords(build_keyword_specs(keywords));
    server.set_library_info(robot_remote::LibraryInfo{
        .name          = "CanNodeSimulator",
        .version       = "0.1.0",
        .documentation = "CAN node simulator Robot Remote library.",
        .scope         = "GLOBAL",
        .named_args    = true,
    });

    if (!server.start()) {
      throw std::runtime_error("Failed to start Robot remote server");
    }

    const auto previous_handler = std::signal(SIGINT, on_signal);
    if (previous_handler == SIG_ERR) {
      throw std::runtime_error("Failed to install SIGINT handler");
    }

    if (!can_iface.empty()) {
      transport.Start(can_iface);
    }

    std::cout << "Loaded EDS: " << eds_path << "\n";
    std::cout << "Warnings: " << parsed.warnings.size() + pdo_result.warnings.size() << "\n";
    std::cout << "Robot port: " << rf_port << "\n";
    if (!config_path.empty()) {
      std::cout << "Config: " << config_path << "\n";
    }
    if (!can_iface.empty()) {
      std::cout << "CAN transport: started on " << can_iface << "\n";
    }
    else {
      std::cout << "CAN transport: disabled\n";
    }
    std::cout << "Simulator initialized\n";
    std::cout << "Press Ctrl+C to stop\n";

    while (!g_stop_requested.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (transport.IsRunning()) {
      transport.Stop();
    }

    server.stop();
    return 0;
  } catch (const std::exception &ex) {
    std::cerr << "Initialization failed: " << ex.what() << "\n";
    return 2;
  }
}
