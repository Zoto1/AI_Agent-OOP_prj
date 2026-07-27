#pragma once
#include <string>

class Environment {
public:
    // Virtual destructor: Rất quan trọng trong C++ để tránh memory leak khi hủy lớp con
    virtual ~Environment() = default;

    // Các hàm thuần ảo (pure virtual) ép buộc các lớp con phải tự định nghĩa logic
    virtual void setup() = 0;
    virtual void teardown() = 0;
    
    // Hàm mô phỏng việc thực thi một lệnh/hành động bên trong môi trường đó
    virtual std::string executeCommand(const std::string& command) = 0;

    // Kiểm tra môi trường có đang sẵn sàng để nhận lệnh hay không
    // (đã setup() thành công và chưa bị teardown()/lỗi giữa chừng).
    virtual bool isHealthy() const = 0;
};