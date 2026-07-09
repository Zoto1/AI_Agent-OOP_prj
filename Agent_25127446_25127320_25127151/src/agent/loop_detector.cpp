#include "loop_detector.h"

LoopDetector::LoopDetector(int warning, int critical)
    : _warning(warning), _critical(critical) {}

std::optional<LoopSeverity> LoopDetector::checkGenericRepeat(const std::vector<Record> &history) const
{
    
}

std::optional<LoopSeverity> LoopDetector::checkPingPong(const std::vector<Record> &history) const
{
}

LoopResult LoopDetector :: detect(const std::vector<Record>&history)const{
    LoopResult res = {LoopType::NONE, LoopSeverity::NORMAL, "working..."};

    if (history.empty()){
        return res;
    }
    std::optional<LoopSeverity>generic_sev= checkGenericRepeat(history);
    if(generic_sev.has_value()) {
        res.type = LoopType::GENERIC_REPEAT;
        res.sev = generic_sev.value();
        res.message ="Phát hiện vòng lặp GENERIC REPEAT Hành động bị lặp lại liên tục.";
        return res;
    }
    
    std::optional<LoopSeverity>pingpong_sev= checkPingPong(history);
    if(pingpong_sev.has_value()) {
        res.type = LoopType:: PING_PONG;
        res.sev = pingpong_sev.value();
        res.message = "Phát hiện vòng lặp PING-PONG: Các hành động đang nhảy qua lại.";
        return res;
    }
    return res;
}