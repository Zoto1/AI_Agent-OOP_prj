## CLASS DIAGRAM:
Thiết kế class diagram                                      --LoopDetector
-AgentLoop (core ReAct loop)
-LLMClient (abstract) <- OllamaClient
-SkillLoader


```mermaid
classDiagram
    class AgentLoop {
        -llmClient: LLMClient
        -skillLoader: SkillLoader
        -loopDetector: LoopDetector
        -stepHook: function
        +void run() 
    }
    
    class LLMClient {
        <<abstract>>
        +string chat(prompt: string)
    }
    class OllamaClient {
        +string chat(prompt: string) 
    }
    LLMClient <|-- OllamaClient : Kế thừa 

    class SkillLoader {
        +void loadSkills(keyword: string) 
        +string injectSkill() 
    }
    class LoopDetector {
        -thoughtHistory: list~string~
        +bool detectLoop(thought: string) 
    }
    
    AgentLoop --> LLMClient : Sử dụng 
    AgentLoop --> SkillLoader : Sử dụng 
    AgentLoop --> LoopDetector : Sử dụng 
```
