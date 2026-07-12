#include <iostream>
#include <memory>
#include <map>
#include <string>
#include "tool_registry.h"         // File chứa ToolRegistry của bạn
#include "read.cpp"   // Tạm thời giữ lại nếu bạn viết gộp toàn bộ class FileReadTool trong này

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "        CHUONG TRINH KIEM THU FILEREADTOOL        " << std::endl;
    std::cout << "==================================================" << std::endl;

    // 1. Lay instance duy nhat cua ToolRegistry (Singleton Pattern)
    auto& registry = ToolRegistry::getInstance();

    // 2. Khoi tao FileReadTool voi thu muc goc la thu muc hien tai "./"
   auto fileTool = std::make_shared<FileReadTool>("./");

    // 3. Dang ky tool vao he thong quan ly (Da sua: Bo cau lenh if gay loi void)
    registry.registerTool(fileTool);
    std::cout << "[SUCCESS] Da dang ky '" << fileTool->getName() << "' vao Registry.\n" << std::endl;

    // 4. Mo phong AI Agent hoac User goi Tool thong qua Registry
    std::cout << "--- Dang thuc thi tool thong qua Registry ---" << std::endl;
    std::map<std::string, std::string> arguments = {{"path", "test.txt"}};
    
    // Registry se tu dong tim "FileReadTool" va goi ham execute() cua no
    // (Luu y: Kiem tra xem trong file res.h ham nay ten la executeTool hay execute_tool nhe)
    std::string result = registry.executeTool("FileReadTool", arguments);

    // 5. In ket qua doc file ra man hinh Terminal
    std::cout << "\n--- KET QUA TRA VE TU TOOL ---" << std::endl;
    std::cout << result << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}
