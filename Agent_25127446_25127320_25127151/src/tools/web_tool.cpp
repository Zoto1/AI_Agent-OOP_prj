#include "web_tool.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <cctype>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>

using json = nlohmann::json;

namespace
{
size_t WebSearchWriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}
} // namespace

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

std::string WebSearchTool::urlEncode(const std::string &value)
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (const unsigned char character : value)
    {
        if (std::isalnum(character) || character == '-' || character == '_' ||
            character == '.' || character == '~')
        {
            escaped << static_cast<char>(character);
        }
        else
        {
            escaped << '%' << std::setw(2)
                    << static_cast<int>(character);
        }
    }
    return escaped.str();
}

std::string WebSearchTool::fetchSearchResults(const std::string &query)
{
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        return "Error: Failed to initialize libcurl.";
    }

    const std::string url =
        "https://api.duckduckgo.com/?q=" + urlEncode(query) +
        "&format=json&no_html=1&skip_disambig=1&pretty=0";

    std::string raw_response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WebSearchWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw_response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "OopAgent/1.0 (web_search tool)");
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
        return "Error fetching search results: " +
               std::string(curl_easy_strerror(result));
    }
    if (http_code != 200)
    {
        return "Error: HTTP status " + std::to_string(http_code) +
               " from search service.";
    }
    if (raw_response.empty())
    {
        return "No results found for query: " + query;
    }

    // Parse the DuckDuckGo Instant Answer JSON and keep only useful fields.
    try
    {
        const json document = json::parse(raw_response);
        std::string summary;

        if (document.contains("Heading") && !document["Heading"].is_null())
        {
            summary += "Heading: " +
                       document["Heading"].get<std::string>() + "\n";
        }
        if (document.contains("AbstractText") &&
            !document["AbstractText"].is_null())
        {
            const std::string abstract =
                document["AbstractText"].get<std::string>();
            if (!abstract.empty())
            {
                summary += "Abstract: " + abstract + "\n";
            }
        }
        if (document.contains("AbstractURL") &&
            !document["AbstractURL"].is_null())
        {
            const std::string abstract_url =
                document["AbstractURL"].get<std::string>();
            if (!abstract_url.empty())
            {
                summary += "Source: " + abstract_url + "\n";
            }
        }
        if (document.contains("RelatedTopics") &&
            document["RelatedTopics"].is_array())
        {
            int count = 0;
            for (const auto &topic : document["RelatedTopics"])
            {
                if (topic.is_object() && topic.contains("Text") &&
                    !topic["Text"].is_null())
                {
                    summary += "- " + topic["Text"].get<std::string>() + "\n";
                    if (++count >= 5)
                    {
                        break;
                    }
                }
            }
        }

        if (summary.empty())
        {
            return "No results found for query: " + query;
        }
        return summary;
    }
    catch (const json::exception &error)
    {
        return "Error parsing search results: " + std::string(error.what());
    }
}

std::string WebSearchTool::execute(const std::map<std::string, std::string> &args)
{
    auto it = args.find("query");
    if (it == args.end() || it->second.empty())
    {
        return "Error: Missing 'query' parameter!";
    }

    return fetchSearchResults(it->second);
}
