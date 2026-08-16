#pragma once

// =============================================================================
// 10.3 Multi-agent Coordination
// =============================================================================
// Cho phép HarnessRunner spawn sub-agent (thread mới) cho từng subtask.
// Agents giao tiếp qua MessageQueue (std::queue + mutex + condition_variable).
//
// Kiến trúc:
//   SubTaskDefinition  — mô tả một subtask con
//   AgentMessage       — tin nhắn trao đổi giữa các agent (producer/consumer)
//   MessageQueue<T>    — thread-safe queue dùng mutex + condition_variable (C++20 jthread)
//   SubAgentHandle     — quản lý vòng đời của 1 sub-agent thread
//   MultiAgentCoordinator — spawn & join nhiều sub-agent, merge kết quả
//
// Design Patterns:
//   - Template class (MessageQueue<T>)             → C++17 requirement
//   - std::jthread (C++20)                         → C++20 requirement
//   - std::stop_token / cooperative cancellation  → C++20 requirement
//   - std::atomic                                  → thread-safe flags
// =============================================================================

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "../client/llm_client.h"
#include "../agent/loop_detector.h"
#include "../agent/skill_loader.h"
#include "../tools/tool_registry.h"
#include "trajectory.h"

// ---------------------------------------------------------------------------
// AgentMessage: đơn vị thông điệp trao đổi giữa các agent
// ---------------------------------------------------------------------------
struct AgentMessage {
    std::string sender_id;    // ID của agent gửi (e.g. "coordinator", "sub_0")
    std::string receiver_id;  // ID agent nhận ("" = broadcast)
    std::string content;      // nội dung thông điệp
    bool is_result = false;   // true nếu đây là kết quả cuối của một sub-agent

    AgentMessage() = default;
    AgentMessage(std::string sender, std::string receiver, std::string msg,
                 bool result = false)
        : sender_id(std::move(sender)),
          receiver_id(std::move(receiver)),
          content(std::move(msg)),
          is_result(result) {}
};

// ---------------------------------------------------------------------------
// MessageQueue<T> — thread-safe queue (Template class, C++17)
// Dùng std::mutex + std::condition_variable để đồng bộ producer/consumer.
// ---------------------------------------------------------------------------
template <typename T>
class MessageQueue {
public:
    MessageQueue() = default;

    // Không cho copy (chỉ dùng qua shared_ptr)
    MessageQueue(const MessageQueue&) = delete;
    MessageQueue& operator=(const MessageQueue&) = delete;

    // Push một message vào queue — thread-safe
    void push(T item) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(std::move(item));
        }
        m_cv.notify_one();
    }

    // Pop blocking: chờ đến khi có phần tử hoặc queue bị đóng (closed)
    // Trả về std::nullopt nếu queue đã closed và rỗng.
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_queue.empty() || m_closed; });
        if (m_queue.empty()) {
            return std::nullopt;
        }
        T item = std::move(m_queue.front());
        m_queue.pop();
        return item;
    }

    // Pop non-blocking: trả về std::nullopt ngay nếu rỗng
    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return std::nullopt;
        }
        T item = std::move(m_queue.front());
        m_queue.pop();
        return item;
    }

    // Pop blocking với timeout (C++11 chrono)
    template <typename Rep, typename Period>
    std::optional<T> pop_for(const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_cv.wait_for(lock, timeout,
                           [this] { return !m_queue.empty() || m_closed; })) {
            return std::nullopt;  // timeout
        }
        if (m_queue.empty()) {
            return std::nullopt;  // closed
        }
        T item = std::move(m_queue.front());
        m_queue.pop();
        return item;
    }

    // Đóng queue: báo hiệu cho tất cả consumer biết không còn item nào nữa
    void close() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_closed = true;
        }
        m_cv.notify_all();
    }

    bool is_closed() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_closed;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<T> m_queue;
    bool m_closed = false;
};

// ---------------------------------------------------------------------------
// SubTaskDefinition: mô tả 1 subtask được giao cho sub-agent
// ---------------------------------------------------------------------------
struct SubTaskDefinition {
    std::string id;           // e.g. "sub_0", "sub_1"
    std::string instruction;  // lệnh cụ thể giao cho sub-agent
    int max_steps = 10;       // giới hạn bước
};

// ---------------------------------------------------------------------------
// SubAgentResult: kết quả do sub-agent báo lại sau khi hoàn thành
// ---------------------------------------------------------------------------
struct SubAgentResult {
    std::string sub_id;
    std::string final_answer;
    bool success = false;
    long long total_tokens = 0;
    long long total_time_ms = 0;
    std::string error;      // rỗng nếu không có lỗi
    Trajectory trajectory;  // toàn bộ trajectory của sub-agent
};

