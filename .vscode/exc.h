#include "tool.h"
#include <cstdio>
#include <memory>
#include <array>

class ExecTool : public Tool {
public:
    ExecTool() : Tool("exec", "Chạy lệnh terminal hệ thống.") {}

    std::string execute(const std::map<std::string, std::string>& args) override {
        if (args.find("command") == args.end()) {
            return "Lỗi: Thiếu tham số 'command'.";
        }

        std::string command = args.at("command");
        std::array<char, 128> buffer;
        std::string result;
#define POPEN popen 
#define PCLOSE pclose
        std::unique_ptr<FILE, decltype(&PCLOSE)> pipe(POPEN(command.c_str(), "r"), PCLOSE);
        if (!pipe) {
            return "Lỗi: Không thể thực thi lệnh hệ thống.";
        }

        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }

        return result.empty() ? "Lệnh thực thi thành công (Không có phản hồi đầu ra)." : result;
    }
};
