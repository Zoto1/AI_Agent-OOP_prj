#include "gemini_client.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <thread>

using json = nlohmann::json;

static std::string urlEncode(const std::string &value)
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (unsigned char c : value)
    {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
        {
            escaped << c;
        }
        else
        {
            escaped << '%' << std::uppercase << std::setw(2) << int(c) << std::nouppercase;
        }
    }
    return escaped.str();
}

// ------------------------------------------------------------------
// Hàm static, KHÔNG phải member của class -> dùng làm callback cho
// libcurl (curl yêu cầu con trỏ hàm kiểu C, không nhận được std::function
// hay non-static member function trực tiếp).
// libcurl gọi hàm này nhiều lần khi dữ liệu response về từng phần (chunk),
// nhiệm vụ của mình là nối chuỗi lại vào buffer do "userdata" trỏ tới.
// ------------------------------------------------------------------
static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userdata)
{
    size_t totalSize = size * nmemb;
    std::string *buffer = static_cast<std::string *>(userdata);
    buffer->append(static_cast<char *>(contents), totalSize);
    return totalSize;
}

// ------------------------------------------------------------------
// Constructor: chỉ đơn giản forward config lên lớp cha (LLMClient).
// Không làm gì thêm ở đây -> giữ constructor "rẻ", không có side effect
// (không mở connection, không validate network) vì đó không phải việc
// của constructor.
// ------------------------------------------------------------------
GeminiClient::GeminiClient(const LLMConfig &cfg) : LLMClient(cfg)
{
}

// ------------------------------------------------------------------
// buildEndpointUrl()
// Gemini không dùng Bearer token trong header như OpenAI, mà nhét
// API key trực tiếp vào query string: ?key=API_KEY
// Format URL: {base_url}/models/{model_name}:generateContent?key={api_key}
//
// Nếm không có api_key -> ném lỗi ngay, vì gọi Gemini mà thiếu key
// chắc chắn sẽ fail phía server, tốt hơn là bắt lỗi sớm (fail fast)
// ngay tại client thay vì đợi HTTP 400 trả về rồi mới parse lỗi.
// ------------------------------------------------------------------
std::string GeminiClient::buildEndpointUrl() const
{
    if (!_config.api_key.has_value() || _config.api_key->empty())
    {
        throw std::invalid_argument("GeminiClient: thieu api_key trong LLMConfig");
    }

    std::ostringstream url;
    url << _config.base_url
        << "/models/" << urlEncode(_config.model_name)
        << ":generateContent?key=" << urlEncode(*_config.api_key);
    return url.str();
}

// ------------------------------------------------------------------
// buildRequestBody()
// Chuyển đổi std::vector<Message> (định dạng chung của LLMClient)
// sang JSON đúng schema Gemini yêu cầu. Đây chính là chỗ "dịch"
// khác biệt giữa Gemini và Ollama/OpenAI, giấu hoàn toàn bên trong
// class này:
//
//   - Gemini không có role "system" nằm chung mảng "contents".
//     Thay vào đó dùng field riêng "systemInstruction".
//   - role "assistant" của mình phải đổi thành "model" (Gemini gọi vậy).
//   - Mỗi message nằm trong "parts": [{ "text": ... }]
//   - Ảnh (nếu có) được thêm vào "parts" của message cuối cùng dưới
//     dạng inline_data base64.
//   - generationConfig chứa temperature / maxOutputTokens lấy từ _config.
// ------------------------------------------------------------------
std::string GeminiClient::buildRequestBody(const std::vector<Message> &messages,
                                            const std::vector<std::string> &images,
                                            const std::string &function_declarations_json) const
{
    json body;
    json contents = json::array();

    for (const auto &msg : messages)
    {
        if (msg.role == "system")
        {
            // Gemini xử lý system prompt riêng, không đưa vào "contents"
            body["systemInstruction"] = {
                {"parts", json::array({{{"text", msg.content}}})}
            };
            continue;
        }

        json entry;
        entry["role"] = (msg.role == "assistant") ? "model" : "user";

        if (msg.kind == MessageKind::FunctionCall)
        {
            json args = json::object();
            if (!msg.tool_args.empty())
            {
                args = json::parse(msg.tool_args);
            }

            json function_call = {
                {"name", msg.tool_name},
                {"args", args}
            };
            if (!msg.tool_call_id.empty())
            {
                function_call["id"] = msg.tool_call_id;
            }

            json part = {{"functionCall", function_call}};
            if (!msg.thought_signature.empty())
            {
                part["thoughtSignature"] = msg.thought_signature;
            }
            entry["parts"] = json::array({part});
        }
        else if (msg.kind == MessageKind::FunctionResponse)
        {
            json function_response = {
                {"name", msg.tool_name},
                {"response", {{"result", msg.content}}}
            };
            if (!msg.tool_call_id.empty())
            {
                function_response["id"] = msg.tool_call_id;
            }
            entry["parts"] = json::array({{{"functionResponse", function_response}}});
        }
        else
        {
            entry["parts"] = json::array({{{"text", msg.content}}});
        }
        contents.push_back(entry);
    }

    // Nếu có ảnh, gắn vào "parts" của message cuối cùng (message user
    // mới nhất) -> giống cách chatMultimodal() của interface mô tả:
    // ảnh đi kèm với lượt hỏi gần nhất, không phải toàn bộ history.
    if (!images.empty() && !contents.empty())
    {
        auto &lastParts = contents.back()["parts"];
        for (const auto &base64Image : images)
        {
            json imagePart;
            imagePart["inline_data"] = {
                {"mime_type", "image/jpeg"},
                {"data", base64Image}
            };
            lastParts.push_back(imagePart);
        }
    }

    body["contents"] = contents;
    body["generationConfig"] = {
        {"temperature", _config.temperature},
        {"maxOutputTokens", _config.max_tokens}
    };

    if (!function_declarations_json.empty())
    {
        const json declarations = json::parse(function_declarations_json);
        if (!declarations.is_array())
        {
            throw std::invalid_argument(
                "GeminiClient: function declarations phai la JSON array");
        }
        if (!declarations.empty())
        {
            body["tools"] = json::array({{{"functionDeclarations", declarations}}});
        }
    }

    return body.dump();
}

