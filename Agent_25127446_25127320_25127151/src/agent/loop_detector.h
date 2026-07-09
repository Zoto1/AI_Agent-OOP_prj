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
    std::optional<LoopSeverity> checkGenericRepeat(const std::vector<Record> &history) const;
    std::optional<LoopSeverity> checkPingPong(const std::vector<Record> &history) const;

public:
    explicit LoopDetector(int warning = 2, int critcal = 4);
    LoopResult detect(const std::vector<Record>&history)const;

};