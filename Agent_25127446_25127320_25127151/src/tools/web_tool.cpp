#include "web_tool.h"

WebSearchTool::WebSearchTool() : Tool("web_search", "Tim kiem thong tin bang tu khoa va tra ve ket qua tra cuu."){}

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
        it = args.find("keywords");
        return "Error: Missing 'query' parameter!";
    }
    if (it == args.end()) {
        it = args.find("q");       

    if (it == args.end()) {
        return "Error: Missing search parameter! Expected 'query' or 'keywords'.";
    }
    std::string query = it->second;
    return performSearchRequest(query);}}
