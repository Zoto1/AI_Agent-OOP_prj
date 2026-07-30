#include "harness.h"

#include "Native_Environment.h"
#include "Sandbox_Environment.h"
#include "keyword_evaluator.h"
#include "functional_evaluator.h"
#include "../agent/agent_loop.h"
#include "../agent/loop_detector.h"
#include "../agent/skill_loader.h"
#include "../tools/tool_registry.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using json = nlohmann::json;
namespace fs = std::filesystem;

HarnessRunner::HarnessRunner(std::shared_ptr<LLMClient> llm,
                              std::shared_ptr<ToolRegistry> tool_registry,
                              std::shared_ptr<SkillLoader> skill_loader,
                              std::shared_ptr<LoopDetector> loop_detector,
                              std::string output_dir,
                              std::string workspace_root,
                              double success_threshold)
    : llm(std::move(llm)),
      tool_registry(std::move(tool_registry)),
      skill_loader(std::move(skill_loader)),
      loop_detector(std::move(loop_detector)),
      output_dir(std::move(output_dir)),
      workspace_root(std::move(workspace_root)),
      success_threshold(success_threshold) {}

// ---------------------------------------------------------------------------
// 7.2 Task Definition Format: đọc benchmark/tasks.json
// ---------------------------------------------------------------------------
std::vector<TaskDefinition> HarnessRunner::loadTasks(const std::string& tasks_json_path) const {
    std::ifstream file(tasks_json_path);
    if (!file.is_open()) {
        throw std::runtime_error(
            "Lỗi [HarnessRunner]: Không mở được file benchmark: " + tasks_json_path);
    }

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        throw std::runtime_error(
            "Lỗi [HarnessRunner]: File tasks.json không hợp lệ (" +
            std::string(e.what()) + ")");
    }

    if (!j.is_array()) {
        throw std::runtime_error(
            "Lỗi [HarnessRunner]: tasks.json phải là 1 mảng (array) các task.");
    }

    std::vector<TaskDefinition> tasks;
    tasks.reserve(j.size());

    for (const auto& item : j) {
        TaskDefinition t;
        // .value(key, default) an toàn hơn operator[]: task thiếu field tùy chọn
        // sẽ không làm cả file load thất bại.
        t.id = item.value("id", "");
        t.description = item.value("description", "");
        t.instruction = item.value("instruction", t.description);
        t.eval_type = item.value("eval_type", "keyword");
        t.eval_script = item.value("eval_script", "");
        t.max_steps = item.value("max_steps", 10);

        if (item.contains("eval_keywords") && item["eval_keywords"].is_array()) {
            for (const auto& kw : item["eval_keywords"]) {
                t.eval_keywords.push_back(kw.get<std::string>());
            }
        }

        if (t.id.empty()) {
            std::cerr << "Cảnh báo [HarnessRunner]: bỏ qua 1 task thiếu 'id' trong tasks.json\n";
            continue;
        }

        tasks.push_back(std::move(t));
    }

    return tasks;
}

// ---------------------------------------------------------------------------
// Strategy pattern: chọn Evaluator theo eval_type, AgentLoop/Environment không
// cần biết Evaluator nào đang được dùng.
// ---------------------------------------------------------------------------
std::shared_ptr<Evaluator> HarnessRunner::makeEvaluator(const TaskDefinition& task) const {
    if (task.eval_type == "functional") {
        if (task.eval_script.empty()) {
            throw std::runtime_error(
                "Lỗi [HarnessRunner]: Task '" + task.id +
                "' khai báo eval_type=functional nhưng thiếu eval_script.");
        }
        return std::make_shared<FunctionalEvaluator>(task.eval_script);
    }

    if (task.eval_type == "keyword") {
        return std::make_shared<KeywordEvaluator>(task.eval_keywords);
    }

    throw std::runtime_error(
        "Lỗi [HarnessRunner]: eval_type không hỗ trợ: '" + task.eval_type +
        "' (task '" + task.id + "')");
}

// Mỗi task chạy trong 1 thư mục làm việc riêng (NativeEnvironment) để không
// dẫm chân lên kết quả của task khác khi chạy batch.
std::shared_ptr<Environment> HarnessRunner::makeEnvironment(const TaskDefinition& task) const {
    std::string task_workdir = (fs::path(workspace_root) / task.id).string();
    return std::make_shared<NativeEnvironment>(task_workdir);
}

void HarnessRunner::exportTrajectory(const Trajectory& trajectory) const {
    fs::create_directories(output_dir);
    fs::path out_path = fs::path(output_dir) / ("trajectory_" + trajectory.task_id + ".json");

    std::ofstream out(out_path);
    if (!out.is_open()) {
        throw std::runtime_error(
            "Lỗi [HarnessRunner]: Không ghi được file trajectory: " + out_path.string());
    }
    out << trajectory.toJson();
}

