#include "tool.h"
#include <memory>
#include <iostream>

class ToolRegistry {
private:
    std::map<std::string, std::shared_ptr<Tool>> tools;

    // Khóa Constructor lại để tránh việc tạo nhiều instance (Singleton)
    ToolRegistry() = default;

public:
    // Hàm xóa Copy Constructor và phép gán để đảm bảo tính duy nhất
    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;

    // Hàm lấy Instance duy nhất của Registry
    static ToolRegistry& getInstance() {
        static ToolRegistry instance;
        return instance;
    }

    // Đăng ký tool động tại runtime
    void registerTool(std::shared_ptr<Tool> tool) {
        if (tool) {
            tools[tool->getName()] = tool;
            std::cout << "-> Da dang ky thanh cong tool: [" << tool->getName() << "]\n";
        }
    }

    // Thực thi tool từ xa thông qua Registry
    std::string executeTool(const std::string& name, const std::map<std::string, std::string>& args) {
        auto it = tools.find(name);
        if (it != tools.end()) {
            return it->second->execute(args);
        }
        return "Loi: Khong tim thay tool mang ten '" + name + "'";
    }
};