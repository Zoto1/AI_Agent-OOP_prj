**TRƯỜNG ĐẠI HỌC KHOA HỌC TỰ NHIÊN**

**ĐẠI HỌC QUỐC GIA TP.HCM**

**KHOA CÔNG NGHỆ THÔNG TIN**

━━━━━━━━━━━━━━━━━━━━━━━━

**ĐỒ ÁN MÔN HỌC**

**LẬP TRÌNH HƯỚNG ĐỐI TƯỢNG**

━━━━━━━━━━━━━━━━━━━━━━━━

**XÂY DỰNG AI AGENT VỚI OLLAMA API**

*Áp dụng mô hình hướng đối tượng cho hệ thống*

*Agent – Tool – Skill – Harness – Evaluator*

Năm 2026

# **MỤC LỤC**

I.  Tổng quan đồ án	3

II.  Mục tiêu học tập	4

III.  Yêu cầu chức năng	4

IV.  Thiết kế hướng đối tượng	6

V.  Kỹ thuật C++ bắt buộc sử dụng	9

VI.  Cấu trúc thư mục dự án	10

VII.  Harness Engineering & Evaluation	11

VIII.  Tiêu chí chấm điểm	12

IX.  Hướng dẫn nộp bài	14

X.  Gợi ý mở rộng (điểm thưởng)	15

# **I. TỔNG QUAN ĐỒ ÁN**

Trong đồ án này, sinh viên sẽ xây dựng một **AI Agent framework** bằng C++17 trở lên kết nối với **Ollama API**, một local LLM inference server. Hệ thống mô phỏng kiến trúc của các framework agent hiện đại như OpenClaw, LangChain, LlamaIndex nhưng được thiết kế và triển khai hoàn toàn từ đầu với ngôn ngữ C++ thuần túy.

Chú ý **Google Colab** / Kaggle có thể được sử dụng để cung cấp backend hỗ trợ suy luận nhờ tài nguyên GPU miễn phí. Sinh viên có thể sử dụng mô hình **Gemma4** để có thể thực hiện tool call lẫn suy luận thị giác.

