classDiagram
    %% ===== LLM Client Layer =====
    class LLMClient {
        <<abstract>>
        #baseUrl: string
        #model: string
        #temperature: double
        #maxTokens: int
        +chat(messages: vector~Message~) string*
        +chatWithImage(messages, imageBase64: string) string*
        +setConfig(config: LLMConfig) void
    }
    class OllamaClient {
        -curlHandle: CURL*
        +chat(messages) string
        +chatWithImage(messages, imageBase64) string
        -sendPost(endpoint: string, body: json) json
        -handleError(code: int) void
    }
    LLMClient <|-- OllamaClient

    %% ===== Tool Layer =====
    class Tool {
        <<abstract>>
      
    }
    class ExecTool
    class FileTool
    class WebSearchTool
    class MemoryTool
    class CalculatorTool
    Tool <|-- ExecTool
    Tool <|-- FileTool
    Tool <|-- WebSearchTool
    Tool <|-- MemoryTool
    Tool <|-- CalculatorTool

    class ToolRegistry {
        
    }
    ToolRegistry o-- "many" Tool : manages

    %% ===== Skill Layer =====
    class SkillLoader {
       
    }
    class Skill {
        +name: string
        +keywords: vector~string~
        +content: string
    }
    SkillLoader o-- "many" Skill

    %% ===== Agent Loop / Action / Step =====
    class ToolCall {
        +type: string
        +tool: string
        +args: string
    }
    class FinalAnswer {
        +type: string
        +text: string
    }
    class Step {
       
    }
    Step *-- ToolCall
    Step *-- FinalAnswer

    class LoopDetector {
        
    }
    class LoopResult {
        +type: LoopType
        +sev: LoopSeverity
        +message: string
    }
    LoopDetector ..> LoopResult : creates

    class AgentLoop {
        -llmClient: LLMClient*
        -toolRegistry: ToolRegistry*
        -skillLoader: SkillLoader*
        -loopDetector: LoopDetector
        -maxSteps: int
        -stepHook: function~void(Step)~
        +run(task: string) Trajectory
        #observe() Step
        #think(context: string) string
        #act(action: variant~ToolCall, FinalAnswer~) string
        +setStepHook(hook: function~void(Step)~) void
    }
    AgentLoop --> LLMClient : uses
    AgentLoop --> ToolRegistry : uses
    AgentLoop --> SkillLoader : uses
    AgentLoop *-- LoopDetector : owns
    AgentLoop ..> Step : creates

    %% ===== Environment =====
    class Environment {
        <<abstract>>
        
    }
    class NativeEnvironment
    class SandboxEnvironment
    Environment <|-- NativeEnvironment
    Environment <|-- SandboxEnvironment

    %% ===== Trajectory =====
    class Trajectory {
     
    }
    Trajectory o-- "many" Step

    %% ===== Evaluator =====
    class Evaluator {
        <<abstract>>
        
    }
    class KeywordEvaluator
    class FunctionalEvaluator
    class VLMEvaluator
    Evaluator <|-- KeywordEvaluator
    Evaluator <|-- FunctionalEvaluator
    Evaluator <|-- VLMEvaluator

    %% ===== Harness =====
    class HarnessRunner {
       
    }
    HarnessRunner *-- Environment : owns
    HarnessRunner o-- "many" Evaluator : uses
    HarnessRunner ..> AgentLoop : creates and runs
    HarnessRunner ..> Trajectory : records
