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
class SkillLoader; // chưa có
class LoopDetector;

class AgentLoop
{
protected:
    std::shared_ptr<LLMClient> llm;
    std::shared_ptr<ToolRegistry> tool_registry;
    std::shared_ptr<SkillLoader> skill_loader;
    std::shared_ptr<LoopDetector> loop_detector;

    std::vector<Message> history;

    const int max_steps;

    std::function<void(const Step &)> step_hook;

public:
    AgentLoop(std::shared_ptr<LLMClient> client,
              std::shared_ptr<ToolRegistry> registry,
              std::shared_ptr<SkillLoader> loader,
              std::shared_ptr<LoopDetector> detector,
              int max_steps = 10);
    virtual ~AgentLoop() = default;

    void setStepHook(std::function<void(const Step &)> hook);

    std::string run(const std::string &task);

protected:
    virtual void observe(const std::string &tool_result);
    virtual std::string think();
    virtual std::variant<ToolCall, FinalAnswer> act(const std::string &thought);

private:
    std::optional<ToolCall> parseToolCall(const std::string &response);
    Message buildSystemMessage(const std::string &task);
};