// ---------------------------------------------------------------------------
// Chạy 1 task: setup env -> chạy AgentLoop (qua step_hook thu thập Trajectory)
// -> teardown env -> chấm điểm -> ghi file JSON.
// ---------------------------------------------------------------------------
TaskResult HarnessRunner::runTask(const TaskDefinition& task) {
    TaskResult result;
    result.task_id = task.id;

    auto env = makeEnvironment(task);

    try {
        env->setup();
    } catch (const std::exception& e) {
        result.error = std::string("Lỗi setup môi trường: ") + e.what();
        return result;
    }

    Trajectory trajectory;
    trajectory.task_id = task.id;
    trajectory.model = llm ? llm->getModelName() : "unknown";

    // AgentLoop KHÔNG biết HarnessRunner tồn tại (mục 4.4 đề bài): nó chỉ gọi
    // step_hook mỗi khi có 1 Step mới, không biết ai đang lắng nghe hay để làm gì.
    AgentLoop agent(llm, tool_registry, skill_loader, loop_detector, task.max_steps);
    agent.setStepHook([&trajectory](const Step& step) {
        trajectory.steps.push_back(step);
        trajectory.total_tokens += step.tokens_used;
    });

    auto t0 = std::chrono::steady_clock::now();
    try {
        agent.run(task.instruction);
    } catch (const std::exception& e) {
        // Task lỗi không được làm sập cả batch: ghi nhận lỗi, dọn env, trả về luôn.
        result.error = std::string("Lỗi khi chạy agent: ") + e.what();
        try { env->teardown(); } catch (...) {}
        return result;
    }
    auto t1 = std::chrono::steady_clock::now();
    trajectory.total_time_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

    // Chấm điểm: với FunctionalEvaluator, eval_script (vd "test -f result.txt")
    // cần chạy đúng bên trong thư mục làm việc của task, không phải cwd của
    // tiến trình HarnessRunner -> tạm cd vào rồi cd lại, chỉ áp dụng khi
    // Environment là NativeEnvironment (chạy thật trên host).
    std::shared_ptr<Evaluator> evaluator;
    double score = 0.0;
    try {
        evaluator = makeEvaluator(task);

        auto native_env = std::dynamic_pointer_cast<NativeEnvironment>(env);
        if (native_env && task.eval_type == "functional") {
            fs::path prev_path = fs::current_path();
            fs::current_path(native_env->getWorkingDir());
            score = evaluator->evaluate(trajectory);
            fs::current_path(prev_path);
        } else {
            score = evaluator->evaluate(trajectory);
        }
    } catch (const std::exception& e) {
        result.error = std::string("Lỗi khi chấm điểm: ") + e.what();
        try { env->teardown(); } catch (...) {}
        return result;
    }

    trajectory.success = score >= success_threshold;

    result.score = score;
    result.success = trajectory.success;
    result.total_time_ms = trajectory.total_time_ms;
    result.total_tokens = trajectory.total_tokens;

    try {
        exportTrajectory(trajectory);
        result.trajectory_path =
            (fs::path(output_dir) / ("trajectory_" + task.id + ".json")).string();
    } catch (const std::exception& e) {
        // Không ghi được file JSON không có nghĩa là task thất bại, chỉ log cảnh báo.
        std::cerr << "Cảnh báo [HarnessRunner]: " << e.what() << "\n";
    }

    try {
        env->teardown();
    } catch (const std::exception& e) {
        std::cerr << "Cảnh báo [HarnessRunner]: lỗi teardown môi trường task '"
                  << task.id << "': " << e.what() << "\n";
    }

    return result;
}

std::vector<TaskResult> HarnessRunner::runBatch(const std::string& tasks_json_path) {
    std::vector<TaskDefinition> tasks = loadTasks(tasks_json_path);
    std::vector<TaskResult> results;
    results.reserve(tasks.size());

    for (const auto& task : tasks) {
        std::cout << "==> Đang chạy task [" << task.id << "]: " << task.description << "\n";
        TaskResult r = runTask(task);

        if (r.error) {
            std::cout << "    THẤT BẠI (lỗi): " << *r.error << "\n";
        } else {
            std::cout << "    " << (r.success ? "PASS" : "FAIL")
                      << " | score=" << r.score
                      << " | time=" << r.total_time_ms << "ms"
                      << " | tokens=" << r.total_tokens << "\n";
        }

        results.push_back(std::move(r));
    }

    printReport(results);
    return results;
}

void HarnessRunner::printReport(const std::vector<TaskResult>& results) const {
    int total = static_cast<int>(results.size());
    int passed = 0;
    int errored = 0;
    long long sum_time_ms = 0;

    for (const auto& r : results) {
        if (r.error) {
            ++errored;
        } else if (r.success) {
            ++passed;
        }
        sum_time_ms += r.total_time_ms;
    }

    double success_rate = total > 0 ? (100.0 * passed / total) : 0.0;

    std::cout << "\n================ BÁO CÁO BENCHMARK ================\n";
    std::cout << "Tổng số task     : " << total << "\n";
    std::cout << "Pass              : " << passed << "\n";
    std::cout << "Fail              : " << (total - passed - errored) << "\n";
    std::cout << "Lỗi (không chạy được): " << errored << "\n";
    std::cout << "Success rate      : " << success_rate << "%\n";
    if (total > 0) {
        std::cout << "Thời gian TB/task : " << (sum_time_ms / total) << " ms\n";
    }
    std::cout << "=====================================================\n";
}