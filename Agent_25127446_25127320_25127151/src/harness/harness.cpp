#include "harness.h"

#include "Native_Environment.h"

#include "../agent/agent_loop.h"
#include "../agent/loop_detector.h"
#include "../agent/skill_loader.h"
#include "../tools/tool_registry.h"
#include "functional_evaluator.h"
#include "keyword_evaluator.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
using json = nlohmann::json;
namespace fs = std::filesystem;

namespace
{
TerminationStatus toTrajectoryStatus(AgentTerminationStatus status)
{
  switch (status) {
  case AgentTerminationStatus::Completed:
    return TerminationStatus::Completed;
  case AgentTerminationStatus::LoopDetected:
    return TerminationStatus::LoopDetected;
  case AgentTerminationStatus::MaxStepsReached:
    return TerminationStatus::MaxStepsReached;
  }
  return TerminationStatus::Unknown;
}
}

class CurrentPathGuard {
private:
  fs::path old_path;

public:
  explicit CurrentPathGuard(const fs::path &new_path)
      : old_path(fs::current_path()) {
    fs::current_path(new_path);
  }

  ~CurrentPathGuard() {
    std::error_code error;
    fs::current_path(old_path, error);
  }
};
HarnessRunner::HarnessRunner(std::shared_ptr<LLMClient> llm,
                             std::shared_ptr<ToolRegistry> tool_registry,
                             std::shared_ptr<SkillLoader> skill_loader,
                             std::shared_ptr<LoopDetector> loop_detector,
                             std::string output_dir, std::string workspace_root,
                             double success_threshold)
    : llm(std::move(llm)), tool_registry(std::move(tool_registry)),
      skill_loader(std::move(skill_loader)),
      loop_detector(std::move(loop_detector)),
      output_dir(std::move(output_dir)),
      workspace_root(std::move(workspace_root)),
      success_threshold(success_threshold) {}

