#include "loop_detector.h"

LoopDetector::LoopDetector(int warning, int critical)
    : _warning(warning), _critical(critical) {}

std::optional<LoopSeverity> LoopDetector::checkGenericRepeat(const std::vector<Record> &history) const
{
    if (history.empty())
    {
        return std::nullopt;
    }
    const std::string &lastAction = history.back().action;

    int repeatCount = 0;
    for (auto i = history.rbegin(); i != history.rend(); ++i)
    {
        if (i->action == lastAction)
        {
            ++repeatCount;
        }
        else
        {
            break;
        }
    }

    if (repeatCount >= _critical)
    {
        return LoopSeverity::CRITICAL;
    }
    if (repeatCount >= _warning)
    {
        return LoopSeverity::WARNING;
    }

    return std::nullopt;
}

std::optional<LoopSeverity> LoopDetector::checkPingPong(const std::vector<Record> &history) const
{
    if (history.size() < 4)
    {
        return std::nullopt;
    }
    int n = history.size();

    const std::string &action_A = history[n - 1].action;
    const std::string &action_B = history[n - 2].action;

    if (action_A == action_B) // TH GENERIC REPEAT
    {
        return std::nullopt;
    }

    int count = 0;
    for (int i = n - 1; i >= 1; i -= 2)
    {
        if (history[i].action == action_A && history[i - 1].action == action_B)
        {
            ++count;
        }
        else
        {
            break;
        }
    }

    if (count >= _critical)
    {
        return LoopSeverity::CRITICAL;
    }
    if (count >= _warning)
    {
        return LoopSeverity::WARNING;
    }

    return std::nullopt;
}

LoopResult LoopDetector ::detect(const std::vector<Record> &history) const
{
    LoopResult res = {LoopType::NONE, LoopSeverity::NORMAL, "working..."};

    if (history.empty())
    {
        return res;
    }
    std::optional<LoopSeverity> generic_sev = checkGenericRepeat(history);
    if (generic_sev.has_value())
    {
        res.type = LoopType::GENERIC_REPEAT;
        res.sev = generic_sev.value();
        res.message = "Phát hiện vòng lặp GENERIC REPEAT Hành động bị lặp lại liên tục.";
        return res;
    }

    std::optional<LoopSeverity> pingpong_sev = checkPingPong(history);
    if (pingpong_sev.has_value())
    {
        res.type = LoopType::PING_PONG;
        res.sev = pingpong_sev.value();
        res.message = "Phát hiện vòng lặp PING-PONG: Các hành động đang nhảy qua lại.";
        return res;
    }
    return res;
}