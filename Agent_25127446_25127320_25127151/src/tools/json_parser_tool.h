#pragma once
#include "tool.h"
#include <map>
#include <string>

class JsonParserTool : public Tool {
public:
    JsonParserTool();
    ~JsonParserTool() override = default;

    std::string execute(const std::map<std::string, std::string>& args) override;
};
