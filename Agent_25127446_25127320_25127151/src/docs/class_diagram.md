<<<<<<< HEAD
# CLASS DIAGRAM — Toàn bộ hệ thống
=======
# CLASS DIAGRAM — Toàn Bộ Hệ Thống
>>>>>>> 76d81ac ( modify md file)

Sơ đồ lớp đầy đủ của framework `OopAgent`, mô tả kiến trúc phân tầng bao gồm: **LLM Client Layer**, **Tool System Layer**, **Skill System Layer**, **Common Core Types (`Step`, `ToolCall`, `FinalAnswer`)**, **Loop Detection Layer**, **Agent Core Layer**, **Environment Layer**, **Trajectory & Evaluator Layer**, **Multi-Agent Coordination Layer**, và **Harness Runner Layer**.

---

## 📌 Ký hiệu Chuẩn UML trong Sơ đồ

| Ký hiệu Mermaid | Quan hệ UML | Ý nghĩa & Quy chuẩn áp dụng |
| :---: | :---: | :--- |
| `X <\|-- Y` | **Inheritance** | Quan hệ kế thừa (Y là lớp con kế thừa từ abstract class / interface X) |
| `X *-- Y` | **Composition** | Quan hệ sở hữu chặt chẽ theo giá trị (Lifetime của Y gắn liền với X) |
| `X o-- Y` | **Aggregation** | Quan hệ tập hợp / Dependency Injection (`std::shared_ptr` được tiêm từ ngoài vào) |
| `X ..> Y` | **Dependency** | Quan hệ phụ thuộc (Sử dụng tạm thời, tham số phương thức, Variant alternative, tạo mới) |

> **Ghi chú về Kiến trúc & Separation of Concerns:**
> 1. **Dependency Injection:** `AgentLoop` và `HarnessRunner` nhận các thành phần `LLMClient`, `ToolRegistry`, `SkillLoader`, `LoopDetector` qua `std::shared_ptr` từ bên ngoài nên biểu diễn bằng quan hệ **Aggregation (`o--`)**.
> 2. **Kiểu dữ liệu chung độc lập (Common Types):** `Step`, `ToolCall`, `FinalAnswer` là các struct dữ liệu thuần túy (Data Transfer Objects), phục vụ cả `AgentLoop` (lưu `step_history`) và `Trajectory` (lưu `steps`) mà không làm `AgentLoop` bị phụ thuộc vào `HarnessRunner`.
> 3. **Observer / Hook Pattern:** `AgentLoop` hoàn toàn không phụ thuộc hay biết `HarnessRunner` tồn tại; `HarnessRunner` tiêm `step_hook` vào `AgentLoop` để thu thập `Step` theo thời gian thực.
> 4. **Enum Types:** Các Enum (`MessageKind`, `LoopType`, `LoopSeverity`, `AgentTerminationStatus`, `TerminationStatus`) được định nghĩa rõ ràng và biểu diễn dưới dạng thuộc tính trong các struct/class liên quan, không dùng mũi tên composition riêng.

---

## 📊 Sơ đồ Lớp Tổng Thể (Full Class Diagram)

