#include "can_node_sim/EdsParser.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace can_node_sim {

namespace {

struct SectionData {
  std::string section;
  std::unordered_map<std::string, std::string> kv;
};

std::string trim(const std::string &input) {
  std::size_t start = 0;
  while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
    ++start;
  }

  std::size_t end = input.size();
  while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
    --end;
  }

  return input.substr(start, end - start);
}

std::string to_lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

std::string strip_inline_comment(const std::string &line) {
  bool in_quote = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (c == '"') {
      in_quote = !in_quote;
      continue;
    }

    if (!in_quote && (c == ';' || c == '#')) {
      return trim(line.substr(0, i));
    }
  }
  return trim(line);
}

bool parse_section_name(const std::string &section, ObjectKey &key_out) {
  static const std::regex kSectionRe(R"(^([A-Fa-f0-9]{4})(?:sub([A-Fa-f0-9]{1,2}))?$)");
  std::smatch match;
  if (!std::regex_match(section, match, kSectionRe)) {
    return false;
  }

  key_out.index = static_cast<uint16_t>(std::stoul(match[1].str(), nullptr, 16));
  if (match[2].matched) {
    key_out.sub_index = static_cast<uint8_t>(std::stoul(match[2].str(), nullptr, 16));
  }
  else {
    key_out.sub_index = 0;
  }
  return true;
}

uint64_t parse_integer_literal(const std::string &literal) {
  return std::stoull(trim(literal), nullptr, 0);
}

uint64_t evaluate_default_expression(const std::string &raw_text, uint8_t node_id) {
  auto text = trim(raw_text);

  if (!text.empty() && text.front() == '"' && text.back() == '"' && text.size() >= 2) {
    throw std::invalid_argument("quoted string is not an integer expression");
  }

  const auto lower = to_lower(text);
  if (lower == "$nodeid") {
    return node_id;
  }

  const auto plus_pos = lower.find('+');
  if (plus_pos != std::string::npos) {
    const auto lhs = trim(lower.substr(0, plus_pos));
    const auto rhs = trim(lower.substr(plus_pos + 1));

    const bool lhs_node_id = lhs == "$nodeid";
    const bool rhs_node_id = rhs == "$nodeid";

    if (lhs_node_id == rhs_node_id) {
      throw std::invalid_argument("invalid $NODEID expression");
    }

    if (lhs_node_id) {
      return static_cast<uint64_t>(node_id) + parse_integer_literal(rhs);
    }
    return parse_integer_literal(lhs) + static_cast<uint64_t>(node_id);
  }

  return parse_integer_literal(text);
}

Value parse_default_value(const std::string &raw_text, DataType type, uint8_t node_id) {
  auto text = trim(raw_text);

  if (type == DataType::kVisibleString || type == DataType::kOctetString) {
    if (!text.empty() && text.front() == '"' && text.back() == '"' && text.size() >= 2) {
      return text.substr(1, text.size() - 2);
    }
    return text;
  }

  if (type == DataType::kReal32 || type == DataType::kReal64) {
    return std::stod(text);
  }

  if (type == DataType::kBoolean) {
    return evaluate_default_expression(text, node_id) != 0U;
  }

  if (type == DataType::kInteger8 || type == DataType::kInteger16 || type == DataType::kInteger32 ||
      type == DataType::kInteger64) {
    return static_cast<int64_t>(evaluate_default_expression(text, node_id));
  }

  return evaluate_default_expression(text, node_id);
}

std::vector<SectionData> parse_ini_sections(const std::string &text) {
  std::vector<SectionData> sections;
  SectionData current;
  bool has_current = false;

  std::istringstream stream(text);
  std::string line;
  while (std::getline(stream, line)) {
    const auto stripped = strip_inline_comment(line);
    if (stripped.empty()) {
      continue;
    }

    if (stripped.front() == '[' && stripped.back() == ']') {
      if (has_current) {
        sections.push_back(current);
      }
      current         = SectionData{};
      current.section = strip_inline_comment(stripped.substr(1, stripped.size() - 2));
      has_current     = true;
      continue;
    }

    if (!has_current) {
      continue;
    }

    const auto eq_pos = stripped.find('=');
    if (eq_pos == std::string::npos) {
      continue;
    }

    auto key        = to_lower(trim(stripped.substr(0, eq_pos)));
    auto value      = trim(stripped.substr(eq_pos + 1));
    current.kv[key] = value;
  }

  if (has_current) {
    sections.push_back(current);
  }

  return sections;
}

}  // namespace

ParsedEds EdsParser::ParseFile(const std::string &file_path, const EdsParseOptions &options) {
  std::ifstream file(file_path);
  if (!file.good()) {
    throw std::runtime_error("Unable to open EDS file: " + file_path);
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  return ParseText(buffer.str(), options);
}

ParsedEds EdsParser::ParseText(const std::string &text, const EdsParseOptions &options) {
  ParsedEds parsed;

  const auto sections = parse_ini_sections(text);
  for (const auto &section_data : sections) {
    ObjectKey key;
    if (!parse_section_name(section_data.section, key)) {
      continue;
    }

    OdEntry entry;
    entry.key = key;

    if (const auto it = section_data.kv.find("parametername"); it != section_data.kv.end()) {
      entry.parameter_name = it->second;
      if (!entry.parameter_name.empty() && entry.parameter_name.front() == '"' &&
          entry.parameter_name.back() == '"' && entry.parameter_name.size() >= 2) {
        entry.parameter_name = entry.parameter_name.substr(1, entry.parameter_name.size() - 2);
      }
    }
    else {
      entry.parameter_name = canonical_key(key.index, key.sub_index);
    }

    if (const auto it = section_data.kv.find("datatype"); it != section_data.kv.end()) {
      try {
        entry.data_type =
            data_type_from_eds_code(static_cast<uint32_t>(parse_integer_literal(it->second)));
      } catch (...) {
        parsed.warnings.push_back("Invalid DataType in section [" + section_data.section + "]");
      }
    }

    if (const auto it = section_data.kv.find("accesstype"); it != section_data.kv.end()) {
      entry.access_type = access_type_from_eds_text(it->second);
    }

    if (const auto it = section_data.kv.find("defaultvalue"); it != section_data.kv.end()) {
      try {
        entry.value = parse_default_value(it->second, entry.data_type, options.node_id);
      } catch (const std::exception &) {
        parsed.warnings.push_back("Invalid DefaultValue in section [" + section_data.section + "]");
      }
    }

    parsed.object_dictionary[entry.key] = std::move(entry);
  }

  return parsed;
}

}  // namespace can_node_sim
