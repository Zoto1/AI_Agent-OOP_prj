#include "calculator.h"
#include <cctype>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
class ExpressionParser
{
private:
    const std::string &input;
    std::size_t position = 0;

    void skipWhitespace()
    {
        while (position < input.size() &&
               std::isspace(static_cast<unsigned char>(input[position])))
        {
            ++position;
        }
    }

    bool consume(char expected)
    {
        skipWhitespace();
        if (position < input.size() && input[position] == expected)
        {
            ++position;
            return true;
        }
        return false;
    }

    double parseExpression()
    {
        double value = parseTerm();
        while (true)
        {
            if (consume('+'))
            {
                value += parseTerm();
            }
            else if (consume('-'))
            {
                value -= parseTerm();
            }
            else
            {
                return value;
            }
        }
    }

    double parseTerm()
    {
        double value = parseFactor();
        while (true)
        {
            if (consume('*'))
            {
                value *= parseFactor();
            }
            else if (consume('/'))
            {
                const double divisor = parseFactor();
                if (divisor == 0.0)
                {
                    throw std::runtime_error("Khong the chia cho 0");
                }
                value /= divisor;
            }
            else
            {
                return value;
            }
        }
    }

    double parseFactor()
    {
        skipWhitespace();

        if (consume('+'))
        {
            return parseFactor();
        }
        if (consume('-'))
        {
            return -parseFactor();
        }
        if (consume('('))
        {
            const double value = parseExpression();
            if (!consume(')'))
            {
                throw std::runtime_error("Thieu dau ngoac dong");
            }
            return value;
        }

        skipWhitespace();
        const std::size_t start = position;
        bool seen_digit = false;
        bool seen_dot = false;

        while (position < input.size())
        {
            const char character = input[position];
            if (std::isdigit(static_cast<unsigned char>(character)))
            {
                seen_digit = true;
                ++position;
            }
            else if (character == '.' && !seen_dot)
            {
                seen_dot = true;
                ++position;
            }
            else
            {
                break;
            }
        }

        if (!seen_digit)
        {
            throw std::runtime_error("Can mot so tai vi tri " +
                                     std::to_string(start));
        }

        return std::stod(input.substr(start, position - start));
    }

public:
    explicit ExpressionParser(const std::string &expression) : input(expression) {}

    double parse()
    {
        const double value = parseExpression();
        skipWhitespace();
        if (position != input.size())
        {
            throw std::runtime_error("Ky tu khong hop le tai vi tri " +
                                     std::to_string(position));
        }
        if (!std::isfinite(value))
        {
            throw std::runtime_error("Ket qua khong huu han");
        }
        return value;
    }
};

std::string formatNumber(double value)
{
    std::ostringstream output;
    output << std::setprecision(15) << value;
    return output.str();
}
} // namespace

CalculatorTool::CalculatorTool()
    : Tool(
          "calculator",
          "Evaluate an arithmetic expression. Supports numbers, parentheses, +, -, *, and /. Args: expression.",
          {
              {"type", "OBJECT"},
              {"properties", {
                  {"expression", {
                      {"type", "STRING"},
                      {"description", "Arithmetic expression, for example: (12 + 8) * 3"}
                  }}
              }},
              {"required", {"expression"}}
          }) {}

std::string CalculatorTool::execute(const std::map<std::string, std::string>& args) {
    try {
        const auto expression = args.find("expression");
        if (expression != args.end()) {
            if (expression->second.empty()) {
                return "Lỗi: Tham số 'expression' không được rỗng.";
            }
            const double value = ExpressionParser(expression->second).parse();
            return "Kết quả: " + formatNumber(value);
        }

        // Backward compatibility for older callers using a, b, op.
        if (args.find("a") == args.end() || args.find("b") == args.end() || args.find("op") == args.end()) {
            return "Lỗi: Thiếu tham số 'expression'.";
        }

        double a = std::stod(args.at("a"));
        double b = std::stod(args.at("b"));
        std::string op = args.at("op");

        if (op == "+") return "Kết quả: " + formatNumber(a + b);
        if (op == "-") return "Kết quả: " + formatNumber(a - b);
        if (op == "*") return "Kết quả: " + formatNumber(a * b);
        if (op == "/") {
            if (b == 0) return "Lỗi: Không thể chia cho 0.";
            return "Kết quả: " + formatNumber(a / b);
        }
        return "Lỗi: Phép tính không hợp lệ.";
    } catch (const std::exception& e) {
        return std::string("Lỗi xử lý dữ liệu: ") + e.what();
    }
}