// ------------------------------------------------------------------
// sendRequest()
// Chịu trách nhiệm DUY NHẤT: gửi HTTP POST và trả về raw JSON string.
// Tách riêng khỏi buildRequestBody() và parseResponse() để mỗi hàm
// chỉ làm một việc (Single Responsibility) -> dễ test độc lập, ví dụ
// sau này có thể mock sendRequest() khi viết unit test cho parseResponse().
//
// Xử lý lỗi theo đúng yêu cầu 3.1 của đề: timeout, connection refused,
// HTTP status lỗi -> tất cả quy về exception, KHÔNG trả chuỗi rỗng
// hay im lặng nuốt lỗi.
// ------------------------------------------------------------------
std::string GeminiClient::sendRequest(const std::string &jsonBody) const
{
    constexpr int max_attempts = 5;
    const std::string url = buildEndpointUrl();
    std::string last_response;
    long last_status = 0;

    for (int attempt = 0; attempt < max_attempts; ++attempt)
    {
        CURL *curl = curl_easy_init();
        if (!curl)
        {
            throw std::runtime_error("GeminiClient: khong the khoi tao CURL");
        }

        std::string response_buffer;
        std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>
            headers(nullptr, curl_slist_free_all);
        headers.reset(curl_slist_append(
            headers.release(), "Content-Type: application/json"));

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                         static_cast<long>(jsonBody.size()));
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,
                         static_cast<long>(_config.timeout_ms));

        const CURLcode request_result = curl_easy_perform(curl);
        long http_status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
        curl_easy_cleanup(curl);

        if (request_result != CURLE_OK)
        {
            throw APIEnvironmentError(
                std::string("GeminiClient: loi ket noi - ") +
                curl_easy_strerror(request_result));
        }

        if (http_status >= 200 && http_status < 300)
        {
            return response_buffer;
        }

        last_status = http_status;
        last_response = std::move(response_buffer);

        const bool transient_status =
            http_status == 429 || http_status == 500 ||
            http_status == 502 || http_status == 503 ||
            http_status == 504;

        if (!transient_status || attempt + 1 == max_attempts)
        {
            break;
        }

        int delay_ms = std::min(30000, 2000 * (1 << attempt));
        try
        {
            const json error_body = json::parse(last_response);
            if (error_body.contains("error") &&
                error_body["error"].contains("details"))
            {
                for (const auto &detail : error_body["error"]["details"])
                {
                    const std::string retry_delay =
                        detail.value("retryDelay", "");
                    if (!retry_delay.empty() && retry_delay.back() == 's')
                    {
                        const double seconds = std::stod(
                            retry_delay.substr(0, retry_delay.size() - 1));
                        delay_ms = std::min(
                            60000,
                            std::max(
                                delay_ms,
                                static_cast<int>(seconds * 1000.0)));
                    }
                }
            }
        }
        catch (const std::exception &)
        {
            // Fall back to exponential backoff when the error body is unknown.
        }

        std::cerr << "[GeminiClient] HTTP " << http_status
                  << "; retry " << (attempt + 2) << "/" << max_attempts
                  << " after " << delay_ms << " ms\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }

    throw APIEnvironmentError(
        "GeminiClient: HTTP status " + std::to_string(last_status) +
        ", body: " + last_response);
}

