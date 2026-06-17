# Class Diagram — Harness Subsystem

```mermaid
classDiagram
    class Environment {
        <<abstract>>
        +setup() void*
        +teardown() void*
        +execute(cmd) string*
        +isHealthy() bool*
    }

    class NativeEnvironment {
        -workingDir : path
        -isSetUp : bool
    }

    class SandboxEnvironment {
        -containerId : string
        -cfg : Config
    }

    class Evaluator {
        <<abstract>>
        +evaluate(t) optional~double~*
        +getName() string*
    }

    class KeywordEvaluator {
        -keywords : vector~string~
        -requireAll : bool
    }

    class FunctionalEvaluator {
        -evalScript : string
        -runScript() int
    }

    class VLMEvaluator {
        -llmClient : shared_ptr~LLMClient~
        -prompt : string
    }

    class Step {
        <<struct>>
        +stepId : int
        +thought : string
        +action : variant~ToolCall_FinalAnswer~
        +toolResult : string
        +tokensUsed : int
        +latencyMs : long
    }

    class Trajectory {
        <<struct>>
        +taskId : string
        +model : string
        +success : bool
        +totalTokens : int
        +steps : vector~Step~
        +toJSON() string
    }

    class HarnessRunner {
        <<orchestrator>>
        -agent : AgentLoop&
        -env : unique_ptr~Environment~
        -evaluators : vector~unique_ptr~Evaluator~~
        -trajectories : vector~Trajectory~
        -stepHook : function~void_Step~
        +runTask(task) Trajectory
        +runBatch(tasks) vector~Trajectory~
        +getSuccessRate() float
        +exportAllJSON(dir) void
    }

    Environment <|-- NativeEnvironment
    Environment <|-- SandboxEnvironment

    Evaluator <|-- KeywordEvaluator
    Evaluator <|-- FunctionalEvaluator
    Evaluator <|-- VLMEvaluator

    Trajectory *-- Step : contains

    HarnessRunner *-- Environment : owns
    HarnessRunner *-- Evaluator : owns
    HarnessRunner *-- Trajectory : owns
```

## Quan hệ giữa các class

| Quan hệ | Ký hiệu | Ý nghĩa |
|---|---|---|
| `Environment <\|-- Native/SandboxEnvironment` | Inheritance | Override toàn bộ pure virtual của `Environment` |
| `Evaluator <\|-- Keyword/Functional/VLMEvaluator` | Inheritance | Strategy Pattern — 3 thuật toán chấm điểm |
| `Trajectory *-- Step` | Composition | `Trajectory` sở hữu `vector<Step>` |
| `HarnessRunner *-- Environment` | Composition | Sở hữu qua `unique_ptr`, đổi impl không cần sửa `HarnessRunner` |
| `HarnessRunner *-- Evaluator` | Composition | Sở hữu nhiều evaluator, chạy tuần tự |
| `HarnessRunner *-- Trajectory` | Composition | Tích lũy kết quả sau mỗi `runTask()` |

## Design Patterns

| Pattern | Class áp dụng |
|---|---|
| **Strategy** | `Evaluator` → `KeywordEvaluator`, `FunctionalEvaluator`, `VLMEvaluator` |
| **Template Method** | `HarnessRunner::runTask()` — skeleton cố định |
| **Observer / Hook** | `stepHook` inject vào `AgentLoop` |
| **Dependency Inversion** | `HarnessRunner` phụ thuộc `Environment` abstract, không phụ thuộc concrete |
