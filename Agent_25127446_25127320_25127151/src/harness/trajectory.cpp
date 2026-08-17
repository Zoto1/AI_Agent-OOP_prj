#include "trajectory.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

json toJson(const ToolCall& tool_call)
{
    return {
        {"type", "tool_call"},
        {"tool", tool_call.tool},
        {"args", tool_call.args}
    };
}

json toJson(const FinalAnswer& final_answer)
{
    return {
        {"type", "final_answer"},
        {"text", final_answer.text}
    };
}

json toJson(const Step& step)
{
    json action_json;

    std::visit(
        [&action_json](const auto& action)
        {
            action_json = toJson(action);
        },
        step.action
    );

    // FinalAnswer không thực thi tool nên không có tool result. Dùng null thay
    // cho chuỗi rỗng để consumer phân biệt rõ "không áp dụng" với một tool
    // thực sự trả về output rỗng.
    const json tool_result_json =
        std::holds_alternative<ToolCall>(step.action)
            ? json(step.tool_result)
            : json(nullptr);

    return {
        {"step_id", step.step_id},
        {"thought", step.thought},
        {"action", action_json},
        {"tool_result", tool_result_json},
        {"tokens_used", step.tokens_used},
        {"latency_ms", step.latency_ms}
    };
}

json toJson(const Trajectory& trajectory)
{
    json steps_json = json::array();

    for (const Step& step : trajectory.steps)
    {
        steps_json.push_back(toJson(step));
    }

    return {
        {"task_id", trajectory.task_id},
        {"model", trajectory.model},
        {"success", trajectory.success},
        {"total_tokens", trajectory.total_tokens},
        {"total_time_ms", trajectory.total_time_ms},
        {"final_answer", trajectory.final_answer},
        {
            "termination_status",
            terminationStatusToString(
                trajectory.termination_status
            )
        },
        {
            "error",
            trajectory.error_message.empty()
                ? json(nullptr)
                : json(trajectory.error_message)
        },
        {"steps", steps_json}
    };
}
