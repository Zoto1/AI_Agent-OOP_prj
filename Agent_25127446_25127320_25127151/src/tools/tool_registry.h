#pragma once
#include "tool.h"
#include <memory>
#include <iostream>
#include <string>
#include <map>

class ToolRegistry {
private:
    std::map<std::string, std::shared_ptr<Tool>> tools;

    ToolRegistry() = default;

public:
    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;

    static ToolRegistry& getInstance();

    std::string describeToolsForPrompt() const;

    void registerTool(std::shared_ptr<Tool> tool);
    std::string executeTool(const std::string& name, const std::map<std::string, std::string>& args); };