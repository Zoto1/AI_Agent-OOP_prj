#include "ollama_embedding_client.h"

#include <curl/curl.h>

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

namespace
{
size_t EmbeddingWriteCallback(void *contents, size_t size, size_t nmemb,
                              void *userp)
{
    static_cast<std::string *>(userp)->append(
        static_cast<char *>(contents), size * nmemb);
    return size * nmemb;
}
} // namespace

OllamaEmbeddingClient::OllamaEmbeddingClient(std::string base_url,
                                             std::string model,
                                             int timeout_ms)
    : base_url_(std::move(base_url)),
      model_(std::move(model)),
      timeout_ms_(timeout_ms),
      curl_handle_(curl_easy_init())
{
    if (!curl_handle_)
    {
        throw std::runtime_error(
            "Loi [OllamaEmbeddingClient]: Khong the khoi tao libcurl.");
    }
    // Khử trailing slash để nối URL sạch: http://host:11434/
    while (!base_url_.empty() && base_url_.back() == '/')
    {
        base_url_.pop_back();
    }
}

OllamaEmbeddingClient::~OllamaEmbeddingClient()
{
    if (curl_handle_)
    {
        curl_easy_cleanup(static_cast<CURL *>(curl_handle_));
        curl_handle_ = nullptr;
    }
}

std::vector<float> OllamaEmbeddingClient::embed(const std::string &text)
{
    if (!curl_handle_)
    {
        throw std::runtime_error(
            "Loi [OllamaEmbeddingClient]: libcurl chua duoc khoi tao.");
    }

    CURL *curl = static_cast<CURL *>(curl_handle_);
    std::string response_string;

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Ollama mới dùng /api/embed; phiên bản cũ dùng /api/embeddings.
    const std::string url_v2 = base_url_ + "/api/embed";
    const std::string url_v1 = base_url_ + "/api/embeddings";

    json payload = {
        {"model", model_},
        {"input", text},
    };

    const std::string payload_str = payload.dump();

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, EmbeddingWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms_);

    auto perform_once = [&](const std::string &url) -> long {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            throw std::runtime_error(
                std::string("Loi mang [OllamaEmbeddingClient]: ") +
                curl_easy_strerror(res));
        }
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        return http_code;
    };

    long http_code = perform_once(url_v2);
    if (http_code == 404 || http_code == 405 || http_code == 400)
    {
        // Endpoint cũ: body yêu cầu trường "prompt" thay vì "input".
        response_string.clear();
        json payload_v1 = {
            {"model", model_},
            {"prompt", text},
        };
        const std::string payload_v1_str = payload_v1.dump();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_v1_str.c_str());
        http_code = perform_once(url_v1);
    }

    curl_slist_free_all(headers);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, nullptr);

    if (http_code != 200)
    {
        throw std::runtime_error(
            "Loi HTTP [OllamaEmbeddingClient]: Ollama tra ve ma " +
            std::to_string(http_code) +
            ". Kiem tra da 'ollama pull " + model_ +
            "' va server da chay chua.");
    }

    try
    {
        json resp = json::parse(response_string);

        // Endpoint mới: {"embeddings": [[...]]}
        if (resp.contains("embeddings") && resp["embeddings"].is_array() &&
            !resp["embeddings"].empty())
        {
            std::vector<float> vec;
            for (const auto &value : resp["embeddings"][0])
            {
                vec.push_back(value.get<float>());
            }
            return vec;
        }

        // Endpoint cũ: {"embedding": [...]}
        if (resp.contains("embedding") && resp["embedding"].is_array())
        {
            std::vector<float> vec;
            for (const auto &value : resp["embedding"])
            {
                vec.push_back(value.get<float>());
            }
            return vec;
        }

        throw std::runtime_error(
            "Loi du lieu [OllamaEmbeddingClient]: Response thieu truong "
            "'embeddings'/'embedding'.");
    }
    catch (const json::parse_error &error)
    {
        throw std::runtime_error(
            std::string("Loi du lieu [OllamaEmbeddingClient]: Malformed JSON "
                        "response - ") +
            error.what());
    }
}

std::shared_ptr<EmbeddingClient> makeOllamaEmbeddingClient(
    const std::string &config_path)
{
    std::string base_url = "http://localhost:11434";
    std::string model = "nomic-embed-text";
    int timeout_ms = 30000;

    std::ifstream file(config_path);
    if (file.is_open())
    {
        try
        {
            json root;
            file >> root;
            if (root.contains("ollama"))
            {
                const auto &section = root["ollama"];
                if (section.contains("embedding") &&
                    section["embedding"].contains("enabled") &&
                    !section["embedding"]["enabled"].get<bool>())
                {
                    return nullptr;
                }
                base_url = section.value("base_url", base_url);
                if (section.contains("embedding"))
                {
                    model = section["embedding"].value("model", model);
                }
                timeout_ms = section.value("timeout_ms", timeout_ms);
            }
        }
        catch (const std::exception &)
        {
            // Config hỏng -> fallback về mặc định.
        }
    }

    return std::make_shared<OllamaEmbeddingClient>(base_url, model, timeout_ms);
}
