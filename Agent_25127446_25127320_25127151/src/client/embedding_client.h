#pragma once

#include <string>
#include <vector>
#include <memory>

// Abstract embedding provider (Strategy pattern).
// Mỗi provider (Ollama, Gemini...) chỉ cần implement embed().
class EmbeddingClient
{
public:
    virtual ~EmbeddingClient() = default;

    // Trả về vector embedding (các giá trị float) cho văn bản đầu vào.
    // Ném LLMClientError nếu không thể tạo embedding.
    virtual std::vector<float> embed(const std::string &text) = 0;

    // Tên model embedding đang dùng (vd "nomic-embed-text").
    virtual std::string getModelName() const = 0;
};

// Factory: tạo OllamaEmbeddingClient từ config.json (mục "ollama").
// base_url mặc định http://localhost:11434, model mặc định nomic-embed-text.
// Trả về nullptr nếu config không bật embedding (embedding.enabled=false).
std::shared_ptr<EmbeddingClient> makeOllamaEmbeddingClient(
    const std::string &config_path);

