#include "client/config_loader.h"
#include "client/gemini_client.h"
#include "client/embedding_client.h"
#include "agent/agent_loop.h"
#include "agent/loop_detector.h"
#include "agent/skill_loader.h"
#include "tools/tool_registry.h"
#include "tools/calculator.h"
#include "tools/datetime_tool.h"
#include "tools/exec.h"
#include "tools/http_get_tool.h"
#include "tools/json_parser_tool.h"
#include "tools/read.h"
#include "tools/write.h"
#include "tools/memory_tool.h"
#include "tools/web_tool.h"
#include "harness/harness.h"

#include <iostream>
#include <memory>
#include <string>

// In hướng dẫn sử dụng khi gọi sai
static void printUsage(const char* prog) {
    std::cout << "Usage:\n"
              << "  " << prog << "                              # interactive REPL\n"
              << "  " << prog << " --benchmark <tasks.json>     # chay toan bo benchmark\n"
              << "  " << prog << " --run-task <id> <tasks.json> # chay 1 task cu the\n"
              << "  " << prog << " --verbose                    # in chi tiet moi buoc\n"
              << "  " << prog << " --config <config.json>       # duong dan config LLM\n";
}

int main(int argc, char* argv[]) {
    try {
        // ── Parse CLI arguments ───────────────────────────────────────────────
        bool verbose       = false;
        std::string config_path  = "config.json";
        std::string mode         = "repl";     // "repl" | "benchmark" | "run-task"
        std::string tasks_json;
        std::string task_id;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--verbose") {
                verbose = true;
            } else if (arg == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else if (arg == "--benchmark" && i + 1 < argc) {
                mode       = "benchmark";
                tasks_json = argv[++i];
            } else if (arg == "--run-task" && i + 2 < argc) {
                mode       = "run-task";
                task_id    = argv[++i];
                tasks_json = argv[++i];
            } else if (arg == "--help" || arg == "-h") {
                printUsage(argv[0]);
                return 0;
            } else {
                // fallback: nếu chỉ truyền 1 arg không nhận ra thì dùng làm config
                config_path = arg;
            }
        }

        // ── Khởi tạo LLM client ───────────────────────────────────────────────
        LLMConfig config = ConfigLoader::loadLLMConfig(
            config_path, "gemini", "GEMINI_API_KEY");
        auto llm = std::make_shared<GeminiClient>(config);

        // ── Đăng ký tools (Registry/Factory pattern - mục 4.2) ───────────────
        ToolRegistry& singleton = ToolRegistry::getInstance();
        singleton.registerTool(std::make_shared<CalculatorTool>());
        singleton.registerTool(std::make_shared<ExecTool>());
        singleton.registerTool(std::make_shared<FileReadTool>("./"));
        singleton.registerTool(std::make_shared<FileWriteTool>(
            "file_write", "Ghi noi dung vao file. Args: path, content", "./"));
        singleton.registerTool(std::make_shared<Memory>(
            makeOllamaEmbeddingClient(config_path), "memory_store.json"));
        singleton.registerTool(std::make_shared<WebSearchTool>());
        singleton.registerTool(std::make_shared<DateTimeTool>());
        singleton.registerTool(std::make_shared<HttpGetTool>());
        singleton.registerTool(std::make_shared<JsonParserTool>());

        // shared_ptr không sở hữu singleton; dùng no-op deleter để tránh double-free
        auto registry = std::shared_ptr<ToolRegistry>(&singleton, [](ToolRegistry*) {});
        auto skills   = std::make_shared<SkillLoader>("src/skills");
        skills->load_skills();
        auto detector = std::make_shared<LoopDetector>();

        // ── Chọn chế độ chạy ─────────────────────────────────────────────────
        if (mode == "benchmark") {
            // Chạy toàn bộ tập task, in báo cáo success rate
            std::cout << "[Benchmark] Chay tap task: " << tasks_json << "\n";
            HarnessRunner harness(llm, registry, skills, detector,
                                  "results/", "workspace/");
            auto results = harness.runBatch(tasks_json);
            harness.printReport(results);
            return 0;

        } else if (mode == "run-task") {
            // Chạy 1 task theo ID, in kết quả
            std::cout << "[RunTask] task_id=" << task_id
                      << " file=" << tasks_json << "\n";
            HarnessRunner harness(llm, registry, skills, detector,
                                  "results/", "workspace/");
            TaskResult res = harness.runSingleTask(tasks_json, task_id);
            std::cout << (res.success ? "[PASS]" : "[FAIL]")
                      << " score=" << res.score
                      << " time=" << res.total_time_ms << "ms"
                      << " tokens=" << res.total_tokens << "\n";
            if (res.error.has_value())
                std::cerr << "Error: " << res.error.value() << "\n";
            return res.success ? 0 : 1;

        } else {
            // ── Interactive REPL (chế độ mặc định) ───────────────────────────
            std::cout << "Nhap yeu cau (go 'exit' de thoat):\n";
            AgentLoop agent(llm, registry, skills, detector, 10);
            agent.setVerbose(verbose);

            std::string task;
            while (std::cout << "> " && std::getline(std::cin, task)) {
                if (task == "exit") break;
                if (task.empty()) continue;

                AgentRunResult result = agent.run(task);
                if (!verbose) {
                    if (result.status == AgentTerminationStatus::Completed) {
                        std::cout << result.final_answer << "\n";
                    } else {
                        std::cout << "[Agent terminated with status: "
                                  << agentTerminationStatusToString(result.status)
                                  << "]\n";
                    }
                }
            }
            return 0;
        }

    } catch (const std::exception& e) {
        std::cerr << "Loi khoi dong: " << e.what() << "\n";
        std::cerr << "Hay copy config.gemini.sample.json thanh config.json "
                     "va dat GEMINI_API_KEY.\n";
        return 1;
    }
}
