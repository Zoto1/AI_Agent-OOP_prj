// =============================================================================
// 10.3 Multi-agent Coordination — Implementation
// =============================================================================
// Implements SubAgentHandle và MultiAgentCoordinator.
//
// C++ features used:
//   C++17: std::optional, std::shared_ptr, structured bindings, std::mutex
//   C++20: std::jthread, std::stop_token, std::atomic (enhanced)
// =============================================================================

#include "multi_agent_coordinator.h"

#include "../agent/agent_loop.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std::chrono_literals;

// =============================================================================
// SubAgentHandle — Implementation
// =============================================================================

SubAgentHandle::SubAgentHandle(
    std::string id,
    std::shared_ptr<MessageQueue<AgentMessage>> inbox,
    std::shared_ptr<MessageQueue<AgentMessage>> outbox,
    std::function<SubAgentResult()> task_fn)
    : m_id(std::move(id)),
      m_inbox(std::move(inbox)),
      m_outbox(std::move(outbox))
{
    // C++20: std::jthread — tự join khi bị huỷ (RAII)
    // stop_token cho phép yêu cầu dừng hợp tác (cooperative cancellation)
    m_thread = std::jthread(
        [this, fn = std::move(task_fn)](std::stop_token stoken) {
            SubAgentResult result;
            try {
                result = fn();
            } catch (const std::exception& e) {
                result.sub_id = m_id;
                result.success = false;
                result.error = e.what();
                std::cerr << "[SubAgent " << m_id
                          << "] Exception: " << e.what() << "\n";
            } catch (...) {
                result.sub_id = m_id;
                result.success = false;
                result.error = "Unknown exception";
            }

            // Báo cáo kết quả qua outbox (coordinator inbox)
            if (m_outbox) {
                AgentMessage done_msg(
                    m_id,
                    "coordinator",
                    result.final_answer,
                    true  // is_result = true
                );
                m_outbox->push(std::move(done_msg));
            }

            m_done->store(true);
            m_promise.set_value(std::move(result));
        }
    );
}

SubAgentHandle::~SubAgentHandle()
{
    // std::jthread tự join trong destructor — không cần gọi join() thủ công
    requestStop();
}

void SubAgentHandle::send(AgentMessage msg)
{
    if (m_inbox) {
        m_inbox->push(std::move(msg));
    }
}

void SubAgentHandle::requestStop()
{
    if (m_thread.joinable()) {
        m_thread.request_stop();
    }
    // Đóng inbox để unblock pop() đang chờ trong thread
    if (m_inbox) {
        m_inbox->close();
    }
}

// =============================================================================
// MultiAgentCoordinator — Implementation
// =============================================================================

MultiAgentCoordinator::MultiAgentCoordinator(
    std::shared_ptr<LLMClient> llm,
    std::shared_ptr<ToolRegistry> tool_registry,
    std::shared_ptr<SkillLoader> skill_loader,
    std::shared_ptr<LoopDetector> loop_detector)
    : m_llm(std::move(llm)),
      m_tool_registry(std::move(tool_registry)),
      m_skill_loader(std::move(skill_loader)),
      m_loop_detector(std::move(loop_detector)),
      m_coordinator_inbox(std::make_shared<MessageQueue<AgentMessage>>())
{
}

MultiAgentCoordinator::~MultiAgentCoordinator()
{
    // Đóng coordinator inbox để unblock bất kỳ waiter nào
    if (m_coordinator_inbox) {
        m_coordinator_inbox->close();
    }
}

