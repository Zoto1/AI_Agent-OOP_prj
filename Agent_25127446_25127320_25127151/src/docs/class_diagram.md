# CLASS DIAGRAM — Toàn bộ hệ thống

Sơ đồ lớp đầy đủ của framework `OopAgent`, bao gồm cả **client LLM**, **tool system**,
**agent core**, **skill system**, **harness/benchmark** và **multi-agent coordination**.

## Ký hiệu

| Ký hiệu  | Ý nghĩa                          |
| -------- | -------------------------------- |
| `X <\|-- Y`  | Inheritance — Y kế thừa X       |
| `X *-- Y`    | Composition — X sở hữu Y (vòng đời buộc chặt) |
| `X o-- Y`    | Aggregation — X chứa tập hợp Y   |
| `X ..> Y`    | Dependency — X dùng/phụ thuộc Y  |

> Ghi chú: toàn bộ dependency được inject qua `std::shared_ptr` và đánh dấu
> `<<shared>>`. `AgentLoop` giữ `shared_ptr` đến `LLMClient`, `ToolRegistry`,
> `SkillLoader`, `LoopDetector` nhưng không *sở hữu* chúng (chúng được tạo bên ngoài
> — Dependency Injection).

```mermaid
classDiagram
    direction LR

    %% ═══════════════════════════════════════════════════════════════
    %% 1. LLM CLIENT LAYER
    %% ═══════════════════════════════════════════════════════════════
    class LLMConfig {
        +base_url: string
        +model_name: string
        +temperature: float
        +max_tokens: int
        +timeout_ms: int
        +api_key: optional~string~
    }

    class MessageKind {
        <<enum>>
        Text
        FunctionCall
        FunctionResponse
    }

    class Message {
        +role: string
        +content: string
        +kind: MessageKind
        +tool_name: string
        +tool_args: string
        +tool_call_id: string
        +thought_signature: string
    }

    class LLMToolCall {
        +name: string
        +args: string
        +id: string
        +thought_signature: string
    }

    class TokenUsage {
        +prompt_tokens: long long
        +candidate_tokens: long long
        +thought_tokens: long long
        +total_tokens: long long
    }

    class LLMResponse {
        +text: string
        +tool_call: optional~LLMToolCall~
        +usage: TokenUsage
    }

    class LLMException {
        <<exception>>
        inherits runtime_error
    }
    class APIEnvironmentError {
        <<exception>>
        inherits LLMException
    }
    class LLMClientError {
        <<exception>>
        inherits LLMException
    }

    class LLMClient {
        <<abstract>>
        #_config: LLMConfig
        +chat(messages: vector~Message~)* string
        +chatMultimodal(messages: vector~Message~, images: vector~string~)* string
        +chatWithTools(messages, function_declarations_json) LLMResponse
        +safeChat(messages) expected~string,string~
        +safeChatWithTools(messages, func_decls_json) expected~LLMResponse,string~
        +setConfig(cfg: LLMConfig) void
        +getModelName() string
    }

    class GeminiClient {
        +chat(messages) string
        +chatWithTools(messages, func_decls_json) LLMResponse
        +chatMultimodal(messages, images) string
        -buildRequestBody(messages, images, func_decls_json) string
        -sendRequest(jsonBody) string
        -buildEndpointUrl() string
        #parseToolAwareResponse(rawJson) LLMResponse
    }

    class OllamaClient {
        -curl_handle: void*
        +chat(messages) string
        +chatMultimodal(messages, images) string
    }

    class ConfigLoader {
        +loadLLMConfig(configPath, provider, envVarName)$ LLMConfig
    }

    LLMResponse *-- TokenUsage : has
    LLMResponse o-- LLMToolCall : optional
    Message *-- MessageKind : kind

    LLMClient <|-- GeminiClient
    LLMClient <|-- OllamaClient
    LLMClient *-- LLMConfig : <<shared>>
    LLMClient ..> Message : uses
    LLMClient ..> LLMResponse : returns
    LLMClient ..> TokenUsage : uses

    LLMException <|-- APIEnvironmentError
    LLMException <|-- LLMClientError

    ConfigLoader ..> LLMConfig : creates

    %% ── Embedding (Strategy pattern) ───────────────────────────────
    class EmbeddingClient {
        <<abstract>>
        +embed(text)* vector~float~
        +getModelName()* string
    }
    class OllamaEmbeddingClient {
        -base_url_: string
        -model_: string
        -timeout_ms_: int
        -curl_handle_: void*
        +embed(text) vector~float~
        +getModelName() string
    }
    class EmbeddingFactory {
        +makeOllamaEmbeddingClient(config_path)$ shared_ptr~EmbeddingClient~
    }

    EmbeddingClient <|-- OllamaEmbeddingClient
    EmbeddingFactory ..> EmbeddingClient : creates

    %% ═══════════════════════════════════════════════════════════════
    %% 2. TOOL LAYER
    %% ═══════════════════════════════════════════════════════════════
    class Tool {
        <<abstract>>
        #name: string
        #description: string
        #parameters_schema: json
        +execute(args: map~string,string~)* string
        +resetState() void
        +getName() string
        +getDescription() string
        +getParametersSchema() json
    }

    class CalculatorTool {
        +execute(args) string
    }
    class ExecTool {
        +execute(args) string
    }
    class FileReadTool {
        -base_directory: string
        +execute(args) string
    }
    class FileWriteTool {
        -base_directory: string
        +execute(args) string
    }
    class WebSearchTool {
        +execute(args) string
        -fetchSearchResults(query)$ string
        -urlEncode(value)$ string
    }
    class DateTimeTool {
        +execute(args) string
    }
    class HttpGetTool {
        +execute(args) string
        -performRequest(url)$ string
    }
    class JsonParserTool {
        +execute(args) string
    }

    class Memory {
        -Entry
        -memory_data: unordered_map~string,Entry~
        -embedder_: shared_ptr~EmbeddingClient~
        -persist_path_: string
        +execute(args) string
        +clear_memory() void
        +resetState() void
        -save_context(key, value) bool
        -load_context(query) optional~string~
        -load_by_embedding(query) optional~string~
        -persist() void
        -load_persisted() void
        +init()$ void
    }

    Tool <|-- CalculatorTool
    Tool <|-- ExecTool
    Tool <|-- FileReadTool
    Tool <|-- FileWriteTool
    Tool <|-- WebSearchTool
    Tool <|-- DateTimeTool
    Tool <|-- HttpGetTool
    Tool <|-- JsonParserTool
    Tool <|-- Memory
    note for Memory "Entry = struct { value; embedding } -- du lieu memory_data"
    Memory *-- EmbeddingClient : <<shared>>

    class ToolRegistry {
        <<singleton>>
        -tools: map~string,shared_ptr~Tool~~
        -denied_tools: unordered_set~string~
        -ToolRegistry() private
        +getInstance()$ ToolRegistry
        +registerTool(tool: shared_ptr~Tool~) void
        +executeTool(name, args) string
        +hasTool(name) bool
        +denyTool(name) void
        +allowTool(name) void
        +isAllowed(name) bool
        +resetToolStates() void
        +describeToolsForPrompt() string
        +functionDeclarationsJson() string
    }

    ToolRegistry o-- "many" Tool : registers
    ToolRegistry ..> Tool : executes

    %% ═══════════════════════════════════════════════════════════════
    %% 3. SKILL LAYER
    %% ═══════════════════════════════════════════════════════════════
    class Skill {
        +name: string
        +keywords: string
        +instruction: string
    }

    class SkillLoader {
        -skill_directory: string
        -loaded_skill: vector~Skill~
        +load_skills() void
        +select_skill(task_description) optional~Skill~
        +inject_into_prompt(system_prompt, skill) string
        -readFile(filepath) string
    }

    SkillLoader o-- "many" Skill : loads .md
    SkillLoader ..> Skill : returns

    %% ═══════════════════════════════════════════════════════════════
    %% 4. AGENT LOOP / STEP / LOOP-DETECTION
    %% ═══════════════════════════════════════════════════════════════
    class AgentTerminationStatus {
        <<enum>>
        Completed
        LoopDetected
        MaxStepsReached
    }

    class AgentRunResult {
        +final_answer: string
        +status: AgentTerminationStatus
        +total_tokens: long long
    }

    class ToolCall {
        +tool: string
        +args: string
    }
    class FinalAnswer {
        +text: string
    }

    class Step {
        +step_id: int
        +thought: string
        +action: variant~ToolCall,FinalAnswer~
        +tool_result: string
        +tokens_used: long long
        +latency_ms: long long
    }

    Step *-- ToolCall : variant<>
    Step *-- FinalAnswer : variant<>

    class LoopType {
        <<enum>>
        NONE
        GENERIC_REPEAT
        PING_PONG
    }
    class LoopSeverity {
        <<enum>>
        NORMAL
        WARNING
        CRITICAL
    }
    class LoopResult {
        +type: LoopType
        +sev: LoopSeverity
        +message: string
    }

    class LoopDetector {
        -_warning: int
        -_critical: int
        +detect(history: vector~Step~) LoopResult
        -checkGenericRepeat(history) optional~LoopSeverity~
        -checkPingPong(history) optional~LoopSeverity~
        -isSameAction(a, b)$ bool
        -isSameStep(a, b)$ bool
    }

    LoopDetector ..> LoopResult : returns
    LoopResult *-- LoopType
    LoopResult *-- LoopSeverity

    class AgentLoop {
        <<Template Method>>
        #llm: shared_ptr~LLMClient~ (shared)
        #tool_registry: shared_ptr~ToolRegistry~ (shared)
        #skill_loader: shared_ptr~SkillLoader~ (shared)
        #loop_detector: shared_ptr~LoopDetector~ (shared)
        #history: vector~Message~
        #step_history: vector~Step~
        #max_steps: const int
        #step_hook: function~const Step&~  (Observer)
        #verbose: bool
        +run(task) AgentRunResult
        +setStepHook(hook) void
        +setVerbose(enable) void
        #observe(tool_result) void
        #think() LLMResponse
        #act(thought) optional~variant~ToolCall,FinalAnswer~~
        -parseToolCall(response) optional~ToolCall~
        -parseFinalAnswer(response) optional~FinalAnswer~
        -buildSystemMessage(task) Message
    }

    AgentLoop *-- LLMClient : <<shared>>
    AgentLoop *-- ToolRegistry : <<shared>>
    AgentLoop *-- SkillLoader : <<shared>>
    AgentLoop *-- LoopDetector : <<shared>>
    AgentLoop *-- "2" Message : history
    AgentLoop *-- "many" Step : step_history
    AgentLoop ..> ToolCall : creates
    AgentLoop ..> FinalAnswer : creates
    AgentLoop ..> AgentRunResult : returns
    AgentLoop ..> LoopDetector : detect(step_history)
    AgentLoop ..> SkillLoader : select_skill(task)
    AgentLoop ..> ToolRegistry : executeTool()
    AgentLoop ..> LLMClient : chatWithTools()

    %% ═══════════════════════════════════════════════════════════════
    %% 5. ENVIRONMENT LAYER
    %% ═══════════════════════════════════════════════════════════════
    class Environment {
        <<abstract>>
        +setup()* void
        +teardown()* void
        +executeCommand(command)* string
        +isHealthy()* bool
    }

    class SandboxConfig {
        <<struct>>
        +image: string
        +containerNamePrefix: string
        +timeoutSec: int
    }

    class NativeEnvironment {
        -workingDir: string
        -isSetUp: bool
        +setup() void
        +teardown() void
        +executeCommand(cmd) string
        +isHealthy() bool
        +getWorkingDir() string
    }

    class SandboxEnvironment {
        -containerId: string
        -cfg: SandboxConfig
        +setup() void
        +teardown() void
        +executeCommand(cmd) string
        +isHealthy() bool
        +getContainerId() string
        -runShell(command) string
    }

    Environment <|-- NativeEnvironment
    Environment <|-- SandboxEnvironment
    SandboxEnvironment *-- SandboxConfig

    %% ═══════════════════════════════════════════════════════════════
    %% 6. TRAJECTORY / EVALUATOR
    %% ═══════════════════════════════════════════════════════════════
    class TerminationStatus {
        <<enum>>
        Unknown
        Completed
        LoopDetected
        MaxStepsReached
        AgentError
        EvaluationError
        EnvironmentError
    }

    class Trajectory {
        <<struct>>
        +task_id: string
        +model: string
        +success: bool
        +total_tokens: long long
        +total_time_ms: long long
        +final_answer: string
        +termination_status: TerminationStatus
        +error_message: string
        +steps: vector~Step~
    }

    Trajectory *-- "many" Step : steps
    Trajectory *-- TerminationStatus

    class Evaluator {
        <<abstract>>
        <<Strategy>>
        +evaluate(trajectory)* double
    }
    class KeywordEvaluator {
        -required_keywords: vector~string~
        +evaluate(trajectory) double
    }
    class FunctionalEvaluator {
        -eval_script: string
        +evaluate(trajectory) double
    }

    Evaluator <|-- KeywordEvaluator
    Evaluator <|-- FunctionalEvaluator
    Evaluator ..> Trajectory : uses

    %% ═══════════════════════════════════════════════════════════════
    %% 7. MULTI-AGENT COORDINATION
    %% ═══════════════════════════════════════════════════════════════
    class AgentMessage {
        +sender_id: string
        +receiver_id: string
        +content: string
        +is_result: bool
    }

    class MessageQueue~T~ {
        <<template>>
        -m_queue: queue~T~
        -m_mutex: mutex
        -m_cv: condition_variable
        -m_closed: bool
        +push(item) void
        +pop() optional~T~
        +try_pop() optional~T~
        +pop_for(timeout) optional~T~
        +close() void
        +is_closed() bool
        +size() size_t
        +empty() bool
    }

    class SubTaskDefinition {
        +id: string
        +instruction: string
        +max_steps: int
    }

    class SubAgentResult {
        +sub_id: string
        +final_answer: string
        +success: bool
        +total_tokens: long long
        +total_time_ms: long long
        +error: string
        +trajectory: Trajectory
    }

    class SubAgentHandle {
        -m_id: string
        -m_inbox: shared_ptr~MessageQueue~AgentMessage~~
        -m_outbox: shared_ptr~MessageQueue~AgentMessage~~
        -m_promise: promise~SubAgentResult~
        -m_done: unique_ptr~atomic~bool~~
        -m_thread: jthread
        +send(msg) void
        +getResultFuture() future~SubAgentResult~
        +isDone() bool
        +requestStop() void
        +getId() string
    }

    class MultiAgentCoordinator {
        -m_llm: shared_ptr~LLMClient~ (shared)
        -m_tool_registry: shared_ptr~ToolRegistry~ (shared)
        -m_skill_loader: shared_ptr~SkillLoader~ (shared)
        -m_loop_detector: shared_ptr~LoopDetector~ (shared)
        -m_coordinator_inbox: shared_ptr~MessageQueue~AgentMessage~~
        -m_agent_inboxes: vector~shared_ptr~MessageQueue~AgentMessage~~~
        +runParallel(subtasks) vector~SubAgentResult~
        +mergeResults(results)$ string
        +broadcast(message) void
        -buildSubAgentTask(subtask, inbox, outbox) function~SubAgentResult~
    }

    note for MessageQueue~T~ "Template queue T -- chua queue<T>,<br/>dong bo bang mutex + condition_variable"

    MultiAgentCoordinator *-- "many" SubAgentHandle : spawns
    MultiAgentCoordinator *-- MessageQueue~AgentMessage~ : inbox/outbox
    MultiAgentCoordinator ..> AgentLoop : creates per sub
    MultiAgentCoordinator ..> SubAgentResult : returns
    SubAgentHandle *-- "2" MessageQueue~AgentMessage~ : in/outbox
    SubAgentHandle ..> SubAgentResult : reports
    SubAgentResult *-- Trajectory : has

    %% ═══════════════════════════════════════════════════════════════
    %% 8. HARNESS RUNNER
    %% ═══════════════════════════════════════════════════════════════
    class TaskDefinition {
        <<struct>>
        +id: string
        +description: string
        +instruction: string
        +eval_type: string
        +eval_script: string
        +eval_keywords: vector~string~
        +max_steps: int
    }

    class TaskResult {
        <<struct>>
        +task_id: string
        +eval_type: string
        +success: bool
        +score: double
        +total_time_ms: long long
        +total_tokens: long long
        +trajectory_path: string
        +error: optional~string~
    }

    class HarnessRunner {
        <<Director / Mediator>>
        -llm: shared_ptr~LLMClient~ (shared)
        -tool_registry: shared_ptr~ToolRegistry~ (shared)
        -skill_loader: shared_ptr~SkillLoader~ (shared)
        -loop_detector: shared_ptr~LoopDetector~ (shared)
        -output_dir: string
        -workspace_root: string
        -success_threshold: double
        +runTask(task) TaskResult
        +runSingleTask(tasks_json_path, task_id) TaskResult
        +runBatch(tasks_json_path) vector~TaskResult~
        +printReport(results) void
        +runMultiAgent(subtasks) string
        +splitTaskIntoSubtasks(combined_instruction, num_agents)$ vector~SubTaskDefinition~
        -loadTasks(tasks_json_path) vector~TaskDefinition~
        -makeEvaluator(task) shared_ptr~Evaluator~
        -makeEnvironment(task) shared_ptr~Environment~
        -exportTrajectory(trajectory) void
        -exportBenchmarkSummary(results) void
    }

    HarnessRunner ..> TaskDefinition : reads
    HarnessRunner ..> TaskResult : returns
    HarnessRunner ..> Trajectory : records
    HarnessRunner ..> AgentLoop : creates & runs
    HarnessRunner ..> Evaluator : makeEvaluator()
    HarnessRunner ..> Environment : makeEnvironment()
    HarnessRunner ..> MultiAgentCoordinator : creates
    HarnessRunner ..> SubTaskDefinition : splitTaskIntoSubtasks()
```

## Tóm tắt Design Patterns

| Pattern | Lớp | Vai trò |
| --- | --- | --- |
| **Template Method** | `AgentLoop.run()` = `think() → act() → observe()` | Skeleton ReAct, subclass override từng bước |
| **Observer / Hook** | `AgentLoop.setStepHook()` → `HarnessRunner` | Agent không biết Harness; Harness thu thập `Step` |
| **Strategy** | `Evaluator` → `KeywordEvaluator` / `FunctionalEvaluator`; `EmbeddingClient` → `OllamaEmbeddingClient` | Chọn thuật toán tại runtime |
| **Singleton** | `ToolRegistry` | Một registry dùng chung toàn app |
| **Factory** | `makeOllamaEmbeddingClient()`, `makeAgentLoop<T>()`, `ConfigLoader::loadLLMConfig()` | Tạo object tránh harcode dependency |
| **Template class** | `MessageQueue<T>` | Queue thread-safe dùng chung cho message |
| **RAII / jthread** | `SubAgentHandle` | Tự join thread khi destructor |
