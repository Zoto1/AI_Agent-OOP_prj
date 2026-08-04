classDiagram
    class ToolRegistry {
        - tools: map~string, shared_ptr~Tool~~
        - ToolRegistry()
        + registerTool(tool: shared_ptr~Tool~) void
        + executeTool(name: string, args: map~string, string~) string
        + getInstance()$ ToolRegistry&
    }

    class Tool {
        <<abstract>>
        # name: string
        # description: string
        + Tool(n: string, d: string)
        + ~Tool()
        + execute(args: map~string, string~) string*
        + getName() string
        + getDescription() string
    }

    class ExecTool {
        + ExecTool()
        + execute(args: map~string, string~) string {override}
    }

    class FileReadTool {
        - base_directory: string
        + FileReadTool(base_dir: string)
        + execute(args: map~string, string~) string {override}
    }

    class FileWriteTool {
        - base_directory: string
        + FileWriteTool(n: string, d: string, base_dir: string)
        + execute(args: map~string, string~) string {override}
    }

    class WebSearchTool {
        - performSearchRequest(query: string) string
        + WebSearchTool()
        + execute(args: map~string, string~) string {override}
    }

    class MemoryTool {
        - storage_path: string
        + MemoryTool()
        + save_context(key: string, value: string) bool
        + load_context(query: string) string
        + execute(args: map~string, string~) string {override}
    }

    class CalculatorTool {
        + CalculatorTool()
        + execute(args: map~string, string~) string {override}
    }

    %% Mối quan hệ giữa các Class
    ToolRegistry "1" o-- "*" Tool : Manages
    Tool <|-- ExecTool : Inherits
    Tool <|-- FileWriteTool : Inherits
    Tool <|-- FileReadTool : Inherits
    Tool <|-- WebSearchTool : Inherits
    Tool <|-- MemoryTool : Inherits
    Tool <|-- CalculatorTool : Inherits