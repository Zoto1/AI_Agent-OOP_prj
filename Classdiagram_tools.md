# Thiết kế Class Diagram cho hệ thống Tool 

> **Trạng thái:** Draft
> **Ngày tạo:** 14/06/2026
> **Người thực hiện:** Nguyễn Ngọc Anh Thư - 25127151

---
## 1. Tổng quan (Overview)
*Mô tả các Tools Agent.*
---

## 2. Sơ đồ lớp (Class Diagram)

```mermaid
classDiagram
    %% Top to Bottom: class cha/ class quản lí nằm phía trên các class con
    direction TB
    %% Khung các Class chính theo yêu cầu
    class ToolRegistry {
        %% TODO: Thêm thuộc tính và phương thức quản lý tool
    }

    class Tool {
        <<abstract>>
        %% TODO: Thêm giao diện chung cho Tool
    }

    %% Các bộ công cụ thực thi kế thừa từ Tool
    class ExecTool
    class FileTool
    class WebSearchTool
    class MemoryTool
    class CalculatorTool

    %% Mối quan hệ giữa các Class
    ToolRegistry "1" o-- "*" Tool : Manages
    Tool <|-- ExecTool : Inherits
    Tool <|-- FileTool : Inherits
    Tool <|-- WebSearchTool : Inherits
    Tool <|-- MemoryTool : Inherits
    Tool <|-- CalculatorTool : Inherits
```