// ---------------------------------------------------------------------------
// 7.2 Task Definition Format: đọc benchmark/tasks.json
// ---------------------------------------------------------------------------
std::vector<TaskDefinition>
HarnessRunner::loadTasks(const std::string &tasks_json_path) const {
  std::ifstream file(tasks_json_path);

  if (!file.is_open()) {
    throw std::runtime_error(
        "Loi [HarnessRunner]: Khong mo duoc file benchmark: " +
        tasks_json_path);
  }

  json task_list;

  try {
    file >> task_list;
  } catch (const json::parse_error &error) {
    throw std::runtime_error(
        "Loi [HarnessRunner]: File tasks.json khong hop le: " +
        std::string(error.what()));
  }

  if (!task_list.is_array()) {
    throw std::runtime_error(
        "Loi [HarnessRunner]: tasks.json phai la mot mang.");
  }

  if (task_list.empty()) {
    throw std::runtime_error(
        "Loi [HarnessRunner]: tasks.json khong co task nao.");
  }

  std::vector<TaskDefinition> tasks;
  std::unordered_set<std::string> used_ids;

  tasks.reserve(task_list.size());

  for (std::size_t index = 0; index < task_list.size(); ++index) {
    const json &item = task_list[index];

    if (!item.is_object()) {
      throw std::runtime_error("Loi [HarnessRunner]: Task tai vi tri " +
                               std::to_string(index) + " phai la mot object.");
    }

    TaskDefinition task;

    try {
      task.id = item.value("id", "");
      task.description = item.value("description", "");
      task.instruction = item.value("instruction", "");
      task.eval_type = item.value("eval_type", "");
      task.eval_script = item.value("eval_script", "");
      task.max_steps = item.value("max_steps", 10);
    } catch (const json::type_error &error) {
      throw std::runtime_error(
          "Loi [HarnessRunner]: Sai kieu du lieu tai task thu " +
          std::to_string(index) + ": " + std::string(error.what()));
    }

    // 1. Kiểm tra ID
    if (task.id.empty()) {
      throw std::runtime_error("Loi [HarnessRunner]: Task thu " +
                               std::to_string(index) + " thieu id.");
    }

    // ID được dùng làm tên workspace nên không được chứa đường dẫn.
    if (task.id.find("..") != std::string::npos ||
        task.id.find('/') != std::string::npos ||
        task.id.find('\\') != std::string::npos) {
      throw std::runtime_error("Loi [HarnessRunner]: Task id khong hop le: '" +
                               task.id + "'.");
    }

    // Không cho phép hai task trùng ID.
    if (!used_ids.insert(task.id).second) {
      throw std::runtime_error("Loi [HarnessRunner]: Trung task id: '" +
                               task.id + "'.");
    }

    // 2. Kiểm tra instruction
    if (task.instruction.empty()) {
      throw std::runtime_error("Loi [HarnessRunner]: Task '" + task.id +
                               "' thieu instruction.");
    }

    // 3. Kiểm tra max_steps
    if (task.max_steps <= 0) {
      throw std::runtime_error("Loi [HarnessRunner]: Task '" + task.id +
                               "' co max_steps phai lon hon 0.");
    }

    // 4. Kiểm tra eval_type
    if (task.eval_type != "keyword" && task.eval_type != "functional") {
      throw std::runtime_error("Loi [HarnessRunner]: Task '" + task.id +
                               "' co eval_type khong duoc ho tro: '" +
                               task.eval_type + "'.");
    }

    // 5. Validation cho KeywordEvaluator
    if (task.eval_type == "keyword") {
      if (!item.contains("eval_keywords") ||
          !item["eval_keywords"].is_array()) {
        throw std::runtime_error("Loi [HarnessRunner]: Task '" + task.id +
                                 "' thieu mang eval_keywords.");
      }

      for (const json &keyword : item["eval_keywords"]) {
        if (!keyword.is_string()) {
          throw std::runtime_error("Loi [HarnessRunner]: Task '" + task.id +
                                   "' co keyword khong phai chuoi.");
        }

        std::string value = keyword.get<std::string>();

        if (value.empty()) {
          throw std::runtime_error("Loi [HarnessRunner]: Task '" + task.id +
                                   "' co keyword rong.");
        }

        task.eval_keywords.push_back(std::move(value));
      }

      if (task.eval_keywords.empty()) {
        throw std::runtime_error("Loi [HarnessRunner]: Task '" + task.id +
                                 "' phai co it nhat mot keyword.");
      }
    }

    // 6. Validation cho FunctionalEvaluator
    if (task.eval_type == "functional" && task.eval_script.empty()) {
      throw std::runtime_error("Loi [HarnessRunner]: Task '" + task.id +
                               "' thieu eval_script.");
    }

    tasks.push_back(std::move(task));
  }

  return tasks;
}

// ---------------------------------------------------------------------------
// Strategy pattern: chọn Evaluator theo eval_type, AgentLoop/Environment không
// cần biết Evaluator nào đang được dùng.
// ---------------------------------------------------------------------------
std::shared_ptr<Evaluator>
HarnessRunner::makeEvaluator(const TaskDefinition &task) const {
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

  throw std::runtime_error("Lỗi [HarnessRunner]: eval_type không hỗ trợ: '" +
                           task.eval_type + "' (task '" + task.id + "')");
}

// Mỗi task chạy trong 1 thư mục làm việc riêng (NativeEnvironment) để không
// dẫm chân lên kết quả của task khác khi chạy batch.
std::shared_ptr<Environment>
HarnessRunner::makeEnvironment(const TaskDefinition &task) const {
  std::string task_workdir = (fs::path(workspace_root) / task.id).string();
  return std::make_shared<NativeEnvironment>(task_workdir);
}

void HarnessRunner::exportTrajectory(
    const Trajectory& trajectory
) const
{
    fs::create_directories(output_dir);

    const fs::path out_path =
        fs::path(output_dir) /
        ("trajectory_" + trajectory.task_id + ".json");

    std::ofstream out(out_path);

    if (!out.is_open())
    {
        throw std::runtime_error(
            "Loi [HarnessRunner]: Khong ghi duoc file trajectory: " +
            out_path.string()
        );
    }

    out << toJson(trajectory).dump(4);
}

