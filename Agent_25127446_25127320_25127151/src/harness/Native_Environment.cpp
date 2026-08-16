#include "Native_Environment.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace fs = std::filesystem;

#define POPEN popen
#define PCLOSE pclose

NativeEnvironment::NativeEnvironment(const std::string& workingDir)
    : workingDir(workingDir) {}

void NativeEnvironment::setup()
{
    std::error_code ec;

    if (std::filesystem::exists(workingDir)) {
        std::filesystem::remove_all(workingDir, ec);

        if (ec) {
            throw std::runtime_error(
                "Khong the xoa workspace cu: " +
                workingDir + " - " + ec.message()
            );
        }
    }

    std::filesystem::create_directories(workingDir, ec);

    if (ec) {
        throw std::runtime_error(
            "Khong the tao workspace: " +
            workingDir + " - " + ec.message()
        );
    }
    isSetUp = true;
}
void NativeEnvironment::teardown() {
    // Cố ý KHÔNG xóa workingDir: đây là nơi lưu output (file agent tạo ra),
    // xóa ở đây có thể làm mất kết quả mà Evaluator cần đọc sau đó.
    isSetUp = false;
}

std::string NativeEnvironment::executeCommand(const std::string& command) {
    if (!isSetUp) {
        throw std::runtime_error(
            "Lỗi [NativeEnvironment]: Môi trường chưa setup(). "
            "Gọi setup() trước khi executeCommand().");
    }

    std::string full_command =
        "cd \"" + workingDir + "\" && ( " + command + " ) 2>&1";

    std::array<char, 256> buffer{};
    std::string result;
    std::unique_ptr<FILE, decltype(&PCLOSE)> pipe(POPEN(full_command.c_str(), "r"), PCLOSE);
    if (!pipe) {
        throw std::runtime_error(
            "Lỗi [NativeEnvironment]: Không thể khởi tạo tiến trình con để chạy lệnh.");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

bool NativeEnvironment::isHealthy() const {
    return isSetUp && fs::exists(workingDir) && fs::is_directory(workingDir);
}