// ---------------------------------------------------------------------------
// SubAgentHandle: quản lý vòng đời 1 sub-agent thread
// Dùng std::jthread (C++20) — tự join khi destructor, hỗ trợ stop_token.
// Note: std::atomic<bool> không movable, dùng unique_ptr để handle có thể move.
// ---------------------------------------------------------------------------
class SubAgentHandle {
public:
    SubAgentHandle(std::string id,
                   std::shared_ptr<MessageQueue<AgentMessage>> inbox,
                   std::shared_ptr<MessageQueue<AgentMessage>> outbox,
                   std::function<SubAgentResult()> task_fn);

    ~SubAgentHandle();

    // Move-only (std::jthread và std::promise không copyable)
    SubAgentHandle(const SubAgentHandle&) = delete;
    SubAgentHandle& operator=(const SubAgentHandle&) = delete;
    SubAgentHandle(SubAgentHandle&&) = default;
    SubAgentHandle& operator=(SubAgentHandle&&) = default;

    const std::string& getId() const { return m_id; }

    // Gửi message tới sub-agent này
    void send(AgentMessage msg);

    // Lấy future kết quả (chỉ gọi 1 lần)
    std::future<SubAgentResult> getResultFuture() { return m_promise.get_future(); }

    bool isDone() const { return m_done && m_done->load(); }

    void requestStop();

private:
    std::string m_id;
    std::shared_ptr<MessageQueue<AgentMessage>> m_inbox;
    std::shared_ptr<MessageQueue<AgentMessage>> m_outbox;
    std::promise<SubAgentResult> m_promise;

    // atomic<bool> không movable — bọc qua unique_ptr để SubAgentHandle movable
    std::unique_ptr<std::atomic<bool>> m_done{
        std::make_unique<std::atomic<bool>>(false)
    };

    // C++20: std::jthread tự join khi destructor
    std::jthread m_thread;
};

// ---------------------------------------------------------------------------
// MultiAgentCoordinator: spawn và điều phối nhiều sub-agent chạy song song.
//
// Luồng hoạt động:
//   1. Nhận task phức tạp từ HarnessRunner
//   2. Phân tách thành SubTaskDefinition[] (do user hoặc LLM phân chia)
//   3. Spawn mỗi subtask vào 1 std::jthread — mỗi agent có inbox/outbox riêng
//   4. Chờ tất cả hoàn thành (std::future::get)
//   5. Tổng hợp kết quả thành 1 chuỗi trả về HarnessRunner
// ---------------------------------------------------------------------------
class MultiAgentCoordinator {
public:
    // Constructor: nhận các dependency giống HarnessRunner
    MultiAgentCoordinator(std::shared_ptr<LLMClient> llm,
                          std::shared_ptr<ToolRegistry> tool_registry,
                          std::shared_ptr<SkillLoader> skill_loader,
                          std::shared_ptr<LoopDetector> loop_detector);

    ~MultiAgentCoordinator();

    // Chạy nhiều subtask song song, trả về list kết quả theo thứ tự subtasks
    std::vector<SubAgentResult> runParallel(
        const std::vector<SubTaskDefinition>& subtasks);

    // Tổng hợp kết quả thành 1 chuỗi trả lời cuối cùng
    static std::string mergeResults(const std::vector<SubAgentResult>& results);

    // Gửi broadcast message đến tất cả sub-agent đang chạy
    void broadcast(const std::string& message);

private:
    std::shared_ptr<LLMClient> m_llm;
    std::shared_ptr<ToolRegistry> m_tool_registry;
    std::shared_ptr<SkillLoader> m_skill_loader;
    std::shared_ptr<LoopDetector> m_loop_detector;

    // Shared outbox: tất cả sub-agent gửi về coordinator qua đây
    std::shared_ptr<MessageQueue<AgentMessage>> m_coordinator_inbox;

    // Per-agent inbox
    std::vector<std::shared_ptr<MessageQueue<AgentMessage>>> m_agent_inboxes;

    // Xây dựng hàm thực thi cho 1 sub-agent
    std::function<SubAgentResult()> buildSubAgentTask(
        const SubTaskDefinition& subtask,
        std::shared_ptr<MessageQueue<AgentMessage>> inbox,
        std::shared_ptr<MessageQueue<AgentMessage>> outbox);
};
