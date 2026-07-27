#pragma once
#include "environnment.h"
#include <string>

// Config: cấu hình cho SandboxEnvironment (theo Classdiagram_benchmark.md: cfg: Config)
struct SandboxConfig {
    std::string image = "alpine:latest";       // Docker image dùng để chạy sandbox
    std::string containerNamePrefix = "oopagent_sandbox"; // Tiền tố tên container
    int timeoutSec = 30;                       // Timeout (giây) cho mỗi lệnh thực thi
};

// SandboxEnvironment: Thực thi lệnh cô lập bên trong một Docker container,
// tránh ảnh hưởng tới máy host khi Agent chạy code/lệnh không tin cậy.
// Yêu cầu: máy host đã cài Docker và user hiện tại có quyền chạy lệnh `docker`.
class SandboxEnvironment : public Environment {
private:
    std::string containerId;  // ID container đang chạy (rỗng nếu chưa setup)
    SandboxConfig cfg;

    // Helper dùng chung: chạy 1 lệnh shell (thường là lệnh docker) và trả về stdout+stderr.
    std::string runShell(const std::string& command) const;

public:
    explicit SandboxEnvironment(const SandboxConfig& cfg = SandboxConfig());
    ~SandboxEnvironment() override;

    void setup() override;      // docker run -d ... để khởi động container nền
    void teardown() override;   // docker rm -f để dừng + xóa container
    std::string executeCommand(const std::string& command) override; // docker exec ...
    bool isHealthy() const override; // docker inspect kiểm tra container còn Running không

    const std::string& getContainerId() const { return containerId; }
};