// ---------------------------------------------------------------------------
// buildSubAgentTask: tạo hàm thực thi cho 1 sub-agent
// Hàm này chạy trong thread riêng của SubAgentHandle.
// ---------------------------------------------------------------------------
std::function<SubAgentResult()>
MultiAgentCoordinator::buildSubAgentTask(
    const SubTaskDefinition& subtask,
    std::shared_ptr<MessageQueue<AgentMessage>> inbox,
    std::shared_ptr<MessageQueue<AgentMessage>> outbox)
{
    // Capture theo value để thread an toàn khi coordinator kết thúc
    return [sub_id      = subtask.id,
            instruction = subtask.instruction,
            max_steps   = subtask.max_steps,
            llm         = m_llm,
            registry    = m_tool_registry,
            skills      = m_skill_loader,
            detector    = m_loop_detector,
            inbox_q     = inbox,
            outbox_q    = outbox]() -> SubAgentResult
    {
        using Clock = std::chrono::steady_clock;
        const auto start = Clock::now();

        SubAgentResult result;
        result.sub_id = sub_id;
        result.trajectory.task_id = sub_id;
        result.trajectory.model = llm ? llm->getModelName() : "unknown";

        std::cout << "[SubAgent " << sub_id << "] Bắt đầu: " << instruction << "\n";

        // ---------------------------------------------------------------
        // Thông báo bắt đầu qua outbox (coordinator có thể log)
        // ---------------------------------------------------------------
        if (outbox_q) {
            outbox_q->push(AgentMessage{
                sub_id, "coordinator",
                "STARTED: " + instruction, false
            });
        }

        try {
            // Tạo AgentLoop riêng cho sub-agent này
            AgentLoop agent(llm, registry, skills, detector, max_steps);

            // Hook: ghi lại từng step vào trajectory của sub-agent
            agent.setStepHook(
                [&result](const Step& step) {
                    result.trajectory.steps.push_back(step);
                    result.trajectory.total_tokens += step.tokens_used;
                }
            );

            // Chạy agent
            AgentRunResult agent_result = agent.run(instruction);

            result.final_answer  = agent_result.final_answer;
            result.total_tokens  = agent_result.total_tokens;
            result.trajectory.final_answer = agent_result.final_answer;
            result.trajectory.total_tokens = agent_result.total_tokens;
            result.success = (agent_result.status == AgentTerminationStatus::Completed);

            // Cập nhật trajectory termination status
            switch (agent_result.status) {
            case AgentTerminationStatus::Completed:
                result.trajectory.termination_status = TerminationStatus::Completed;
                result.trajectory.success = true;
                break;
            case AgentTerminationStatus::LoopDetected:
                result.trajectory.termination_status = TerminationStatus::LoopDetected;
                break;
            case AgentTerminationStatus::MaxStepsReached:
                result.trajectory.termination_status = TerminationStatus::MaxStepsReached;
                break;
            }

        } catch (const std::exception& e) {
            result.success = false;
            result.error   = e.what();
            result.trajectory.termination_status = TerminationStatus::AgentError;
            result.trajectory.error_message      = e.what();
            std::cerr << "[SubAgent " << sub_id << "] Lỗi: " << e.what() << "\n";
        }

        const auto end = Clock::now();
        result.total_time_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        result.trajectory.total_time_ms = result.total_time_ms;

        std::cout << "[SubAgent " << sub_id << "] Xong. "
                  << (result.success ? "SUCCESS" : "FAILED")
                  << " | tokens=" << result.total_tokens
                  << " | time=" << result.total_time_ms << "ms\n";

        return result;
    };
}

// ---------------------------------------------------------------------------
// runParallel: spawn tất cả subtask vào thread riêng, chờ tất cả xong
// ---------------------------------------------------------------------------
std::vector<SubAgentResult>
MultiAgentCoordinator::runParallel(
    const std::vector<SubTaskDefinition>& subtasks)
{
    if (subtasks.empty()) {
        return {};
    }

    const std::size_t n = subtasks.size();

    std::cout << "[Coordinator] Spawning " << n << " sub-agent(s)...\n";

    // Tạo inbox riêng cho từng sub-agent
    m_agent_inboxes.clear();
    m_agent_inboxes.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        m_agent_inboxes.push_back(
            std::make_shared<MessageQueue<AgentMessage>>()
        );
    }

    // Spawn sub-agents:
    // Dùng unique_ptr<SubAgentHandle> để tránh move SubAgentHandle vào vector
    // (std::atomic + std::promise không movable trong mọi trường hợp realloc).
    std::vector<std::unique_ptr<SubAgentHandle>> handles;
    std::vector<std::future<SubAgentResult>> futures;
    handles.reserve(n);
    futures.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        const SubTaskDefinition& sub = subtasks[i];

        auto task_fn = buildSubAgentTask(
            sub,
            m_agent_inboxes[i],
            m_coordinator_inbox
        );

        auto handle = std::make_unique<SubAgentHandle>(
            sub.id,
            m_agent_inboxes[i],
            m_coordinator_inbox,
            std::move(task_fn)
        );

        futures.push_back(handle->getResultFuture());
        handles.push_back(std::move(handle));
    }

    // Chờ tất cả future hoàn thành (blocking)
    std::vector<SubAgentResult> results;
    results.reserve(n);

    std::cout << "[Coordinator] Chờ " << n << " sub-agent hoàn thành...\n";

    for (auto& fut : futures) {
        SubAgentResult res = fut.get();  // blocking
        results.push_back(std::move(res));
    }

    std::cout << "[Coordinator] Tất cả sub-agent đã hoàn thành.\n";
    return results;
}

// ---------------------------------------------------------------------------
// mergeResults: gộp kết quả của tất cả sub-agent thành 1 chuỗi
// ---------------------------------------------------------------------------
std::string
MultiAgentCoordinator::mergeResults(
    const std::vector<SubAgentResult>& results)
{
    if (results.empty()) {
        return "(Không có sub-agent nào chạy)";
    }

    std::ostringstream oss;
    oss << "=== Kết quả tổng hợp từ " << results.size() << " sub-agent ===\n\n";

    for (const auto& res : results) {
        oss << "--- [" << res.sub_id << "] "
            << (res.success ? "SUCCESS" : "FAILED") << " ---\n";

        if (!res.error.empty()) {
            oss << "Lỗi: " << res.error << "\n";
        } else {
            oss << res.final_answer << "\n";
        }

        oss << "(tokens=" << res.total_tokens
            << ", time=" << res.total_time_ms << "ms)\n\n";
    }

    return oss.str();
}

// ---------------------------------------------------------------------------
// broadcast: gửi message tới tất cả sub-agent inbox
// ---------------------------------------------------------------------------
void MultiAgentCoordinator::broadcast(const std::string& message)
{
    for (auto& inbox : m_agent_inboxes) {
        if (inbox) {
            inbox->push(AgentMessage{
                "coordinator", "", message, false
            });
        }
    }
}
