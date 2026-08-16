#pragma once
#include "tool.h"
#include <memory>
#include <iostream>
#include <string>
#include <map>
#include <unordered_set>

class ToolRegistry {
private:
    std::map<std::string, std::shared_ptr<Tool>> tools;
    std::unordered_set<std::string> denied_tools;

    ToolRegistry() = default;

public:
    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;

    static ToolRegistry& getInstance();

    std::string describeToolsForPrompt() const;
    std::string functionDeclarationsJson() const;
    bool hasTool(const std::string& name) const;

    void registerTool(std::shared_ptr<Tool> tool);
    std::string executeTool(const std::string& name, const std::map<std::string, std::string>& args);

    void denyTool(const std::string& name);
    void allowTool(const std::string& name);
    bool isAllowed(const std::string& name) const;
};
