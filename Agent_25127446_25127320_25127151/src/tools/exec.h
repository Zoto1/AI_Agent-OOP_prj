#pragma once
#include "tool.h"
#include <memory>
#include <string>
#include <map>
#include <array>

class ExecTool : public Tool {
public:
    ExecTool();

    std::string execute(const std::map<std::string, std::string>& args) override;};  