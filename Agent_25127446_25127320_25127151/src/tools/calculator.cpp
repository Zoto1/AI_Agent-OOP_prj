#include "calculator.h"
#include <map>
#include <string>

CalculatorTool::CalculatorTool()
    : Tool("calculator", "Thực hiện phép tính cơ bản giữa 2 số a và b.") {}

std::string CalculatorTool::execute(const std::map<std::string, std::string>& args) {
    try {
        if (args.find("a") == args.end() || args.find("b") == args.end() || args.find("op") == args.end()) {
            return "Lỗi: Thiếu tham số (a, b, op).";
        }

        double a = std::stod(args.at("a"));
        double b = std::stod(args.at("b"));
        std::string op = args.at("op");

        if (op == "+") return "Kết quả: " + std::to_string(a + b);
        if (op == "-") return "Kết quả: " + std::to_string(a - b);
        if (op == "*") return "Kết quả: " + std::to_string(a * b);
        if (op == "/") {
            if (b == 0) return "Lỗi: Không thể chia cho 0.";
            return "Kết quả: " + std::to_string(a / b);
        }
        return "Lỗi: Phép tính không hợp lệ.";
    } catch (const std::exception& e) {
        return std::string("Lỗi xử lý dữ liệu: ") + e.what();
    }
}

