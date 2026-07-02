#include "ollama_client.h"
#include <iostream>
#include <stdexcept>
#include <curl/curl.h>

using json = nlohmann::json;


static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    auto* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}


OllamaClient::OllamaClient(const LLMConfig& cfg) : LLMClient(cfg) {
    // Khởi tạo handle cURL khi sinh đối tượng
    curl_handle = curl_easy_init();
    if (!curl_handle) {
        throw std::runtime_error("Lỗi [OllamaClient]: Không thể khởi tạo cURL.");
    }
}

OllamaClient::~OllamaClient() {
    // RAII: Đảm bảo giải phóng tài nguyên cURL khi hủy đối tượng
    if (curl_handle) {
        curl_easy_cleanup(static_cast<CURL*>(curl_handle));
        curl_handle = nullptr;
    }
}


std::string OllamaClient::chat(const std::vector<Message>& messages) {
    return chatMultimodal(messages, {}); 
}

std::string OllamaClient::chatMultimodal(const std::vector<Message>& messages, const std::vector<std::string>& images) {
    if (!curl_handle) {
        throw std::runtime_error("Lỗi [OllamaClient]: cURL handle chưa được khởi tạo.");
    }

    CURL* curl = static_cast<CURL*>(curl_handle);
    std::string response_string;
    
    std::string url = config.base_url + "/api/chat";
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // --- Chuẩn bị Headers ---
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);


    json payload = {
        {"model", _config.model_name},
        {"stream", false}, // Tắt stream để nhận cục bộ 1 lần
        {"options", {
            {"temperature", _config.temperature},
            {"num_predict", _config.max_tokens}
        }}

//         {
//   "username": "nguyen_van_a",
//   "age": 28,
//   "is_active": true,
//   "roles": ["user", "editor"]
// }

    };

    json json_messages = json::array();
    for (size_t i = 0; i < messages.size(); ++i) {
        json j_msg = {
            {"role", messages[i].role},
            {"content", messages[i].content}
        };
        
        if (i == messages.size() - 1 && !images.empty()) {
            j_msg["images"] = images;
        }
        json_messages.push_back(j_msg);
    }
    payload["messages"] = json_messages;

    std::string payload_str = payload.dump();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, config.timeout_ms); // Yêu cầu 4: Set timeout

    CURLcode res = curl_easy_perform(curl);
    
    curl_slist_free_all(headers); 

    if (res != CURLE_OK) {
        if (res == CURLE_OPERATION_TIMEDOUT) {
            throw std::runtime_error("Lỗi mạng [Timeout]: Ollama phản hồi quá chậm (vượt ngưỡng timeout_ms).");
        } else if (res == CURLE_COULDNT_CONNECT) {
            throw std::runtime_error("Lỗi mạng [Connection Refused]: Không thể kết nối tới máy chủ Ollama. Bạn đã bật Ollama chưa?");
        }
        throw std::runtime_error(std::string("Lỗi mạng [cURL]: ") + curl_easy_strerror(res));
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
        throw std::runtime_error("Lỗi HTTP [Ollama API]: Máy chủ trả về mã lỗi " + std::to_string(http_code));
    }

    try {
        json resp_json = json::parse(response_string);
        if (resp_json.contains("message") && resp_json["message"].contains("content")) {
            return resp_json["message"]["content"].get<std::string>();
        } else {
            throw std::runtime_error("Lỗi Dữ Liệu: Ollama trả về JSON hợp lệ nhưng bị thiếu trường 'message.content'.");
        }
    } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("Lỗi Dữ Liệu [Malformed JSON]: Không thể phân tích cú pháp chuỗi trả về - ") + e.what());
    }
}