// =============================================================================
// test_multi_agent.cpp
// Unit test cho 10.3 Multi-agent Coordination
// =============================================================================
// Test strategy:
//   - Dùng MockLLMClient trả lời cố định để test không cần Ollama thật.
//   - Kiểm tra MessageQueue thread-safety (producer/consumer trên thread khác).
//   - Kiểm tra MultiAgentCoordinator.runParallel() chạy đúng 2 sub-agent.
//   - Kiểm tra HarnessRunner.splitTaskIntoSubtasks() phân chia đúng.
//   - Kiểm tra mergeResults() ghép output đúng định dạng.
// =============================================================================

#include "../harness/multi_agent_coordinator.h"
#include "../harness/harness.h"
#include "../tools/tool_registry.h"
#include "../agent/loop_detector.h"
#include "../agent/skill_loader.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static int g_pass = 0;
static int g_fail = 0;

#define TEST_ASSERT(cond, msg)                                      \
    do {                                                            \
        if (cond) {                                                 \
            std::cout << "  [PASS] " << (msg) << "\n";             \
            ++g_pass;                                               \
        } else {                                                    \
            std::cerr << "  [FAIL] " << (msg) << "\n";             \
            ++g_fail;                                               \
        }                                                           \
    } while (false)

// ---------------------------------------------------------------------------
// MockLLMClient — trả lời cố định, không gọi mạng
// ---------------------------------------------------------------------------
class MockLLMClient : public LLMClient {
public:
    explicit MockLLMClient(std::string answer = "FINAL_ANSWER: mock result")
        : LLMClient(LLMConfig{
              .base_url   = "http://mock",
              .model_name = "mock-model",
          }),
          m_answer(std::move(answer)) {}

    std::string chat(const std::vector<Message>& /*messages*/) override {
        return m_answer;
    }

    std::string chatMultimodal(const std::vector<Message>& /*messages*/,
                               const std::vector<std::string>& /*images*/) override {
        return m_answer;
    }

private:
    std::string m_answer;
};

// ---------------------------------------------------------------------------
// Test 1: MessageQueue — push/pop cơ bản
// ---------------------------------------------------------------------------
void test_message_queue_basic() {
    std::cout << "\n[Test 1] MessageQueue basic push/pop\n";

    MessageQueue<int> q;
    q.push(42);
    q.push(100);

    auto v1 = q.pop();
    auto v2 = q.pop();

    TEST_ASSERT(v1.has_value() && *v1 == 42,   "Pop first value = 42");
    TEST_ASSERT(v2.has_value() && *v2 == 100,  "Pop second value = 100");
}

// ---------------------------------------------------------------------------
// Test 2: MessageQueue — close + pop trả nullopt
// ---------------------------------------------------------------------------
void test_message_queue_close() {
    std::cout << "\n[Test 2] MessageQueue close/nullopt\n";

    MessageQueue<std::string> q;
    q.close();

    auto v = q.pop();
    TEST_ASSERT(!v.has_value(), "Pop on closed empty queue returns nullopt");
    TEST_ASSERT(q.is_closed(),  "Queue is_closed() = true after close()");
}

// ---------------------------------------------------------------------------
// Test 3: MessageQueue — multi-thread producer/consumer
// ---------------------------------------------------------------------------
void test_message_queue_threaded() {
    std::cout << "\n[Test 3] MessageQueue multi-thread producer/consumer\n";

    constexpr int N = 100;
    MessageQueue<int> q;
    std::atomic<int> sum{0};

    // Producer thread
    std::thread producer([&] {
        for (int i = 0; i < N; ++i) {
            q.push(i);
        }
        q.close();
    });

    // Consumer on main thread
    int consumed = 0;
    while (true) {
        auto val = q.pop();
        if (!val.has_value()) break;
        sum.fetch_add(*val);
        ++consumed;
    }

    producer.join();

    const int expected = N * (N - 1) / 2;  // 0+1+...+99 = 4950
    TEST_ASSERT(consumed == N,           "Consumed all " + std::to_string(N) + " items");
    TEST_ASSERT(sum.load() == expected,  "Sum == " + std::to_string(expected));
}

// ---------------------------------------------------------------------------
// Test 4: MessageQueue — try_pop non-blocking
// ---------------------------------------------------------------------------
void test_message_queue_try_pop() {
    std::cout << "\n[Test 4] MessageQueue try_pop (non-blocking)\n";

    MessageQueue<int> q;
    auto empty_result = q.try_pop();
    TEST_ASSERT(!empty_result.has_value(), "try_pop on empty queue = nullopt");

    q.push(7);
    auto result = q.try_pop();
    TEST_ASSERT(result.has_value() && *result == 7, "try_pop returns 7");
}

// ---------------------------------------------------------------------------
// Test 5: MessageQueue — pop_for timeout
// ---------------------------------------------------------------------------
void test_message_queue_timeout() {
    std::cout << "\n[Test 5] MessageQueue pop_for timeout\n";

    MessageQueue<int> q;
    auto result = q.pop_for(50ms);  // timeout after 50ms
    TEST_ASSERT(!result.has_value(), "pop_for times out on empty queue");
}

