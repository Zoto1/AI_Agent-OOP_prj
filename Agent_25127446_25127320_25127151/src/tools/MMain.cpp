#include "res.h"
#include "Cal.h"
#include "ex.h"
#include <iostream>

int main() {
    // 1. Lấy bộ quản lý duy nhất (Singleton)
    ToolRegistry& registry = ToolRegistry::getInstance();

    // 2. Đăng ký động các tool vào hệ thống tại runtime
    registry.registerTool(std::make_shared<CalculatorTool>());
    registry.registerTool(std::make_shared<ExecTool>());

    std::cout << "-------------------------------------------\n";

    // 3. Thử gọi Tool Calculator
    std::map<std::string, std::string> calcArgs = {{"a", "12.5"}, {"b", "4"}, {"op", "*"}};
    std::string calcResult = registry.executeTool("calculator", calcArgs);
    std::cout << "AI goi Calculator:\n" << calcResult << "\n\n";

    // 4. Thử gọi Tool Exec (Lệnh 'echo' hoạt động trên cả Win/Linux)
    std::map<std::string, std::string> execArgs = {{"command", "echo Hello_tu_C++_Terminal"}};
    std::string execResult = registry.executeTool("exec", execArgs);
    std::cout << "AI goi Exec:\n" << execResult << "\n";

    return 0;
}