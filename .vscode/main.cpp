#include "res.h"
#include "cal.h"
#include "exc.h"
#include <iostream>

int main() {
    ToolRegistry& registry = ToolRegistry::getInstance();
  
    registry.registerTool(std::make_shared<CalculatorTool>());
    registry.registerTool(std::make_shared<ExecTool>());

    std::cout << "-------------------------------------------\n";
    std::map<std::string, std::string> calcArgs = {{"a", "12.5"}, {"b", "4"}, {"op", "*"}};
    std::string calcResult = registry.executeTool("calculator", calcArgs);
    std::cout << "AI goi Calculator:\n" << calcResult << "\n\n";

    std::map<std::string, std::string> execArgs = {{"command", "echo Hello_tu_C++_Terminal"}};
    std::string execResult = registry.executeTool("exec", execArgs);
    std::cout << "AI goi Exec:\n" << execResult << "\n";

    return 0;
}
