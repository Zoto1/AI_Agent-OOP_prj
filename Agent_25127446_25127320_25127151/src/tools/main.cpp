#include <iostream>
#include <string>
#include "web_tool.h" // Nhớ include file header của WebSearchTool

int main() {
    // 1. Khởi tạo đối tượng tool
    WebSearchTool webTool;

    // 2. Kiểm tra các thông tin định danh của Tool
    std::cout << "=== KIỂM TRA THÔNG TIN TOOL ===" << std::endl;
    std::cout << "Ten Tool: " << webTool.getName() << std::endl;
    std::cout << "Mo ta: " << webTool.getDescription() << std::endl;
    std::cout << "----------------------------------" << std::endl;

    // 3. Test hàm execute với các đầu vào khác nhau
    std::string query1 = "Thoi tiet Hom Nay";
    std::string query2 = ""; // Test trường hợp chuỗi rỗng

    std::cout << "\n=== TEST 1: Tim kiem tu khoa '" << query1 << "' ===" << std::endl;
    std::string result1 = webTool.execute(query1);
    std::cout << "Ket qua tra ve:\n" << result1 << std::endl;

    std::cout << "\n=== TEST 2: Tim kiem voi chuoi rong ===" << std::endl;
    std::string result2 = webTool.execute(query2);
    std::cout << "Ket qua tra ve: '" << result2 << "'" << std::endl;

    return 0;
}