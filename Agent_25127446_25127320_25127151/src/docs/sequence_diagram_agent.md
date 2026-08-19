# SEQUENCE DIAGRAM — Một lần Agent Run hoàn chỉnh

Mô tả luồng thực thi đầy đủ **một lần `AgentLoop::run(task)`** từ khi *nhận task*
đến khi *trả kết quả* (`AgentRunResult`). Đây là lõi ReAct loop được dùng chung bởi
cả chế độ REPL (`main.cpp`) lẫn Benchmark (`HarnessRunner`).

---

## Happy Path: tool call → … → final answer

```mermaid
sequenceDiagram
    autonumber
    participant CALLER as Caller<br/>(REPL / HarnessRunner)
    participant AL as AgentLoop
    participant SL as SkillLoader
    participant TR as ToolRegistry
    participant LLM as LLMClient
    participant TOOL as Tool (concrete)
    participant LD as LoopDetector
    participant OBS as step_hook<br/>(Observer → Trajectory)

    Note over CALLER,AL: begin: run(task)
    CALLER->>AL: run(task)

    %% ---- setup history ----
    AL->>AL: history.clear()
    AL->>AL: step_history.clear()
    AL->>AL: buildSystemMessage(task)

    %% inject skill
    AL->>SL: select_skill(task)
    SL-->>AL: optional<Skill>
    Note over AL,SL: Skill ghi chú vào system prompt

    %% inject tool descriptions
    AL->>TR: describeToolsForPrompt()
    TR-->>AL: text liệt kê tools + schema

    AL->>AL: history.push_back(system_msg)
    AL->>AL: history.push_back(user_msg{role=user, content=task})

    %% ══════════════════ ReAct LOOP ══════════════════
    loop i = 0 .. max_steps - 1
        AL->>AL: cur_step = Step{step_id=i}

        %% ── THINK ────────────────────────────────────────
        AL->>TR: functionDeclarationsJson()
        TR-->>AL: JSON function declarations
        AL->>LLM: safeChatWithTools(history, declarations)
        LLM-->>AL: std::expected<LLMResponse,string>

        opt has_error (API/lỗi mạng)
            AL->>AL: throw LLMClientError / APIEnvironmentError
            Note over AL: exception bắt bởi caller (Harness ghi AgentError)
        end

        Note over AL: LLMResponse { text, tool_call?, usage }
        AL->>AL: cur_step.tokens_used = usage.total_tokens
        AL->>AL: cur_step.latency_ms = elapsed

        %% ── Parse action ──────────────────────────────────
        alt native tool call (tool_call.has_value)
            AL->>AL: ToolCall{native_call.name, native_call.args}
            AL->>AL: thought = JSON mô tả tool call
        else text response
            AL->>AL: thought = model_response.text
            AL->>AL: act(thought) → parseToolCall()
            alt tìm thấy {"type":"tool_call"}
                AL->>AL: parsed_action = ToolCall
            else tìm thấy {"type":"final_answer"}
                AL->>AL: parsed_action = FinalAnswer
            else sau ≥1 tool đã chạy và là văn xuôi
                AL->>AL: parsed_action = FinalAnswer{text}
            else format không hợp lệ
                AL->>AL: history.push_back(FORMAT_ERROR msg)
                Note over AL: continue → iteration kế tiếp
            end
        end

        AL->>AL: history.push_back(assistant_message)
        AL->>AL: step_history.push_back(cur_step)

        %% ── Loop detection (pre-execute) ─────────────────
        AL->>LD: detect(step_history)
        LD-->>AL: LoopResult{type, sev, message}
        alt severity == CRITICAL
            AL->>OBS: step_hook(cur_step)
            AL-->>CALLER: AgentRunResult{status=LoopDetected}
            Note over AL,CALLER: abort ngay
        else WARNING / NORMAL
            Note over AL,LD: logging cảnh báo, tiếp tục
        end

        %% ── ACT ───────────────────────────────────────────
        alt action is FinalAnswer
            AL->>OBS: step_hook(cur_step)
            AL-->>CALLER: AgentRunResult{final_answer, status=Completed, total_tokens}
        else action is ToolCall
            AL->>TR: hasTool(tool_call.tool)
            TR-->>AL: true / false
            alt tool tồn tại
                AL->>TR: executeTool(name, parsed_args)
                TR->>TOOL: execute(args)
                TOOL-->>TR: result string
                TR-->>AL: result string
            else tool không tồn tại
                AL->>AL: result = "Error: unknown tool '...'"
            end

            alt native call
                AL->>AL: history.push_back(FunctionResponse msg)
            else text-based call
                AL->>AL: observe("Tool result: ...") → "tool" role
            end

            AL->>AL: has_executed_tool = true
            AL->>AL: cur_step.tool_result = result
            AL->>OBS: step_hook(cur_step)
            Note over OBS: Trajectory.steps.push_back(step)

            %% ── Loop detection (post-execute) ────────────
            AL->>LD: detect(step_history) [sau khi có tool_result]
            LD-->>AL: LoopResult
            alt CRITICAL sau result
                AL-->>CALLER: AgentRunResult{status=LoopDetected}
            end
        end
    end

    %% ── max_steps hết ─────────────────────────────────-----
    AL-->>CALLER: AgentRunResult{status=MaxStepsReached}

    Note over AL,CALLER: end: AgentRunResult<br/>final_answer + status (Completed / LoopDetected / MaxStepsReached)
```

---

## Luồng rút gọn theo trạng thái kết thúc

| Điều kiện | `AgentRunResult.status` |
| --- | --- |
| Model trả `final_answer` JSON (hoặc văn xuôi sau khi đã chạy tool) | `Completed` |
| `LoopDetector` báo `CRITICAL` (GenericRepeat ≥ 4 lần giống hệt, hoặc PingPong ≥ 4 cặp) | `LoopDetected` |
| Vòng lặp chạy đủ `max_steps` mà chưa có final answer | `MaxStepsReached` |
| `safeChatWithTools` → `std::unexpected` (API key / mạng / rate-limit) | ném exception → caller map sang `AgentError` |

> Ghi chú triển khai (`src/agent/agent_loop.cpp`): `step_hook` là `std::function`.
> Ở chế độ REPL không set hook; ở chế độ benchmark `HarnessRunner` set hook để
> *thu thập* `Step` vào `Trajectory` mà không can thiệp logic agent.