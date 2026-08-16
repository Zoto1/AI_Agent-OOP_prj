#pragma once
#include "tool.h"
#include <map>
#include <string>

class DateTimeTool : public Tool {
public:
    DateTimeTool();
    ~DateTimeTool() override = default;

    std::string execute(const std::map<std::string, std::string>& args) override;
};
