#pragma once

#include <string>
#include <vector>
#include <optional>

struct LLMConfig
{
    std::string base_url;
    std::string model_name;
    float temperature = 0.7f;
    int max_tokens = 1024;
    int timeout_ms = 30000;

    std::optional<std::string> api_key;
};
struct Message
{
    std::string role; // "system", "user", "assistant"
    std::string content;
};

class LLMClient
{
protected:
    LLMConfig _config;

public:
    explicit LLMClient(const LLMConfig &cfg) : _config(cfg) {}

    virtual ~LLMClient() = default;

public:
    void setConfig(const LLMConfig &cfg)
    {
        _config = cfg;
    }
    std::string getModelName() const
    {
        return _config.model_name;
    }

public:
    virtual std::string chat(const std::vector<Message> &messages) = 0;

    virtual std::string chatMultimodal(const std::vector<Message> &messages,
                                       const std::vector<std::string> &images) = 0;
};
