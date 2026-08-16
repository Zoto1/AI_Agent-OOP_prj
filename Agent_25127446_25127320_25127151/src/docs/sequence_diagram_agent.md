# SEQUENCE DIAGRAM – Agent Run Flow

Mô tả luồng thực thi đầy đủ từ khi `HarnessRunner::runTask()` được gọi đến khi trả về `TaskResult`.

---

## Sơ đồ tổng thể (Happy Path – Tool Call → Final Answer)

```mermaid
sequenceDiagram
    autonumber
    participant MAIN as main.cpp
    participant HR as HarnessRunner
    participant ENV as Environment
    participant AL as AgentLoop
    participant SL as SkillLoader
    participant LLM as LLMClient
    participant TR as ToolRegistry
    participant TOOL as Tool (concrete)
    participant LD as LoopDetector
    participant EVAL as Evaluator

    MAIN->>HR: runBatch(tasks_json_path)
    HR->>HR: loadTasks(tasks_json_path)
    HR-->>HR: tasks: vector<TaskDefinition>

    loop For each TaskDefinition
        MAIN->>HR: runTask(task)

        %% Environment setup
        HR->>ENV: setup()
        ENV-->>HR: OK

        %% Build AgentLoop with injected dependencies
        HR->>AL: new AgentLoop(llm, toolRegistry, skillLoader, loopDetector, max_steps)
        HR->>AL: setStepHook(hook) [Observer Pattern]

        %% Agent Run starts
        HR->>AL: run(task.instruction)

        %% Build system message
        AL->>AL: buildSystemMessage(task)
        AL->>SL: select_skill(task)
        SL-->>AL: optional<Skill>
        Note over AL,SL: Skill hướng dẫn agent cách hoàn thành task

        AL->>AL: history.push_back(system_msg)
        AL->>AL: history.push_back(user_msg)

        loop AgentLoop (max_steps iterations)
            %% ── THINK ──────────────────────────────────────────
            AL->>LLM: chatWithTools(history, function_declarations_json)
            LLM-->>AL: LLMResponse {text, tool_call?, usage}

            alt Native function call returned
                AL->>AL: parsed_action = ToolCall from LLMToolCall
                Note over AL: thought = JSON description of call
            else Text response
                AL->>AL: act(thought)
                AL->>AL: parseToolCall(response)
                alt Tool call JSON found
                    AL-->>AL: parsed_action = ToolCall
                else Final answer JSON found
                    AL->>AL: parseFinalAnswer(response)
                    AL-->>AL: parsed_action = FinalAnswer
                else Format error
                    AL->>AL: history.push_back(FORMAT_ERROR correction)
                    Note over AL: continue to next iteration
                end
            end

            %% ── LOOP DETECTION (pre-execute) ────────────────────
            AL->>LD: detect(step_history)
            LD-->>AL: LoopResult {type, severity, message}

            alt severity == CRITICAL
                AL-->>HR: AgentRunResult {status=LoopDetected}
                Note over AL,HR: Abort run immediately
            else severity == WARNING
                Note over AL,LD: Log warning, continue
            end

            %% ── ACT: ToolCall branch ────────────────────────────
            alt action is ToolCall
                AL->>TR: hasTool(tool_name)
                TR-->>AL: true / false

                alt tool exists
                    AL->>TR: executeTool(tool_name, args)
                    TR->>TOOL: execute(args)
                    TOOL-->>TR: result string
                    TR-->>AL: result string
                else unknown tool
                    AL-->>AL: result = "Error: unknown tool"
                end

                alt native call
                    AL->>AL: history.push_back(FunctionResponse message)
                else text-based call
                    AL->>AL: observe("Tool result: " + result)
                    Note over AL: adds tool role message to history
                end

                AL->>AL: cur_step.tool_result = result
                AL->>AL: step_hook(cur_step) [notify Harness]
                HR-->>HR: hook appends Step to Trajectory

                %% ── LOOP DETECTION (post-execute) ───────────────
                AL->>LD: detect(step_history) [re-check after result]
                LD-->>AL: LoopResult

                alt CRITICAL after result
                    AL-->>HR: AgentRunResult {status=LoopDetected}
                end

            %% ── ACT: FinalAnswer branch ─────────────────────────
            else action is FinalAnswer
                AL->>AL: step_hook(cur_step)
                AL-->>HR: AgentRunResult {final_answer, status=Completed, total_tokens}
            end
        end

        %% max_steps exceeded
        AL-->>HR: AgentRunResult {status=MaxStepsReached}

        %% Build Trajectory from collected steps
        HR->>HR: Build Trajectory {task_id, model, steps, tokens, time, status}

        %% Evaluate
        HR->>EVAL: evaluate(trajectory)
        EVAL-->>HR: optional<double> score

        %% Export
        HR->>HR: exportTrajectory(trajectory)  → results/trajectory_{id}.json

        %% Environment teardown
        HR->>ENV: teardown()

        HR-->>MAIN: TaskResult {task_id, success, score, time, tokens}
    end

    HR->>HR: exportBenchmarkSummary(results)
    HR->>HR: printReport(results)
    HR-->>MAIN: vector<TaskResult>
```
