#pragma once
#include <vector>
#include <string>
#include <optional>

#include "../harness/trajectory.h"

enum class LoopType
{
    NONE,
    GENERIC_REPEAT,
    PING_PONG,
};

enum class LoopSeverity
{
    NORMAL,
    WARNING,
    CRITICAL,
};

struct LoopResult
{
    LoopType type;
    LoopSeverity sev;
    std::string message;
};

class LoopDetector
{
private:
    int _warning;
    int _critical;
    // c++17
    static bool isSameAction(const std::variant<ToolCall, FinalAnswer> &a,
                             const std::variant<ToolCall, FinalAnswer> &b);                                   
    static bool isSameStep(const Step &a, const Step &b);
    std::optional<LoopSeverity> checkGenericRepeat(const std::vector<Step> &history) const;
    std::optional<LoopSeverity> checkPingPong(const std::vector<Step> &history) const;

public:
    explicit LoopDetector(int warning = 2, int critical = 4);
    LoopResult detect(const std::vector<Step> &history) const;
};