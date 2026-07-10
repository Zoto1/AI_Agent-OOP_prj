#include "loop_detector.h"
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
                                  return arg_a.tool == arg_b.tool && arg_a.args == arg_b.args;
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
// A A A A A A A A 
std::optional<LoopSeverity> LoopDetector::checkGenericRepeat(const std::vector<Step> &history) const
{
    if (history.empty())
        return std::nullopt;

    int count = 1;
    const auto &last_action = history.back().action;

    for (auto it = history.rbegin() + 1; it != history.rend(); ++it)
    {
        if (isSameAction(it->action, last_action))
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

// A B A B A B A B A B 
std::optional<LoopSeverity> LoopDetector::checkPingPong(const std::vector<Step> &history) const
{
    int n = history.size();
    if (n < 4)
    {
        return std::nullopt;
    }

    const auto &action_A = history[n - 1].action;
    const auto &action_B = history[n - 2].action;

    if (isSameAction(action_A, action_B))
    {
        return std::nullopt;
    }

    int count = 0;

    for (int i = n - 1; i >= 1; i -= 2)
    {
        if (isSameAction(history[i].action, action_A) &&
            isSameAction(history[i - 1].action, action_B))
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
    if (generic_severity.has_value())
    {
        result.type = LoopType::GENERIC_REPEAT;
        result.sev = generic_severity.value();
        result.message = "Generic Repeat Detected: Agent keeps repeating the same action.";
        return result;
    }

    auto pingpong_severity = checkPingPong(history);
    if (pingpong_severity.has_value())
    {
        result.type = LoopType::PING_PONG;
        result.sev = pingpong_severity.value();
        result.message = "Ping-Pong Detected: Agent is stuck in an alternating loop.";
        return result;
    }

    return result;
}