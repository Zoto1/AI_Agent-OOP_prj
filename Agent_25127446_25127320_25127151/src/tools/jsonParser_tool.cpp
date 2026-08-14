#include "jsonParser_tool.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <vector>

using json = nlohmann::json;

JsonParserTool::JsonParserTool()
    : Tool(
          "json_parse",
          "Parse a JSON string and extract a value using dot notation key (e.g., 'user.name'). Args: json, key.",
          {
              {"type", "OBJECT"},
              {"properties", {
                  {"json", {
                      {"type", "STRING"},
                      {"description", "Valid JSON string"}
                  }},
                  {"key", {
                      {"type", "STRING"},
                      {"description", "Dot notation key path, e.g., 'data.user.name'"}
                  }}
              }},
              {"required", {"json", "key"}}
          }) {}

std::string JsonParserTool::execute(const std::map<std::string, std::string>& args) {
    auto it_json = args.find("json");
    auto it_key = args.find("key");

    if (it_json == args.end() || it_key == args.end()) {
        return "Lỗi: Thiếu tham số 'json' hoặc 'key'.";
    }

    try {
        json parsed_json = json::parse(it_json->second);
        
        // Phân tách key dạng Dot Notation (vd: user.name -> ["user", "name"])
        std::stringstream ss(it_key->second);
        std::string token;
        json current = parsed_json;

        while (std::getline(ss, token, '.')) {
            if (token.empty()) continue;
            if (current.contains(token)) {
                current = current[token];
            } else {
                return "Lỗi: Key '" + it_key->second + "' không tồn tại trong JSON.";
            }
        }

        if (current.is_string()) {
            return current.get<std::string>();
        }
        return current.dump();

    } catch (const json::exception& e) {
        return std::string("Lỗi cú pháp JSON: ") + e.what();
    }
}