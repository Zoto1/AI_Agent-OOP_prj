#include "web_search.h"
#include <iostream>
#include <sstream>
WebSearchTool::WebSearchTool() {
}

std::string WebSearchTool::getName() const {
    return "web_search";
}
std::string WebSearchTool::getDescription() const {
    return "Tim kiem thong tin tren internet theo tu khoa va tra ve ket qua tra cuu moi nhat.";
}
std::string WebSearchTool::performSearchRequest(const std::string& query) {
    std::ostringstream response;
    response << "{\n"
             << "  \"query\": \"" << query << "\",\n"
             << "  \"results\": [\n"
             << "    {\"title\": \"Ket qua 1 cho " << query << "\", \"snippet\": \"Mo ta chi tiet ket qua 1...\"},\n"
             << "    {\"title\": \"Ket qua 2 cho " << query << "\", \"snippet\": \"Mo ta chi tiet ket qua 2...\"}\n"
             << "  ]\n"
             << "}";
    return response.str();
}

std::string WebSearchTool::execute(const std::string& input) {
    if (input.empty()) {
        return "Loi: Truy van tim kiem khong duoc de rong.";
    }

    std::cout << "[WebSearchTool] Dang tim kiem voi tu khoa: " << input << std::endl;
    std::string searchResult = performSearchRequest(input);
    return searchResult;
}