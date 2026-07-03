#include "tool.h"
#include <memory>
#include <iostream>

class ToolRegistry {
private:
    std::map<std::string, std::shared_ptr<Tool>> tools;

    ToolRegistry() = default;

public:
    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;

    static ToolRegistry& getInstance() {
        static ToolRegistry instance;
        return instance;
    }

    void registerTool(std::shared_ptr<Tool> tool) {
        if (tool) {
            tools[tool->getName()] = tool;
            std::cout << "-> Da dang ky thanh cong tool: [" << tool->getName() << "]\n";
        }
    }
s
    std::string executeTool(const std::string& name, const std::map<std::string, std::string>& args) {
        auto it = tools.find(name);
        if (it != tools.end()) {
            return it->second->execute(args);
        }
        return "Loi: Khong tim thay tool mang ten '" + name + "'";
    }
};