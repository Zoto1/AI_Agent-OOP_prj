#include "../client/gemini_client.h"

#include <cassert>
#include <iostream>
#include <string>

class TestableGeminiClient : public GeminiClient
{
public:
    explicit TestableGeminiClient(const LLMConfig &config)
        : GeminiClient(config) {}

    using GeminiClient::parseToolAwareResponse;
};

int main()
{
    LLMConfig config;
    config.model_name = "gemma-test";
    TestableGeminiClient client(config);

    const std::string function_response = R"({
        "candidates": [{
            "content": {
                "parts": [
                    {"thought": true, "text": "I should calculate first."},
                    {
                        "functionCall": {
                            "name": "calculator",
                            "args": {"expression": "128 / 8"},
                            "id": "call-1"
                        },
                        "thoughtSignature": "signature-1"
                    }
                ]
            }
        }],
        "usageMetadata": {
            "promptTokenCount": 100,
            "candidatesTokenCount": 10,
            "thoughtsTokenCount": 20,
            "totalTokenCount": 130
        }
    })";

    const LLMResponse tool = client.parseToolAwareResponse(function_response);
    assert(tool.tool_call.has_value());
    assert(tool.tool_call->name == "calculator");
    assert(tool.tool_call->args.find("128 / 8") != std::string::npos);
    assert(tool.tool_call->id == "call-1");
    assert(tool.tool_call->thought_signature == "signature-1");
    assert(tool.usage.prompt_tokens == 100);
    assert(tool.usage.candidate_tokens == 10);
    assert(tool.usage.thought_tokens == 20);
    assert(tool.usage.total_tokens == 130);

    const std::string text_response = R"({
        "candidates": [{
            "content": {
                "parts": [
                    {"thought": true, "text": "Internal plan"},
                    {"text": "Visible final answer"}
                ]
            }
        }],
        "usageMetadata": {
            "promptTokenCount": 200,
            "candidatesTokenCount": 5,
            "thoughtsTokenCount": 15,
            "totalTokenCount": 220
        }
    })";

    const LLMResponse text = client.parseToolAwareResponse(text_response);
    assert(!text.tool_call.has_value());
    assert(text.text == "Visible final answer");
    assert(text.usage.total_tokens == 220);

    const std::string response_without_usage = R"({
        "candidates": [{
            "content": {"parts": [{"text": "No usage metadata"}]}
        }]
    })";
    const LLMResponse no_usage =
        client.parseToolAwareResponse(response_without_usage);
    assert(no_usage.usage.total_tokens == 0);

    std::cout << "[PASS] Gemini multipart response tests\n";
    return 0;
}
