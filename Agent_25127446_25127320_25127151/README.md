# Đồ Án Lập Trình Hướng Đối Tượng: AI Agent Framework

> **Trường Đại học Khoa học Tự nhiên — ĐHQG-HCM (HCMUS)**  
> **Khoa Công nghệ Thông tin — Bộ môn Công nghệ Phần mềm**  
> **Đồ án môn học:** Lập trình Hướng đối tượng (OOP 2026)  
> **Framework:** AI Agent ReAct (Reason + Act + Observe) viết bằng **C++23 / C++20 / C++17 / C++26** hỗ trợ đa nhà cung cấp LLM (Google Gemini & Ollama / Local LLM / Google Colab / Kaggle Gemma 4), Tool Calling, Skill System, Loop Detector, SQLite Persistent Vector Memory, Multi-Agent Parallel Coordination và Evaluation Harness.

---

## 📑 Mục lục

1. [Tổng quan dự án](#1-tổng-quan-dự-án)
2. [Yêu cầu hệ thống & Cài đặt thư viện](#2-yêu-cầu-hệ-thống--cài-đặt-thư-viện)
3. [Hướng dẫn biên dịch (Build)](#3-hướng-dẫn-biên-dịch-build)
4. [Cấu hình API & Model Provider](#4-cấu-hình-api--model-provider)
5. [Hướng dẫn chạy chương trình](#5-hướng-dẫn-chạy-chương-trình)
   - [5.1 Single-Agent Interactive REPL (`agent_run`)](#51-single-agent-interactive-repl-agent_run)
   - [5.2 Multi-Agent Parallel Coordinator (`demo_multi_agent`)](#52-multi-agent-parallel-coordinator-demo_multi_agent)
   - [5.3 Chạy Benchmark toàn bộ tập Task](#53-chạy-benchmark-toàn-bộ-tập-task)
   - [5.4 Chạy 1 Task đơn lẻ theo ID](#54-chạy-1-task-đơn-lẻ-theo-id)
   - [5.5 Tổng hợp CLI Flags của `agent_run`](#55-tổng-hợp-cli-flags-của-agent_run)
6. [Cấu trúc thư mục dự án](#6-cấu-trúc-thư-mục-dự-án)
7. [Kiến trúc hệ thống & Thiết kế Hướng đối tượng](#7-kiến-trúc-hệ-thống--thiết-kế-hướng-đối-tượng)
   - [7.1 Mô hình Phân tầng (Separation of Concerns)](#71-mô-hình-phân-tầng-separation-of-concerns)
   - [7.2 Các Design Patterns áp dụng](#72-các-design-patterns-áp-dụng)
8. [Bảng kỹ thuật C++ Hiện đại (C++17 / C++20 / C++23 / C++26)](#8-bảng-kỹ-thuật-c-hiện-đại-c17--c20--c23--c26)
9. [Danh mục Công cụ (Tools Catalog — 9 Tools)](#9-danh-mục-công-cụ-tools-catalog--9-tools)
10. [Hệ thống Kỹ năng Động (Skill System)](#10-hệ-thống-kỹ-năng-động-skill-system)
11. [Cơ chế Phát hiện & Phòng chống Vòng lặp (Loop Detection)](#11-cơ-chế-phát-hiện--phòng-chống-vòng-lặp-loop-detection)
12. [Đo kiểm & Đánh giá (Harness & Benchmark Suite)](#12-đo-kiểm--đánh-giá-harness--benchmark-suite)
13. [Tính năng Mở rộng — Điểm thưởng (Bonus Features)](#13-tính-năng-mở-rộng--điểm-thưởng-bonus-features)
    - [13.1 Persistent Memory với SQLite & Vector Embedding Search (+4đ)](#131-persistent-memory-với-sqlite--vector-embedding-search-4đ)
    - [13.2 Multi-Agent Coordination đa luồng song song (+3đ)](#132-multi-agent-coordination-đa-luồng-song-song-3đ)
14. [Bộ Kiểm thử Đơn vị (Unit Tests & CTest)](#14-bộ-kiểm-thử-đơn-vị-unit-tests--ctest)
15. [Tài liệu Thiết kế UML (Mermaid Diagrams)](#15-tài-liệu-thiết-kế-uml-mermaid-diagrams)
16. [Thông tin Nhóm sinh viên](#16-thông-tin-nhóm-sinh-viên)

---

## 1. Tổng quan dự án

Dự án hiện thực một **AI Agent Framework** hoàn chỉnh viết hoàn toàn bằng C++ hiện đại (C++23) với các đặc điểm nổi bật:
- **Chu trình ReAct Engine:** Lập luận (Think) $\rightarrow$ Hành động (Act) $\rightarrow$ Quan sát kết quả (Observe).
- **Hybrid Tool Calling:** Hỗ trợ song song cả **Native Function Calling** (Gemini / OpenAI API) và **Self-Repairing JSON Fallback Parser** (Local LLM qua Ollama).
- **Phân tầng kiến trúc độc lập (Decoupling):** `AgentLoop` hoàn toàn không phụ thuộc vào `HarnessRunner`, giao tiếp qua cơ chế **Observer Pattern / Hook**.
- **Quản lý bộ nhớ hiện đại:** Sử dụng 100% smart pointers (`std::shared_ptr`, `std::unique_ptr`), RAII, không rò rỉ bộ nhớ (Memory-leak free).
- **Mở rộng cao cấp:** Tích hợp lưu trữ cơ sở dữ liệu **SQLite** kết hợp tìm kiếm ngữ nghĩa Vector Embedding (Cosine similarity) và điều phối **Multi-Agent** chạy song song đa luồng qua `std::jthread`.

---

## 2. Yêu cầu hệ thống & Cài đặt thư viện

### Yêu cầu môi trường:
- **Hệ điều hành:** Linux (Ubuntu 22.04 / 24.04 LTS) hoặc Windows Subsystem for Linux (WSL / WSL2).
- **Trình biên dịch:** GCC $\ge$ 15.0 (hỗ trợ **C++26**, gồm deleted functions với diagnostic message; đồng thời hỗ trợ `std::expected`, `std::print`, `std::jthread` và Concepts).
- **Công cụ build:** CMake $\ge$ 3.20.

### Cài đặt các thư viện phụ thuộc:

```bash
sudo apt update
sudo apt install -y cmake g++ \
    libcurl4-openssl-dev \
    zlib1g-dev \
    nlohmann-json3-dev \
    libsqlite3-dev \
    sqlite3
```

*(Lưu ý: Nếu hệ thống chưa có sẵn `nlohmann/json`, CMakeLists.txt đã cấu hình FetchContent để tự động tải mã nguồn dự phòng).*

---

## 3. Hướng dẫn biên dịch (Build)

Từ thư mục gốc dự án `Agent_25127446_25127320_25127151/`:

```bash
# 1. Tạo thư mục build và cấu hình CMake
cmake -S . -B build

# 2. Biên dịch toàn bộ targets
cmake --build build -j$(nproc)
```

Sau khi biên dịch thành công, các file thực thi sẽ được sinh ra trong thư mục `build/`:

| File thực thi | Vai trò & Mục đích |
| :--- | :--- |
| `build/agent_run` | **Entry point chính:** Single-Agent REPL, chạy benchmark toàn bộ hoặc chạy 1 task theo ID |
| `build/demo_multi_agent` | **Entry point Multi-Agent:** Interactive Console / CLI chia subtask song song đa luồng |
| `build/benchmark_run` | Mặc định chạy một batch 20 task; vẫn hỗ trợ chỉ định một file task |
| `build/test_*` | Bộ 11 chương trình Unit & Integration Test |

---

## 4. Cấu hình API & Model Provider

Dự án đọc cấu hình từ file `config.json` đặt tại thư mục gốc.

### Cấu trúc file mẫu `config.json`:

```json
{
  "gemini": {
    "base_url": "https://generativelanguage.googleapis.com/v1",
    "model_name": "gemma-4-26b-a4b-it",
    "temperature": 0.7,
    "max_tokens": 1024,
    "timeout_ms": 30000,
    "api_key": "YOUR_GEMINI_API_KEY"
  },
  "ollama": {
    "base_url": "http://localhost:11434",
    "model_name": "qwen2.5:7b",
    "temperature": 0.7,
    "max_tokens": 1024,
    "timeout_ms": 30000,
    "embedding": {
      "enabled": true,
      "model": "nomic-embed-text"
    }
  }
}
```

### Ý nghĩa các thông số cấu hình:

| Trường | Kiểu | Mô tả |
| :--- | :--- | :--- |
| `base_url` | `string` | Endpoint URL của nhà cung cấp LLM |
| `model_name` | `string` | Tên model (ví dụ: `gemma-4-26b-a4b-it`, `gemini-1.5-flash`, `qwen2.5:7b`) |
| `temperature` | `float` | Độ sáng tạo / ngẫu nhiên của câu trả lời ($0.0 \rightarrow 1.0$) |
| `max_tokens` | `int` | Số token tối đa cho mỗi lượt phản hồi |
| `timeout_ms` | `int` | Thời gian chờ phản hồi tối đa (milliseconds) |
| `api_key` | `string` | Khóa API (ưu tiên đọc từ file; fallback tự động qua biến môi trường) |
| `embedding` | `object` | Cấu hình mô hình vector embedding qua Ollama (`nomic-embed-text`) |

> **Bảo mật:** `config.json` đã được đưa vào `.gitignore`. Bạn cũng có thể thiết lập API Key qua biến môi trường:
> ```bash
> export GEMINI_API_KEY="your_actual_gemini_api_key"
> ```

---

## 5. Hướng dẫn chạy chương trình

### 5.1 Single-Agent Interactive REPL (`agent_run`)

Dành cho các tác vụ tương tác đàm thoại trực tiếp, có tính **tuần tự** và duy trì ngữ cảnh xuyên suốt:

```bash
# Khởi động REPL mặc định
./build/agent_run

# Chạy với chế độ chi tiết (Verbose mode - hiển thị Thought / Tool Call / Observation)
./build/agent_run --verbose

# Chỉ định file cấu hình khác
./build/agent_run --config custom_config.json --verbose
```

**Minh họa tương tác:**

```text
Nhap yeu cau (go 'exit' de thoat):
> Tính 128 chia 8 và lưu kết quả vào file result.txt
[Thought] Cần tính biểu thức 128 / 8 trước.
[Action] calculator {"expression":"128/8"}
[Observation] 16
[Thought] Ghi kết quả 16 vào file result.txt.
[Action] file_write {"filename":"result.txt","content":"16"}
[Observation] Successfully wrote to result.txt
[Final Answer] Đã tính 128/8 = 16 và lưu vào result.txt thành công.
> exit
```

---

### 5.2 Multi-Agent Parallel Coordinator (`demo_multi_agent`)

Dành cho các tác vụ phức tạp có thể **phân rã thành nhiều subtask độc lập chạy song song** trên các luồng `std::jthread` riêng biệt:

```bash
# Khởi động Interactive Console (mặc định 3 sub-agents)
./build/demo_multi_agent

# Tuỳ chỉnh số lượng sub-agents (ví dụ: 4) và file config
./build/demo_multi_agent -i 4 config.json
```

#### Các phương thức nhập liệu trong Multi-Agent Console:

1. **Nhập một dòng phân tách bởi dấu chấm phẩy (`;`):**
   ```text
   multi-agent> Tính 15 * 17; Tính 2024 - 1999; Ghi 'Hello HCMUS' vào status.txt
   ```
   *Hệ thống tự động kích hoạt 3 sub-agent song song, trao đổi qua `MessageQueue` và xuất bảng tổng hợp kết quả Markdown.*

2. **Chế độ soạn thảo nhiều dòng (`:multi`):**
   ```text
   multi-agent> :multi
   --- Chế độ nhập nhiều dòng (nhập ':run' để thực thi, ':cancel' để huỷ) ---
    subtask #1> Tính căn bậc 2 của 144
    subtask #2> Lấy ngày giờ hiện tại của hệ thống
    subtask #3> :run
   ```

3. **Chạy trực tiếp dạng Batch One-Shot từ dòng lệnh CLI:**
   ```bash
   ./build/demo_multi_agent "Tính 15 * 17; Tính 2024 - 1999" 2 config.json
   ```

---

### 5.3 Chạy Benchmark toàn bộ tập Task

Chạy cả 20 task (10 functional + 10 keyword) trong **một batch duy nhất**.
Harness chỉ dọn artifact một lần, giữ đủ 20 trajectory và xuất một
`benchmark_summary.json` hợp nhất:

```bash
./build/agent_run --benchmark \
  src/benchmark/task.json \
  src/benchmark/keyword_tasks.json
```

---

### 5.4 Chạy 1 Task đơn lẻ theo ID

```bash
./build/agent_run --run-task task_001 src/benchmark/task.json
```

---

### 5.5 Tổng hợp CLI Flags của `agent_run`

| Cú pháp Flag | Tham số | Mô tả chức năng |
| :--- | :--- | :--- |
| *(không đối số)* | — | Mở Interactive REPL (Single-Agent) |
| `--benchmark` | `<tasks.json> [more.json ...]` | Gộp một hoặc nhiều file task vào cùng một batch |
| `--run-task` | `<task_id> <tasks.json>` | Chạy 1 task cụ thể theo mã định danh |
| `--verbose` | — | Bật chế độ in chi tiết từng bước ReAct |
| `--config` | `<config.json>` | Chỉ định đường dẫn file cấu hình LLM |
| `--help` / `-h` | — | Hiển thị thông tin trợ giúp dòng lệnh |

---

## 6. Cấu trúc thư mục dự án

```text
Agent_25127446_25127320_25127151/
├── src/                                      # Toàn bộ mã nguồn C++
│   │
│   ├── agent/                                # Phân hệ Agent Core & Vòng lặp ReAct
│   │   ├── agent_loop.h / .cpp              # AgentLoop: Template Method (Think → Act → Observe)
│   │   ├── loop_detector.h / .cpp           # LoopDetector: Phát hiện Generic Repeat & Ping-Pong
│   │   └── skill_loader.h / .cpp            # SkillLoader: Quét và nạp Markdown Skill theo từ khóa
│   │
│   ├── client/                               # Phân hệ LLM & Embedding Client
│   │   ├── llm_client.h                     # Abstract LLMClient, Struct Message/Token, Concept LLMBackend
│   │   ├── config_loader.h                  # Load config.json và fallback environment variable
│   │   ├── gemini_client.h / .cpp           # Google Gemini Client (Native function calling)
│   │   ├── ollama_client.h / .cpp           # Ollama Client (Local LLM inference)
│   │   ├── embedding_client.h               # Abstract EmbeddingClient & Factory method
│   │   └── ollama_embedding_client.h/.cpp   # Embedding Client qua Ollama (/api/embed)
│   │
│   ├── tools/                                # Phân hệ Quản lý & Thực thi Công cụ (9 Tools)
│   │   ├── tool.h                           # Abstract Tool interface (execute + resetState)
│   │   ├── tool_registry.h / .cpp           # Singleton ToolRegistry & Tool Policy (Allow / Deny)
│   │   ├── calculator.h / .cpp              # [Tool 1] Tính toán biểu thức số học
│   │   ├── exec.h / .cpp                    # [Tool 2] Thực thi lệnh shell (stdout/stderr/timeout)
│   │   ├── read.h / .cpp                    # [Tool 3] Đọc nội dung file trong workspace
│   │   ├── write.h / .cpp                   # [Tool 4] Ghi nội dung vào file trong workspace
│   │   ├── memory_tool.h / .cpp             # [Tool 5] SQLite Persistent Memory + Cosine Vector Search
│   │   ├── web_tool.h / .cpp                # [Tool 6] Tìm kiếm thông tin web qua DuckDuckGo API
│   │   ├── datetime_tool.h / .cpp           # [Tool 7] Lấy thời gian hệ thống định dạng strftime
│   │   ├── http_get_tool.h / .cpp           # [Tool 8] Gửi request HTTP GET lấy dữ liệu REST API
│   │   └── json_parser_tool.h / .cpp        # [Tool 9] Parse chuỗi JSON theo Dot Notation
│   │
│   ├── harness/                              # Phân hệ Đo kiểm, Môi trường & Đánh giá
│   │   ├── harness.h / .cpp                 # HarnessRunner: Điều phối toàn bộ quy trình đo kiểm
│   │   ├── trajectory.h / .cpp              # Cấu trúc Step, Trajectory & Serialization JSON
│   │   ├── environnment.h                   # Abstract Environment interface
│   │   ├── Native_Environment.h / .cpp      # Môi trường thực thi trực tiếp trên host filesystem
│   │   ├── Sandbox_Environment.h / .cpp     # Môi trường cô lập Docker container
│   │   ├── evaluator.h                      # Abstract Evaluator interface
│   │   ├── keyword_evaluator.h / .cpp       # Keyword-based Evaluator
│   │   ├── functional_evaluator.h / .cpp    # Script-based Functional Evaluator
│   │   └── multi_agent_coordinator.h/.cpp   # Coordinator Multi-Agent: std::jthread + MessageQueue
│   │
│   ├── benchmark/                            # Tập dữ liệu & Entry point Benchmark
│   │   ├── task.json                        # 10 task functional
│   │   ├── keyword_tasks.json               # 10 task keyword
│   │   └── run_eval.cpp                     # main() cho benchmark_run
│   │
│   ├── skills/                               # Tri thức nghiệp vụ lưu dạng Markdown (.md)
│   │   ├── task_planner.md                 # Chỉ dẫn lập kế hoạch ReAct cho bài toán tính toán
│   │   ├── file_operations.md              # Chỉ dẫn thao tác tệp an toàn trong workspace
│   │   └── error_recovery.md              # Chỉ dẫn chiến lược tự phục hồi khi gặp lỗi
│   │
│   ├── tests/                                # Bộ 11 Unit & Integration Tests
│   │   ├── test_agent_tool_call.cpp         # Kiểm thử Agent gọi đúng Tool
│   │   ├── test_gemini_response.cpp         # Kiểm thử Parse response Gemini function calling
│   │   ├── test_keyword_evaluator.cpp       # Kiểm thử KeywordEvaluator
│   │   ├── test_functional_evaluator.cpp    # Kiểm thử FunctionalEvaluator
│   │   ├── test_trajectory.cpp              # Kiểm thử Serialization Trajectory → JSON
│   │   ├── test_termination_status.cpp      # Kiểm thử Chuyển đổi trạng thái kết thúc
│   │   ├── test_tools.cpp                   # Kiểm thử Tool policy, File safety, JSON, Memory
│   │   ├── test_exec_tool.cpp               # Kiểm thử Exec stdout, stderr, timeout, exit code
│   │   ├── test_harness_integration.cpp     # Kiểm thử Pipeline Harness end-to-end (Mock LLM)
│   │   ├── test_embedding_memory.cpp        # Kiểm thử SQLite Memory, Vector Search & Thread safety
│   │   └── test_multi_agent.cpp             # Kiểm thử MultiAgentCoordinator song song
│   │
│   └── docs/                                 # Tài liệu thiết kế hệ thống chuẩn Mermaid UML
│       ├── class_diagram.md                 # Class Diagram toàn bộ hệ thống
│       ├── component_diagram.md             # Component Diagram kiến trúc phân tầng
│       ├── sequence_diagram_agent.md        # Sequence Diagram 1 phiên chạy AgentLoop
│       └── sequence_diagram_harness.md      # Sequence Diagram HarnessRunner chạy batch
│
├── results/                                  # Thư mục chứa kết quả benchmark (JSON format)
│   ├── benchmark_summary.json               # Tổng hợp tỷ lệ thành công, điểm số, thời gian
│   └── trajectory_task_001..020.json        # Dấu vết của batch đầy đủ 20 task
│
├── workspace/                                # Không gian làm việc cô lập cho các task
├── main.cpp                                 # Entry point chính của agent_run
├── demo_multi_agent.cpp                      # Entry point của demo_multi_agent
├── config.json                              # Cấu hình API LLM & Database (gitignored)
├── CMakeLists.txt                           # File build CMake đa mục tiêu chuẩn C++23
├── .gitignore                               # Quy tắc loại trừ file nhạy cảm và binary
└── README.md                                # Hướng dẫn kỹ thuật và sử dụng chi tiết
```

---

## 7. Kiến trúc hệ thống & Thiết kế Hướng đối tượng

### 7.1 Mô hình Phân tầng (Separation of Concerns)

Hệ thống được thiết kế phân tầng nghiêm ngặt nhằm đảm bảo **tính độc lập và khớp nối lỏng (Loose Coupling)**:

```text
[User Task / CLI Input]
          │
          ▼
┌───────────────────────────────────────────────────────────┐
│                        AgentLoop                          │
│                                                           │
│  ┌───────────────┐     ┌───────────────┐     ┌──────────┐ │
│  │     Think     │────▶│      Act      │────▶│ Observe  │ │
│  │ (LLM Response)│     │  (Tool Call)  │     │ (Result) │ │
│  └───────────────┘     └───────────────┘     └──────────┘ │
│          ▲                     │                   │      │
│          │                     ▼                   │      │
│     SkillLoader          ToolRegistry              │      │
│   (Prompt Inject)     (9 Concrete Tools)           │      │
│          │                     │                   │      │
│     LoopDetector ◀─────────────┴───────────────────┘      │
│  (Anti-Repeat Guard)                                      │
└───────────────────────────────────────────────────────────┘
          │ step_hook (std::function Callback / Observer Pattern)
          ▼
┌───────────────────────────────────────────────────────────┐
│                       HarnessRunner                       │
│                                                           │
│  Environment ──▶ Trajectory Recorder ──▶ Evaluator       │
│  (Native/Docker)  (results/trajectory.json) (Keyword/Func) │
└───────────────────────────────────────────────────────────┘
```

> **Nguyên tắc then chốt:** `AgentLoop` **hoàn toàn không biết** `HarnessRunner` tồn tại. Khi chạy benchmark, Harness tiêm một hàm callback (`step_hook`) vào `AgentLoop`. Mỗi khi hoàn thành một bước ReAct, Agent kích hoạt hook để truyền dữ liệu `Step` sang Harness mà không gây phụ thuộc ngược (Dependency Inversion).

---

### 7.2 Các Design Patterns áp dụng

Dự án áp dụng chặt chẽ **6 mẫu thiết kế hướng đối tượng (GoF & Modern Patterns)**:

| Pattern | Vị trí áp dụng trong mã nguồn | Mục đích & Lợi ích thiết kế |
| :--- | :--- | :--- |
| **Template Method** | `AgentLoop::run()` định nghĩa khung thuật toán `think()` $\rightarrow$ `act()` $\rightarrow$ `observe()` | Khung xương ReAct cố định; các bước con khai báo `virtual protected` cho phép lớp con tùy biến hoặc mock test |
| **Observer / Hook** | `AgentLoop::setStepHook()` kết hợp `HarnessRunner` | Thu thập `Trajectory` thời gian thực mà không làm Agent bị phụ thuộc vào tầng Harness |
| **Strategy** | `Evaluator` (`KeywordEvaluator`, `FunctionalEvaluator`) và `EmbeddingClient` (`OllamaEmbeddingClient`) | Hoán đổi linh hoạt thuật toán đánh giá và vector nhúng tại runtime |
| **Singleton / Registry** | `ToolRegistry::getInstance()` | Điểm quản lý tập trung toàn bộ công cụ, hỗ trợ đăng ký động và kiểm soát Tool Policy (Allow / Deny) |
| **Factory Method** | `makeAgentLoop<T>()`, `makeOllamaEmbeddingClient()`, `ConfigLoader` | Khởi tạo đối tượng trừu tượng, tận dụng C++20 Concepts kiểm tra tính tương thích tại compile-time |
| **Producer-Consumer** | `MessageQueue<T>` template class | Hàng đợi an toàn đa luồng (`std::mutex` + `std::condition_variable`) phục vụ Multi-Agent |

---

## 8. Bảng kỹ thuật C++ Hiện đại (C++17 / C++20 / C++23 / C++26)

Hệ thống tận dụng tối đa sức mạnh của các chuẩn C++ mới nhất:

| Chuẩn C++ | Tính năng áp dụng | Vị trí minh chứng trong Codebase |
| :--- | :--- | :--- |
| **C++17** | `std::variant<ToolCall, FinalAnswer>` | Biểu diễn type-safe hành động của Agent (`trajectory.h`, `agent_loop.h`) |
| **C++17** | `std::optional<T>` | Kết quả trả về có thể rỗng của Tool, Skill, Token usage (`tool.h`, `skill_loader.h`) |
| **C++17** | `std::visit` + `if constexpr` | Thuật toán so khớp variant an toàn tuyệt đối trong `LoopDetector::isSameAction` |
| **C++17** | `std::filesystem` | Quét và nạp tệp kỹ năng Markdown động trong `SkillLoader::load_skills` |
| **C++17** | `std::function` + Lambdas | Callback hook trong Observer Pattern (`AgentLoop::setStepHook`) |
| **C++17** | Smart Pointers (`shared_ptr`, `unique_ptr`) | Quản lý vòng đời đối tượng tự động (RAII), triệt tiêu memory leak |
| **C++20** | **Concepts (`template <LLMBackend T>`)** | Ràng buộc kiểu dữ liệu cho LLM Client tại compile-time (`llm_client.h`, `agent_loop.h`) |
| **C++20** | **`std::jthread` & `std::stop_token`** | Quản lý vòng đời thread tự động join khi hủy trong `SubAgentHandle` (`multi_agent_coordinator.h`) |
| **C++23** | **`std::expected<LLMResponse, std::string>`** | Xử lý lỗi API/Network tường minh không dùng exception (`llm_client.h`, `agent_loop.cpp`) |
| **C++23** | **`std::print` / `std::println`** | Định dạng xuất nhập hiện đại, hiệu năng cao (`main.cpp`, `run_eval.cpp`) |
| **C++26** | **Deleted functions with message (P2573R2)** | Ngăn chặn sao chép `HarnessRunner` kèm thông điệp chẩn đoán rõ ràng (`harness.h`) |

---

## 9. Danh mục Công cụ (Tools Catalog — 9 Tools)

Mỗi công cụ kế thừa từ lớp cơ sở trừu tượng `Tool` (`execute`, `resetState`, `getName`, `getDescription`):

| Tên Tool | Lớp đối tượng | Phân loại | Mô tả chức năng |
| :--- | :--- | :---: | :--- |
| `calculator` | `CalculatorTool` | Bắt buộc | Tính toán biểu thức toán học (cộng, trừ, nhân, chia, lũy thừa `^`) |
| `exec` | `ExecTool` | Bắt buộc | Thực thi lệnh shell với timeout, thu thập stdout, stderr và mã thoát |
| `file_read` | `FileReadTool` | Bắt buộc | Đọc nội dung file an toàn trong phạm vi workspace |
| `file_write` | `FileWriteTool` | Bắt buộc | Ghi hoặc tạo mới file trong workspace |
| `memory` | `Memory` | Bắt buộc (Bonus) | Lưu và truy vấn bộ nhớ ngữ cảnh qua **SQLite Database** + **Cosine Vector Search** |
| `web_search` | `WebSearchTool` | Mở rộng | Tìm kiếm thông tin trực tuyến qua DuckDuckGo API |
| `datetime` | `DateTimeTool` | Mở rộng | Truy xuất ngày, giờ hệ thống theo định dạng chuẩn |
| `http_get` | `HttpGetTool` | Mở rộng | Gửi HTTP GET request đến các dịch vụ Web REST API |
| `json_parse` | `JsonParserTool` | Mở rộng | Phân tích cú pháp JSON và trích xuất dữ liệu theo Dot Notation |

---

## 10. Hệ thống Kỹ năng Động (Skill System)

`SkillLoader` chịu trách nhiệm mở rộng năng lực Agent mà không cần biên dịch lại mã nguồn C++:
1. **Quét tự động:** Sử dụng `std::filesystem` nạp toàn bộ file `.md` trong thư mục `src/skills/`.
2. **Khớp từ khóa (Keyword Matching):** Tính điểm tần suất xuất hiện của các từ khóa trong yêu cầu của người dùng để chọn ra kỹ năng phù hợp nhất.
3. **Dynamic Prompt Injection:** Tự động chèn nội dung kỹ năng được chọn vào System Prompt cùng với Schema của toàn bộ Tool đã đăng ký.

### Các Skill tích hợp sẵn:
- **`task_planner.md`:** Hướng dẫn chia nhỏ bước giải quyết bài toán toán học và logic.
- **`file_operations.md`:** Hướng dẫn quy chuẩn đọc, ghi, kiểm tra file an toàn trong thư mục làm việc.
- **`error_recovery.md`:** Hướng dẫn phân tích mã lỗi từ Tool để tự điều chỉnh tham số ở bước kế tiếp.

---

## 11. Cơ chế Phát hiện & Phòng chống Vòng lặp (Loop Detection)

`LoopDetector` bảo vệ hệ thống khỏi hiện tượng lặp vô tận (Hallucination Loop):
- **Generic Repeat ($A \rightarrow A \rightarrow A$):** Phát hiện Agent gọi cùng một tool với cùng tham số và nhận về cùng kết quả liên tục.
- **Ping-Pong Loop ($A \rightarrow B \rightarrow A \rightarrow B$):** Phát hiện Agent dao động qua lại giữa 2 hành động liên tiếp.
- **2 Cấp độ xử lý:**
  - `WARNING` ($\ge 2$ lần lặp): Ghi log cảnh báo vào dòng suy luận.
  - `CRITICAL` ($\ge 4$ lần lặp): Chủ động ngắt Agent và trả về trạng thái `AgentTerminationStatus::LoopDetected`.

---

## 12. Đo kiểm & Đánh giá (Harness & Benchmark Suite)

### 12.1 Dataset Benchmark 20 task

Bộ benchmark đầy đủ gồm `task.json` (10 functional) và `keyword_tasks.json`
(10 keyword), được truyền đồng thời cho `runBatch()` để tạo một báo cáo 20 task:

| Cấp độ | Mã Task | Tóm tắt yêu cầu tác vụ | Cơ chế đánh giá |
| :---: | :--- | :--- | :---: |
| **DỄ** | `task_001` | Tính $128 \div 8$ và lưu vào `result.txt` | Functional |
| **DỄ** | `task_002` | Ghi chuỗi chào mừng vào `greeting.txt` | Functional |
| **DỄ** | `task_003` | Ghi nội dung rồi đọc lại từ file `notes.txt` | Functional |
| **DỄ** | `task_004` | Lấy thời gian hệ thống và lưu vào `time.txt` | Functional |
| **TRUNG BÌNH** | `task_005` | Tính toán có điều kiện $\rightarrow$ quyết định file ghi tương ứng | Functional |
| **TRUNG BÌNH** | `task_006` | Chuỗi thao tác: Write $\rightarrow$ Read $\rightarrow$ Calculator $\rightarrow$ Write | Functional |
| **TRUNG BÌNH** | `task_007` | Lưu ngữ cảnh vào `memory` và truy xuất lại chính xác | Functional |
| **TRUNG BÌNH** | `task_008` | Chạy lệnh `exec` có điều kiện kết hợp ghi file kết quả | Functional |
| **KHÓ** | `task_009` | Multi-step: Tính toán, lưu file, đọc lại, cộng dồn và ghi đè kết quả | Functional |
| **KHÓ** | `task_010` | Tìm kiếm `web_search`, phân tích kết quả và tổng hợp vào file | Functional |
| **DỄ–KHÓ** | `task_011`–`task_020` | Tính toán, JSON, exec, file và memory; kiểm tra nội dung câu trả lời cuối | Keyword |

`HarnessRunner::runBatch(const std::vector<std::string>&)` nạp tất cả file,
kiểm tra ID trùng trên toàn bộ dataset, dọn trajectory cũ đúng một lần rồi mới
chạy task. Vì vậy kết quả functional không bị xóa khi chuyển sang nhóm keyword,
và `benchmark_summary.json` luôn phản ánh toàn bộ batch.

### 12.2 Cơ chế Đánh giá (Evaluators)
- **`KeywordEvaluator`:** Kiểm tra sự xuất hiện của các từ khóa bắt buộc trong `final_answer`.
- **`FunctionalEvaluator`:** Thực thi kịch bản shell script kiểm tra trực tiếp trạng thái file hệ thống, exit code 0 là **PASS**.

---

## 13. Tính năng Mở rộng — Điểm thưởng (Bonus Features)

### 13.1 Persistent Memory với SQLite & Vector Embedding Search (+4đ)

- **Cơ sở dữ liệu SQLite (`memory_store.db`):** Sử dụng thư viện `libsqlite3` để lưu trữ khóa-giá trị vĩnh viễn giữa các phiên thực thi của Agent.
- **Vector Embedding Search:** Tích hợp mô hình `nomic-embed-text` (qua Ollama `/api/embed`). Khi lưu ngữ cảnh, vector nhúng được tính toán và lưu trực tiếp.
- **Cosine Similarity:** Khi truy vấn, câu hỏi được nhúng vector và tính toán độ tương đồng Cosine trong C++ để trích xuất nội dung sát nghĩa nhất.
- **Thread-safe:** Sử dụng `std::mutex` bảo vệ vùng nhớ khi chạy đồng thời trong môi trường Multi-Agent.

### 13.2 Multi-Agent Coordination đa luồng song song (+3đ)

- **Kiến trúc Đa luồng:** `MultiAgentCoordinator` phân chia tác vụ tổng thể thành nhiều `SubTaskDefinition` độc lập.
- **Quản lý Luồng Hiện đại:** Mỗi sub-agent được quản lý bởi `SubAgentHandle` sử dụng `std::jthread` (tự động thu hồi tài nguyên và join luồng qua RAII).
- **Hàng đợi Thread-Safe:** Giao tiếp giữa các Agent thông qua template class `MessageQueue<T>`.
- **Độc lập Ngữ cảnh:** Mỗi sub-agent sở hữu một `AgentLoop` riêng biệt, xuất file trajectory riêng (`results/trajectory_sub_*.json`) và được tổng hợp thành bảng báo cáo Markdown.

---

## 14. Bộ Kiểm thử Đơn vị (Unit Tests & CTest)

Chạy toàn bộ 11 test cases tự động bằng CTest:

```bash
cd build
ctest --output-on-failure
```

**Kết quả kiểm thử:**

```text
      Start  1: KeywordEvaluatorTest
 1/11 Test  #1: KeywordEvaluatorTest .............   Passed    0.03 sec
      Start  2: FunctionalEvaluatorTest
 2/11 Test  #2: FunctionalEvaluatorTest ..........   Passed    0.08 sec
      Start  3: TrajectoryTest
 3/11 Test  #3: TrajectoryTest ...................   Passed    0.02 sec
      Start  4: TerminationStatusTest
 4/11 Test  #4: TerminationStatusTest ............   Passed    0.02 sec
      Start  5: AgentToolCallTest
 5/11 Test  #5: AgentToolCallTest ................   Passed    0.04 sec
      Start  6: GeminiResponseTest
 6/11 Test  #6: GeminiResponseTest ...............   Passed    0.04 sec
      Start  7: MultiAgentTest
 7/11 Test  #7: MultiAgentTest ...................   Passed    0.10 sec
      Start  8: ToolsTest
 8/11 Test  #8: ToolsTest ........................   Passed    0.05 sec
      Start  9: ExecToolTest
 9/11 Test  #9: ExecToolTest .....................   Passed    0.24 sec
      Start 10: HarnessIntegrationTest
10/11 Test #10: HarnessIntegrationTest ...........   Passed    0.12 sec
      Start 11: EmbeddingMemoryTest
11/11 Test #11: EmbeddingMemoryTest ..............   Passed    3.26 sec

100% tests passed, 0 tests failed out of 11
Total Test time (real) = 4.12 sec
```

---

## 15. Tài liệu Thiết kế UML (Mermaid Diagrams)

Toàn bộ sơ đồ thiết kế chi tiết chuẩn Mermaid được lưu trữ trong thư mục `src/docs/`:

| Tài liệu | Đường dẫn liên kết | Mô tả nội dung sơ đồ |
| :--- | :--- | :--- |
| **Class Diagram** | [`src/docs/class_diagram.md`](src/docs/class_diagram.md) | Sơ đồ toàn bộ các lớp trong hệ thống, quan hệ thừa kế, sở hữu, quan hệ kết tập và phụ thuộc |
| **Component Diagram** | [`src/docs/component_diagram.md`](src/docs/component_diagram.md) | Sơ đồ cấu trúc các phân hệ (`agent`, `client`, `tools`, `harness`, `skills`) |
| **Sequence Diagram (Agent)** | [`src/docs/sequence_diagram_agent.md`](src/docs/sequence_diagram_agent.md) | Biểu đồ tuần tự chi tiết chu trình ReAct của `AgentLoop` |
| **Sequence Diagram (Harness)** | [`src/docs/sequence_diagram_harness.md`](src/docs/sequence_diagram_harness.md) | Biểu đồ tuần tự quá trình `HarnessRunner` thực thi batch benchmark |

---

## 16. Thông tin Nhóm sinh viên

| STT | Mã số sinh viên (MSSV) | Vai trò & Phân công |
| :---: | :---: | :--- |
| 1 | **25127320** | Agent Core (`AgentLoop`, `LoopDetector`, `SkillLoader`), C++ Modern Features |
| 2 | **25127151** | LLM Client (`GeminiClient`, `OllamaClient`), Tool System (9 Tools, SQLite Memory) |
| 3 | **25127446** | Harness Runner, Environment, Evaluators, Multi-Agent Coordination & Benchmark |

---
*Thành phố Hồ Chí Minh, Năm 2026*
