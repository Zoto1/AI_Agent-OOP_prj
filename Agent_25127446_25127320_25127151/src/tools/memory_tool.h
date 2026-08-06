#pragma once
#include "tool.h"
#include <map>
#include <string>
#include <optional>
#include <iostream>
#include <unordered_map>
class Memory: public Tool{
private:
    std::unordered_map<std::string, std::string> memory_ data;
    bool save_context (const std:: string &key, const std:: string &value );
    // const_de doc thoi, khong sua len nen co dung tham chieu cung duoc
    std :: optional<std::string> load_context (const std::string &query) const;
public:
    Memory();
    ~Memory() override = default;
    void clear_memory();
    std :: string execute (const std::map<std::string, std :: string> &args) override;
    static void init(); // kiểu để đỡ phải khai báo phức tạp- như một phương thức rút gọn khi gọi hàm ở trong file thực thi
};

