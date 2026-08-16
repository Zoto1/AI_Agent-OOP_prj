#pragma once

#include <nlohmann/json_fwd.hpp>

#include <string>
#include <variant>
#include <vector>

struct ToolCall
{
    std::string tool;
    std::string args;
};

struct FinalAnswer
{
    std::string text;
};

struct Step
{
    int step_id = 0;

    std::string thought;

    std::variant<ToolCall, FinalAnswer> action;

    std::string tool_result;

    long long tokens_used = 0;
    long long latency_ms = 0;
};

enum class TerminationStatus
{
    Unknown,
    Completed,
    LoopDetected,
    MaxStepsReached,
    AgentError,
    EvaluationError,
    EnvironmentError
};

inline std::string terminationStatusToString(
    TerminationStatus status
)
{
    switch (status)
    {
    case TerminationStatus::Unknown:
        return "unknown";

    case TerminationStatus::Completed:
        return "completed";

    case TerminationStatus::LoopDetected:
        return "loop_detected";

    case TerminationStatus::MaxStepsReached:
        return "max_steps_reached";

    case TerminationStatus::AgentError:
        return "agent_error";

    case TerminationStatus::EvaluationError:
        return "evaluation_error";

    case TerminationStatus::EnvironmentError:
        return "environment_error";
    }

    return "unknown";
}

struct Trajectory
{
    std::string task_id;
    std::string model;

    bool success = false;

    long long total_tokens = 0;
    long long total_time_ms = 0;

    std::string final_answer;

    TerminationStatus termination_status =
        TerminationStatus::Unknown;

    std::string error_message;

    std::vector<Step> steps;
};

nlohmann::json toJson(const ToolCall& tool_call);
nlohmann::json toJson(const FinalAnswer& final_answer);
nlohmann::json toJson(const Step& step);
nlohmann::json toJson(const Trajectory& trajectory);