```mermaid
classDiagram
    direction TB

    %% ═══════════════════════════════════════════════════════════════
    %% 1. LLM CLIENT LAYER
    %% ═══════════════════════════════════════════════════════════════
    class MessageKind {
        <<enum>>
        Text
        FunctionCall
        FunctionResponse
    }

    class LLMConfig {
        +base_url: string
        +model_name: string
        +temperature: float
        +max_tokens: int
        +timeout_ms: int
        +api_key: optional~string~
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
        +chatWithTools(messages: vector~Message~, func_decls_json: string) LLMResponse
        +safeChat(messages: vector~Message~) expected~string,string~
        +safeChatWithTools(messages: vector~Message~, func_decls_json: string) expected~LLMResponse,string~
        +setConfig(cfg: LLMConfig) void
        +getModelName() string
    }

    class GeminiClient {
        +chat(messages: vector~Message~) string
        +chatMultimodal(messages: vector~Message~, images: vector~string~) string
        +chatWithTools(messages: vector~Message~, func_decls_json: string) LLMResponse
        -buildRequestBody(messages, images, func_decls_json) string
        -sendRequest(jsonBody: string) string
        -buildEndpointUrl() string
        #parseToolAwareResponse(rawJson: string) LLMResponse
    }

    class OllamaClient {
        -curl_handle: void*
        +chat(messages: vector~Message~) string
        +chatMultimodal(messages: vector~Message~, images: vector~string~) string
    }

    class ConfigLoader {
        +loadLLMConfig(configPath: string, provider: string, envVarName: string)$ LLMConfig
    }

    class EmbeddingClient {
        <<abstract>>
        <<Strategy>>
        +embed(text: string)* vector~float~
        +getModelName()* string
    }

    class OllamaEmbeddingClient {
        -base_url_: string
        -model_: string
        -timeout_ms_: int
        -curl_handle_: void*
        +embed(text: string) vector~float~
        +getModelName() string
    }

    LLMClient <|-- GeminiClient
    LLMClient <|-- OllamaClient
    EmbeddingClient <|-- OllamaEmbeddingClient
    LLMException <|-- APIEnvironmentError
    LLMException <|-- LLMClientError

    LLMResponse *-- TokenUsage : usage
    LLMResponse o-- "0..1" LLMToolCall : tool_call
    LLMClient *-- LLMConfig : _config
    LLMClient ..> Message : processes
    LLMClient ..> LLMResponse : returns
    ConfigLoader ..> LLMConfig : loads

    %% ═══════════════════════════════════════════════════════════════
    %% 2. COMMON CORE TYPES (Step, ToolCall, FinalAnswer)
    %% ═══════════════════════════════════════════════════════════════
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

    Step ..> ToolCall : variant alternative
    Step ..> FinalAnswer : variant alternative

    %% ═══════════════════════════════════════════════════════════════
    %% 3. TOOL LAYER
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
        +execute(args: map~string,string~) string
    }
    class ExecTool {
        +execute(args: map~string,string~) string
    }
    class FileReadTool {
        -base_directory: string
        +execute(args: map~string,string~) string
    }
    class FileWriteTool {
        -base_directory: string
        +execute(args: map~string,string~) string
    }
    class WebSearchTool {
        +execute(args: map~string,string~) string
        -fetchSearchResults(query: string)$ string
        -urlEncode(value: string)$ string
    }
    class DateTimeTool {
        +execute(args: map~string,string~) string
    }
    class HttpGetTool {
        +execute(args: map~string,string~) string
        -performRequest(url: string)$ string
    }
    class JsonParserTool {
        +execute(args: map~string,string~) string
    }

    class Memory {
        -memory_data: unordered_map~string,Entry~
        -embedder_: shared_ptr~EmbeddingClient~
        -persist_path_: string
        -db_: sqlite3*
        -mtx_: mutex
        +execute(args: map~string,string~) string
        +clear_memory() void
        +resetState() void
        -save_context(key: string, value: string) bool
        -load_context(query: string) optional~string~
        -load_by_embedding(query: string) optional~string~
        -persist_entry(key: string, entry: Entry) bool
        -load_all_from_db() bool
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

    Memory o-- "0..1" EmbeddingClient : embedder

    class ToolRegistry {
        <<singleton>>
        -tools: map~string,shared_ptr~Tool~~
        -denied_tools: unordered_set~string~
        -ToolRegistry() private
        +getInstance()$ ToolRegistry
        +registerTool(tool: shared_ptr~Tool~) void
        +executeTool(name: string, args: map~string,string~) string
        +hasTool(name: string) bool
        +denyTool(name: string) void
        +allowTool(name: string) void
        +isAllowed(name: string) bool
        +resetToolStates() void
        +describeToolsForPrompt() string
        +functionDeclarationsJson() string
    }

    ToolRegistry o-- "0..*" Tool : registers / manages

    %% ═══════════════════════════════════════════════════════════════
    %% 4. SKILL LAYER
    %% ═══════════════════════════════════════════════════════════════
    class Skill {
        +name: string
        +keywords: string
        +instruction: string
    }

    class SkillLoader {
        -skill_directory: string
        -loaded_skill: vector~Skill~
        +SkillLoader(skill_directory: string)
        +load_skills() void
        +select_skill(task_description: string) optional~Skill~
        +inject_into_prompt(system_prompt: string, skill: Skill) string
        -readFile(filepath: path) string
    }

    SkillLoader *-- "0..*" Skill : loaded_skills

    %% ═══════════════════════════════════════════════════════════════
    %% 5. LOOP DETECTOR
    %% ═══════════════════════════════════════════════════════════════
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
        +LoopDetector(warning: int, critical: int)
        +detect(history: vector~Step~) LoopResult
        -checkGenericRepeat(history: vector~Step~) optional~LoopSeverity~
        -checkPingPong(history: vector~Step~) optional~LoopSeverity~
        -isSameAction(a: variant, b: variant)$ bool
        -isSameStep(a: Step, b: Step)$ bool
    }

    LoopDetector ..> Step : analyzes
    LoopDetector ..> LoopResult : produces

    %% ═══════════════════════════════════════════════════════════════
    %% 6. AGENT CORE LAYER
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

    class AgentLoop {
        <<Template Method>>
        #llm: shared_ptr~LLMClient~
        #tool_registry: shared_ptr~ToolRegistry~
        #skill_loader: shared_ptr~SkillLoader~
        #loop_detector: shared_ptr~LoopDetector~
        #history: vector~Message~
        #step_history: vector~Step~
        #max_steps: const int
        #step_hook: function~void(const Step&)~
        #verbose: bool
        +AgentLoop(llm, registry, loader, detector, max_steps)
        +run(task: string) AgentRunResult
        +setStepHook(hook: function~void(const Step&)~) void
        +setVerbose(enable: bool) void
        #observe(tool_result: string) void
        #think() LLMResponse
        #act(thought: string) optional~variant~ToolCall,FinalAnswer~~
        -parseToolCall(response: string) optional~ToolCall~
        -parseFinalAnswer(response: string) optional~FinalAnswer~
        -buildSystemMessage(task: string) Message
    }

    AgentLoop o-- LLMClient : injected
    AgentLoop o-- ToolRegistry : injected
    AgentLoop o-- SkillLoader : injected
    AgentLoop o-- LoopDetector : injected
    AgentLoop *-- "0..*" Message : history
    AgentLoop *-- "0..*" Step : step_history
    AgentLoop ..> AgentRunResult : returns
    AgentLoop ..> ToolCall : parses
    AgentLoop ..> FinalAnswer : parses

    %% ═══════════════════════════════════════════════════════════════
    %% 7. ENVIRONMENT LAYER
    %% ═══════════════════════════════════════════════════════════════
    class Environment {
        <<abstract>>
        +setup()* void
        +teardown()* void
        +executeCommand(command: string)* string
        +isHealthy()* bool
    }

    class SandboxConfig {
        +image: string
        +containerNamePrefix: string
        +timeoutSec: int
    }

    class NativeEnvironment {
        -workingDir: string
        -isSetUp: bool
        +setup() void
        +teardown() void
        +executeCommand(cmd: string) string
        +isHealthy() bool
        +getWorkingDir() string
    }

    class SandboxEnvironment {
        -containerId: string
        -cfg: SandboxConfig
        +setup() void
        +teardown() void
        +executeCommand(cmd: string) string
        +isHealthy() bool
        +getContainerId() string
        -runShell(command: string) string
    }

    Environment <|-- NativeEnvironment
    Environment <|-- SandboxEnvironment
    SandboxEnvironment *-- SandboxConfig : cfg

    %% ═══════════════════════════════════════════════════════════════
    %% 8. TRAJECTORY & EVALUATOR LAYER
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

    class Evaluator {
        <<abstract>>
        <<Strategy>>
        +evaluate(trajectory: Trajectory)* double
    }

    class KeywordEvaluator {
        -required_keywords: vector~string~
        +KeywordEvaluator(keywords: vector~string~)
        +evaluate(trajectory: Trajectory) double
    }

    class FunctionalEvaluator {
        -eval_script: string
        +FunctionalEvaluator(eval_script: string)
        +evaluate(trajectory: Trajectory) double
    }

    Evaluator <|-- KeywordEvaluator
    Evaluator <|-- FunctionalEvaluator

    Trajectory *-- "0..*" Step : records
    Evaluator ..> Trajectory : evaluates

    %% ═══════════════════════════════════════════════════════════════
    %% 9. MULTI-AGENT COORDINATION LAYER
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
        +push(item: T) void
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
        +send(msg: AgentMessage) void
        +getResultFuture() future~SubAgentResult~
        +isDone() bool
        +requestStop() void
        +getId() string
    }

    class MultiAgentCoordinator {
        -m_llm: shared_ptr~LLMClient~
        -m_tool_registry: shared_ptr~ToolRegistry~
        -m_skill_loader: shared_ptr~SkillLoader~
        -m_loop_detector: shared_ptr~LoopDetector~
        -m_coordinator_inbox: shared_ptr~MessageQueue~AgentMessage~~
        -m_agent_inboxes: vector~shared_ptr~MessageQueue~AgentMessage~~~
        +MultiAgentCoordinator(llm, registry, loader, detector)
        +runParallel(subtasks: vector~SubTaskDefinition~) vector~SubAgentResult~
        +mergeResults(results: vector~SubAgentResult~)$ string
        +broadcast(message: AgentMessage) void
        -buildSubAgentTask(subtask, inbox, outbox) function~SubAgentResult()~
    }

    MultiAgentCoordinator o-- LLMClient : injected
    MultiAgentCoordinator o-- ToolRegistry : injected
    MultiAgentCoordinator o-- SkillLoader : injected
    MultiAgentCoordinator o-- LoopDetector : injected
    MultiAgentCoordinator ..> SubAgentHandle : creates
    MultiAgentCoordinator ..> AgentLoop : creates per sub-agent
    MultiAgentCoordinator ..> SubTaskDefinition : processes
    MultiAgentCoordinator ..> SubAgentResult : returns
    SubAgentHandle ..> AgentMessage : sends / receives
    SubAgentHandle ..> SubAgentResult : produces
    SubAgentResult *-- Trajectory : trajectory

    %% ═══════════════════════════════════════════════════════════════
    %% 10. HARNESS RUNNER LAYER
    %% ═══════════════════════════════════════════════════════════════
    class TaskDefinition {
        +id: string
        +description: string
        +instruction: string
        +eval_type: string
        +eval_script: string
        +eval_keywords: vector~string~
        +max_steps: int
    }

    class TaskResult {
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
        -llm: shared_ptr~LLMClient~
        -tool_registry: shared_ptr~ToolRegistry~
        -skill_loader: shared_ptr~SkillLoader~
        -loop_detector: shared_ptr~LoopDetector~
        -output_dir: string
        -workspace_root: string
        -success_threshold: double
        +HarnessRunner(llm, registry, loader, detector, output_dir, workspace_root, success_threshold)
        +runTask(task: TaskDefinition) TaskResult
        +runSingleTask(tasks_json_path: string, task_id: string) TaskResult
        +runBatch(tasks_json_path: string) vector~TaskResult~
        +printReport(results: vector~TaskResult~) void
        +runMultiAgent(subtasks: vector~SubTaskDefinition~) string
        +splitTaskIntoSubtasks(combined_instruction: string, num_agents: int)$ vector~SubTaskDefinition~
        -loadTasks(tasks_json_path: string) vector~TaskDefinition~
        -makeEvaluator(task: TaskDefinition) shared_ptr~Evaluator~
        -makeEnvironment(task: TaskDefinition) shared_ptr~Environment~
        -exportTrajectory(trajectory: Trajectory) void
        -exportBenchmarkSummary(results: vector~TaskResult~) void
    }

    HarnessRunner o-- LLMClient : injected
    HarnessRunner o-- ToolRegistry : injected
    HarnessRunner o-- SkillLoader : injected
    HarnessRunner o-- LoopDetector : injected

    HarnessRunner ..> AgentLoop : creates / runs
    HarnessRunner ..> Evaluator : creates / uses
    HarnessRunner ..> Environment : creates / manages
    HarnessRunner ..> MultiAgentCoordinator : creates / uses
    HarnessRunner ..> TaskDefinition : loads
    HarnessRunner ..> TaskResult : returns
    HarnessRunner ..> Trajectory : records
```

