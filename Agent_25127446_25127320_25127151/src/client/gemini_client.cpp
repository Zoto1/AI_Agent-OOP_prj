#include "gemini_client.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>

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
                                            const std::vector<std::string> &images) const
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
        entry["parts"] = json::array({{{"text", msg.content}}});
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
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        throw std::runtime_error("GeminiClient: khong the khoi tao CURL");
    }

    std::string responseBuffer;
    std::string url = buildEndpointUrl();

    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(nullptr, curl_slist_free_all);
    headers.reset(curl_slist_append(headers.release(), "Content-Type: application/json"));

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(jsonBody.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(_config.timeout_ms));

    CURLcode res = curl_easy_perform(curl);

    long httpStatus = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);

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

// ------------------------------------------------------------------
// parseResponse()
// Nhận raw JSON string từ Gemini, trích ra phần text trả lời.
// Cấu trúc response thành công của Gemini (rút gọn):
//   { "candidates": [ { "content": { "parts": [ { "text": "..." } ] } } ] }
//
// Dùng try/catch quanh json::parse + truy cập field, vì:
//   - JSON có thể malformed (đúng yêu cầu 3.1: "malformed JSON response")
//   - Gemini có thể trả lỗi dạng {"error": {"message": ...}} thay vì
//     "candidates" -> phải bắt riêng để thông báo lỗi có ý nghĩa,
//     thay vì để chương trình crash vì truy cập field không tồn tại.
// ------------------------------------------------------------------
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
        std::string message;
        if (parsed["error"].is_string())
        {
            message = parsed["error"].get<std::string>();
        }
        else
        {
            message = parsed["error"].value("message", "unknown error");
        }
        throw std::runtime_error("GeminiClient: Gemini tra ve loi - " + message);
    }

    if (!parsed.contains("candidates") || !parsed["candidates"].is_array() || parsed["candidates"].empty())
    {
        throw std::runtime_error("GeminiClient: response thieu candidates hoac candidates rong");
    }

    try
    {
        const auto &candidate = parsed["candidates"].at(0);
        return candidate.at("content").at("parts").at(0).at("text").get<std::string>();
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
    std::string requestBody = buildRequestBody(messages, {});
    std::string rawResponse = sendRequest(requestBody);
    return parseResponse(rawResponse);
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
    std::string requestBody = buildRequestBody(messages, images);
    std::string rawResponse = sendRequest(requestBody);
    return parseResponse(rawResponse);
}