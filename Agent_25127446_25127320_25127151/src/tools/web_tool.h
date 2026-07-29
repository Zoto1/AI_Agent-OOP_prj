#pragma once
#include <iostream>
#include <sstream>
#include "tool.h"
#include <string>
#include <memory>
#include <sstream>
#include <map>

class WebSearchTool : public Tool {
public:
    WebSearchTool();
    std::string execute(const std::map<std::string, std::string>& args) override;
private:
    std::string performSearchRequest(const std::string& query);
};