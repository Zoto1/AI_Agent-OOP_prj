#pragma once
#include "tool.h"
#include <stdexcept>

class CalculatorTool : public Tool {
public:
    CalculatorTool() ;
    std::string execute(const std::map<std::string, std::string>& args) ;
};