// ---------------------------------------------------------------------------
// Test 6: splitTaskIntoSubtasks
// ---------------------------------------------------------------------------
void test_split_task() {
    std::cout << "\n[Test 6] HarnessRunner::splitTaskIntoSubtasks\n";

    const std::string combined =
        "Tính 15 * 17\n"
        "Lưu kết quả vào file result.txt\n"
        "In nội dung file ra màn hình";

    auto subs = HarnessRunner::splitTaskIntoSubtasks(combined, 2);
    TEST_ASSERT(subs.size() == 2, "Split 3 lines into 2 agents");
    TEST_ASSERT(!subs[0].id.empty() && !subs[1].id.empty(), "Agent IDs not empty");
    TEST_ASSERT(!subs[0].instruction.empty(), "Agent 0 instruction not empty");
    TEST_ASSERT(!subs[1].instruction.empty(), "Agent 1 instruction not empty");

    std::cout << "    sub_0: " << subs[0].instruction << "\n";
    std::cout << "    sub_1: " << subs[1].instruction << "\n";
}

// ---------------------------------------------------------------------------
// Test 7: splitTaskIntoSubtasks — single line
// ---------------------------------------------------------------------------
void test_split_task_single_line() {
    std::cout << "\n[Test 7] splitTaskIntoSubtasks single line\n";

    const std::string single = "Tính tổng 1 đến 100";
    auto subs = HarnessRunner::splitTaskIntoSubtasks(single, 2);

    // Chỉ 1 dòng → chỉ 1 subtask (actual_agents = min(1, 2) = 1)
    TEST_ASSERT(subs.size() == 1, "Single line → 1 subtask");
    TEST_ASSERT(subs[0].instruction == single, "Instruction preserved");
}

// ---------------------------------------------------------------------------
// Test 8: mergeResults
// ---------------------------------------------------------------------------
void test_merge_results() {
    std::cout << "\n[Test 8] MultiAgentCoordinator::mergeResults\n";

    SubAgentResult r1;
    r1.sub_id       = "sub_0";
    r1.success      = true;
    r1.final_answer = "Kết quả A";
    r1.total_tokens = 100;
    r1.total_time_ms = 500;

    SubAgentResult r2;
    r2.sub_id       = "sub_1";
    r2.success      = false;
    r2.error        = "Lỗi kết nối";
    r2.total_tokens = 50;
    r2.total_time_ms = 200;

    auto merged = MultiAgentCoordinator::mergeResults({r1, r2});

    TEST_ASSERT(merged.find("sub_0") != std::string::npos, "merged contains sub_0");
    TEST_ASSERT(merged.find("sub_1") != std::string::npos, "merged contains sub_1");
    TEST_ASSERT(merged.find("Kết quả A") != std::string::npos, "merged contains answer");
    TEST_ASSERT(merged.find("Lỗi kết nối") != std::string::npos, "merged contains error");

    std::cout << "--- Merged output ---\n" << merged << "---------------------\n";
}

// ---------------------------------------------------------------------------
// Test 9: MultiAgentCoordinator — runParallel với MockLLMClient
// ---------------------------------------------------------------------------
void test_multi_agent_run_parallel() {
    std::cout << "\n[Test 9] MultiAgentCoordinator::runParallel (2 mock agents)\n";

    auto llm      = std::make_shared<MockLLMClient>("FINAL_ANSWER: mock done");
    auto registry = std::shared_ptr<ToolRegistry>(&ToolRegistry::getInstance(),
                                                  [](ToolRegistry*) {}); // non-owning
    auto skills   = std::make_shared<SkillLoader>("skills/");
    auto detector = std::make_shared<LoopDetector>(2, 4);

    MultiAgentCoordinator coordinator(llm, registry, skills, detector);

    std::vector<SubTaskDefinition> subtasks = {
        {"sub_0", "Tính 1 + 1",   3},
        {"sub_1", "Tính 2 + 2",   3},
    };

    auto results = coordinator.runParallel(subtasks);

    TEST_ASSERT(results.size() == 2, "runParallel returns 2 results");
    TEST_ASSERT(results[0].sub_id == "sub_0" || results[1].sub_id == "sub_0",
                "sub_0 in results");
    TEST_ASSERT(results[0].sub_id == "sub_1" || results[1].sub_id == "sub_1",
                "sub_1 in results");

    for (const auto& res : results) {
        std::cout << "    [" << res.sub_id << "] "
                  << (res.success ? "SUCCESS" : "FAIL")
                  << " answer='" << res.final_answer << "'"
                  << " tokens=" << res.total_tokens
                  << " time=" << res.total_time_ms << "ms\n";
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main()
{
    std::cout << "========== Test 10.3 Multi-agent Coordination ==========\n";

    test_message_queue_basic();
    test_message_queue_close();
    test_message_queue_threaded();
    test_message_queue_try_pop();
    test_message_queue_timeout();
    test_split_task();
    test_split_task_single_line();
    test_merge_results();
    test_multi_agent_run_parallel();

    std::cout << "\n=========================================================\n";
    std::cout << "PASS: " << g_pass << "  FAIL: " << g_fail << "\n";
    std::cout << "=========================================================\n";

    return g_fail > 0 ? 1 : 0;
}
