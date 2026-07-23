#pragma once

#include "tool.h"
#include <string>
#include <memory>

class WebSearchTool : public Tool {
public:
    WebSearchTool();
    ~WebSearchTool() override = default;

    std::string execute(const std::string& input) override;

    std::string getName() const override;

    std::string getDescription() const override;

private:
    std::string performSearchRequest(const std::string& query);
};