#pragma once

#include "llm_client.h"
#include <string>
#include <vector>

class OllamaClient : public LLMClient {
private:
    void* curl_handle; 



public:
    explicit OllamaClient(const LLMConfig& cfg);
    ~OllamaClient() override;
    std::string chat(const std::vector<Message>& messages) override;
    std::string chatMultimodal(const std::vector<Message>& messages, 
                               const std::vector<std::string>& images) override;
};

