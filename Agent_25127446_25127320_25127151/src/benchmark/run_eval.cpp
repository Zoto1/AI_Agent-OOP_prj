#include "client/config_loader.h"
#include "client/gemini_client.h"

#include "agent/loop_detector.h"
#include "agent/skill_loader.h"

#include "harness/harness.h"

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

int main(int argc, char* argv[])
{
    try {
        // Tham số 1 là config, tham số 2 là file task
        std::string config_path =
            argc > 1 ? argv[1] : "config.json";

        std::string tasks_path =
            argc > 2 ? argv[2] : "src/benchmark/task.json";

        // Tạo Gemini client
        LLMConfig config = ConfigLoader::loadLLMConfig(
            config_path,
            "gemini",
            "GEMINI_API_KEY"
        );

        auto llm = std::make_shared<GeminiClient>(config);

        // Đăng ký các tool
        ToolRegistry& singleton = ToolRegistry::getInstance();

        singleton.registerTool(std::make_shared<CalculatorTool>());
        singleton.registerTool(std::make_shared<ExecTool>());
        singleton.registerTool(std::make_shared<FileReadTool>("./"));

        singleton.registerTool(
            std::make_shared<FileWriteTool>(
                "file_write",
                "Ghi noi dung vao file. Args: path, content",
                "./"
            )
        );

        singleton.registerTool(std::make_shared<Memory>());
        singleton.registerTool(std::make_shared<WebSearchTool>());

        // shared_ptr không sở hữu singleton
        auto registry = std::shared_ptr<ToolRegistry>(
            &singleton,
            [](ToolRegistry*) {}
        );

        auto skills = std::make_shared<SkillLoader>("src/skills");
        skills->load_skills();

        auto detector = std::make_shared<LoopDetector>();

        // Tạo Harness
        HarnessRunner runner(
            llm,
            registry,
            skills,
            detector,
            "results",
            "workspace",
            1.0
        );

        std::cout << "Dang chay benchmark: "
                  << tasks_path << "\n";

        runner.runBatch(tasks_path);

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Loi Harness: "
                  << e.what() << "\n";
        return 1;
    }
}