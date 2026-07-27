#include "Sandbox_Environment.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>

#define POPEN popen
#define PCLOSE pclose

namespace {
// Sinh hậu tố ngẫu nhiên để tên container không đụng nhau khi chạy nhiều task song song.
std::string generateSuffix() {
    static std::mt19937_64 rng(
        static_cast<unsigned long>(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::ostringstream oss;
    oss << std::hex << rng();
    return oss.str().substr(0, 8);
}
}  // namespace

SandboxEnvironment::SandboxEnvironment(const SandboxConfig& config) : cfg(config) {}

SandboxEnvironment::~SandboxEnvironment() {
    // Đảm bảo container luôn bị dọn dẹp kể cả khi teardown() chưa được gọi thủ công.
    if (!containerId.empty()) {
        try {
            teardown();
        } catch (...) {
            // Destructor không được phép ném exception ra ngoài.
        }
    }
}

std::string SandboxEnvironment::runShell(const std::string& command) const {
    std::array<char, 256> buffer{};
    std::string result;
    std::unique_ptr<FILE, decltype(&PCLOSE)> pipe(POPEN(command.c_str(), "r"), PCLOSE);
    if (!pipe) {
        throw std::runtime_error("Lỗi [SandboxEnvironment]: Không thể chạy lệnh docker.");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Xóa newline/carriage-return thừa ở cuối (docker thường in kèm '\n').
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

void SandboxEnvironment::setup() {
    std::string containerName = cfg.containerNamePrefix + "_" + generateSuffix();

    // --rm: tự xóa container khi bị dừng, tránh rác tồn đọng nếu teardown() không được gọi.
    // sleep infinity: giữ container sống để executeCommand() có thể `docker exec` nhiều lần.
    std::ostringstream cmd;
    cmd << "docker run -d --rm --name " << containerName
        << " " << cfg.image << " sleep infinity 2>&1";

    std::string output = runShell(cmd.str());

    // Docker in ra full container ID (64 ký tự hex) khi thành công,
    // ngược lại output sẽ là dòng thông báo lỗi (thường ngắn/có chữ).
    if (output.size() < 12) {
        throw std::runtime_error(
            "Lỗi [SandboxEnvironment]: Không thể khởi động container Docker. "
            "Kiểm tra Docker đã được cài/chạy chưa. Chi tiết: " + output);
    }

    containerId = output;
}

void SandboxEnvironment::teardown() {
    if (containerId.empty()) return;
    runShell("docker rm -f " + containerId + " 2>&1");
    containerId.clear();
}

std::string SandboxEnvironment::executeCommand(const std::string& command) {
    if (containerId.empty()) {
        throw std::runtime_error(
            "Lỗi [SandboxEnvironment]: Container chưa được setup(). Gọi setup() trước.");
    }

    // Bọc bằng `timeout` ở phía host để tránh agent treo vô hạn nếu lệnh trong sandbox bị đứng.
    std::ostringstream cmd;
    cmd << "timeout " << cfg.timeoutSec << "s docker exec " << containerId
        << " sh -c \"" << command << "\" 2>&1";

    return runShell(cmd.str());
}

bool SandboxEnvironment::isHealthy() const {
    if (containerId.empty()) return false;
    std::string output = runShell(
        "docker inspect -f '{{.State.Running}}' " + containerId + " 2>/dev/null");
    return output == "true";
}