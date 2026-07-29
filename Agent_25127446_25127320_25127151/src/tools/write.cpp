#include "write.h"
#include <sstream>
#include <iostream>
#include <fstream>
    FileWriteTool::FileWriteTool(const std::string& n, const std::string& d, const std::string& base_dir)
        : Tool(n, d), base_directory(base_dir) {
        if (!base_directory.empty() && base_directory.back() != '/' && base_directory.back() != '\\') {
            base_directory += "/";
        }
    }

    std::string FileWriteTool::execute(const std::map<std::string, std::string>& args) {
        // 1. check path
        auto path_it = args.find("path");
        if (path_it == args.end() || path_it->second.empty()) {
            return "Error: không tồn tại đường đẫn";
        }
        std::string relative_path = path_it->second;

        // 2. check content
        auto content_it = args.find("content");
        if (content_it == args.end()) {
            return "Error: không tồn tại nội dung";
        }
        std::string content = content_it->second;

        // 3. Ngăn chặn backtracking
        if (relative_path.find("..") != std::string::npos) {
            return "Error: vượt mức truy cập";
        }

        // 4.tạo đường dẫn 
        std::string full_path = base_directory + relative_path;

        // 5. Tiến hành mở file để ghi 
        std::ofstream file(full_path, std::ios::out);
        if (!file.is_open()) {
            return "không thể mở " + full_path;
        }

        // 6. Ghi nội dung vào file và đóng luồng
        file << content;
        file.close();

        return "nội dung thành công được ghi lại tại: " + relative_path;
    }
