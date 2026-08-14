#pragma once
#include "tool.h"
#include <map>
#include <string>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>
class CalculatorTool : public Tool {
public:
    // Khởi tạo tên tool, description và Schema JSON
    CalculatorTool();
    
    ~CalculatorTool() override = default;

    // Hàm thực thi chính nhận arguments map từ Agent/LLM
    std::string execute(const std::map<std::string, std::string>& args) override;
};