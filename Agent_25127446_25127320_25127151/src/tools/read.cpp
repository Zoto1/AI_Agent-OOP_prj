#include "read.h"
   FileReadTool::FileReadTool(const std::string& base_dir)
        : Tool("FileReadTool", "Cong cu doc noi dung tu mot file text"), base_directory(base_dir) {}
    
    std::string FileReadTool::execute(const std::map<std::string, std::string>& args)  {
        // 1. Kiểm tra path truyền vào
        auto it = args.find("path");
        if (it == args.end()) {
            return "Error: Thieu tham so 'path' de doc file.";
        }

        // 2. Tổng lại các file cần đọc
        std::string file_path = base_directory + it->second;

        // 3. Mở filed
        std::ifstream file(file_path);
        if (!file.is_open()) {
            return "Error: Khong the mo file: " + file_path;
        }

        // 4. Đọc toàn bộ nội dung file
        std::stringstream content;
        content << file.rdbuf();
        file.close();

        // 5. Trả về chuỗi nội dung file
        return content.str();
    }

