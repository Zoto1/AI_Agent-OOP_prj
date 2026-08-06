#include "web_tool.h"

WebSearchTool::WebSearchTool()
    : Tool(
          "web_search",
          "Search for information using a text query. Args: query.",
          {
              {"type", "OBJECT"},
              {"properties", {
                  {"query", {
                      {"type", "STRING"},
                      {"description", "Search query"}
                  }}
              }},
              {"required", {"query"}}
          }) {}

std::string WebSearchTool::performSearchRequest(const std::string& query) {
    std::ostringstream response;
    response << " query :" << query << "\n"
             << " results: "
             << " Ket qua 1 cho " << query << " mo ta chi tiet ket qua 1\n" 
             << " Ket qua 2 cho " << query << " mo ta chi tiet ket qua 2\n";
    return response.str();
}

std::string WebSearchTool::execute(const std::map<std::string, std::string>& args) {
    auto it = args.find("query");
    if (it == args.end()) {
        return "Error: Missing 'query' parameter!";
    }
    return performSearchRequest(it->second);
}
