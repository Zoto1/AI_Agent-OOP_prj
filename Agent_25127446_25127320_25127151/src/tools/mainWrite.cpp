#include <iostream>
#include <memory>
#include <map>
#include <string>
#include "res.h"         // File chứa ToolRegistry của bạn
#include "readfile.cpp"   // Nhúng file chứa các class Tool của bạn
#include "writefile.cpp" 
int main() {
    system("chcp 65001 > nul");
    std::cout << "==================================================" << std::endl;
    std::cout << "        CHUONG TRINH KIEM THU FILEWRITETOOL        " << std::endl;
    std::cout << "==================================================" << std::endl;

    // 1. Lay instance duy nhat cua ToolRegistry (Singleton Pattern)
    auto& registry = ToolRegistry::getInstance();

    // 2. Khoi tao FileWriteTool với thư mục gốc hiện tại "./"
    // (Hãy chắc chắn rằng bạn đã định nghĩa class FileWriteTool trong file readfile.cpp)
    auto fileWriteTool = std::make_shared<FileWriteTool>("FileWriteTool", "Ghi file", "./");
    

    // 3. Dang ky tool vao he thong quan ly
    registry.registerTool(fileWriteTool);
    std::cout << "[SUCCESS] Da dang ky '" << fileWriteTool->getName() << "' vao Registry.\n" << std::endl;

    // 4. Cấu hình tham số để GHI FILE
    std::cout << "--- Dang thuc thi ghi file thong qua Registry ---" << std::endl;
    
    std::map<std::string, std::string> arguments;
    arguments["path"] = "output_test.txt"; // Tên file muốn tạo ra
    arguments["content"] = "Chuc mung! Du lieu nay duoc ghi tu FileWriteTool thong qua Registry successfully!"; // Nội dung cần ghi

    // 5. Registry tìm "FileWriteTool" và gọi hàm execute() của nó
    // (Hãy kiểm tra xem class của bạn tên là "FileWriteTool" hay đặt tên khác để truyền vào cho đúng nhé)
    std::string result = registry.executeTool("FileWriteTool", arguments);

    // 6. In kết quả phản hồi từ tool ra màn hình
    std::cout << "\n--- KET QUA TRA VE TU TOOL ---" << std::endl;
    std::cout << result << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}
