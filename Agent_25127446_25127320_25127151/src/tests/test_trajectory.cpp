#include "../src/harness/trajectory.h"

#include <cassert>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main()
{
    Trajectory trajectory;

    trajectory.task_id = "task_test";
    trajectory.model = "gemini-test";
    trajectory.success = true;
    trajectory.total_tokens = 150;
    trajectory.total_time_ms = 1200;
    trajectory.final_answer = "Da hoan thanh";
    trajectory.termination_status =
        TerminationStatus::Completed;

    Step tool_step;
    tool_step.step_id = 0;
    tool_step.thought =
        "Can dung calculator";

    tool_step.action = ToolCall{
        "calculator",
        R"({"expression":"15*17"})"
    };

    tool_step.tool_result = "255";
    tool_step.tokens_used = 100;
    tool_step.latency_ms = 800;

    trajectory.steps.push_back(tool_step);

    Step final_step;
    final_step.step_id = 1;
    final_step.thought =
        "Da co ket qua";

    final_step.action = FinalAnswer{
        "Ket qua la 255"
    };

    final_step.tokens_used = 50;
    final_step.latency_ms = 400;

    trajectory.steps.push_back(final_step);

    const json output =
        toJson(trajectory);

    assert(output["task_id"] == "task_test");
    assert(output["model"] == "gemini-test");
    assert(output["success"] == true);
    assert(output["total_tokens"] == 150);
    assert(output["total_time_ms"] == 1200);

    assert(
        output["termination_status"] ==
        "completed"
    );

    assert(output["error"].is_null());
    assert(output["steps"].size() == 2);

    // Kiểm tra ToolCall
    assert(
        output["steps"][0]["action"]["type"] ==
        "tool_call"
    );

    assert(
        output["steps"][0]["action"]["tool"] ==
        "calculator"
    );

    assert(
        output["steps"][0]["action"]["args"] ==
        R"({"expression":"15*17"})"
    );

    // Kiểm tra FinalAnswer
    assert(
        output["steps"][1]["action"]["type"] ==
        "final_answer"
    );

    assert(
        output["steps"][1]["action"]["text"] ==
        "Ket qua la 255"
    );

    std::cout
        << "[PASS] Trajectory JSON tests\n";

    return 0;
}