// ---------------------------------------------------------------------------
// Chạy 1 task: setup env -> chạy AgentLoop (qua step_hook thu thập Trajectory)
// -> teardown env -> chấm điểm -> ghi file JSON.
// ---------------------------------------------------------------------------
TaskResult HarnessRunner::runTask(
    const TaskDefinition& task
)
{
    using Clock = std::chrono::steady_clock;

    TaskResult result;
    result.task_id = task.id;

    const fs::path trajectory_path =
        fs::path(output_dir) /
        ("trajectory_" + task.id + ".json");

    result.trajectory_path =
        trajectory_path.string();

    Trajectory trajectory;
    trajectory.task_id = task.id;
    trajectory.model =
        llm ? llm->getModelName() : "unknown";

    trajectory.success = false;
    trajectory.termination_status =
        TerminationStatus::Unknown;

    const auto start_time = Clock::now();

    auto updateElapsedTime =
        [&]()
        {
            const auto end_time = Clock::now();

            trajectory.total_time_ms =
                std::chrono::duration_cast<
                    std::chrono::milliseconds
                >(end_time - start_time)
                    .count();

            result.total_time_ms =
                trajectory.total_time_ms;

            result.total_tokens =
                trajectory.total_tokens;
        };

    auto safelyExportTrajectory =
        [&]()
        {
            try
            {
                exportTrajectory(trajectory);
            }
            catch (const std::exception& error)
            {
                std::cerr
                    << "Canh bao [HarnessRunner]: "
                    << error.what()
                    << '\n';
            }
        };

    /*
     * 1. Tạo Environment
     */
    std::shared_ptr<Environment> env;

    try
    {
        env = makeEnvironment(task);
    }
    catch (const std::exception& error)
    {
        trajectory.termination_status =
            TerminationStatus::EnvironmentError;

        trajectory.error_message =
            std::string("Loi tao moi truong: ") +
            error.what();

        result.error =
            trajectory.error_message;

        updateElapsedTime();
        safelyExportTrajectory();

        return result;
    }

    /*
     * 2. Setup workspace
     */
    try
    {
        env->setup();
    }
    catch (const std::exception& error)
    {
        trajectory.termination_status =
            TerminationStatus::EnvironmentError;

        trajectory.error_message =
            std::string("Loi setup moi truong: ") +
            error.what();

        result.error =
            trajectory.error_message;

        updateElapsedTime();
        safelyExportTrajectory();

        return result;
    }

    /*
     * 3. Tạo AgentLoop
     */
    AgentLoop agent(
        llm,
        tool_registry,
        skill_loader,
        loop_detector,
        task.max_steps
    );

    /*
     * Observer/Hook:
     * Harness chỉ nhận Step, không can thiệp logic Agent.
     */
    agent.setStepHook(
        [&trajectory](const Step& step)
        {
            trajectory.steps.push_back(step);

            trajectory.total_tokens +=
                step.tokens_used;
        }
    );

    /*
     * 4. Chạy Agent
     */
    try
    {
        AgentRunResult agent_result;
        auto native_env =
            std::dynamic_pointer_cast<
                NativeEnvironment
            >(env);

        if (native_env)
        {
            CurrentPathGuard path_guard(
                native_env->getWorkingDir()
            );

            agent_result = agent.run(task.instruction);
        }
        else
        {
            agent_result = agent.run(task.instruction);
        }

        trajectory.final_answer = agent_result.final_answer;
        trajectory.total_tokens = agent_result.total_tokens;
        trajectory.termination_status =
            toTrajectoryStatus(agent_result.status);
    }
    catch (const std::exception& error)
    {
        trajectory.success = false;

        trajectory.termination_status =
            TerminationStatus::AgentError;

        trajectory.error_message =
            std::string("Loi khi chay agent: ") +
            error.what();

        result.success = false;
        result.score = 0.0;
        result.error =
            trajectory.error_message;

        updateElapsedTime();
        safelyExportTrajectory();

        try
        {
            env->teardown();
        }
        catch (const std::exception& teardown_error)
        {
            std::cerr
                << "Canh bao [HarnessRunner]: "
                << "loi teardown task '"
                << task.id
                << "': "
                << teardown_error.what()
                << '\n';
        }

        return result;
    }

    /*
     * 5. Chấm điểm
     */
    try
    {
        std::shared_ptr<Evaluator> evaluator =
            makeEvaluator(task);

        auto native_env =
            std::dynamic_pointer_cast<
                NativeEnvironment
            >(env);

        if (
            native_env &&
            task.eval_type == "functional"
        )
        {
            CurrentPathGuard path_guard(
                native_env->getWorkingDir()
            );

            result.score =
                evaluator->evaluate(trajectory);
        }
        else
        {
            result.score =
                evaluator->evaluate(trajectory);
        }

        result.success =
            result.score >= success_threshold &&
            trajectory.termination_status == TerminationStatus::Completed;

        trajectory.success =
            result.success;
    }
    catch (const std::exception& error)
    {
        trajectory.success = false;

        trajectory.termination_status =
            TerminationStatus::EvaluationError;

        trajectory.error_message =
            std::string("Loi khi cham diem: ") +
            error.what();

        result.success = false;
        result.score = 0.0;
        result.error =
            trajectory.error_message;

        updateElapsedTime();
        safelyExportTrajectory();

        try
        {
            env->teardown();
        }
        catch (const std::exception& teardown_error)
        {
            std::cerr
                << "Canh bao [HarnessRunner]: "
                << "loi teardown task '"
                << task.id
                << "': "
                << teardown_error.what()
                << '\n';
        }

        return result;
    }

    /*
     * 6. Ghi thời gian và token
     */
    updateElapsedTime();

    /*
     * 7. Xuất trajectory
     */
    try
    {
        exportTrajectory(trajectory);
    }
    catch (const std::exception& error)
    {
        result.success = false;

        result.error =
            std::string(
                "Loi khi xuat trajectory: "
            ) +
            error.what();

        std::cerr
            << "Canh bao [HarnessRunner]: "
            << error.what()
            << '\n';
    }

    /*
     * 8. Teardown environment
     */
    try
    {
        env->teardown();
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "Canh bao [HarnessRunner]: "
            << "loi teardown task '"
            << task.id
            << "': "
            << error.what()
            << '\n';
    }

    return result;
}

