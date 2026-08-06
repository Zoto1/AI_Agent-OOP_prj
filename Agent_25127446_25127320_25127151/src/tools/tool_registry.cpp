#include "tool_registry.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

ToolRegistry &ToolRegistry::getInstance()
{
    static ToolRegistry instance;
    return instance;
}

void ToolRegistry::registerTool(std::shared_ptr<Tool> tool)
{
    if (tool)
    {
        tools[tool->getName()] = tool;
        std::cout << "-> Da dang ky thanh cong tool: [" << tool->getName() << "]\n";
    }
}
std::string ToolRegistry::describeToolsForPrompt() const
{
    std::string result;
    for (const auto &[name, tool] : tools)
    {
        result += "- " + tool->getName() + ": " + tool->getDescription() + "\n";
        result += "  Parameters: " + tool->getParametersSchema().dump() + "\n";
    }
    return result;
}

std::string ToolRegistry::functionDeclarationsJson() const
{
    json declarations = json::array();
    for (const auto &[name, tool] : tools)
    {
        declarations.push_back({
            {"name", tool->getName()},
            {"description", tool->getDescription()},
            {"parameters", tool->getParametersSchema()}
        });
    }
    return declarations.dump();
}

bool ToolRegistry::hasTool(const std::string &name) const
{
    return tools.find(name) != tools.end();
}

std::string ToolRegistry::executeTool(const std::string &name, const std::map<std::string, std::string> &args)
{
    auto it = tools.find(name);
    if (it != tools.end())
    {
        return it->second->execute(args);
    }
    return "Loi: Khong tim thay tool mang ten '" + name + "'";
}
