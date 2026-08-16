#include "client/config_loader.h"
#include "client/gemini_client.h"

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

#include <iostream>
#include <memory>
#include <string>

int main(int argc, char* argv[])
{
    try
    {
        /*
         * Cách dùng:
         *
         * Chạy toàn bộ:
         * benchmark_run
         * benchmark_run config.json src/benchmark/task.json
         *
         * Chạy một task:
         * benchmark_run config.json src/benchmark/task.json task_007
         */

        if (argc > 4)
        {
            std::cerr
                << "Cach dung:\n"
                << "  " << argv[0] << "\n"
                << "  " << argv[0]
                << " <config.json> <tasks.json>\n"
                << "  " << argv[0]
                << " <config.json> <tasks.json> <task_id>\n";

            return 1;
        }

        const std::string config_path =
            argc > 1
                ? argv[1]
                : "config.json";

        const std::string tasks_path =
            argc > 2
                ? argv[2]
                : "src/benchmark/task.json";

        const std::string task_id =
            argc > 3
                ? argv[3]
                : "";

        LLMConfig config = ConfigLoader::loadLLMConfig(
            config_path,
            "gemini",
            "GEMINI_API_KEY"
        );

        auto llm =
            std::make_shared<GeminiClient>(config);

        ToolRegistry& singleton =
            ToolRegistry::getInstance();

        singleton.registerTool(
            std::make_shared<CalculatorTool>()
        );

        singleton.registerTool(
            std::make_shared<ExecTool>()
        );

        singleton.registerTool(
            std::make_shared<FileReadTool>("./")
        );

        singleton.registerTool(
            std::make_shared<FileWriteTool>(
                "file_write",
                "Ghi noi dung vao file. Args: path, content",
                "./"
            )
        );

        singleton.registerTool(
            std::make_shared<Memory>()
        );

        singleton.registerTool(
            std::make_shared<WebSearchTool>()
        );

        singleton.registerTool(
            std::make_shared<DateTimeTool>()
        );

        singleton.registerTool(
            std::make_shared<HttpGetTool>()
        );

        singleton.registerTool(
            std::make_shared<JsonParserTool>()
        );

        // shared_ptr không sở hữu singleton.
        auto registry = std::shared_ptr<ToolRegistry>(
            &singleton,
            [](ToolRegistry*) {}
        );

        auto skills =
            std::make_shared<SkillLoader>("src/skills");

        skills->load_skills();

        auto detector =
            std::make_shared<LoopDetector>();

        HarnessRunner runner(
            llm,
            registry,
            skills,
            detector,
            "results",
            "workspace",
            1.0
        );

        if (!task_id.empty())
        {
            std::cout
                << "Dang chay mot task: "
                << task_id << "\n";

            runner.runSingleTask(
                tasks_path,
                task_id
            );
        }
        else
        {
            std::cout
                << "Dang chay toan bo benchmark: "
                << tasks_path << "\n";

            runner.runBatch(tasks_path);
        }

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "Loi Harness: "
            << error.what()
            << "\n";

        return 1;
    }
}