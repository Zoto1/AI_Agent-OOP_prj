#include "../agent/agent_loop.h"
#include "../client/llm_client.h"
#include "../tools/calculator.h"
#include "../tools/tool_registry.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class TextMockClient : public LLMClient
{
private:
    std::vector<std::string> responses;
    std::size_t next_response = 0;

public:
    std::vector<std::vector<Message>> requests;

    explicit TextMockClient(std::vector<std::string> scripted_responses)
        : LLMClient(LLMConfig{}), responses(std::move(scripted_responses)) {}

    std::string chat(const std::vector<Message> &messages) override
    {
        requests.push_back(messages);
        assert(next_response < responses.size());
        return responses[next_response++];
    }

    std::string chatMultimodal(
        const std::vector<Message> &messages,
        const std::vector<std::string> &) override
    {
        return chat(messages);
    }
};

class NativeToolMockClient : public LLMClient
{
public:
    int calls = 0;
    bool saw_function_call_history = false;
    bool saw_function_response_history = false;

    NativeToolMockClient() : LLMClient(LLMConfig{}) {}

    std::string chat(const std::vector<Message> &) override
    {
        return "";
    }

    LLMResponse chatWithTools(
        const std::vector<Message> &messages,
        const std::string &declarations) override
    {
        ++calls;
        assert(declarations.find("calculator") != std::string::npos);
        assert(declarations.find("expression") != std::string::npos);

        for (const Message &message : messages)
        {
            saw_function_call_history =
                saw_function_call_history ||
                message.kind == MessageKind::FunctionCall;
            saw_function_response_history =
                saw_function_response_history ||
                message.kind == MessageKind::FunctionResponse;
        }

        if (calls == 1)
        {
            return {
                "I should use the calculator tool.",
                std::nullopt,
                TokenUsage{4, 1, 0, 5}
            };
        }

        if (calls == 2)
        {
            return {
                "",
                LLMToolCall{
                    "calculator",
                    R"({"expression":"128 / 8"})",
                    "call-1",
                    "signature-1"
                },
                TokenUsage{100, 10, 20, 130}
            };
        }

        return {
            R"({"type":"final_answer","answer":"16"})",
            std::nullopt,
            TokenUsage{200, 5, 15, 220}
        };
    }

    std::string chatMultimodal(
        const std::vector<Message> &messages,
        const std::vector<std::string> &) override
    {
        return chat(messages);
    }
};

class TestableAgentLoop : public AgentLoop
{
public:
    using AgentLoop::AgentLoop;
    using AgentLoop::act;
};

int main()
{
    ToolRegistry &singleton = ToolRegistry::getInstance();
    singleton.registerTool(std::make_shared<CalculatorTool>());
    auto registry = std::shared_ptr<ToolRegistry>(&singleton, [](ToolRegistry *) {});

    // Prompt-based fallback: prose must be rejected, then a valid call executes.
    auto text_client = std::make_shared<TextMockClient>(
        std::vector<std::string>{
            "I should use the calculator tool.",
            R"({"type":"tool_call","tool":"calculator","args":{"expression":"128 / 8"}})",
            R"({"type":"final_answer","answer":"16"})"
        });

    std::vector<Step> text_steps;
    AgentLoop text_agent(text_client, registry, nullptr, nullptr, 5);
    text_agent.setStepHook([&text_steps](const Step &step) {
        text_steps.push_back(step);
    });

    const AgentRunResult text_result =
        text_agent.run("Tinh 128 chia cho 8 bang bao nhieu?");

    assert(text_result.status == AgentTerminationStatus::Completed);
    assert(text_result.final_answer == "16");
    assert(text_client->requests.size() == 3);
    assert(text_steps.size() == 2);
    assert(std::holds_alternative<ToolCall>(text_steps[0].action));
    assert(std::get<ToolCall>(text_steps[0].action).tool == "calculator");
    assert(text_steps[0].tool_result.find("16") != std::string::npos);
    assert(std::holds_alternative<FinalAnswer>(text_steps[1].action));

    // A literal newline inside a quoted JSON value is invalid JSON, but it is
    // a common model formatting error. The parser should escape and recover it.
    TestableAgentLoop parser_agent(
        text_client, registry, nullptr, nullptr, 2);
    const std::string raw_newline_call =
        "{\"type\":\"tool_call\",\"tool\":\"calculator\","
        "\"args\":{\"expression\":\"128 / 8\n\"}}";
    const auto repaired_action = parser_agent.act(raw_newline_call);
    assert(repaired_action.has_value());
    assert(std::holds_alternative<ToolCall>(*repaired_action));
    const ToolCall &repaired_call = std::get<ToolCall>(*repaired_action);
    assert(repaired_call.tool == "calculator");
    assert(repaired_call.args.find("\\n") != std::string::npos);

    // After a tool has run, an unrepairable structured action must trigger a
    // format retry instead of being mistaken for the final answer.
    auto retry_client = std::make_shared<TextMockClient>(
        std::vector<std::string>{
            R"({"type":"tool_call","tool":"calculator","args":{"expression":"128 / 8"}})",
            R"({"type":"tool_call","tool":"calculator","args":BROKEN})",
            R"({"type":"final_answer","answer":"16"})"
        });
    std::vector<Step> retry_steps;
    AgentLoop retry_agent(retry_client, registry, nullptr, nullptr, 4);
    retry_agent.setStepHook([&retry_steps](const Step &step) {
        retry_steps.push_back(step);
    });

    const AgentRunResult retry_result =
        retry_agent.run("Tinh 128 chia cho 8 bang bao nhieu?");
    assert(retry_result.status == AgentTerminationStatus::Completed);
    assert(retry_result.final_answer == "16");
    assert(retry_client->requests.size() == 3);
    assert(retry_steps.size() == 2);
    assert(std::holds_alternative<ToolCall>(retry_steps[0].action));
    assert(std::holds_alternative<FinalAnswer>(retry_steps[1].action));

    // Native function calling: preserve call/response metadata in history.
    auto native_client = std::make_shared<NativeToolMockClient>();
    std::vector<Step> native_steps;
    AgentLoop native_agent(native_client, registry, nullptr, nullptr, 4);
    native_agent.setStepHook([&native_steps](const Step &step) {
        native_steps.push_back(step);
    });

    const AgentRunResult native_result =
        native_agent.run("Tinh 128 chia cho 8 bang bao nhieu?");

    assert(native_result.status == AgentTerminationStatus::Completed);
    assert(native_result.final_answer == "16");
    assert(native_client->calls == 3);
    assert(native_client->saw_function_call_history);
    assert(native_client->saw_function_response_history);
    assert(native_steps.size() == 2);
    assert(native_steps[0].tool_result.find("16") != std::string::npos);
    assert(native_steps[0].tokens_used == 130);
    assert(native_steps[1].tokens_used == 220);
    // Includes the rejected 5-token prose response as well as both valid steps.
    assert(native_result.total_tokens == 355);

    CalculatorTool calculator;
    const std::string complex_result = calculator.execute({
        {"expression", "(12 + 8) * 3"}
    });
    assert(complex_result.find("60") != std::string::npos);

    std::cout << "[PASS] Agent tool-call integration tests\n";
    return 0;
}
