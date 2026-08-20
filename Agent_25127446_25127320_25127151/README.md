# OOP AI Agent

> Framework AI Agent viết bằng **C++23** (Concepts, `std::expected`, `std::jthread`, `std::print`), hỗ trợ nhiều LLM Provider (Gemini / Ollama), Tool Calling, Skill System, Multi-Agent Coordination, Benchmark Harness và phát hiện vòng lặp.

---

## Mục lục

1. [Yêu cầu & Cài đặt](#1-yêu-cầu--cài-đặt)
2. [Build project](#2-build-project)
3. [Cấu hình API](#3-cấu-hình-api)
4. [Chạy Agent (Single-Agent & Multi-Agent)](#4-chạy-agent-single-agent--multi-agent)
5. [Chạy Benchmark](#5-chạy-benchmark)
6. [Cấu trúc project](#6-cấu-trúc-project)
7. [Kiến trúc & Module](#7-kiến-trúc--module)
8. [Tools có sẵn](#8-tools-có-sẵn)
9. [Skill System](#9-skill-system)
10. [Benchmark & Đánh giá](#10-benchmark--đánh-giá)
11. [Unit Tests](#11-unit-tests)
12. [Tài liệu thiết kế UML](#12-tài-liệu-thiết-kế-uml)
13. [Thành viên](#13-thành-viên)

---

## 1. Yêu cầu & Cài đặt

**Hệ điều hành:** Ubuntu / WSL

**Công cụ:**

- CMake >= 3.20
- GCC >= 14 / Clang >= 18 (hỗ trợ **C++23**: `std::expected`, `std::print`, `std::jthread`, Concepts)

**Cài đặt thư viện hệ thống:**

```bash
sudo apt update
sudo apt install cmake g++ \
    libcurl4-openssl-dev \
    zlib1g-dev \
    nlohmann-json3-dev \
    libsqlite3-dev
```

*(Lưu ý: Nếu hệ thống chưa có sẵn `nlohmann/json`, CMakeLists.txt sẽ tự động tải dự phòng qua FetchContent).*

---

## 2. Build project

Từ thư mục gốc `Agent_25127446_25127320_25127151/`:

```bash
cmake -S . -B build
cmake --build build
```

Sau khi build thành công, các file thực thi được tạo trong `build/`:

| File thực thi | Mục đích |
| --- | --- |
| `build/agent_run` | Entry point chính — Single-Agent REPL (duy trì ngữ cảnh chuỗi), benchmark, run-task |
| `build/demo_multi_agent` | Entry point Multi-Agent — Interactive REPL & CLI điều phối nhiều sub-agent song song |
| `build/benchmark_run` | Chạy benchmark không qua CLI flags (`config.json tasks.json [task_id]`) |
| `build/test_*` | Các file unit test (bao gồm `test_multi_agent`) |

---

## 3. Cấu hình API

Project đọc cấu hình từ file `config.json` ở thư mục gốc.

**Cấu trúc file `config.json`:**

```json
{
  "gemini": {
    "base_url": "https://generativelanguage.googleapis.com/v1",
    "model_name": "gemma-4-26b-a4b-it",
    "temperature": 0.7,
    "max_tokens": 1024,
    "timeout_ms": 30000,
    "api_key": "YOUR_GEMINI_API_KEY"
  }
}
```

**Ý nghĩa các trường:**

| Trường | Kiểu | Ý nghĩa |
| --- | --- | --- |
| `base_url` | string | Endpoint của provider |
| `model_name` | string | Tên model sử dụng |
| `temperature` | float | Độ ngẫu nhiên của câu trả lời (0.0–1.0) |
| `max_tokens` | int | Số token tối đa cho mỗi response |
| `timeout_ms` | int | Thời gian chờ tối đa (milliseconds) |
| `api_key` | string | API Key (ưu tiên file; fallback: biến môi trường `GEMINI_API_KEY`) |

> **Lưu ý:** `config.json` đã được thêm vào `.gitignore`. Không commit file này lên Git vì chứa API Key.

**Cách dùng biến môi trường thay thế:**

```bash
export GEMINI_API_KEY="your_api_key_here"
```

---

## 4. Chạy Agent (Single-Agent & Multi-Agent)

### 4.1 Single-Agent Interactive REPL (`agent_run`)

Dành cho các tác vụ hội thoại tương tác, có tính **tuần tự** và duy trì ngữ cảnh xuyên suốt các bước:

```bash
./build/agent_run
```

Chỉ định config khác hoặc bật verbose:

```bash
./build/agent_run --config my_config.json
./build/agent_run --verbose
./build/agent_run --verbose --config my_config.json
```

**Ví dụ tương tác:**

```text
Nhap yeu cau (go 'exit' de thoat):
> Tính 128 chia 8 và lưu kết quả vào result.txt
16
> exit
```

Agent dừng với một trong 3 trạng thái:

| Trạng thái | Mô tả |
| --- | --- |
| `Completed` | Hoàn thành, trả về `final_answer` |
| `LoopDetected` | Phát hiện vòng lặp (GenericRepeat / PingPong) |
| `MaxStepsReached` | Đã chạy hết số bước tối đa (mặc định 10) |

---

### 4.2 Multi-Agent Interactive REPL (`demo_multi_agent`)

Dành cho các tác vụ **phân tán song song** (embarrassingly parallel), điều phối đồng thời nhiều sub-agent trên các luồng `std::jthread` độc lập:

```bash
# Khởi động chế độ Interactive REPL (mặc định 3 sub-agents, config.json)
./build/demo_multi_agent
```

Tuỳ chỉnh số lượng sub-agent và file cấu hình:

```bash
./build/demo_multi_agent -i 4             # Tối đa 4 sub-agents song song
./build/demo_multi_agent -i 4 config.json # Chỉ định file cấu hình
```

#### Các cách sử dụng `demo_multi_agent`:

1. **Gõ 1 dòng phân tách bởi dấu `;` trong REPL:**
   ```text
   multi-agent> Tính 15 * 17; Tính 2024 - 1999; Ghi dòng chữ 'Hello HCMUS' vào output.txt
   ```
   *Hệ thống tự động tách thành 3 subtask độc lập, kích hoạt 3 sub-agent chạy song song trên các thread `std::jthread` khác nhau và tổng hợp bảng kết quả Markdown.*

2. **Chế độ soạn thảo nhiều dòng (`:multi`) trong REPL:**
   ```text
   multi-agent> :multi
   --- Chế độ nhập nhiều dòng (nhập ':run' để chạy, ':cancel' để huỷ) ---
    subtask #1> Tính căn bậc 2 của 144
    subtask #2> Lấy ngày giờ hiện tại của hệ thống
    subtask #3> :run
   ```

3. **Chạy batch one-shot trực tiếp từ dòng lệnh CLI (không cần vào REPL):**
   ```bash
   # Cách 1: Phân cách bằng dấu ';'
   ./build/demo_multi_agent "Tính 15 * 17; Tính 2024 - 1999" 2 config.json

   # Cách 2: Phân cách bằng ký tự xuống dòng '\n'
   ./build/demo_multi_agent "Tính 15 * 17
Tính 2024 - 1999" 2 config.json
   ```

4. **Thoát chương trình:**
   ```text
   multi-agent> exit
   # hoặc: quit
   ```

5. **Xem trợ giúp:**
   ```bash
   ./build/demo_multi_agent --help
   ```

#### Bảng tổng hợp tham số dòng lệnh của `demo_multi_agent`:

| Tham số | Ví dụ | Ý nghĩa |
| --- | --- | --- |
| *(không đối số)* | `./build/demo_multi_agent` | Chạy Interactive REPL với 3 sub-agents mặc định, đọc `config.json` |
| `-i` / `--interactive` | `./build/demo_multi_agent -i 4 config.json` | Bật Interactive REPL với số sub-agent và file config chỉ định |
| `"<task>"` | `./build/demo_multi_agent "Task A; Task B" 2` | Chạy batch one-shot task (phân tách bởi `;` hoặc `\n`) rồi thoát |
| `-h` / `--help` | `./build/demo_multi_agent --help` | In cú pháp hướng dẫn sử dụng dòng lệnh |

> **Lưu ý về tính độc lập của Multi-Agent:**
> Trong mô hình Multi-Agent song song, mỗi sub-agent chạy độc lập trong thread riêng với ngữ cảnh (`m_history`) tách biệt tại thời điểm chạy. Đối với các tác vụ phụ thuộc logic bước trước $\rightarrow$ bước sau (như *lấy kết quả câu 1 để ghi vào câu 3*), hãy sử dụng Single-Agent `agent_run`.

---

### 4.3 Benchmark — chạy toàn bộ tập task

```bash
./build/agent_run --benchmark src/benchmark/task.json
```

Chạy tất cả 10 task, ghi kết quả vào `results/`, in báo cáo success rate ra stdout.

---

### 4.4 Run-task — chạy 1 task theo ID

```bash
./build/agent_run --run-task task_001 src/benchmark/task.json
```

Chạy đúng 1 task, in kết quả `[PASS/FAIL]`, score, thời gian, token. Exit code = 0 nếu pass, 1 nếu fail.

---

### 4.5 Xem trợ giúp CLI Flags

```bash
./build/agent_run --help
```

**Tổng hợp tất cả flags của `agent_run`:**

| Flag | Tham số | Mô tả |
| --- | --- | --- |
| *(không có)* | — | Interactive REPL (Single-Agent) |
| `--benchmark` | `<tasks.json>` | Chạy toàn bộ benchmark |
| `--run-task` | `<id> <tasks.json>` | Chạy 1 task theo ID |
| `--verbose` | — | In chi tiết Thought / Tool Call / Observation |
| `--config` | `<config.json>` | Đường dẫn file config LLM (mặc định: `config.json`) |
| `--help` / `-h` | — | In hướng dẫn sử dụng |

---

## 5. Chạy Benchmark

Chạy toàn bộ 10 task benchmark:

```bash
./build/agent_run --benchmark src/benchmark/task.json
```

Chạy 1 task cụ thể (ví dụ task_001):

```bash
./build/agent_run --run-task task_001 src/benchmark/task.json
```

Kết quả được ghi tự động vào thư mục `results/`:

```text
results/
├── benchmark_summary.json       # Tổng hợp: pass rate, score, thời gian
├── trajectory_task_001.json     # Chi tiết từng bước của task 001
├── trajectory_task_002.json
└── ...
```

**In báo cáo tổng hợp** (được in ra stdout sau khi benchmark hoàn tất):

```text
[Benchmark Summary]
Total tasks  : 10
Passed       : 10
Failed       : 0
Success rate : 100%
Avg score    : 1
Avg time/task: 14789 ms
```

---

## 6. Cấu trúc project

```text
Agent_25127446_25127320_25127151/
├── src/                                      # Toàn bộ source code
│   │
│   ├── agent/                                # Agent Loop & Điều phối suy luận
│   │   ├── agent_loop.h / .cpp              # Vòng lặp ReAct: Think → Act → Observe (Template Method)
│   │   ├── loop_detector.h / .cpp           # Phát hiện vòng lặp (GenericRepeat, PingPong)
│   │   └── skill_loader.h / .cpp            # Nạp & chọn Markdown Skill theo keyword
│   │
│   ├── client/                               # LLM Client Layer
│   │   ├── llm_client.h                     # Abstract LLMClient + struct dùng chung + concept LLMBackend
│   │   ├── config_loader.h                  # Đọc config.json + fallback env var
│   │   ├── gemini_client.h / .cpp           # Client Google Gemini API (native function calling)
│   │   ├── ollama_client.h / .cpp           # Client Ollama (local LLM)
│   │   ├── embedding_client.h               # Abstract EmbeddingClient + factory makeOllamaEmbeddingClient()
│   │   └── ollama_embedding_client.h/.cpp   # Embedding qua Ollama (nomic-embed-text)
│   │
│   ├── tools/                                # Tool Registry & các Tool cụ thể
│   │   ├── tool.h                           # Abstract Tool (execute + resetState)
│   │   ├── tool_registry.h / .cpp           # Singleton ToolRegistry (+ deny/allow tool policy)
│   │   ├── calculator.h / .cpp              # Tool: tính toán biểu thức số học
│   │   ├── exec.h / .cpp                    # Tool: thực thi lệnh shell (exec)
│   │   ├── read.h / .cpp                    # Tool: đọc nội dung file (file_read)
│   │   ├── write.h / .cpp                   # Tool: ghi nội dung vào file (file_write)
│   │   ├── memory_tool.h / .cpp             # Tool: memory (key-value + embedding search + persist JSON)
│   │   ├── web_tool.h / .cpp                # Tool: tìm kiếm web (web_search)
│   │   ├── datetime_tool.h / .cpp           # Tool: lấy thời gian hệ thống (datetime)
│   │   ├── http_get_tool.h / .cpp           # Tool: gửi HTTP GET (http_get)
│   │   └── json_parser_tool.h / .cpp        # Tool: parse JSON theo dot notation (json_parse)
│   │
│   ├── harness/                              # Benchmark Harness, Environment & Evaluator
│   │   ├── harness.h / .cpp                 # HarnessRunner: điều phối pipeline benchmark
│   │   ├── trajectory.h / .cpp              # Struct Step, Trajectory, TerminationStatus
│   │   ├── evaluator.h                      # Abstract class Evaluator
│   │   ├── keyword_evaluator.h / .cpp       # Đánh giá bằng cách kiểm tra từ khóa
│   │   ├── functional_evaluator.h / .cpp    # Đánh giá bằng cách chạy shell script
│   │   ├── environnment.h                   # Abstract class Environment (setup/teardown/execute)
│   │   ├── Native_Environment.h / .cpp      # Môi trường thực: làm việc trên filesystem host
│   │   ├── Sandbox_Environment.h / .cpp     # Môi trường cô lập: Docker container
│   │   └── multi_agent_coordinator.h/.cpp   # Multi-agent: jthread + MessageQueue thread-safe
│   │
│   ├── benchmark/                            # Dataset & Entry point benchmark
│   │   ├── task.json                        # 10 task benchmark (3 mức: DE / TB / KHO)
│   │   └── run_eval.cpp                     # main() cho benchmark_run executable
│   │
│   ├── skills/                               # Markdown Prompt Skills
│   │   ├── task_planner.md                 # Skill: lập kế hoạch, dùng ReAct loop
│   │   ├── file_operations.md              # Skill: thao tác file an toàn trong workspace
│   │   └── error_recovery.md              # Skill: phục hồi khi tool trả về lỗi
│   │
│   ├── tests/                                # Unit & integration tests (11 file)
│   │   ├── test_agent_tool_call.cpp         # Agent gọi tool đúng
│   │   ├── test_gemini_response.cpp         # Parse response từ Gemini API
│   │   ├── test_keyword_evaluator.cpp       # KeywordEvaluator
│   │   ├── test_functional_evaluator.cpp    # FunctionalEvaluator (shell script)
│   │   ├── test_trajectory.cpp              # Serialization Trajectory → JSON
│   │   ├── test_termination_status.cpp      # Enum TerminationStatus → string
│   │   ├── test_tools.cpp                   # Tool policy, file safety, calculator, JSON, memory
│   │   ├── test_exec_tool.cpp               # stdout/stderr/exit code/timeout của exec
│   │   ├── test_harness_integration.cpp     # Pipeline Harness end-to-end (Fake LLM)
│   │   ├── test_embedding_memory.cpp        # Embedding search + persistence + fallback
│   │   └── test_multi_agent.cpp             # Coordinator + subtask song song
│   │
│   └── docs/                                 # Tài liệu thiết kế UML
│       ├── class_diagram.md                 # Class Diagram toàn bộ hệ thống
│       ├── component_diagram.md             # Component Diagram các module & dependency
│       ├── sequence_diagram_agent.md        # Sequence Diagram 1 lần agent run
│       └── sequence_diagram_harness.md      # Sequence Diagram HarnessRunner run batch
│
├── results/                                  # Output benchmark (tự động tạo, gitignored)
│   ├── benchmark_summary.json               # Tổng hợp kết quả toàn bộ batch
│   └── trajectory_task_001..010.json        # Chi tiết trajectory từng task
│
├── workspace/                                # Workspace riêng từng task khi chạy benchmark (gitignored)
│   ├── task_001/                            # Workspace riêng biệt cho task_001
│   ├── task_002/
│   └── ... (task_003 → task_010)
│
├── demo_multi_agent.cpp                      # File thực thi tương tác Multi-Agent REPL
├── main.cpp                                 # Entry point chính cho agent_run
├── config.json                              # Cấu hình API (gitignored — chứa API Key)
├── CMakeLists.txt                           # Build script CMake (C++23, targets: agent_run, demo_multi_agent, benchmark_run, tests)
├── .gitignore
└── README.md
```

---

## 7. Kiến trúc & Module

Project theo mô hình **ReAct (Reason + Act)** với các layer tách biệt hoàn toàn:

```text
[User Task]
     │
     ▼
┌──────────────────────────────────────────────────────┐
│                      AgentLoop                       │
│                                                      │
│  ┌────────────┐   ┌────────────┐    ┌──────────────┐ │
│  │   Think    │──▶│    Act     │──▶│   Observe    │ │
│  │ (LLMClient)│   │ (ToolCall) │    │ (ToolResult) │ │
│  └────────────┘   └────────────┘    └──────────────┘ │
│        ▲                                             │
│        │  inject system prompt + skills              │
│   SkillLoader                                        │
│        │                                             │
│   LoopDetector (GenericRepeat / PingPong)            │
└──────────────────────────────────────────────────────┘
     │ step_hook (Observer pattern)
     ▼
┌──────────────────────────────────────┐
│            HarnessRunner             │
│   Environment  →  AgentLoop          │
│   → Trajectory  →  Evaluator         │
│   → trajectory_task_XXX.json         │
└──────────────────────────────────────┘
```

### Các nguyên tắc thiết kế chính

- **AgentLoop không biết Harness tồn tại**: HarnessRunner inject `step_hook` vào AgentLoop từ bên ngoài (Observer/Hook pattern).
- **Separation of Concerns**: `ConfigLoader` đọc config, `LLMClient` chỉ lo gọi API, `ToolRegistry` singleton quản lý tool.
- **Polymorphism**: `LLMClient`, `Tool`, `Evaluator`, `Environment`, `EmbeddingClient` đều là abstract class với pure virtual methods.
- **Cô lập lỗi trong Benchmark**: 1 task lỗi không sập cả batch — mỗi task có workspace riêng.

### Design Patterns được dùng

| Pattern | Lớp / Vị trí | Mô tả |
| --- | --- | --- |
| **Template Method** | `AgentLoop::run()` = `think() → act() → observe()` | Skeleton ReAct; subclass override từng bước |
| **Observer / Hook** | `AgentLoop::setStepHook()` → `HarnessRunner` | Agent không biết Harness, Harness thu thập `Step` |
| **Strategy** | `Evaluator` / `EmbeddingClient` | Đổi thuật toán chấm điểm / embedding tại runtime |
| **Singleton** | `ToolRegistry::getInstance()` | Registry tool dùng chung toàn app |
| **Factory** | `makeOllamaEmbeddingClient()`, `makeAgentLoop<T>()`, `ConfigLoader` | Tạo object, ràng buộc compile-time (concept `LLMBackend`) |
| **Template class** | `MessageQueue<T>` | Queue thread-safe cho multi-agent |

---

## 8. Tools có sẵn

| Tool name | Class | Mô tả |
| --- | --- | --- |
| `calculator` | `CalculatorTool` | Tính toán biểu thức số học (cộng, trừ, nhân, chia, lũy thừa) |
| `exec` | `ExecTool` | Chạy lệnh shell và trả về stdout |
| `file_read` | `FileReadTool` | Đọc nội dung một file trong workspace |
| `file_write` | `FileWriteTool` | Ghi nội dung vào một file trong workspace |
| `memory` | `Memory` | Lưu và truy xuất key-value trong session hiện tại (hỗ trợ vector search + embedding-based search + persist JSON) |
| `web_search` | `WebSearchTool` | Tìm kiếm web qua DuckDuckGo Instant Answer API và trả về kết quả |
| `datetime` | `DateTimeTool` | Lấy thời gian hệ thống theo định dạng `strftime` |
| `http_get` | `HttpGetTool` | Gửi request HTTP GET và trả về nội dung response |
| `json_parse` | `JsonParserTool` | Parse chuỗi JSON và truy xuất giá trị theo dot notation |

**Cách Agent gọi tool** (native function calling qua Gemini API):

```text
LLM → { "tool_call": { "name": "calculator", "args": { "expression": "128/8" } } }
       → ToolRegistry::executeTool("calculator", args)
       → "16"
       → Observation: "16"
```

---

## 9. Skill System

`SkillLoader` quét thư mục `src/skills/`, đọc các file `.md`, và **tự động chọn skill phù hợp** dựa trên keyword match với task description.

| Skill file | Keywords kích hoạt | Mô tả |
| --- | --- | --- |
| `task_planner.md` | tính, toán, calculator, cộng, trừ, nhân, chia... | Hướng dẫn ReAct loop cho bài toán tính toán |
| `file_operations.md` | file, tệp, đọc, ghi, lưu, thư mục | Hướng dẫn thao tác file an toàn trong workspace |
| `error_recovery.md` | *(kích hoạt khi tool trả về lỗi)* | Hướng dẫn phục hồi khi gặp lỗi |

Skill được **inject vào system prompt** trước khi gọi LLM, giúp Agent có context hành động đúng hơn.

---

## 10. Benchmark & Đánh giá

### 10.1 Dataset (`src/benchmark/task.json`)

10 task chia theo 3 mức độ:

| Mức | Task | Mô tả |
| --- | --- | --- |
| **DE** (Dễ) | task_001 | Tính 128÷8, lưu vào `result.txt` |
| **DE** | task_002 | Ghi chuỗi vào `greeting.txt` |
| **DE** | task_003 | Ghi rồi đọc lại file `notes.txt` |
| **DE** | task_004 | Lấy thời gian hệ thống, lưu vào `time.txt` |
| **TB** (Trung bình) | task_005 | Tính có điều kiện → chọn file ghi |
| **TB** | task_006 | Chuỗi: write → read → calculator → write |
| **TB** | task_007 | Dùng `memory` tool lưu và truy xuất |
| **TB** | task_008 | Exec có điều kiện kết hợp file_write |
| **KHO** (Khó) | task_009 | Multi-step: tính tổng, lưu, đọc, cộng thêm, ghi đè |
| **KHO** | task_010 | web_search + xử lý kết quả → lưu file |

### 10.2 Cơ chế đánh giá

| Eval type | Class | Cơ chế |
| --- | --- | --- |
| `keyword` | `KeywordEvaluator` | Kiểm tra `final_answer` có chứa tất cả từ khóa yêu cầu |
| `functional` | `FunctionalEvaluator` | Chạy shell script (`eval_script`), exit code 0 = pass |

### 10.3 Persistent Memory với Vector Search (Bonus)

`Memory` tool hỗ trợ **embedding-based similarity search** thay cho keyword search:

- Khi `save`, nội dung được nhúng qua **`nomic-embed-text`** (Ollama `/api/embed`) và lưu kèm embedding.
- Khi `load`, query được nhúng và tìm bằng **cosine similarity** trong C++ (`cosineSimilarityVectors`).
- Memory được **persist** sang `memory_store.json` nên sống sót giữa các lần chạy.
- Nếu Ollama chưa chạy, tự động fallback về trigram vector search.

**Cấu hình embedding trong `config.json`:**

```json
{
  "ollama": {
    "base_url": "http://localhost:11434",
    "embedding": {
      "enabled": true,
      "model": "nomic-embed-text"
    }
  }
}
```

Chạy `ollama pull nomic-embed-text` trước khi dùng. Các file liên quan: `src/client/embedding_client.h`, `src/client/ollama_embedding_client.h/.cpp`, `src/tools/memory_tool.h/.cpp`.

### 10.4 Multi-Agent Coordination (Bonus)

Phân tách 1 task phức tạp thành nhiều `SubTaskDefinition` và điều phối chạy song song đa luồng với nhiều sub-agent:

#### A. Cách chạy trên Terminal

1. **Khởi động Interactive Multi-Agent Console (REPL):**
   ```bash
   ./build/demo_multi_agent                  # Mặc định tối đa 3 sub-agents
   ./build/demo_multi_agent -i 4             # Tùy chỉnh số lượng sub-agents (vd: 4)
   ./build/demo_multi_agent -i 4 config.json # Chỉ định file config
   ```

2. **Cách gõ lệnh trong Console `multi-agent>`:**
   * **Cách 1: Gõ 1 dòng phân tách bởi dấu `;`**
     ```text
     multi-agent> Tính 15 * 17; Tính 2024 - 1999; Ghi nội dung 'DONE' vào status.txt
     ```
     *Mỗi mệnh đề ngăn cách bởi dấu `;` sẽ được tự động phân cho 1 sub-agent riêng biệt chạy song song trên 1 thread `std::jthread`.*

   * **Cách 2: Chế độ soạn nhiều dòng (`:multi`)**
     ```text
     multi-agent> :multi
     --- Chế độ nhập nhiều dòng (nhập ':run' để chạy, ':cancel' để huỷ) ---
      subtask #1> Tính 100 * 25
      subtask #2> Lấy ngày giờ hiện tại
      subtask #3> :run
     ```

   * **Cách 3: Thoát chương trình**
     ```text
     multi-agent> exit
     ```

3. **Chạy trực tiếp từ dòng lệnh CLI (Batch / One-shot):**
   ```bash
   ./build/demo_multi_agent "Tính 15 * 17\nTính 2024 - 1999" 2 config.json
   ```

4. **Chạy Unit Test tự động cho Multi-Agent:**
   ```bash
   ./build/test_multi_agent
   ```

---

#### B. Kiến trúc & Luồng thực thi

```text
HarnessRunner::runMultiAgent(subtasks)
        │
        ▼
MultiAgentCoordinator::runParallel(subtasks)
        │  spawn 1 std::jthread / sub-agent (SubAgentHandle)
        ├──▶ sub-agent 0: AgentLoop.run(subtask[0])
        │        │  trao đổi qua MessageQueue (mutex + condition_variable)
        ├──▶ sub-agent 1: AgentLoop.run(subtask[1])
        │        │
        ├──▶ sub-agent N: AgentLoop.run(subtask[N])
        │
        ▼
  futures.get() → vector<SubAgentResult>
        │  mỗi sub-agent có trajectory riêng → export kết quả
        ▼
MultiAgentCoordinator::mergeResults(results) → Bảng tổng hợp Markdown
```

- **SubAgentHandle**: quản lý vòng đời 1 sub-agent thread (`std::jthread` tự join khi hủy qua RAII, `std::stop_token` cho cooperative cancellation).
- **MessageQueue<T>**: template class thread-safe queue (`push`, blocking `pop`, `try_pop`, `pop_for`, `close`).
- **Phân tách subtask**: `splitTaskIntoSubtasks(instruction, num_agents)` tự động bẻ nhỏ task theo ký tự xuống dòng `\n` hoặc dấu `;`.
- **Độc lập ngữ cảnh (Context Isolation)**: Mỗi sub-agent sở hữu một `AgentLoop` riêng biệt chạy đồng thời. Đối với bài toán cần dữ liệu tuần tự từ bước trước, hãy sử dụng Single-Agent `agent_run`.
- **Files liên quan**: `src/harness/multi_agent_coordinator.h/.cpp`, `demo_multi_agent.cpp`, `src/tests/test_multi_agent.cpp`.

---

## 11. Unit Tests

Chạy toàn bộ test với CTest:

```bash
cd build
ctest --output-on-failure
```

Hoặc chạy từng test riêng lẻ:

```bash
./build/test_keyword_evaluator
./build/test_functional_evaluator
./build/test_trajectory
./build/test_termination_status
./build/test_agent_tool_call
./build/test_gemini_response
./build/test_tools
./build/test_exec_tool
./build/test_harness_integration
./build/test_embedding_memory
./build/test_multi_agent
```

| Test executable | CTest name | Nội dung |
| --- | --- | --- |
| `test_keyword_evaluator` | `KeywordEvaluatorTest` | Kiểm tra logic match keyword |
| `test_functional_evaluator` | `FunctionalEvaluatorTest` | Kiểm tra chạy shell script evaluator |
| `test_trajectory` | `TrajectoryTest` | Kiểm tra serialization Step/Trajectory → JSON |
| `test_termination_status` | `TerminationStatusTest` | Kiểm tra enum → string conversion |
| `test_agent_tool_call` | `AgentToolCallTest` | Test tích hợp: Agent nhận task và gọi tool đúng |
| `test_gemini_response` | `GeminiResponseTest` | Kiểm tra parse multipart/function-call response từ Gemini |
| `test_tools` | `ToolsTest` | Kiểm tra tool policy, file safety, calculator, JSON và memory |
| `test_exec_tool` | `ExecToolTest` | Kiểm tra stdout, stderr, exit code và timeout của exec |
| `test_harness_integration` | `HarnessIntegrationTest` | Chạy pipeline Harness end-to-end bằng Fake LLM, không gọi API |
| `test_embedding_memory` | `EmbeddingMemoryTest` | Kiểm tra embedding search, persistence và fallback |
| `test_multi_agent` | `MultiAgentTest` | Kiểm tra coordinator và chia subtask song song |

---

## 12. Tài liệu thiết kế UML

Toàn bộ tài liệu kỹ thuật dạng **Mermaid** nằm trong `src/docs/` (xem được trực tiếp trên GitHub):

| File | Nội dung |
| --- | --- |
| [`src/docs/class_diagram.md`](src/docs/class_diagram.md) | **Class Diagram** toàn bộ hệ thống: thể hiện rõ inheritance (`<|--`), composition (`*--`), aggregation (`o--`), dependency (`..>`) giữa các lớp |
| [`src/docs/component_diagram.md`](src/docs/component_diagram.md) | **Component Diagram** tổng quan các module (`agent/`, `client/`, `tools/`, `harness/`, `benchmark/`, `skills/`) và quan hệ dependency |
| [`src/docs/sequence_diagram_agent.md`](src/docs/sequence_diagram_agent.md) | **Sequence Diagram** một lần `AgentLoop::run()` hoàn chỉnh: từ khi nhận task → Think/Act/Observe → trả `AgentRunResult` |
| [`src/docs/sequence_diagram_harness.md`](src/docs/sequence_diagram_harness.md) | **Sequence Diagram** `HarnessRunner::runBatch()` chạy batch evaluation: loadTasks → từng task (env → agent → evaluator) → báo cáo |

---

## 13. Thành viên

| MSSV |
| --- |
| 25127446 |
| 25127320 |
| 25127151 |
