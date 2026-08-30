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

class NetworkFailingClient : public LLMClient
{
public:
    NetworkFailingClient()
        : LLMClient(LLMConfig{.model_name = "offline-test"}) {}

    std::string chat(const std::vector<Message> &) override
    {
        throw APIEnvironmentError("API_ENVIRONMENT: simulated network failure");
    }

    std::string chatMultimodal(const std::vector<Message> &messages,
                               const std::vector<std::string> &) override
    {
        return chat(messages);
    }
};

class MultiFileHarnessFakeClient : public LLMClient
{
private:
    const std::vector<std::string> responses = {
        R"({"type":"tool_call","tool":"file_write","args":{"path":"result.txt","content":"255"}})",
        R"({"type":"final_answer","answer":"Da ghi ket qua 255"})",
        R"({"type":"tool_call","tool":"file_write","args":{"path":"keyword.txt","content":"KEYWORD_OK"}})",
        R"({"type":"final_answer","answer":"KEYWORD_OK trong keyword.txt"})"
    };
    std::size_t index = 0;

public:
    MultiFileHarnessFakeClient()
        : LLMClient(LLMConfig{.model_name = "fake-multi-file"}) {}

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
    const fs::path keyword_tasks_path = root / "keyword_tasks.json";
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

    const json keyword_tasks = json::array({{
        {"id", "integration_002"},
        {"description", "Second benchmark file"},
        {"instruction", "Ghi KEYWORD_OK vao keyword.txt"},
        {"eval_type", "keyword"},
        {"eval_keywords", json::array({"KEYWORD_OK", "keyword.txt"})},
        {"max_steps", 4}
    }});
    std::ofstream(keyword_tasks_path) << keyword_tasks.dump(2);

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

    // Multiple task files must form one batch: cleanup happens once and the
    // final summary/trajectory set contains results from both files.
    const fs::path combined_output = root / "combined_results";
    const fs::path combined_workspace = root / "combined_workspace";
    HarnessRunner combined_harness(std::make_shared<MultiFileHarnessFakeClient>(),
                                   registry, nullptr, nullptr,
                                   combined_output.string(),
                                   combined_workspace.string());
    const std::vector<TaskResult> combined_results = combined_harness.runBatch(
        std::vector<std::string>{tasks_path.string(),
                                 keyword_tasks_path.string()});

    assert(combined_results.size() == 2);
    assert(combined_results[0].success);
    assert(combined_results[1].success);
    assert(fs::exists(combined_output / "trajectory_integration_001.json"));
    assert(fs::exists(combined_output / "trajectory_integration_002.json"));

    json combined_summary;
    std::ifstream(combined_output / "benchmark_summary.json") >> combined_summary;
    assert(combined_summary["total_tasks"] == 2);
    assert(combined_summary["passed"] == 2);
    assert(combined_summary["results"].size() == 2);
    assert(combined_summary["results"][0]["eval_type"] == "functional");
    assert(combined_summary["results"][1]["eval_type"] == "keyword");

    // Network failures must still produce both a per-task trajectory and the
    // batch summary instead of only being printed to stderr.
    const fs::path offline_output = root / "offline_results";
    const fs::path offline_workspace = root / "offline_workspace";
    HarnessRunner offline_harness(std::make_shared<NetworkFailingClient>(),
                                  registry, nullptr, nullptr,
                                  offline_output.string(),
                                  offline_workspace.string());
    const std::vector<TaskResult> offline_results =
        offline_harness.runBatch(tasks_path.string());

    assert(offline_results.size() == 1);
    assert(!offline_results[0].success);
    assert(offline_results[0].error.has_value());
    assert(fs::exists(offline_output / "trajectory_integration_001.json"));
    assert(fs::exists(offline_output / "benchmark_summary.json"));

    json offline_summary;
    std::ifstream(offline_output / "benchmark_summary.json") >> offline_summary;
    assert(offline_summary["total_tasks"] == 1);
    assert(offline_summary["errored"] == 1);
    assert(!offline_summary["results"][0]["error"].is_null());

    fs::remove_all(root, error);
    return 0;
}
