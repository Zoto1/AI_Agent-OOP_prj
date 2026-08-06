#pragma once

#include "llm_client.h"
#include <stdexcept>




class GeminiClient : public LLMClient
{
public:
    explicit GeminiClient(const LLMConfig &cfg);

    std::string chat(const std::vector<Message> &messages) override;

    LLMResponse chatWithTools(
        const std::vector<Message> &messages,
        const std::string &function_declarations_json) override;

    std::string chatMultimodal(const std::vector<Message> &messages,
                               const std::vector<std::string> &images) override;

private:
    std::string buildRequestBody(const std::vector<Message> &messages,
                                 const std::vector<std::string> &images,
                                 const std::string &function_declarations_json) const;

    std::string sendRequest(const std::string &jsonBody) const;

    std::string buildEndpointUrl() const;

protected:
    LLMResponse parseToolAwareResponse(const std::string &rawJson) const;
};