// ------------------------------------------------------------------
// parseToolAwareResponse()
// Duyệt toàn bộ parts vì model thinking có thể đặt suy nghĩ ở part đầu và
// functionCall/text trả lời thật ở part sau.
//
// Dùng try/catch quanh json::parse + truy cập field, vì:
//   - JSON có thể malformed (đúng yêu cầu 3.1: "malformed JSON response")
//   - Gemini có thể trả lỗi dạng {"error": {"message": ...}} thay vì
//     "candidates" -> phải bắt riêng để thông báo lỗi có ý nghĩa,
//     thay vì để chương trình crash vì truy cập field không tồn tại.
// ------------------------------------------------------------------
LLMResponse GeminiClient::parseToolAwareResponse(const std::string &rawJson) const
{
    json parsed;
    try
    {
        parsed = json::parse(rawJson);
    }
    catch (const json::parse_error &e)
    {
        throw LLMClientError(
            std::string("GeminiClient: JSON response khong hop le - ") + e.what());
    }

    // Gemini bao lỗi trong field "error" thay vì HTTP status khác 200
    // trong một số trường hợp (vd content bị chặn bởi safety filter).
    if (parsed.contains("error"))
    {
        std::string message;
        if (parsed["error"].is_string())
        {
            message = parsed["error"].get<std::string>();
        }
        else
        {
            message = parsed["error"].value("message", "unknown error");
        }
        throw LLMClientError("GeminiClient: Gemini tra ve loi - " + message);
    }

    if (!parsed.contains("candidates") || !parsed["candidates"].is_array() || parsed["candidates"].empty())
    {
        throw LLMClientError("GeminiClient: response thieu candidates hoac candidates rong");
    }

    TokenUsage usage;
    if (parsed.contains("usageMetadata") && parsed["usageMetadata"].is_object())
    {
        const auto &metadata = parsed["usageMetadata"];
        usage.prompt_tokens = metadata.value("promptTokenCount", 0LL);
        usage.candidate_tokens = metadata.value("candidatesTokenCount", 0LL);
        usage.thought_tokens = metadata.value("thoughtsTokenCount", 0LL);
        usage.total_tokens = metadata.value("totalTokenCount", 0LL);
    }

    try
    {
        const auto &candidate = parsed["candidates"].at(0);
        const auto &parts = candidate.at("content").at("parts");
        if (!parts.is_array() || parts.empty())
        {
            throw LLMClientError("GeminiClient: response khong co content parts");
        }

        std::string answer_text;
        std::string fallback_thought;

        for (const auto &part : parts)
        {
            if (part.contains("functionCall") && part["functionCall"].is_object())
            {
                const auto &function_call = part["functionCall"];
                LLMToolCall call;
                call.name = function_call.at("name").get<std::string>();
                call.args = function_call.value("args", json::object()).dump();
                call.id = function_call.value("id", "");
                call.thought_signature = part.value("thoughtSignature", "");
                return {"", call, usage};
            }

            if (!part.contains("text") || !part["text"].is_string())
            {
                continue;
            }

            const std::string text = part["text"].get<std::string>();
            if (part.value("thought", false))
            {
                if (!fallback_thought.empty())
                {
                    fallback_thought += '\n';
                }
                fallback_thought += text;
                continue;
            }

            if (!answer_text.empty())
            {
                answer_text += '\n';
            }
            answer_text += text;
        }

        if (!answer_text.empty())
        {
            return {answer_text, std::nullopt, usage};
        }
        if (!fallback_thought.empty())
        {
            return {fallback_thought, std::nullopt, usage};
        }

        throw LLMClientError(
            "GeminiClient: response khong co text hoac functionCall");
    }
    catch (const json::exception &e)
    {
        throw std::runtime_error(
            std::string("GeminiClient: response thieu field can thiet hoac sai kieu - ") + e.what());
    }
}

// ------------------------------------------------------------------
// chat()
// Đây là hàm public duy nhất mà AgentLoop thực sự gọi cho text-only.
// Nó chỉ điều phối 3 bước, KHÔNG chứa logic chi tiết -> đúng nguyên tắc
// "mỗi hàm một việc": build -> send -> parse.
// images rỗng vì đây là chat text-only.
// ------------------------------------------------------------------
std::string GeminiClient::chat(const std::vector<Message> &messages)
{
    std::string requestBody = buildRequestBody(messages, {}, "");
    std::string rawResponse = sendRequest(requestBody);
    LLMResponse response = parseToolAwareResponse(rawResponse);
    if (response.tool_call.has_value())
    {
        return json({
            {"type", "tool_call"},
            {"tool", response.tool_call->name},
            {"args", json::parse(response.tool_call->args)}
        }).dump();
    }
    return response.text;
}

LLMResponse GeminiClient::chatWithTools(
    const std::vector<Message> &messages,
    const std::string &function_declarations_json)
{
    std::string requestBody = buildRequestBody(
        messages, {}, function_declarations_json);
    std::string rawResponse = sendRequest(requestBody);
    return parseToolAwareResponse(rawResponse);
}

// ------------------------------------------------------------------
// chatMultimodal()
// Giống hệt chat() nhưng truyền thêm danh sách ảnh (base64) vào
// buildRequestBody(). Việc dùng chung sendRequest()/parseResponse()
// với chat() chứng minh interface LLMClient thiết kế đúng: text-only
// và multimodal chỉ khác nhau ở bước build request, còn gửi/nhận/parse
// response xử lý giống nhau hoàn toàn.
// ------------------------------------------------------------------
std::string GeminiClient::chatMultimodal(const std::vector<Message> &messages,
                                          const std::vector<std::string> &images)
{
    std::string requestBody = buildRequestBody(messages, images, "");
    std::string rawResponse = sendRequest(requestBody);
    LLMResponse response = parseToolAwareResponse(rawResponse);
    return response.text;
}
