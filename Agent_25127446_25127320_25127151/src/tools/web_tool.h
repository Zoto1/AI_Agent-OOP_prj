#pragma once
#include "tool.h"
#include <string>
#include <map>
#include <curl/curl.h>
#include <sstream>
class WebSearchTool : public Tool {
private:
    std::string performSearchRequest(const std::string& query);

public:
    WebSearchTool();
    ~WebSearchTool() override = default;

    std::string execute(const std::map<std::string, std::string>& args) override;
};