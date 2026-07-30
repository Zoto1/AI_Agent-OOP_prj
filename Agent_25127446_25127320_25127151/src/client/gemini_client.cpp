#include "gemini_client.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userdata)
{
    size_t totalSize = size * nmemb;
    std::string *buffer = static_cast<std::string *>(userdata);
    buffer->append(static_cast<char *>(contents), totalSize);
    return totalSize;
}

GeminiClient::GeminiClient(const LLMConfig &cfg) : LLMClient(cfg) {}



std::string GeminiClient::chat(const std::vector<Message> &messages)
{
    std::string requestBody = buildRequestBody(messages, {});
    std::string rawResponse = sendRequest(requestBody);
    return parseResponse(rawResponse);
}



std::string GeminiClient::chatMultimodal(const std::vector<Message> &messages,
                                         const std::vector<std::string> &images)
{
    std::string requestBody = buildRequestBody(messages, images);
    std::string rawResponse = sendRequest(requestBody);
    return parseResponse(rawResponse);
}

std::string GeminiClient::buildEndpointUrl() const
{
    if (!_config.api_key.has_value() || _config.api_key->empty())
    {
        throw std::invalid_argument("GeminiClient: thieu api_key trong LLMConfig");
    }

    std::ostringstream url;
    url << _config.base_url
        << "/models/" << _config.model_name
        << ":generateContent?key=" << *_config.api_key;
    return url.str();
}

std::string GeminiClient::sendRequest(const std::string &jsonBody) const
{
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        throw std::runtime_error("GeminiClient: khong the khoi tao CURL");
    }

    std::string responseBuffer;
    std::string url = buildEndpointUrl();

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(_config.timeout_ms));

    CURLcode res = curl_easy_perform(curl);

    // Lấy HTTP status code trước khi cleanup, để phân biệt
    // "kết nối thất bại" (res != CURLE_OK) với "kết nối OK nhưng
    // server trả lỗi" (vd 400/401/429).
    long httpStatus = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        // Bao gồm cả timeout (CURLE_OPERATION_TIMEDOUT) và
        // connection refused (CURLE_COULDNT_CONNECT).
        throw std::runtime_error(
            std::string("GeminiClient: loi ket noi - ") + curl_easy_strerror(res));
    }

    if (httpStatus < 200 || httpStatus >= 300)
    {
        throw std::runtime_error(
            "GeminiClient: HTTP status " + std::to_string(httpStatus) +
            ", body: " + responseBuffer);
    }

    return responseBuffer;
}

std::string GeminiClient::parseResponse(const std::string &rawJson) const
{
    json parsed;
    try
    {
        parsed = json::parse(rawJson);
    }
    catch (const json::parse_error &e)
    {
        throw std::runtime_error(
            std::string("GeminiClient: JSON response khong hop le - ") + e.what());
    }

    // Gemini bao lỗi trong field "error" thay vì HTTP status khác 200
    // trong một số trường hợp (vd content bị chặn bởi safety filter).
    if (parsed.contains("error"))
    {
        std::string message = parsed["error"].value("message", "unknown error");
        throw std::runtime_error("GeminiClient: Gemini tra ve loi - " + message);
    }

    try
    {
        return parsed.at("candidates").at(0).at("content").at("parts").at(0).at("text").get<std::string>();
    }
    catch (const json::out_of_range &e)
    {
        throw std::runtime_error(
            std::string("GeminiClient: response thieu field can thiet - ") + e.what());
    }
}

std::string GeminiClient::buildEndpointUrl() const
{
    if (!_config.api_key.has_value() || _config.api_key->empty())
    {
        throw std::invalid_argument("GeminiClient: thieu api_key trong LLMConfig");
    }

    std::ostringstream url;
    url << _config.base_url
        << "/models/" << _config.model_name
        << ":generateContent?key=" << *_config.api_key;
    return url.str();
}
