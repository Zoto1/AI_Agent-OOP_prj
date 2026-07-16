#pragma once
#include "tool.h"
#include <map>
#include <string>
#include <optional>

class Memory: public Tool{
private:
    std :: string storage_path;

    bool save_context (const std:: string &key, const std:: string &value )
    // const_de doc thoi, khong sua len nen co dung tham chieu cung duoc
    std :: optional load_context (const std::string &query);
public:
    Memory(const std :: string &path = "memory_data.txt");
    ~Memory() override = default;
    std :: string excute (const std::map<std::string, std :: string> &args) override;
    static void init(); // kiểu để đỡ phải khai báo phức tạp- như một phương thức rút gọn khi gọi hàm ở trong file thực thi
};

