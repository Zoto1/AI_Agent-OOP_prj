#include "loop_detector.h"
#include <iostream>
#include <type_traits>

LoopDetector::LoopDetector(int warning_thresh, int critical_thresh)
    : _warning(warning_thresh), _critical(critical_thresh) {}


    
bool LoopDetector::isSameAction(const std::variant<ToolCall, FinalAnswer> &a,
                                const std::variant<ToolCall, FinalAnswer> &b)
{
    return std::visit([](const auto &arg_a, const auto &arg_b) -> bool
                      {
                          // Lấy kiểu dữ liệu thực tế đang chứa bên trong variant
                          using T1 = std::decay_t<decltype(arg_a)>;
                          using T2 = std::decay_t<decltype(arg_b)>;

                          // C++17: if constexpr cho phép biên dịch nhánh code có kiểu dữ liệu khớp
                          if constexpr (std::is_same_v<T1, T2>)
                          {
                              if constexpr (std::is_same_v<T1, ToolCall>)
                              {
                                 bool same = (arg_a.tool == arg_b.tool && arg_a.args == arg_b.args);
                                 
                                 return same;
                              }
                              else if constexpr (std::is_same_v<T1, FinalAnswer>)
                              {
                                  return arg_a.text == arg_b.text;
                              }
                          }
                          return false; 
                      },
                      a, b);
}

bool LoopDetector::isSameStep(const Step &a, const Step &b)
{
    // First compare action types and contents
    if (!isSameAction(a.action, b.action))
        return false;

    // For tool calls, we can only confirm a true repeat after observing results.
    // If either result is missing, avoid false positives by not declaring the steps identical yet.
    if (std::holds_alternative<ToolCall>(a.action) && std::holds_alternative<ToolCall>(b.action))
    {
        if (a.tool_result.empty() || b.tool_result.empty())
            return false;
        return a.tool_result == b.tool_result;
    }

    // For final answers, compare the action content directly.
    return true;
}
// A A A A A A A A 
std::optional<LoopSeverity> LoopDetector::checkGenericRepeat(const std::vector<Step> &history) const
{
    if (history.empty())
        return std::nullopt;

    int count = 1;
    const auto &last_step = history.back();

    for (auto it = history.rbegin() + 1; it != history.rend(); ++it)
    {
        if (isSameStep(*it, last_step))
        {
            count++;
        }
        else
        {
            break;
        }
    }

    if (count >= _critical)
        return LoopSeverity::CRITICAL;
    if (count >= _warning)
    {
        std::cerr << "[LoopDetector DEBUG] generic repeat count=" << count << " (warning_threshold=" << _warning << ", critical=" << _critical << ")\n";
        return LoopSeverity::WARNING;
    }

    return std::nullopt;
}

// A B A B A B A B A B 
std::optional<LoopSeverity> LoopDetector::checkPingPong(const std::vector<Step> &history) const
{
    int n = history.size();
    if (n < 4)
    {
        return std::nullopt;
    }

    const auto &step_A = history[n - 1];
    const auto &step_B = history[n - 2];

    if (isSameStep(step_A, step_B))
    {
        return std::nullopt;
    }

    int count = 0;

    for (int i = n - 1; i >= 1; i -= 2)
    {
        if (isSameStep(history[i], step_A) &&
            isSameStep(history[i - 1], step_B))
        {
            count++;
        }
        else
        {
            break;
        }
    }
    if (count >= _critical)
        return LoopSeverity::CRITICAL;
    if (count >= _warning)
        return LoopSeverity::WARNING;

    return std::nullopt;
}

// Hàm thực thi tổng hợp
LoopResult LoopDetector::detect(const std::vector<Step> &history) const
{
    LoopResult result = {LoopType::NONE, LoopSeverity::NORMAL, "Agent running normally."};

    auto generic_severity = checkGenericRepeat(history);
    auto pingpong_severity = checkPingPong(history);

    if (!generic_severity.has_value() && !pingpong_severity.has_value())
    {
        return result;
    }

    if (generic_severity.has_value() && pingpong_severity.has_value())
    {
        if (generic_severity.value() == LoopSeverity::CRITICAL ||
            pingpong_severity.value() == LoopSeverity::CRITICAL)
        {
            result.sev = LoopSeverity::CRITICAL;
            result.type = (generic_severity.value() == LoopSeverity::CRITICAL)
                              ? LoopType::GENERIC_REPEAT
                              : LoopType::PING_PONG;
        }
        else
        {
            result.sev = LoopSeverity::WARNING;
            result.type = LoopType::GENERIC_REPEAT;
        }
    }
    else if (generic_severity.has_value())
    {
        result.sev = generic_severity.value();
        result.type = LoopType::GENERIC_REPEAT;
    }
    else
    {
        result.sev = pingpong_severity.value();
        result.type = LoopType::PING_PONG;
    }

    return result;
}