std::vector<TaskResult>
HarnessRunner::runBatch(const std::string &tasks_json_path) {
  // Chi xoa trajectory cu; giu nguyen cac file ket qua khac.
  fs::create_directories(output_dir);

  for (const auto &entry : fs::directory_iterator(output_dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    const fs::path &path = entry.path();
    const std::string filename = path.filename().string();

    if (filename.rfind("trajectory_", 0) == 0 && path.extension() == ".json") {
      fs::remove(path);
    }
  }

  std::vector<TaskDefinition> tasks = loadTasks(tasks_json_path);
  std::vector<TaskResult> results;
  results.reserve(tasks.size());

  for (const auto &task : tasks) {
    std::cout << "==> Đang chạy task [" << task.id << "]: " << task.description
              << "\n";
    TaskResult r = runTask(task);

    if (r.error) {
      std::cout << "    THẤT BẠI (lỗi): " << *r.error << "\n";
    } else {
      std::cout << "    " << (r.success ? "PASS" : "FAIL")
                << " | score=" << r.score << " | time=" << r.total_time_ms
                << "ms"
                << " | tokens=" << r.total_tokens << "\n";
    }

    results.push_back(std::move(r));
  }

  printReport(results);

try
{
    exportBenchmarkSummary(results);

    std::cout
        << "Da xuat bao cao JSON: "
        << (
            fs::path(output_dir) /
            "benchmark_summary.json"
        ).string()
        << "\n";
}
catch (const std::exception& error)
{
    std::cerr
        << "Canh bao [HarnessRunner]: "
        << error.what()
        << "\n";
}

return results;
}

TaskResult HarnessRunner::runSingleTask(
    const std::string& tasks_json_path,
    const std::string& task_id
)
{
    fs::create_directories(output_dir);

    fs::remove(
        fs::path(output_dir) /
        ("trajectory_" + task_id + ".json")
    );

    const std::vector<TaskDefinition> tasks =
        loadTasks(tasks_json_path);

    for (const TaskDefinition& task : tasks)
    {
        if (task.id != task_id)
        {
            continue;
        }

        std::cout
            << "Dang chay task: "
            << task.id
            << " - "
            << task.description
            << '\n';

        TaskResult result = runTask(task);

        std::cout
            << "\n========== KET QUA TASK ==========\n";

        std::cout
            << "Task ID    : "
            << result.task_id
            << '\n';

        std::cout
            << "Trang thai : "
            << (result.success ? "PASS" : "FAIL")
            << '\n';

        std::cout
            << "Score      : "
            << result.score
            << '\n';

        std::cout
            << "Thoi gian  : "
            << result.total_time_ms
            << " ms\n";

        std::cout
            << "Tokens     : "
            << result.total_tokens
            << '\n';

        if (!result.trajectory_path.empty())
        {
            std::cout
                << "Trajectory : "
                << result.trajectory_path
                << '\n';
        }

        if (result.error.has_value())
        {
            std::cout
                << "Loi        : "
                << result.error.value()
                << '\n';
        }

        std::cout
            << "==================================\n";

        return result;
    }

    throw std::runtime_error(
        "Khong tim thay task co id: " + task_id
    );
}
void HarnessRunner::printReport(const std::vector<TaskResult> &results) const {
  int total = static_cast<int>(results.size());
  int passed = 0;
  int errored = 0;
  long long sum_time_ms = 0;

  for (const auto &r : results) {
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
void HarnessRunner::exportBenchmarkSummary(
    const std::vector<TaskResult> &results) const {
  fs::create_directories(output_dir);

  int passed = 0;
  int failed = 0;
  int errored = 0;

  long long total_time_ms = 0;
  long long total_tokens = 0;
  double total_score = 0.0;

  json result_list = json::array();

  for (const TaskResult &result : results) {
    if (result.error.has_value()) {
      ++errored;
    } else if (result.success) {
      ++passed;
    } else {
      ++failed;
    }

    total_time_ms += result.total_time_ms;
    total_tokens += result.total_tokens;
    total_score += result.score;

    json item;

    item["task_id"] = result.task_id;
    item["success"] = result.success;
    item["score"] = result.score;
    item["total_time_ms"] = result.total_time_ms;
    item["total_tokens"] = result.total_tokens;
    item["trajectory_path"] = result.trajectory_path;

    if (result.error.has_value()) {
      item["error"] = result.error.value();
    } else {
      item["error"] = nullptr;
    }

    result_list.push_back(item);
  }

  const int total_tasks = static_cast<int>(results.size());

  const double success_rate = total_tasks > 0
                                  ? 100.0 * static_cast<double>(passed) /
                                        static_cast<double>(total_tasks)
                                  : 0.0;

  const double average_score =
      total_tasks > 0 ? total_score / static_cast<double>(total_tasks) : 0.0;

  const double average_time_ms = total_tasks > 0
                                     ? static_cast<double>(total_time_ms) /
                                           static_cast<double>(total_tasks)
                                     : 0.0;

  json summary;

  summary["total_tasks"] = total_tasks;
  summary["passed"] = passed;
  summary["failed"] = failed;
  summary["errored"] = errored;
  summary["success_rate"] = success_rate;
  summary["average_score"] = average_score;
  summary["total_time_ms"] = total_time_ms;
  summary["average_time_ms"] = average_time_ms;
  summary["total_tokens"] = total_tokens;
  summary["results"] = result_list;

  const fs::path output_path = fs::path(output_dir) / "benchmark_summary.json";

  std::ofstream output_file(output_path);

  if (!output_file.is_open()) {
    throw std::runtime_error(
        "Loi [HarnessRunner]: Khong ghi duoc file benchmark summary: " +
        output_path.string());
  }

  output_file << summary.dump(4);
}

// =============================================================================
// 10.3 Multi-agent Coordination — HarnessRunner integration
// =============================================================================

// ---------------------------------------------------------------------------
// runMultiAgent: spawn sub-agent cho từng subtask, chờ tất cả và gộp kết quả.
// ---------------------------------------------------------------------------
std::string HarnessRunner::runMultiAgent(
    const std::vector<SubTaskDefinition>& subtasks)
{
    if (subtasks.empty()) {
        return "(Không có subtask nào được cung cấp)";
    }

    std::cout << "\n[HarnessRunner] Bắt đầu Multi-agent Coordination với "
              << subtasks.size() << " sub-agent(s)...\n";

    // Tạo coordinator, truyền toàn bộ dependency của HarnessRunner
    MultiAgentCoordinator coordinator(
        llm,
        tool_registry,
        skill_loader,
        loop_detector
    );

    // Chạy song song tất cả subtask
    const std::vector<SubAgentResult> results = coordinator.runParallel(subtasks);

    // Xuất trajectory riêng cho từng sub-agent vào output_dir
    for (const SubAgentResult& res : results) {
        if (!res.trajectory.task_id.empty()) {
            try {
                exportTrajectory(res.trajectory);
            } catch (const std::exception& e) {
                std::cerr << "[HarnessRunner] Canh bao: khong xuat duoc trajectory cua '"
                          << res.sub_id << "': " << e.what() << "\n";
            }
        }
    }

    // In tóm tắt nhanh
    int success_count = 0;
    for (const SubAgentResult& res : results) {
        if (res.success) ++success_count;
    }

    std::cout << "[HarnessRunner] Multi-agent xong: "
              << success_count << "/" << results.size()
              << " sub-agent thành công.\n\n";

    // Gộp và trả về kết quả tổng hợp
    return MultiAgentCoordinator::mergeResults(results);
}

// ---------------------------------------------------------------------------
// splitTaskIntoSubtasks: chia 1 task thành num_agents subtask.
//
// Chiến lược:
//   - Nếu combined_instruction chứa ký tự '\n', tách theo dòng.
//   - Mỗi dòng (không rỗng) là 1 subtask.
//   - Nếu số dòng < num_agents, mỗi subtask là toàn bộ instruction + index.
// ---------------------------------------------------------------------------
std::vector<SubTaskDefinition>
HarnessRunner::splitTaskIntoSubtasks(const std::string& combined_instruction,
                                     int num_agents)
{
    std::vector<SubTaskDefinition> subtasks;

    if (combined_instruction.empty() || num_agents <= 0) {
        return subtasks;
    }

    // Tách theo dòng
    std::vector<std::string> lines;
    {
        std::istringstream iss(combined_instruction);
        std::string line;
        while (std::getline(iss, line)) {
            // Bỏ dòng trắng
            if (!line.empty() &&
                line.find_first_not_of(" \t\r\n") != std::string::npos) {
                lines.push_back(std::move(line));
            }
        }
    }

    if (lines.empty()) {
        // Fallback: 1 subtask duy nhất
        SubTaskDefinition sub;
        sub.id          = "sub_0";
        sub.instruction = combined_instruction;
        sub.max_steps   = 10;
        subtasks.push_back(std::move(sub));
        return subtasks;
    }

    // Gán mỗi dòng cho 1 sub-agent (vòng tròn nếu ít agent hơn dòng)
    // Nếu nhiều dòng hơn num_agents: ghép dồn vào agent cuối cùng
    const int actual_agents = std::min(static_cast<int>(lines.size()), num_agents);
    subtasks.resize(actual_agents);

    for (int i = 0; i < actual_agents; ++i) {
        subtasks[i].id        = "sub_" + std::to_string(i);
        subtasks[i].max_steps = 10;
    }

    // Phân phối dòng vào sub-agent
    for (std::size_t i = 0; i < lines.size(); ++i) {
        int agent_idx = static_cast<int>(i) % actual_agents;
        if (!subtasks[agent_idx].instruction.empty()) {
            subtasks[agent_idx].instruction += "\n";
        }
        subtasks[agent_idx].instruction += lines[i];
    }

    return subtasks;
}
