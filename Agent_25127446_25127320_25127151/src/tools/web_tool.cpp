#include "web_tool.h"

WebSearchTool::WebSearchTool() {
}

std::string WebSearchTool::getName() const {
    return "web_search";
}
std::string WebSearchTool::getDescription() const {
    return "Tim kiem thong tin bang tu khoa va tra ve ket qua tra cuu.";
}
std::string WebSearchTool::performSearchRequest(const std::string& query) {
    std::ostringstream  response;
    response << " query :" << query << "\n"
             << " results: "
             << " Ket qua 1 cho " << query << " mo ta chi tiet ket qua 1\n" 
             << " Ket qua 2 cho " << query << " mo ta chi tiet ket qua 2\n";
    return response.str();
}

std::string WebSearchTool::execute(const std::string& input) {
    if (input.empty()) {
        return "Loi: Truy van tim kiem rong.";
    }
    std::cout << "[WebSearchTool] Dang tim kiem voi tu khoa: " << input << std::endl;
    std::string searchResult = performSearchRequest(input);
    return searchResult;
}