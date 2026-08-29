#pragma once

#include <string>
#include <variant>

struct ToolCall
{
    std::string tool;
    std::string args;
};

struct FinalAnswer
{
    std::string text;
};

// Du lieu cua mot vong lap ReAct. Kieu nay thuoc agent core va khong phu
// thuoc vao Harness; Harness chi quan sat va luu cac Step vao Trajectory.
struct Step
{
    int step_id = 0;

    std::string thought;

    std::variant<ToolCall, FinalAnswer> action;

    std::string tool_result;

    long long tokens_used = 0;
    long long latency_ms = 0;
};
