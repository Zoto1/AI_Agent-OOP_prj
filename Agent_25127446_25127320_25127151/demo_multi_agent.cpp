#include "client/config_loader.h"
#include "client/gemini_client.h"
#include "client/embedding_client.h"

#include "agent/loop_detector.h"
#include "agent/skill_loader.h"

#include "harness/harness.h"

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

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

static void runTasks(HarnessRunner& runner, const std::string& combined_task, int num_agents)
{
    // Chuyển đổi dấu ';' thành newline nếu là một dòng đơn chứa nhiều lệnh
    std::string task_formatted = combined_task;
    if (task_formatted.find('\n') == std::string::npos && task_formatted.find(';') != std::string::npos) {
        for (char& c : task_formatted) {
            if (c == ';') c = '\n';
        }
    }

    auto subtasks = HarnessRunner::splitTaskIntoSubtasks(task_formatted, num_agents);
    if (subtasks.empty()) {
        std::cout << "[!] Khong co subtask hop le de thuc thi.\n";
        return;
    }

    std::cout << "\n=======================================================\n";
    std::cout << "  [MultiAgent] Dang dieu phoi " << subtasks.size() << " subtask song song\n";
    std::cout << "=======================================================\n";
    for (const auto& sub : subtasks)
    {
        std::cout << "  [" << sub.id << "] (max_steps=" << sub.max_steps << "): "
                  << sub.instruction << "\n";
    }
    std::cout << "-------------------------------------------------------\n";

    const auto start = std::chrono::steady_clock::now();
    std::string merged = runner.runMultiAgent(subtasks);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    std::cout << "\n=== KET QUA TONG HOP (SYNTHESIS) ===\n";
    std::cout << merged;
    std::cout << "\n[Tong thoi gian chay song song: " << elapsed << " ms]\n\n";
}

int main(int argc, char* argv[])
{
    try
    {
        std::string config_path = "config.json";
        bool interactive_mode = false;
        std::string custom_task = "";
        int num_agents = 3;

        if (argc <= 1) {
            interactive_mode = true;
        } else {
            std::string first_arg = argv[1];
            if (first_arg == "-i" || first_arg == "--interactive") {
                interactive_mode = true;
                if (argc > 2) num_agents = std::stoi(argv[2]);
                if (argc > 3) config_path = argv[3];
            } else if (first_arg == "-h" || first_arg == "--help") {
                std::cout << "Su dung:\n"
                          << "  " << argv[0] << "                       # Interactive REPL (nhap lenh truc tiep)\n"
                          << "  " << argv[0] << " \"<task>\" [num_agents] [config.json] # Chay 1 task cu the\n"
                          << "  " << argv[0] << " -i [num_agents] [config.json]          # Bat Interactive REPL\n";
                return 0;
            } else {
                custom_task = first_arg;
                if (argc > 2) num_agents = std::stoi(argv[2]);
                if (argc > 3) config_path = argv[3];
            }
        }

        // ---- 1. LLM client (Gemini) ----
        LLMConfig config = ConfigLoader::loadLLMConfig(
            config_path, "gemini", "GEMINI_API_KEY");
        auto llm = std::make_shared<GeminiClient>(config);

        // ---- 2. Dang ky tools ----
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

        auto registry = std::shared_ptr<ToolRegistry>(
            &singleton, [](ToolRegistry*) {});

        // ---- 3. Skills + LoopDetector ----
        auto skills = std::make_shared<SkillLoader>("src/skills");
        skills->load_skills();
        auto detector = std::make_shared<LoopDetector>(2, 4);

        // ---- 4. HarnessRunner ----
        HarnessRunner runner(llm, registry, skills, detector,
                             "results", "workspace", 1.0);

        if (!interactive_mode) {
            runTasks(runner, custom_task, num_agents);
            return 0;
        }

        // ---- 5. Interactive REPL Mode ----
        std::cout << "========================================================\n";
        std::cout << "       MULTI-AGENT INTERACTIVE TERMINAL (REPL)          \n";
        std::cout << "========================================================\n";
        std::cout << "Cach go lenh:\n";
        std::cout << " 1. Go 1 dong gom nhieu subtask ngan cach bang dau ';' :\n";
        std::cout << "    > Tinh 15 * 17; Tinh 2024 - 1999; Ghi ket qua vao kq.txt\n";
        std::cout << " 2. Hoac go ':multi' de nhap nhieu dong (ket thuc bang ':run'):\n";
        std::cout << "    > :multi\n";
        std::cout << " 3. Go 'exit' hoac 'quit' de thoat.\n";
        std::cout << "========================================================\n\n";

        std::string line;
        while (true)
        {
            std::cout << "multi-agent> ";
            if (!std::getline(std::cin, line)) break;

            size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) continue;
            size_t last = line.find_last_not_of(" \t\r\n");
            std::string input = line.substr(first, (last - first + 1));

            if (input == "exit" || input == "quit") {
                std::cout << "Tam biet!\n";
                break;
            }

            if (input == ":multi") {
                std::cout << "--- Che do nhap nhieu dong (nhap ':run' de chay, ':cancel' de huy) ---\n";
                std::string block = "";
                std::string subline;
                int line_num = 1;
                while (true) {
                    std::cout << " subtask #" << line_num << "> ";
                    if (!std::getline(std::cin, subline)) break;
                    if (subline == ":run" || subline == "run") break;
                    if (subline == ":cancel" || subline == "cancel") {
                        std::cout << "[Da huy input]\n";
                        block.clear();
                        break;
                    }
                    if (!subline.empty()) {
                        block += subline + "\n";
                        line_num++;
                    }
                }
                if (!block.empty()) {
                    runTasks(runner, block, num_agents);
                }
                continue;
            }

            runTasks(runner, input, num_agents);
        }

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Loi: " << error.what() << "\n";
        return 1;
    }
}