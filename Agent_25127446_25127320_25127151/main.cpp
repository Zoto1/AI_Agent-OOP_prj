#include "client/config_loader.h"
#include "client/gemini_client.h"
#include "agent/agent_loop.h"
#include "agent/loop_detector.h"
#include "agent/skill_loader.h"
#include "tools/tool_registry.h"
#include "tools/calculator.h"
#include "tools/exec.h"
#include "tools/read.h"
#include "tools/write.h"
#include "tools/memory_tool.h"
#include "tools/web_tool.h"

#include <iostream>
#include <memory>
#include <string>

int main(int argc, char* argv[]) {
    try {
        bool verbose = false;
        std::string config_path = "config.json";

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--verbose") {
                verbose = true;
            } else if (arg == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else {
                config_path = arg;
            }
        }
        LLMConfig config = ConfigLoader::loadLLMConfig(
            config_path, "gemini", "GEMINI_API_KEY");
        auto llm = std::make_shared<GeminiClient>(config);

        ToolRegistry& singleton = ToolRegistry::getInstance();
        singleton.registerTool(std::make_shared<CalculatorTool>());
        singleton.registerTool(std::make_shared<ExecTool>());
        singleton.registerTool(std::make_shared<FileReadTool>("./"));
        singleton.registerTool(std::make_shared<FileWriteTool>(
            "file_write", "Ghi noi dung vao file. Args: path, content", "./"));
        singleton.registerTool(std::make_shared<Memory>());
        singleton.registerTool(std::make_shared<WebSearchTool>());

        // shared_ptr không sở hữu singleton, vì singleton tự quản lý vòng đời.
        auto registry = std::shared_ptr<ToolRegistry>(&singleton, [](ToolRegistry*) {});
        auto skills = std::make_shared<SkillLoader>("src/skills");
        skills->load_skills();
        auto detector = std::make_shared<LoopDetector>();

        std::cout << "Nhap yeu cau (go 'exit' de thoat):\n";
        std::string task;
        while (std::cout << "> " && std::getline(std::cin, task)) {
            if (task == "exit") break;
            if (task.empty()) continue;
            AgentLoop agent(llm, registry, skills, detector, 10);
            agent.setVerbose(verbose);
            std::string result = agent.run(task);
            if (!verbose) {
                std::cout << result << "\n";
            }
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Loi khoi dong: " << e.what() << "\n";
        std::cerr << "Hay copy config.gemini.sample.json thanh config.json "
                     "va dat GEMINI_API_KEY.\n";
        return 1;
    }
}