Mã nguồn mẫu sử dụng Google Colab với Gemma4 ở đây: [https://colab.research.google.com/drive/1chSCUNnLCj4Gj4daq6Erp7h8tF0C28lE?usp=sharing](https://colab.research.google.com/drive/1chSCUNnLCj4Gj4daq6Erp7h8tF0C28lE?usp=sharing) 

Một số giải pháp khác để có token miễn phí và model mạnh hơn là:

+ NVIDIA NIM  
+ OpenRouter  
+ Google Gemini

Sinh viên lập nhóm từ **1-3** người.

Thay vì chỉ "gọi API và in kết quả", sinh viên phải thiết kế một **hệ thống phân lớp rõ ràng**: LLM client, Tool registry, Skill loader, Agent loop, và Harness evaluation. Mỗi tầng là một bài tập OOP độc lập nhưng kết hợp thành hệ thống hoàn chỉnh.

| Bối cảnh kỹ thuật |
| :---- |
| • Ollama: local LLM server, expose REST API tương thích OpenAI format  • Tool calling: LLM sinh text có format đặc biệt, agent parse và thực thi hàm C++ • Skill system: Markdown instruction files đặt vào system prompt • Harness: tầng infrastructure bao quanh agent loop để đo lường và reproduce |

# **II. MỤC TIÊU HỌC TẬP**

Sau khi hoàn thành đồ án, sinh viên đạt được:

* **Thiết kế class hierarchy phù hợp cho hệ thống agent thực tế (không chỉ bài toán sách giáo khoa)**

* **Áp dụng tối thiểu 4 design patterns trong một hệ thống thống nhất**

* **Sử dụng thành thạo C++17 trở lên**

* **Hiểu và triển khai tầng abstraction giữa agent logic và infrastructure**

* **Viết evaluation harness có khả năng đo lường performance của LLM agent**

* **Đọc và tích hợp thư viện bên thứ ba (libcurl, nlohmann/json) vào dự án C++**

# **III. YÊU CẦU CHỨC NĂNG**

## **3.1  Tầng LLM Client**

Sinh viên xây dựng lớp OllamaClient với các yêu cầu sau:

* Gửi HTTP POST đến Ollama API endpoint /api/chat \- đặc trưng của ollama (hoặc /chat/completions \- tương thích openai hoặc endpoint tùy theo lựa chọn của sinh viên)

* Hỗ trợ cả text-only và multimodal (gửi ảnh base64) qua cùng một interface (nên chọn model hỗ trợ ảnh)

* Xử lý lỗi: timeout, connection refused, malformed JSON response

* Cấu hình được: base URL, model name, temperature, max\_tokens

## **3.2  Tầng Tool Registry**

Hệ thống tool phải đáp ứng:

* Đăng ký tool động tại runtime (không hardcode)

* Mỗi tool có: name, description (cho LLM đọc), execute function

* **Sinh viên tự triển khai tối thiểu 5 tool sau:**

  * exec: chạy lệnh shell, trả về stdout/stderr

  * read\_file / write\_file: đọc và ghi file

  * web\_search: tìm kiếm qua SearXNG hoặc DuckDuckGo API

  * memory\_save / memory\_search: lưu và truy vấn memory qua SQLite

  * calculator: tính biểu thức số học

* Tool policy: có thể allow/deny theo danh sách tên

Sinh viên cần tham khảo OpenClaw hoặc Hermes để thêm vào ít nhất 3 tool thuộc 3 loại khác nhau. 

## **3.3  Tầng Skill System**

* Load skill từ file .md trong thư mục skills/

* Skill selection: chọn skill phù hợp với task dựa trên keyword matching

* Sinh viên tự viết tối thiểu 3 skill file có nội dung thực sự hướng dẫn agent

* Skill được inject vào system prompt trước mỗi agent run

## **3.4  Agent Loop**

* Vòng lặp ReAct: Observe → Think → Act → Observe...

* Parse tool call từ LLM response (regex hoặc JSON parsing)

* Duy trì conversation history đúng format Ollama

* Giới hạn max\_steps và xử lý graceful khi đạt giới hạn

## **3.5  Loop Detection**

* Phát hiện ít nhất 2 loại loop: generic repeat và ping-pong

* Configurable threshold (warning vs critical)

* Khi phát hiện loop: log cảnh báo và dừng agent

## **3.6  Harness & Evaluator**

* HarnessRunner: setup environment → run agent → evaluate → record

* Trajectory recording: lưu từng bước (thought, action, result, latency, tokens)

* Tối thiểu 2 Evaluator: KeywordEvaluator và FunctionalEvaluator

* Batch evaluation: chạy tập task, tính success rate

* Export kết quả ra JSON

# **IV. THIẾT KẾ HƯỚNG ĐỐI TƯỢNG**

## **4.1  Class Diagram tổng thể**

| Các lớp cần thiết kế (tối thiểu) |
| :---- |
| LLMClient (abstract)  ←  OllamaClient Tool  ←  ExecTool, FileTool, WebSearchTool, MemoryTool, CalculatorTool ToolRegistry  (manages Tool instances) SkillLoader  (loads \+ selects SKILL.md files) LoopDetector  (detects agent loops) AgentLoop  (core ReAct loop) Environment (abstract)  ←  NativeEnvironment, SandboxEnvironment Evaluator (abstract)  ←  KeywordEvaluator, FunctionalEvaluator, VLMEvaluator Trajectory  \+  Step  (data classes) HarnessRunner  (orchestrates all layers) |

## **4.2  Design Patterns bắt buộc**

| Tiêu chí | Nội dung đánh giá |
| ----- | ----- |
| **Strategy** | Evaluator hierarchy: KeywordEvaluator, FunctionalEvaluator, VLMEvaluator dùng chung interface evaluate() |
| **Template Method** | AgentLoop::run() định nghĩa skeleton, các bước observe()/act() có thể override |
| **Registry / Factory** | ToolRegistry đăng ký và tạo tool instance theo tên |
| **Observer / Hook** | HarnessRunner inject step\_hook vào AgentLoop để record trajectory |

*Ghi chú:* Sinh viên có thể dùng thêm pattern khác (Decorator, Command, Builder...) và được cộng điểm nếu áp dụng đúng chỗ tùy theo độ khó của Design Pattern. 

## **4.3  Yêu cầu UML**

Sinh viên nộp các diagram sau (dùng mermaid):

1. Class Diagram: toàn bộ hệ thống, thể hiện rõ inheritance, composition, dependency

2. Sequence Diagram: một lần agent run hoàn chỉnh (từ lúc nhận task đến khi trả kết quả)

3. Sequence Diagram: HarnessRunner chạy batch evaluation

4. Component Diagram: tổng quan các module và dependency

## **4.4  Abstraction layers**

| Nguyên tắc thiết kế quan trọng |
| :---- |
| AgentLoop KHÔNG biết Harness tồn tại \=\> chỉ expose hook interface Tool implementations KHÔNG phụ thuộc vào AgentLoop Evaluator KHÔNG phụ thuộc vào cách agent thực thi \=\> chỉ nhìn kết quả LLMClient interface đủ generic để thay Ollama bằng OpenAI chỉ bằng 1 class mới Vi phạm các nguyên tắc này bị trừ điểm thiết kế. |

# **V. KỸ THUẬT C++17 BẮT BUỘC SỬ DỤNG**

Các chức năng C++17 liệt kê bên dưới chỉ cần dùng tối thiểu **4** là được.

| Tính năng C++17 | Ứng dụng trong đồ án |
| :---- | :---- |
| **std::unique\_ptr / shared\_ptr** | Quản lý lifetime của Tool, Evaluator instances |
| **std::function \+ Lambda** | Tool::execute callback, step\_hook trong HarnessRunner |
| **std::variant\<...\>** | Action type: Click | TypeText | KeyPress | Done |
| **std::filesystem** | SkillLoader scan thư mục skills/ |
| **if constexpr / std::visit** | Xử lý các loại Action trong agent loop |
| **Structured bindings** | Unpack JSON response fields |
| **std::optional\<T\>** | Tool result có thể rỗng, Evaluator partial score |
| **Range-based for \+ auto** | Duyệt tool registry, trajectory steps |
| **Abstract class / pure virtual** | LLMClient, Tool, Evaluator, Environment interfaces |
| **Template class** | Generic Registry\<T\> hoặc EventBus\<Event\> |

Ngoài ra cần dùng ít nhất **2** kĩ thuật từ C++20, **2** kĩ thuật từ C++23 và **1** kĩ thuật từ C++26.

# **VI. CẤU TRÚC THƯ MỤC DỰ ÁN (GỢI Ý)**

| Cây thư mục gợi ý |
| :---- |
| Agent\_MSSV1\_MSSV2\_MSSV3/ ├── CMakeLists.txt   (Không có không sao) ├── README.md     ( hướng dẫn build và chạy) ├── src/ │   ├── agent/ │   │   ├── agent\_loop.h/.cpp │   │   ├── loop\_detector.h/.cpp │   │   └── skill\_loader.h/.cpp │   ├── client/ │   │   ├── llm\_client.h      ← abstract interface │   │   └── ollama\_client.h/.cpp │   ├── tools/ │   │   ├── tool.h            ← abstract Tool │   │   ├── tool\_registry.h/.cpp │   │   └── \[exec, file, web, memory, calculator\]\_tool.cpp │   └── harness/ │       ├── harness\_runner.h/.cpp │       ├── trajectory.h/.cpp │       ├── evaluator.h       ← abstract Evaluator │       └── \[keyword, functional\]\_evaluator.cpp ├── skills/ │   ├── task\_planner.md │   ├── error\_recovery.md │   └── \[skill tự viết\].md ├── benchmark/ │   ├── tasks.json │   └── run\_eval.cpp ├── tests/ │   └── \[unit tests\] └── docs/     ├── class\_diagram.png     ├── sequence\_diagram\_agent.png     └── component\_diagram.png |

Chú ý code phải được commit qua git, các thành viên chênh lệch số lượng mã nguồn không quá 20% (chỉ làm báo cáo hoặc test mà không đóng góp mã nguồn thì không tính là đóng góp đủ cho đồ án)

Số lượng commit cần tối thiểu 6 cho một thành viên (\>=12 cho nhóm 2 thành viên và \>=18 cho nhóm 3 thành viên): điều này giúp đảm bảo mã nguồn được commit là đủ nhỏ ở dạng module có thể quản lí được.

Khoảng cách thời gian giữa 2 commit gần nhất không lệch quá 7 ngày, đảm bảo mỗi tuần đều phải có ít nhất một tính năng được tạo ra. 

Khi nộp nhớ tạo Personal access token (quyền read only) để giáo viên có thể truy cập vào git của các bạn để chấm bài từ một repository private.

# **VII. HARNESS ENGINEERING & EVALUATION**

## **7.1  Trajectory Format**

Mỗi lần chạy agent phải tạo ra file JSON theo format gợi ý (không nhất thiết giống hoàn toàn, đáp ứng chức năng là được)

| trajectory\_{task\_id}.json |
| :---- |
| {   "task\_id": "task\_001",   "model": "qwen3-vl:7b",   "success": true,   "total\_tokens": 1842,   "total\_time\_ms": 12400,   "steps": \[     {       "step\_id": 0,       "thought": "Cần tính 15 \* 17 trước...",       "action": {"type": "tool\_call", "tool": "calculator", "args": "15\*17"},       "tool\_result": "255",       "tokens\_used": 312,       "latency\_ms": 890     }   \] } |

## **7.2  Task Definition Format**

| tasks.json |
| :---- |
| \[   {     "id": "task\_001",     "description": "Tính 15 \* 17 và lưu kết quả vào file result.txt",     "instruction": "Tính 15 nhân 17\. Lưu kết quả vào file result.txt",     "eval\_type": "functional",     "eval\_script": "test \-f result.txt && grep 255 result.txt && echo PASS",     "max\_steps": 10   } \] |

## **7.3  Benchmark yêu cầu tối thiểu**

Sinh viên phải cung cấp tập benchmark gồm ít nhất 10 task, phân bố:

* 4 task đơn giản: tính toán, đọc/ghi file, lấy thời gian

* 4 task trung bình: kết hợp 2-3 tool liên tiếp, có điều kiện

* 2 task khó: multi-step, agent cần tự quyết định thứ tự tool call

Báo cáo phải ghi rõ **success rate** của mô hình được chọn trên tập benchmark này.

# **VIII. TIÊU CHÍ CHẤM ĐIỂM**

| Tiêu chí | Nội dung đánh giá | Điểm tối đa |
| ----- | ----- | :---: |
| **Thiết kế OOP (25đ)** | • Class diagram đầy đủ, đúng notation UML (5đ) • Inheritance hierarchy hợp lý, không vi phạm LSP (5đ) • Áp dụng đủ 4 design patterns, đúng context (10đ) • Separation of concerns: Agent không biết Harness (5đ) | **25** |
| **Kỹ thuật C++ (20đ)** | • Dùng đủ tính năng C++17 trở lên trong bảng V (12đ) • Memory management: không leak, dùng smart pointers (4đ) • Exception handling có ý nghĩa (4đ) | **20** |
| **Chức năng (25đ)** | • 5 tool hoạt động đúng (10đ) • Agent loop \+ loop detection (5đ) • Skill system load và inject đúng (5đ) • Harness runner \+ trajectory output (5đ) | **25** |
| **Benchmark (15đ)** | • 10 task hợp lệ, đa dạng (5đ) • Evaluator chạy đúng, kết quả JSON hợp lệ (5đ) • Báo cáo success rate có phân tích (5đ) | **15** |
| **Tài liệu (15đ)** | • README: build, run, cấu hình Ollama rõ ràng (4đ) • Báo cáo: mô tả thiết kế, khó khăn, kết quả (6đ) • Slide thuyết trình mạch lạc (5đ) | **15** |
| **TỔNG** |  | **100** |

# **IX. HƯỚNG DẪN NỘP BÀI**

## **9.1  Deadline**

| Lịch nộp bài |
| :---- |
| ~~Tuần 11  (trước 21:00 Chủ nhật): Nộp bản thiết kế: class diagram \+ sequence diagram~~ Tuần 12  (trước 21:00 Chủ nhật): Nộp bản thiết kế: class diagram \+ sequence diagram \+ Nộp source code \+ báo cáo hoàn chỉnh \+ link youtube video demo ở chế độ Unlisted  ~~Tuần 13: Thuyết trình \+ demo trực tiếp (lịch cụ thể giảng viên thông báo)~~ |

## **9.2  Format nộp**

* Tên file ZIP: **MSSV1\_MSSV2\_MSSV3\_OopAgent.zip** (Hoặc chỉ cần có 3 MSSV của 3 thành viên là được không cần hậu tố OopAgent)

* Trong ZIP phải có đủ cấu trúc thư mục theo mục VI

* Cần có hướng dẫn biên dịch mã nguồn

* Nộp qua hệ thống LMS của khoa \- Moodle (không nhận qua email)

## **9.3  Yêu cầu demo** 

**Đã bỏ yêu cầu demo live, chỉ chấm qua video demo.** 

~~Trong buổi thuyết trình, nhóm sinh viên phải demo LIVE các phần sau:~~

1. ~~Khởi động Ollama từ Google Colab/Kaggle (hoặc nếu backed chạy rồi thì không cần), chạy một agent task hoàn chỉnh từ command line~~  
2. ~~Thêm một tool mới (giảng viên chỉ định) vào registry và chạy lại~~  
3. ~~Chạy batch benchmark, show file JSON output~~  
4. ~~Giải thích một design pattern đã dùng và chỉ ra trong code~~

~~Chú ý: **với mỗi câu hỏi sinh viên trả lời sẽ được giáo viên chỉ định ngẫu nhiên. Điểm sẽ ảnh hưởng điểm của cả nhóm.**~~

## **9.4  Quy định về tính nguyên vẹn học thuật**

| ⚠  Lưu ý quan trọng |
| :---- |
| • Được dùng AI (ChatGPT, Claude...) để tra cứu, học khái niệm, debug • KHÔNG được nộp code do AI generate toàn bộ mà không hiểu • Giảng viên sẽ hỏi ngẫu nhiên về bất kỳ phần code nào trong buổi demo • Không giải thích được code của mình \=\> bị trừ điểm tương ứng phần đó • Phát hiện copy từ nhóm khác \=\> cả hai nhóm nhận điểm 0 |

# **X. GỢI Ý MỞ RỘNG (Điểm thưởng tối đa \+15đ)**

Các tính năng sau không bắt buộc nhưng được cộng điểm nếu triển khai đúng và có demo:

## **10.1  GUI Agent (Screenshot \+ Action): \+8đ**

* Thêm tool capture\_screenshot() chụp màn hình desktop

* Dùng VLM (qwen3-vl, gemma4 qua Ollama) nhận ảnh và ra action

* Triển khai action executor: click(x,y), type\_text(), key\_press() qua libxdo (Linux)

* Demo: agent tự mở browser, tìm kiếm, copy kết quả

## **10.2  Persistent Memory với Vector Search: \+4đ**

* Thay SQLite keyword search bằng embedding-based similarity search

* Dùng nomic-embed-text (qua Ollama) để tạo embedding cho mỗi memory entry

* Tìm kiếm bằng cosine similarity trong C++

## **10.3  Multi-agent Coordination: \+3đ**

* HarnessRunner có thể spawn sub-agent (thread mới) cho subtask

* Agent giao tiếp qua message queue (std::queue \+ mutex)

* Demo: task phức tạp được phân chia cho 2 agent chạy song song

| Hướng nghiên cứu tiếp theo |
| :---- |
| Đồ án này là thu nhỏ của các hệ thống nghiên cứu thực sự: • OSWorld, ScreenSpot-Pro: benchmark GUI agent có harness đầy đủ • OpenClaw: production agent framework, tool catalog phong phú • UI-TARS, Agent S2: VLM-based GUI agent state-of-the-art Sinh viên có thể phát triển thành đề tài nghiên cứu / luận văn trong tương lai nếu thích vì thật sự hướng nghiên cứu này hiện nay vẫn còn khá nhiều khoảng trống. |

*–Hết –*