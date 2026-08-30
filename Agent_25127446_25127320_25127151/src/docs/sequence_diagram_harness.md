# SEQUENCE DIAGRAM — HarnessRunner chạy Batch Evaluation

Mô tả pipeline benchmark từ khi `HarnessRunner::runBatch(task_files)` được gọi
đến khi xuất `benchmark_summary.json` và trả về toàn bộ `TaskResult`.
Sơ đồ này cho thấy vai trò *Director* của HarnessRunner: nó chỉ NỐI các layer
(Environment → AgentLoop → Trajectory → Evaluator) mà không chứa logic suy luận.

---

```mermaid
sequenceDiagram
    autonumber
    participant MAIN as main.cpp / run_eval.cpp
    participant HR as HarnessRunner
    participant TASK_JSON as task.json + keyword_tasks.json
    participant ENV as Environment<br/>(NativeEnvironment)
    participant TR as ToolRegistry
    participant FS as results/ + workspace/
    participant AL as AgentLoop
    participant EVAL as Evaluator<br/>(Keyword/Functional)

    %% ────────────────────────────────────────────────────────────
    %% BATCH — STARTUP
    %% ────────────────────────────────────────────────────────────
    MAIN->>HR: runBatch({task.json, keyword_tasks.json})

    HR->>FS: create_directories(results/)
    loop Với mỗi file task
        HR->>TASK_JSON: loadTasks(tasks_json_path)
        TASK_JSON-->>HR: vector<TaskDefinition>
    end
    Note over HR,TASK_JSON: validate fields và ID trùng<br/>trên toàn bộ batch trước khi dọn output
    HR->>FS: xoá trajectory_*.json cũ đúng một lần

    %% ────────────────────────────────────────────────────────────
    %% BATCH — LOOP qua từng task
    %% ────────────────────────────────────────────────────────────
    loop Với mỗi TaskDefinition [task_001..task_020]
        MAIN->>HR: runTask(task)

        %% (1) Make environment + setup workspace
        HR->>HR: makeEnvironment(task)
        HR-->>HR: NativeEnvironment{workspace_root/task.id}
        HR->>ENV: setup()
        ENV-->>HR: ok

        opt env/setup lỗi
            HR-->>HR: TerminationStatus=EnvironmentError
            HR-->>HR: result.error=...
            HR->>FS: exportTrajectory()
            HR-->>MAIN: TaskResult{error}
        end

        %% (2) Reset tool states trong đúng workspace
        HR->>TR: resetToolStates()  [CurrentPathGuard → workspace]
        TR-->>HR: ok (Memory cleared, ...)

        %% (3) Build agent + inject observer hook
        HR->>HR: AgentLoop agent(llm, registry, skills, detector, max_steps)
        HR->>AL: setStepHook(hook)
        Note over HR,AL: Observer: hook chỉ append Step vào Trajectory

        %% (4) Chạy agent (ReAct loop)
        HR->>AL: run(task.instruction)
        Note over AL: Think → Act → Observe (xem sequence_diagram_agent.md)

        loop mỗi step hoàn thành
            AL-->>HR: step_hook(step)
            HR->>HR: trajectory.steps.push_back(step)
            HR->>HR: trajectory.total_tokens += step.tokens_used
        end

        AL-->>HR: AgentRunResult{final_answer, status, total_tokens}

        alt agent ném exception
            HR->>HR: TerminationStatus=AgentError
            HR->>HR: trajectory.error_message=...
        else agent hoàn thành
            HR->>HR: trajectory.final_answer = ans
            HR->>HR: trajectory.total_tokens = tokens
            HR->>HR: termination_status = toTrajectoryStatus(status)
        end

        %% (5) Đánh giá (Strategy)
        HR->>HR: makeEvaluator(task)
        alt eval_type == "keyword"
            HR-->>HR: KeywordEvaluator{required_keywords}
        else eval_type == "functional"
            HR-->>HR: FunctionalEvaluator{eval_script}
        end

        HR->>EVAL: evaluate(trajectory)
        EVAL->>EVAL: functional → chạy eval_script trong workspace
        EVAL-->>HR: score (0 hoặc 1)

        HR->>HR: success = (score >= threshold)] AND status==Completed
        HR->>HR: updateElapsedTime()
        HR->>HR: result.total_tokens = ...

        opt evaluation lỗi
            HR-->>HR: TerminationStatus=EvaluationError
            HR-->>HR: score=0, error=...
        end

        %% (6) Xuất trajectory + teardown
        HR->>FS: exportTrajectory() → results/trajectory_{task.id}.json
        HR->>ENV: teardown()
        ENV-->>HR: ok

        HR-->>MAIN: TaskResult{task_id, success, score, time, tokens}
        Note over MAIN,HR: 1 task lỗi KHÔNG sập cả batch (cô lập lỗi)
    end

    %% ────────────────────────────────────────────────────────────
    %% BATCH — BÁO CÁO
    %% ────────────────────────────────────────────────────────────
    HR->>HR: printReport(results)
    Note over HR: in BÁO CÁO BENCHMARK: pass/fail/error,<br/>success rate, thời gian, breakdown theo eval_type

    HR->>FS: exportBenchmarkSummary(results)
    HR->>FS: results/benchmark_summary.json

    HR-->>MAIN: vector<TaskResult> (toàn bộ batch)
```

---

## Chi tiết `runTask()` — các trạng thái lỗi có thể xảy ra

| Giai đoạn | Trạng thái `TerminationStatus` | Kết quả `TaskResult` |
| --- | --- | --- |
| `makeEnvironment` / `env->setup()` thất bại | `EnvironmentError` | `error="Loi tao moi truong: ..."` |
| `agent.run()` ném exception | `AgentError` | `success=false, score=0, error=...` |
| `evaluate()` ném exception | `EvaluationError` | `success=false, score=0, error=...` |
| Không lỗi nhưng score < `success_threshold` (mặc định 1.0) hoặc status ≠ Completed | trạng thái gốc | `success=false` |
| Hoàn thành đúng | `Completed` | `success=true, score=1` |

- Các task bị lỗi vẫn export `trajectory_<id>.json` (qua `safelyExportTrajectory`).
- Teardown luôn chạy trong `finally`-style (try/catch riêng), dù pass hay fail.
- Ngưỡng thành công có thể cấu hình khi khởi tạo `HarnessRunner` (mặc định `1.0`).
