#include "../client/llm_client.h"
#include "../harness/harness.h"
#include "../tools/memory_tool.h"
#include "../tools/tool_registry.h"
#include "../tools/write.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

class HarnessFakeClient : public LLMClient
{
private:
    std::vector<std::string> responses = {
        R"({"type":"tool_call","tool":"file_write","args":{"path":"result.txt","content":"255"}})",
        R"({"type":"final_answer","answer":"Da ghi ket qua 255"})"
    };
    std::size_t index = 0;

public:
    HarnessFakeClient() : LLMClient(LLMConfig{.model_name = "fake-harness"}) {}

    std::string chat(const std::vector<Message> &) override
    {
        assert(index < responses.size());
        return responses[index++];
    }

    std::string chatMultimodal(const std::vector<Message> &messages,
                               const std::vector<std::string> &) override
    {
        return chat(messages);
    }
};

int main()
{
    const fs::path root = fs::absolute("test_harness_integration_artifacts");
    const fs::path output = root / "results";
    const fs::path workspace = root / "workspace";
    const fs::path tasks_path = root / "tasks.json";
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root);

    const json tasks = json::array({{
        {"id", "integration_001"},
        {"description", "Harness integration test"},
        {"instruction", "Ghi 255 vao result.txt"},
        {"eval_type", "functional"},
        {"eval_script", "test -f result.txt && grep -Fxq 255 result.txt"},
        {"max_steps", 4}
    }});
    std::ofstream(tasks_path) << tasks.dump(2);

    ToolRegistry &singleton = ToolRegistry::getInstance();
    singleton.registerTool(std::make_shared<FileWriteTool>(
        "file_write", "Write a file", "./"));
    auto memory = std::make_shared<Memory>();
    memory->execute({
        {"action", "save"}, {"key", "stale_key"}, {"value", "stale_value"}
    });
    singleton.registerTool(memory);
    auto registry = std::shared_ptr<ToolRegistry>(&singleton,
                                                  [](ToolRegistry *) {});

    HarnessRunner harness(std::make_shared<HarnessFakeClient>(), registry,
                          nullptr, nullptr, output.string(),
                          workspace.string());
    const std::vector<TaskResult> results = harness.runBatch(tasks_path.string());

    assert(results.size() == 1);
    assert(results[0].success);
    assert(results[0].score == 1.0);
    assert(results[0].eval_type == "functional");
    assert(fs::exists(output / "trajectory_integration_001.json"));
    assert(fs::exists(output / "benchmark_summary.json"));
    assert(fs::exists(workspace / "integration_001" / "result.txt"));

    json summary;
    std::ifstream(output / "benchmark_summary.json") >> summary;
    assert(summary["total_tasks"] == 1);
    assert(summary["passed"] == 1);
    assert(summary["results"][0]["eval_type"] == "functional");

    const std::string stale = memory->execute({
        {"action", "load"}, {"query", "stale_key"}
    });
    assert(stale.find("Khong tim thay") != std::string::npos);

    fs::remove_all(root, error);
    return 0;
}
