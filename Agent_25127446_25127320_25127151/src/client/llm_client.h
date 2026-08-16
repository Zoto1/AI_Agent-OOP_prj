#pragma once

#include <string>
#include <vector>
#include <optional>
#include <stdexcept>

// =============================================================================
// C++20: concepts — requires <concepts>
// =============================================================================
#include <concepts>

// =============================================================================
// C++23: std::expected — requires <expected>
// =============================================================================
#include <expected>

struct LLMConfig
{
    std::string base_url;
    std::string model_name;
    float temperature = 0.7f;
    int max_tokens = 1024;
    int timeout_ms = 30000;

    std::optional<std::string> api_key;
};

enum class MessageKind
{
    Text,
    FunctionCall,
    FunctionResponse
};

struct Message
{
    std::string role; // "system", "user", "assistant"
    std::string content;
    MessageKind kind = MessageKind::Text;
    std::string tool_name;
    std::string tool_args;
    std::string tool_call_id;
    std::string thought_signature;
};

struct LLMToolCall
{
    std::string name;
    std::string args;
    std::string id;
    std::string thought_signature;
};

struct TokenUsage
{
    long long prompt_tokens = 0;
    long long candidate_tokens = 0;
    long long thought_tokens = 0;
    long long total_tokens = 0;
};

struct LLMResponse
{
    std::string text;
    std::optional<LLMToolCall> tool_call;
    TokenUsage usage;
};

struct LLMException : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct APIEnvironmentError : LLMException {
    using LLMException::LLMException;
};

struct LLMClientError : LLMException {
    using LLMException::LLMException;
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

    // Providers with native function calling should override this method.
    // Text-only providers keep working through the default prompt-based path.
    virtual LLMResponse chatWithTools(
        const std::vector<Message> &messages,
        const std::string &function_declarations_json)
    {
        (void)function_declarations_json;
        return {chat(messages), std::nullopt};
    }

    virtual std::string chatMultimodal(const std::vector<Message> &messages,
                                       const std::vector<std::string> &images) = 0;

    // =========================================================================
    // S-3 — C++23: std::expected<T, E>
    // =========================================================================
    // safeChat / safeChatWithTools bọc các lời gọi ảo tương ứng trong try/catch.
    // Thay vì ném ngoại lệ ra ngoài, chúng trả về:
    //   - std::expected<..., std::string> chứa kết quả nếu thành công, hoặc
    //   - std::unexpected(error_message) nếu có lỗi.
    // Điều này cho phép caller kiểm tra lỗi rõ ràng mà không cần try/catch.

    std::expected<std::string, std::string>
    safeChat(const std::vector<Message> &messages)
    {
        try {
            return chat(messages);
        } catch (const std::exception &e) {
            return std::unexpected(std::string(e.what()));
        } catch (...) {
            return std::unexpected(std::string("Unknown error in chat()"));
        }
    }

    std::expected<LLMResponse, std::string>
    safeChatWithTools(const std::vector<Message> &messages,
                      const std::string &function_declarations_json)
    {
        try {
            return chatWithTools(messages, function_declarations_json);
        } catch (const std::exception &e) {
            return std::unexpected(std::string(e.what()));
        } catch (...) {
            return std::unexpected(std::string("Unknown error in chatWithTools()"));
        }
    }
};

// =============================================================================
// S-2 — C++20: Concepts
// =============================================================================
// LLMBackend<T> ràng buộc T phải là subclass (derived_from) của LLMClient.
// Dùng để ràng buộc kiểu template tại compile-time, thay thế SFINAE.
//
// Ví dụ sử dụng:
//   template <LLMBackend T>
//   void process(std::shared_ptr<T> client) { ... }
//
// GeminiClient và OllamaClient đều thoả mãn concept này.

template <typename T>
concept LLMBackend = std::derived_from<T, LLMClient>;
