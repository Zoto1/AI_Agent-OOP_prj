#include "json_parser_tool.h"

#include <nlohmann/json.hpp>

#include <sstream>
#include <string>

using json = nlohmann::json;

JsonParserTool::JsonParserTool()
    : Tool(
          "json_parse",
          "Parse a JSON string and extract a value using a dot-notation key "
          "path, e.g. user.name or users.0.email. Args: json, key.",
          {
              {"type", "OBJECT"},
              {"properties", {
                  {"json", {
                      {"type", "STRING"},
                      {"description", "JSON text to parse"}
                  }},
                  {"key", {
                      {"type", "STRING"},
                      {"description", "Dot-notation key path, e.g. user.name"}
                  }}
              }},
              {"required", {"json", "key"}}
          }) {}

std::string JsonParserTool::execute(const std::map<std::string, std::string> &args)
{
    auto json_it = args.find("json");
    auto key_it = args.find("key");
    if (json_it == args.end() || json_it->second.empty())
    {
        return "Error: Missing 'json' parameter!";
    }
    if (key_it == args.end() || key_it->second.empty())
    {
        return "Error: Missing 'key' parameter!";
    }

    json root;
    try
    {
        root = json::parse(json_it->second);
    }
    catch (const json::parse_error &error)
    {
        return "Error: Invalid JSON: " + std::string(error.what());
    }

    const json *current = &root;
    std::istringstream path_stream(key_it->second);
    std::string part;
    while (std::getline(path_stream, part, '.'))
    {
        if (part.empty())
        {
            return "Error: Invalid key path '" + key_it->second + "'.";
        }

        if (current->is_object())
        {
            auto it = current->find(part);
            if (it == current->end())
            {
                return "Error: Key '" + part + "' not found in JSON.";
            }
            current = &(*it);
        }
        else if (current->is_array())
        {
            std::size_t index = 0;
            try
            {
                const std::size_t parsed = std::stoull(part);
                index = parsed;
            }
            catch (...)
            {
                return "Error: Expected an array index, got '" + part + "'.";
            }
            if (index >= current->size())
            {
                return "Error: Array index " + part + " out of range.";
            }
            current = &((*current)[index]);
        }
        else
        {
            return "Error: Cannot descend into a non-container value at '" +
                   part + "'.";
        }
    }

    if (current->is_string())
    {
        return current->get<std::string>();
    }
    return current->dump();
}
