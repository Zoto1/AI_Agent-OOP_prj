#pragma once
#include "tool.h"
#include <map>
#include <string>

class HttpGetTool : public Tool {
private:
    static std::string performRequest(const std::string& url);

public:
    HttpGetTool();
    ~HttpGetTool() override = default;

    std::string execute(const std::map<std::string, std::string>& args) override;
};
