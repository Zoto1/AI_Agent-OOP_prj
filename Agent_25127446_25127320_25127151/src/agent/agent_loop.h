#pragma once
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <functional>
#include <variant>
#include "../harness/trajectory.h" // sử dụng Step
#include "../client/llm_client.h"  // sử dụng Message, LLMClient, LLMBackend

// Forward declarations cho các class chưa được include
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

// =============================================================================
// DESIGN PATTERN: Template Method (GoF - Behavioral)
// =============================================================================
// AgentLoop::run() định nghĩa "skeleton" của thuật toán ReAct:
//   run() → [loop] think() → act() → observe() → ...
// Các bước think / act / observe được khai báo là `virtual protected`.
// Subclass có thể override từng bước mà không cần viết lại toàn bộ loop.
//
// Ví dụ: Một "DryRunAgentLoop" có thể override act() để giả lập tool calls
// mà không thực thi thật — hữu ích khi chạy unit test.
// =============================================================================
//
// =============================================================================
// DESIGN PATTERN: Observer / Hook (GoF - Behavioral)
// =============================================================================
// AgentLoop không biết HarnessRunner tồn tại (tuân thủ mục 4.4 đề bài).
// Thay vào đó, HarnessRunner tiêm (inject) step_hook từ bên ngoài qua
// setStepHook(). Mỗi khi hoàn thành 1 step, AgentLoop gọi step_hook(step).
//
//   AgentLoop::run()           HarnessRunner ("observer")
//         │                          │
//         │── step_hook(step) ──────▶│ trajectory.steps.push_back(step)
//         │                          │ [ghi lại trajectory]
//
// Đầu mối trong code: setStepHook(), m_step_hook trong run()
// =============================================================================
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

// =============================================================================
// S-2 — C++20: Concepts — sử dụng concept LLMBackend
// =============================================================================
// makeAgentLoop<T>() là factory function được ràng buộc bằng concept LLMBackend.
// Trình biên dịch sẽ báo lỗi tại compile-time nếu T không phải subclass của LLMClient.
// Điều này thay thế SFINAE và làm rõ ràng ý định hơn so với C++17.
//
// Ví dụ:
//   auto agent = makeAgentLoop<GeminiClient>(client, registry, skills, detector, 10);
//   // auto agent = makeAgentLoop<std::string>(...); // ← Lỗi biên dịch rõ ràng!

template <LLMBackend T>
AgentLoop makeAgentLoop(std::shared_ptr<T> client,
    
                        std::shared_ptr<ToolRegistry> registry,
                        std::shared_ptr<SkillLoader> loader,
                        std::shared_ptr<LoopDetector> detector,
                        int max_steps = 10)
{
    return AgentLoop(
        std::static_pointer_cast<LLMClient>(client),
        registry, loader, detector, max_steps
    );
}
