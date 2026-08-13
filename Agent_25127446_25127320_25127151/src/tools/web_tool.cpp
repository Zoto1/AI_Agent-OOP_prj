#include "web_tool.h"

WebSearchTool::WebSearchTool()
    : Tool(
          "web_search",
          "Search for information using a text query. Args: query.",
          {
              {"type", "OBJECT"},
              {"properties", {
                  {"query", {
                      {"type", "STRING"},
                      {"description", "Search query"}
                  }}
              }},
              {"required", {"query"}}
          }) {}

// Callback cho libcurl nhận dữ liệu HTTP response
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string WebSearchTool::performSearchRequest(const std::string& query) {
    CURL* curl = curl_easy_init();
    std::string readBuffer;

    if (curl) {
        // Tự động encode khoảng trắng trong query (vd: "tin tuc" -> "tin+tuc")
        std::string encoded_query = query;
        for (char &c : encoded_query) {
            if (c == ' ') c = '+';
        }

        std::string url = "https://api.duckduckgo.com/?q=" + encoded_query + "&format=json";

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK) {
            return readBuffer; // Trả về chuỗi JSON lấy từ DuckDuckGo
        }
    }
    return "Error: Failed to fetch search results from web!";
}

std::string WebSearchTool::execute(const std::map<std::string, std::string>& args) {
    auto it = args.find("query");
    if (it == args.end()) {
        return "Error: Missing 'query' parameter!";
    }

    std::string query_str = it->second;
    return performSearchRequest(query_str);
}