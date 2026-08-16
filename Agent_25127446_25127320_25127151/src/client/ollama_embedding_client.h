#pragma once

#include "embedding_client.h"
#include <string>

// EmbeddingClient dùng Ollama /api/embed với model embedding
// (mặc định nomic-embed-text). Hỗ trợ cả endpoint cũ /api/embeddings.
class OllamaEmbeddingClient : public EmbeddingClient
{
public:
    // base_url: http://localhost:11434 (không bao gồm hậu tố /api)
    explicit OllamaEmbeddingClient(std::string base_url,
                                   std::string model = "nomic-embed-text",
                                   int timeout_ms = 30000);

    ~OllamaEmbeddingClient() override;

    std::vector<float> embed(const std::string &text) override;
    std::string getModelName() const override { return model_; }

private:
    std::string base_url_;
    std::string model_;
    int timeout_ms_;
    void *curl_handle_; // CURL*
};
