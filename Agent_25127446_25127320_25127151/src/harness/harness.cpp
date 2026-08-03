#include "harness.h"

#include "Native_Environment.h"

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
#include<unordered_set>
using json = nlohmann::json;
namespace fs = std::filesystem;
class CurrentPathGuard {
private:
    fs::path old_path;

public:
    explicit CurrentPathGuard(const fs::path& new_path)
        : old_path(fs::current_path())
    {
        fs::current_path(new_path);
    }

    ~CurrentPathGuard()
    {
        std::error_code error;
        fs::current_path(old_path, error);
    }
};
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
std::vector<TaskDefinition> HarnessRunner::loadTasks(
    const std::string& tasks_json_path
) const
{
    std::ifstream file(tasks_json_path);

    if (!file.is_open())
    {
        throw std::runtime_error(
            "Loi [HarnessRunner]: Khong mo duoc file benchmark: "
            + tasks_json_path
        );
    }

    json task_list;

    try
    {
        file >> task_list;
    }
    catch (const json::parse_error& error)
    {
        throw std::runtime_error(
            "Loi [HarnessRunner]: File tasks.json khong hop le: "
            + std::string(error.what())
        );
    }

    if (!task_list.is_array())
    {
        throw std::runtime_error(
            "Loi [HarnessRunner]: tasks.json phai la mot mang."
        );
    }

    if (task_list.empty())
    {
        throw std::runtime_error(
            "Loi [HarnessRunner]: tasks.json khong co task nao."
        );
    }

    std::vector<TaskDefinition> tasks;
    std::unordered_set<std::string> used_ids;

    tasks.reserve(task_list.size());

    for (std::size_t index = 0;
         index < task_list.size();
         ++index)
    {
        const json& item = task_list[index];

        if (!item.is_object())
        {
            throw std::runtime_error(
                "Loi [HarnessRunner]: Task tai vi tri "
                + std::to_string(index)
                + " phai la mot object."
            );
        }

        TaskDefinition task;

        try
        {
            task.id = item.value("id", "");
            task.description = item.value("description", "");
            task.instruction = item.value("instruction", "");
            task.eval_type = item.value("eval_type", "");
            task.eval_script = item.value("eval_script", "");
            task.max_steps = item.value("max_steps", 10);
        }
        catch (const json::type_error& error)
        {
            throw std::runtime_error(
                "Loi [HarnessRunner]: Sai kieu du lieu tai task thu "
                + std::to_string(index)
                + ": "
                + std::string(error.what())
            );
        }

        // 1. Kiểm tra ID
        if (task.id.empty())
        {
            throw std::runtime_error(
                "Loi [HarnessRunner]: Task thu "
                + std::to_string(index)
                + " thieu id."
            );
        }

        // ID được dùng làm tên workspace nên không được chứa đường dẫn.
        if (task.id.find("..") != std::string::npos ||
            task.id.find('/') != std::string::npos ||
            task.id.find('\\') != std::string::npos)
        {
            throw std::runtime_error(
                "Loi [HarnessRunner]: Task id khong hop le: '"
                + task.id + "'."
            );
        }

        // Không cho phép hai task trùng ID.
        if (!used_ids.insert(task.id).second)
        {
            throw std::runtime_error(
                "Loi [HarnessRunner]: Trung task id: '"
                + task.id + "'."
            );
        }

        // 2. Kiểm tra instruction
        if (task.instruction.empty())
        {
            throw std::runtime_error(
                "Loi [HarnessRunner]: Task '"
                + task.id
                + "' thieu instruction."
            );
        }

        // 3. Kiểm tra max_steps
        if (task.max_steps <= 0)
        {
            throw std::runtime_error(
                "Loi [HarnessRunner]: Task '"
                + task.id
                + "' co max_steps phai lon hon 0."
            );
        }

        // 4. Kiểm tra eval_type
        if (task.eval_type != "keyword" &&
            task.eval_type != "functional")
        {
            throw std::runtime_error(
                "Loi [HarnessRunner]: Task '"
                + task.id
                + "' co eval_type khong duoc ho tro: '"
                + task.eval_type + "'."
            );
        }

        // 5. Validation cho KeywordEvaluator
        if (task.eval_type == "keyword")
        {
            if (!item.contains("eval_keywords") ||
                !item["eval_keywords"].is_array())
            {
                throw std::runtime_error(
                    "Loi [HarnessRunner]: Task '"
                    + task.id
                    + "' thieu mang eval_keywords."
                );
            }

            for (const json& keyword : item["eval_keywords"])
            {
                if (!keyword.is_string())
                {
                    throw std::runtime_error(
                        "Loi [HarnessRunner]: Task '"
                        + task.id
                        + "' co keyword khong phai chuoi."
                    );
                }

                std::string value = keyword.get<std::string>();

                if (value.empty())
                {
                    throw std::runtime_error(
                        "Loi [HarnessRunner]: Task '"
                        + task.id
                        + "' co keyword rong."
                    );
                }

                task.eval_keywords.push_back(std::move(value));
            }

            if (task.eval_keywords.empty())
            {
                throw std::runtime_error(
                    "Loi [HarnessRunner]: Task '"
                    + task.id
                    + "' phai co it nhat mot keyword."
                );
            }
        }

        // 6. Validation cho FunctionalEvaluator
        if (task.eval_type == "functional" &&
            task.eval_script.empty())
        {
            throw std::runtime_error(
                "Loi [HarnessRunner]: Task '"
                + task.id
                + "' thieu eval_script."
            );
        }

        tasks.push_back(std::move(task));
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
TaskResult HarnessRunner::runTask(const TaskDefinition& task)
{
    TaskResult result;
    result.task_id = task.id;

    auto env = makeEnvironment(task);

    // 1. Chuẩn bị workspace sạch cho task
    try {
        env->setup();
    }
    catch (const std::exception& e) {
        result.error =
            std::string("Loi setup moi truong: ") + e.what();

        return result;
    }

    // 2. Khởi tạo trajectory
    Trajectory trajectory;
    trajectory.task_id = task.id;
    trajectory.model =
        llm ? llm->getModelName() : "unknown";

    // 3. Khởi tạo AgentLoop theo max_steps của task
    AgentLoop agent(
        llm,
        tool_registry,
        skill_loader,
        loop_detector,
        task.max_steps
    );

    // Harness thu thập từng bước thông qua step hook
    agent.setStepHook(
        [&trajectory](const Step& step)
        {
            trajectory.steps.push_back(step);
            trajectory.total_tokens += step.tokens_used;
        }
    );

    auto start_time = std::chrono::steady_clock::now();

    // 4. Chạy agent trong workspace riêng của task
    try {
        auto native_env =
            std::dynamic_pointer_cast<NativeEnvironment>(env);

        if (native_env) {
            CurrentPathGuard path_guard(
                native_env->getWorkingDir()
            );

            agent.run(task.instruction);
        }
        else {
            agent.run(task.instruction);
        }
    }
    catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();

        trajectory.total_time_ms = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time
            ).count()
        );

        trajectory.success = false;

        result.success = false;
        result.total_time_ms = trajectory.total_time_ms;
        result.total_tokens = trajectory.total_tokens;
        result.error =
            std::string("Loi khi chay agent: ") + e.what();

        // Task lỗi vẫn phải xuất trajectory
        try {
            exportTrajectory(trajectory);

            result.trajectory_path =
                (
                    fs::path(output_dir) /
                    ("trajectory_" + task.id + ".json")
                ).string();
        }
        catch (const std::exception& export_error) {
            std::cerr
                << "Canh bao [HarnessRunner]: "
                << export_error.what()
                << "\n";
        }

        try {
            env->teardown();
        }
        catch (...) {
        }

        return result;
    }

    auto end_time = std::chrono::steady_clock::now();

    trajectory.total_time_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        ).count()
    );

    // 5. Chấm điểm task
    double score = 0.0;

    try {
        std::shared_ptr<Evaluator> evaluator =
            makeEvaluator(task);

        auto native_env =
            std::dynamic_pointer_cast<NativeEnvironment>(env);

        if (native_env && task.eval_type == "functional") {
            CurrentPathGuard path_guard(
                native_env->getWorkingDir()
            );

            score = evaluator->evaluate(trajectory);
        }
        else {
            score = evaluator->evaluate(trajectory);
        }
    }
    catch (const std::exception& e) {
        trajectory.success = false;

        result.success = false;
        result.total_time_ms = trajectory.total_time_ms;
        result.total_tokens = trajectory.total_tokens;
        result.error =
            std::string("Loi khi cham diem: ") + e.what();

        // Lỗi evaluator vẫn xuất trajectory
        try {
            exportTrajectory(trajectory);

            result.trajectory_path =
                (
                    fs::path(output_dir) /
                    ("trajectory_" + task.id + ".json")
                ).string();
        }
        catch (const std::exception& export_error) {
            std::cerr
                << "Canh bao [HarnessRunner]: "
                << export_error.what()
                << "\n";
        }

        try {
            env->teardown();
        }
        catch (...) {
        }

        return result;
    }

    // 6. Ghi kết quả
    trajectory.success =
        score >= success_threshold;

    result.score = score;
    result.success = trajectory.success;
    result.total_time_ms = trajectory.total_time_ms;
    result.total_tokens = trajectory.total_tokens;

    // 7. Xuất trajectory JSON
    try {
        exportTrajectory(trajectory);

        result.trajectory_path =
            (
                fs::path(output_dir) /
                ("trajectory_" + task.id + ".json")
            ).string();
    }
    catch (const std::exception& e) {
        std::cerr
            << "Canh bao [HarnessRunner]: "
            << e.what()
            << "\n";
    }

    // 8. Dọn môi trường
    try {
        env->teardown();
    }
    catch (const std::exception& e) {
        std::cerr
            << "Canh bao [HarnessRunner]: loi teardown task '"
            << task.id
            << "': "
            << e.what()
            << "\n";
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
TaskResult HarnessRunner::runSingleTask(
    const std::string& tasks_json_path,
    const std::string& task_id
)
{
    const std::vector<TaskDefinition> tasks =
        loadTasks(tasks_json_path);

    for (const TaskDefinition& task : tasks)
    {
        if (task.id != task_id)
        {
            continue;
        }

        std::cout << "Dang chay task: " << task.id << '\n';

        TaskResult result = runTask(task);

        std::cout << "Ket qua: "
                  << (result.success ? "PASS" : "FAIL")
                  << '\n';

        std::cout << "Xem trajectory chi tiet trong thu muc results.\n";

        return result;
    }

    throw std::runtime_error(
        "Khong tim thay task co id: " + task_id
    );
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