#include "exec.h"
#include <array>
#include <cstdio>
#include <map>
#include <memory>
#include <string>

#define POPEN popen
#define PCLOSE pclose

ExecTool::ExecTool()
    : Tool(
          "exec",
          "Run a terminal command in the current task workspace. Args: command.",
          {
              {"type", "OBJECT"},
              {"properties", {
                  {"command", {
                      {"type", "STRING"},
                      {"description", "Shell command to execute"}
                  }}
              }},
              {"required", {"command"}}
          }) {}

std::string ExecTool::execute(const std::map<std::string, std::string>& args) {
    if (args.find("command") == args.end()) {
        return "Lỗi: Thiếu tham số 'command'.";
    }

    std::string command = args.at("command");
    if (command.empty()) {
        return "Lỗi: Tham số 'command' không được rỗng.";
    }
    std::array<char, 128> buffer{};
    std::string result;
    std::unique_ptr<FILE, decltype(&PCLOSE)> pipe(POPEN(command.c_str(), "r"), PCLOSE);
    if (!pipe) {
        return "Lỗi: Không thể thực thi lệnh hệ thống: " + command;
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result.empty() ? "Lệnh thực thi thành công (Không có phản hồi đầu ra)." : result;
}