---

<<<<<<< HEAD
| Pattern | Lớp | Vai trò |
| --- | --- | --- |
| **Template Method** | `AgentLoop.run()` = `think() → act() → observe()` | Skeleton ReAct, subclass override từng bước |
| **Observer / Hook** | `AgentLoop.setStepHook()` → `HarnessRunner` | Agent không biết Harness; Harness thu thập `Step` |
| **Strategy** | `Evaluator` → `KeywordEvaluator` / `FunctionalEvaluator`; `EmbeddingClient` → `OllamaEmbeddingClient` | Chọn thuật toán tại runtime |
| **Singleton** | `ToolRegistry` | Một registry dùng chung toàn app |
| **Factory** | `makeOllamaEmbeddingClient()`, `makeAgentLoop<T>()`, `ConfigLoader::loadLLMConfig()` | Tạo object tránh harcode dependency |
| **Template class** | `MessageQueue<T>` | Queue thread-safe dùng chung cho message |
| **RAII / jthread** | `SubAgentHandle` | Tự join thread khi destructor |
=======
## 🛠️ Bảng Tổng Kết Design Patterns & Kỹ Thuật Áp Dụng

| Design Pattern / Nguyên lý | Vị trí áp dụng | Bản chất & Vai trò thiết kế |
| :--- | :--- | :--- |
| **Template Method** | `AgentLoop::run()` | Định nghĩa khung xương ReAct bất biến: `think()` $\rightarrow$ `act()` $\rightarrow$ `observe()`. Các bước con được khai báo `virtual protected` cho phép lớp con tùy biến hoặc mock. |
| **Observer / Hook** | `AgentLoop::setStepHook()` $\rightarrow$ `HarnessRunner` | `AgentLoop` không cần biết `HarnessRunner` tồn tại. `HarnessRunner` đăng ký hook callback (`std::function`) để thu thập dữ liệu `Step` thời gian thực. |
| **Strategy** | `Evaluator` hierarchy (`KeywordEvaluator`, `FunctionalEvaluator`) & `EmbeddingClient` hierarchy | Hoán đổi linh hoạt thuật toán đánh giá (Keyword vs Shell Script) và thuật toán Vector Nhúng (`OllamaEmbeddingClient`) tại runtime. |
| **Singleton / Registry** | `ToolRegistry::getInstance()` | Điểm quản lý tập trung toàn bộ Tool, hỗ trợ đăng ký động tại runtime và kiểm soát Tool Policy (`allowTool`, `denyTool`). |
| **Factory Method** | `makeOllamaEmbeddingClient()`, `makeAgentLoop<T>()`, `ConfigLoader` | Khởi tạo đối tượng trừu tượng, tận dụng C++20 Concepts (`LLMBackend`) kiểm tra ràng buộc kiểu tại compile-time. |
| **Producer-Consumer** | `MessageQueue<T>` | Template class hàng đợi an toàn đa luồng (`std::mutex` + `std::condition_variable`), phục vụ giao tiếp giữa các sub-agent. |
| **Dependency Injection** | `AgentLoop`, `HarnessRunner`, `MultiAgentCoordinator` | Nhận dependencies qua `std::shared_ptr` (quan hệ Aggregation `o--`), giúp hệ thống đạt độ khớp nối lỏng (Loose Coupling) và dễ viết Unit Test độc lập. |
>>>>>>> 76d81ac ( modify md file)
