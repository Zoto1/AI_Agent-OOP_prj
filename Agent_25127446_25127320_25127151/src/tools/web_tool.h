#pragma once
#include "tool.h"
#include <string>
#include <map>

class WebSearchTool : public Tool {
private:
    static std::string urlEncode(const std::string& value);
    static std::string fetchSearchResults(const std::string& query);

public:
    WebSearchTool();
    ~WebSearchTool() override = default;

    std::string execute(const std::map<std::string, std::string>& args) override;
};
