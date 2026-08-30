#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../client/llm_client.h"
#include "environnment.h"
#include "evaluator.h"
#include "multi_agent_coordinator.h"
#include "trajectory.h"

class ToolRegistry;
class SkillLoader;
class LoopDetector;

// Mô tả 1 task đọc từ benchmark/tasks.json (mục 7.2 đề bài)
struct TaskDefinition {
  std::string id;
  std::string description;
  std::string instruction;
  std::string eval_type;                  // "keyword" | "functional"
  std::string eval_script;                // dùng khi eval_type == "functional"
  std::vector<std::string> eval_keywords; // dùng khi eval_type == "keyword"
  int max_steps = 10;
};

// Kết quả tổng hợp sau khi chạy + chấm điểm 1 task, dùng để in báo cáo batch
struct TaskResult {
  std::string task_id;
  std::string eval_type;
  bool success = false;
  double score = 0.0;
  long long total_time_ms = 0;
  long long total_tokens = 0;
  std::string trajectory_path;
  std::optional<std::string>
      error; // có giá trị nếu task bị lỗi khi chạy (không chấm điểm được)
};

// HarnessRunner: "nhạc trưởng" điều phối toàn bộ pipeline benchmark.
// - KHÔNG chứa logic suy luận của agent (đó là việc của AgentLoop)
// - KHÔNG chứa logic chấm điểm (đó là việc của Evaluator)
// - Chỉ NỐI các layer lại: Environment -> AgentLoop -> Trajectory -> Evaluator
// -> file JSON Đúng nguyên tắc "AgentLoop KHÔNG biết Harness tồn tại" (mục 4.4
// đề bài): HarnessRunner tiêm (inject) step_hook vào AgentLoop từ bên ngoài
// (Observer/Hook pattern), AgentLoop chỉ gọi hook đó ra mà không biết ai đang
// lắng nghe.
class HarnessRunner {
private:
  std::shared_ptr<LLMClient> llm;
  std::shared_ptr<ToolRegistry> tool_registry;
  std::shared_ptr<SkillLoader> skill_loader;
  std::shared_ptr<LoopDetector> loop_detector;

  std::string output_dir;     // nơi ghi trajectory_{task_id}.json
  std::string workspace_root; // nơi mỗi task có 1 thư mục làm việc riêng
                              // (NativeEnvironment)
  double success_threshold; // score >= ngưỡng này thì coi task là "success"
  void exportBenchmarkSummary(const std::vector<TaskResult> &results) const;
  std::vector<TaskDefinition>
  loadTasks(const std::string &tasks_json_path) const;
  std::shared_ptr<Evaluator> makeEvaluator(const TaskDefinition &task) const;
  std::shared_ptr<Environment>
  makeEnvironment(const TaskDefinition &task) const;
  void exportTrajectory(const Trajectory &trajectory) const;

public:
  HarnessRunner(std::shared_ptr<LLMClient> llm,
                std::shared_ptr<ToolRegistry> tool_registry,
                std::shared_ptr<SkillLoader> skill_loader,
                std::shared_ptr<LoopDetector> loop_detector,
                std::string output_dir = "results/",
                std::string workspace_root = "workspace/",
                double success_threshold = 1.0);

  // C++26 — deleted functions with diagnostic messages (P2573R2).
  // HarnessRunner giữ trạng thái của một lần chạy benchmark, vì vậy không
  // được sao chép vô tình. Di chuyển vẫn được phép để chuyển ownership.
  HarnessRunner(const HarnessRunner &) =
      delete("HarnessRunner owns benchmark execution state and must not be copied");
  HarnessRunner &operator=(const HarnessRunner &) =
      delete("HarnessRunner owns benchmark execution state and must not be copied");
  HarnessRunner(HarnessRunner &&) noexcept = default;
  HarnessRunner &operator=(HarnessRunner &&) noexcept = default;

  TaskResult runTask(const TaskDefinition &task);

  TaskResult runSingleTask(const std::string &tasks_json_path,
                           const std::string &task_id);
  // Chạy một hoặc nhiều file task trong cùng một batch. Overload vector nạp
  // toàn bộ task trước, dọn output đúng một lần và xuất một summary hợp nhất.
  // Mỗi task vẫn được cô lập lỗi để một task lỗi không làm sập cả batch.
  std::vector<TaskResult> runBatch(const std::string &tasks_json_path);
  std::vector<TaskResult>
  runBatch(const std::vector<std::string> &tasks_json_paths);

  // In báo cáo tổng hợp: success rate tổng, breakdown theo eval_type
  // (mục 7.3/VIII đề bài)
  void printReport(const std::vector<TaskResult> &results) const;

  // =========================================================================
  // 10.3 Multi-agent Coordination
  // =========================================================================
  // Phân tách task phức tạp thành các SubTaskDefinition, spawn mỗi subtask
  // vào một thread riêng (std::jthread C++20), gộp kết quả cuối.
  //
  // subtasks: danh sách subtask đã được phân chia sẵn (do caller chuẩn bị
  //           hoặc do LLM phân tích từ task gốc).
  // Trả về: chuỗi tổng hợp kết quả của tất cả sub-agent.
  std::string runMultiAgent(const std::vector<SubTaskDefinition>& subtasks);

  // Tiện ích: tự động phân chia 1 task thành N subtask đơn giản
  // bằng cách chia đều hoặc theo dấu phân cách '\n'.
  // Hữu ích khi demo "task phức tạp được chia cho 2 agent chạy song song".
  static std::vector<SubTaskDefinition>
  splitTaskIntoSubtasks(const std::string& combined_instruction,
                        int num_agents = 2);
};
