#include "../tools/calculator.h"
#include "../tools/datetime_tool.h"
#include "../tools/http_get_tool.h"
#include "../tools/json_parser_tool.h"
#include "../tools/memory_tool.h"
#include "../tools/read.h"
#include "../tools/tool_registry.h"
#include "../tools/web_tool.h"
#include "../tools/write.h"

#include <cassert>
#include <cctype>
#include <iostream>
#include <memory>
#include <string>

static void testDatetimeTool()
{
    DateTimeTool tool;

    std::string default_result = tool.execute({});
    assert(!default_result.empty());
    assert(default_result.size() >= 19);
    assert(default_result.find_first_not_of("0123456789-: ") ==
           std::string::npos);

    std::string year_only = tool.execute({{"format", "%Y"}});
    assert(year_only.size() == 4);
    for (const char character : year_only)
    {
        assert(std::isdigit(static_cast<unsigned char>(character)));
    }
}

static void testJsonParserTool()
{
    JsonParserTool tool;

    const std::string nested = tool.execute({
        {"json", R"({"user":{"name":"Alice"},"items":[10,20,30]})"},
        {"key", "user.name"}
    });
    assert(nested.find("Alice") != std::string::npos);

    const std::string array_item = tool.execute({
        {"json", R"({"items":[10,20,30]})"},
        {"key", "items.1"}
    });
    assert(array_item.find("20") != std::string::npos);

    const std::string array_out_of_range = tool.execute({
        {"json", R"({"items":[10,20]})"},
        {"key", "items.5"}
    });
    assert(array_out_of_range.find("out of range") != std::string::npos);

    const std::string missing_key = tool.execute({
        {"json", R"({"a":1})"},
        {"key", "missing"}
    });
    assert(missing_key.find("not found") != std::string::npos);

    const std::string invalid_json = tool.execute({
        {"json", "not json"},
        {"key", "a"}
    });
    assert(invalid_json.find("Invalid JSON") != std::string::npos);

    const std::string missing_param = tool.execute({
        {"json", R"({"a":1})"},
        {"key", ""}
    });
    assert(missing_param.find("Missing 'key'") != std::string::npos);
}

static void testMemoryTool()
{
    Memory memory;
    memory.clear_memory();

    const std::string saved = memory.execute({
        {"action", "save"},
        {"key", "ngon_ngu_yeu_thich"},
        {"value", "Cpp"}
    });
    assert(saved == "True");

    const std::string exact = memory.execute({
        {"action", "load"},
        {"query", "ngon_ngu_yeu_thich"}
    });
    assert(exact == "Cpp");

    const std::string fuzzy = memory.execute({
        {"action", "load"},
        {"query", "ngon ngu yeu thich"}
    });
    assert(fuzzy == "Cpp");

    const std::string unrelated = memory.execute({
        {"action", "load"},
        {"query", "xyzzy qqq zzz"}
    });
    assert(unrelated.find("Khong tim thay") != std::string::npos);
}

static void testToolPolicy()
{
    ToolRegistry &registry = ToolRegistry::getInstance();
    registry.registerTool(std::make_shared<CalculatorTool>());

    assert(registry.isAllowed("calculator"));

    registry.denyTool("calculator");
    assert(!registry.isAllowed("calculator"));

    const std::string blocked = registry.executeTool(
        "calculator", {{"expression", "1 + 1"}});
    assert(blocked.find("disabled by policy") != std::string::npos);

    registry.allowTool("calculator");
    assert(registry.isAllowed("calculator"));

    const std::string unblocked = registry.executeTool(
        "calculator", {{"expression", "1 + 1"}});
    assert(unblocked.find("2") != std::string::npos);
}

static void testFileToolSafety()
{
    FileWriteTool write("file_write", "write tool", "./");
    FileReadTool read("./");

    const std::string write_traversal = write.execute({
        {"path", "../evil.txt"},
        {"content", "x"}
    });
    assert(write_traversal.find("truy") != std::string::npos);

    const std::string write_absolute = write.execute({
        {"path", "/etc/evil.txt"},
        {"content", "x"}
    });
    assert(write_absolute.find("truy") != std::string::npos);

    const std::string read_traversal = read.execute({{"path", "../secret"}});
    assert(read_traversal.find("truy") != std::string::npos);

    const std::string read_absolute = read.execute({{"path", "/etc/passwd"}});
    assert(read_absolute.find("truy") != std::string::npos);

    const std::string read_missing = read.execute({{"path", ""}});
    assert(read_missing.find("Thieu") != std::string::npos);
}

static void testCalculatorEdges()
{
    CalculatorTool calculator;

    assert(calculator.execute({{"expression", ""}}).find("expression") !=
           std::string::npos);
    assert(calculator.execute({{"expression", "1 / 0"}}).find("0") !=
           std::string::npos);
    assert(calculator.execute({{"expression", "(1 + 2"}}).find("Thieu") !=
           std::string::npos);

    const std::string backward_compat = calculator.execute({
        {"a", "10"},
        {"b", "5"},
        {"op", "+"}
    });
    assert(backward_compat.find("15") != std::string::npos);
}

static void testNetworkToolParams()
{
    WebSearchTool web_search;
    assert(web_search.execute({}).find("Missing 'query'") !=
           std::string::npos);

    HttpGetTool http_get;
    assert(http_get.execute({}).find("Missing 'url'") != std::string::npos);
    assert(http_get.execute({{"url", ""}}).find("Missing 'url'") !=
           std::string::npos);
}

int main()
{
    testDatetimeTool();
    testJsonParserTool();
    testMemoryTool();
    testToolPolicy();
    testFileToolSafety();
    testCalculatorEdges();
    testNetworkToolParams();

    std::cout << "[PASS] Tools, policy and memory vector search tests\n";
    return 0;
}
