#pragma once
#include "environnment.h"
#include <string>

// NativeEnvironment: Thực thi lệnh trực tiếp trên máy host (không cô lập/sandbox).
// Phù hợp cho dev/test local, KHÔNG nên dùng để chạy code không tin cậy.
class NativeEnvironment : public Environment {
private:
    std::string workingDir;   // Thư mục làm việc riêng cho môi trường này
    bool isSetUp = false;     // Đánh dấu đã setup() thành công hay chưa

public:
    // workingDir: đường dẫn thư mục sẽ được tạo (nếu chưa có) làm nơi chạy lệnh.
    explicit NativeEnvironment(const std::string& workingDir = "./native_env_workspace");

    void setup() override;
    void teardown() override;
    std::string executeCommand(const std::string& command) override;
    

    const std::string& getWorkingDir() const { return workingDir; }
};