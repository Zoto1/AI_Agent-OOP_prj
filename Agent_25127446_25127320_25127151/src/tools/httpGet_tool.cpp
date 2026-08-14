#include "httpGet_tool.h"
#include <curl/curl.h>

HttpGetTool::HttpGetTool()
    : Tool(
          "http_get",
          "Perform an HTTP GET request to fetch web raw data or API response. Args: url.",
          {
              {"type", "OBJECT"},
              {"properties", {
                  {"url", {
                      {"type", "STRING"},
                      {"description", "The URL to send GET request to (e.g. https://api.github.com)"}
                  }}
              }},
              {"required", {"url"}}
          }) {}

size_t HttpGetTool::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

std::string HttpGetTool::execute(const std::map<std::string, std::string>& args) {
    auto it = args.find("url");
    if (it == args.end() || it->second.empty()) {
        return "Lỗi: Thiếu tham số 'url'.";
    }

    std::string url = it->second;
    CURL* curl = curl_easy_init();
    if (!curl) return "Lỗi: Không thể khởi tạo libcurl.";

    std::string response_buffer;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "C++-Agent-HttpGetTool/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return std::string("Lỗi HTTP Request: ") + curl_easy_strerror(res);
    }

    return response_buffer;
}