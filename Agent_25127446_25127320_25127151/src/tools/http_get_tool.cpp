#include "http_get_tool.h"

#include <curl/curl.h>

#include <string>

namespace
{
size_t HttpGetWriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}
} // namespace

HttpGetTool::HttpGetTool()
    : Tool(
          "http_get",
          "Perform an HTTP GET request to a URL and return the response body. "
          "Args: url.",
          {
              {"type", "OBJECT"},
              {"properties", {
                  {"url", {
                      {"type", "STRING"},
                      {"description", "The URL to fetch, e.g. https://example.com"}
                  }}
              }},
              {"required", {"url"}}
          }) {}

std::string HttpGetTool::performRequest(const std::string &url)
{
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        return "Error: Failed to initialize libcurl.";
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HttpGetWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "OopAgent/1.0 (http_get tool)");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);

    const CURLcode result = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK)
    {
        return "Error fetching URL: " + std::string(curl_easy_strerror(result));
    }
    if (http_code >= 400)
    {
        return "Error: HTTP status " + std::to_string(http_code) + " for " + url;
    }
    if (response.empty())
    {
        return "Error: Empty response body from " + url;
    }
    return response;
}

std::string HttpGetTool::execute(const std::map<std::string, std::string> &args)
{
    auto url_it = args.find("url");
    if (url_it == args.end() || url_it->second.empty())
    {
        return "Error: Missing 'url' parameter!";
    }
    return performRequest(url_it->second);
}
