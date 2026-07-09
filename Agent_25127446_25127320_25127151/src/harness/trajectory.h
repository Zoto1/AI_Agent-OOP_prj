#pragma once
#include <string>

struct Record
{
    std::string thought;
    std::string action;
    std::string tool_result;
    double latency_ms;
    int tokens;
};