#pragma once
#include "tool.h"
#include <map>
#include <string>

class HttpGetTool : public Tool {
private:
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

public:
    HttpGetTool();
    ~HttpGetTool() override = default;

    std::string execute(const std::map<std::string, std::string>& args) override;
};