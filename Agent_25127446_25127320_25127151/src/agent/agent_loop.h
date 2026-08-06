#pragma once
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <functional>
#include <variant>
#include "../harness/trajectory.h" // sử dụng Step
#include "../client/llm_client.h"  // sử dụng Message

class LLMClient;
class ToolRegistry;
class SkillLoader; 
class LoopDetector;

enum class AgentTerminationStatus {
    Completed,
    LoopDetected,
    MaxStepsReached,
};

struct AgentRunResult {
    std::string final_answer;
    AgentTerminationStatus status = AgentTerminationStatus::MaxStepsReached;
    long long total_tokens = 0;

    operator std::string() const
    {
        return final_answer;
    }
};

inline std::string agentTerminationStatusToString(
    AgentTerminationStatus status)
{
    switch (status) {
    case AgentTerminationStatus::Completed:
        return "Completed";
    case AgentTerminationStatus::LoopDetected:
        return "LoopDetected";
    case AgentTerminationStatus::MaxStepsReached:
        return "MaxStepsReached";
    }
    return "Unknown";
}

class AgentLoop
{
protected:
    std::shared_ptr<LLMClient> llm;
    std::shared_ptr<ToolRegistry> tool_registry;
    std::shared_ptr<SkillLoader> skill_loader;
    std::shared_ptr<LoopDetector> loop_detector;

    
    std::vector<Message> history;
    std::vector<Step> step_history;

    const int max_steps;

    std::function<void(const Step &)> step_hook;
    bool verbose = false;

public:
    AgentLoop(std::shared_ptr<LLMClient> client,
              std::shared_ptr<ToolRegistry> registry,
              std::shared_ptr<SkillLoader> loader,
              std::shared_ptr<LoopDetector> detector,
              int max_steps = 10);
    virtual ~AgentLoop() = default;

    void setStepHook(std::function<void(const Step &)> hook);
    void setVerbose(bool enable);

    AgentRunResult run(const std::string &task);

protected:
    virtual void observe(const std::string &tool_result);
    virtual LLMResponse think();
    virtual std::optional<std::variant<ToolCall, FinalAnswer>>
    act(const std::string &thought);

private:
    std::optional<ToolCall> parseToolCall(const std::string &response);
    std::optional<FinalAnswer> parseFinalAnswer(const std::string &response);
    Message buildSystemMessage(const std::string &task);
};
