#include "memory_tool.h"
#include "tool_registry.h"

#include <cmath>
#include <cctype>
#include <map>

namespace
{
constexpr double kSimilarityThreshold = 0.2;

std::map<std::string, int> buildTokenVector(const std::string &text)
{
    std::map<std::string, int> vector;

    std::string normalized;
    normalized.reserve(text.size());
    for (const unsigned char character : text)
    {
        if (std::isalnum(character))
        {
            normalized.push_back(static_cast<char>(std::tolower(character)));
        }
        else if (character == '_' || character == '-' || character == ' ' ||
                 character == '\t')
        {
            normalized.push_back(' ');
        }
    }

    if (normalized.size() < 3)
    {
        if (!normalized.empty())
        {
            ++vector[normalized];
        }
        return vector;
    }

    for (std::size_t i = 0; i + 3 <= normalized.size(); ++i)
    {
        ++vector[normalized.substr(i, 3)];
    }
    return vector;
}

double cosineSimilarity(const std::map<std::string, int> &a,
                        const std::map<std::string, int> &b)
{
    double norm_a = 0.0;
    double norm_b = 0.0;
    for (const auto &[feature, count] : a)
    {
        norm_a += static_cast<double>(count) * count;
    }
    for (const auto &[feature, count] : b)
    {
        norm_b += static_cast<double>(count) * count;
    }
    if (norm_a == 0.0 || norm_b == 0.0)
    {
        return 0.0;
    }

    double dot = 0.0;
    for (const auto &[feature, count] : a)
    {
        auto it = b.find(feature);
        if (it != b.end())
        {
            dot += static_cast<double>(count) * it->second;
        }
    }
    return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
}
} // namespace

Memory::Memory()
    : Tool(
          "memory",
          "Save or load an in-memory value. For save use action, key, value. "
          "For load use action, query (supports fuzzy vector search).",
          {
              {"type", "OBJECT"},
              {"properties", {
                  {"action", {
                      {"type", "STRING"},
                      {"enum", {"save", "load"}},
                      {"description", "Operation to perform"}
                  }},
                  {"key", {{"type", "STRING"}, {"description", "Key used by save"}}},
                  {"value", {{"type", "STRING"}, {"description", "Value used by save"}}},
                  {"query", {{"type", "STRING"}, {"description", "Query used by load"}}}
              }},
              {"required", {"action"}}
          }) {}

std::string Memory::execute(const std::map<std::string, std::string> &args)
{
    auto it = args.find("action");
    if (it == args.end())
    {
        return "Loi: Thieu tham so, yeu cau 'save' hoac 'load'!";
    }
    std::string action = it->second;
    if (action == "save")
    {
        auto keyIt = args.find("key");
        auto valueIt = args.find("value");
        if (keyIt == args.end() || valueIt == args.end())
        {
            return "Loi: Thieu tham so 'key' hoac 'value' de thuc hien save!";
        }

        bool success = save_context(keyIt->second, valueIt->second);
        return success ? "True" : "False";
    }
    if (action == "load")
    {
        auto queryIt = args.find("query");
        if (queryIt == args.end())
        {
            return "Loi: Thieu tham so 'query' de thuc hien load!";
        }
        return load_context(queryIt->second).value_or(
            "Loi: Khong tim thay ngu canh phu hop trong bo nho!");
    }
    return "Loi: Hanh dong '" + action + "' khong hop le!";
}

bool Memory::save_context(const std::string &key, const std::string &value)
{
    if (key.empty())
    {
        return false;
    }
    memory_data[key] = value;
    return true;
}

std::optional<std::string> Memory::load_context(const std::string &query) const
{
    auto found = memory_data.find(query);
    if (found != memory_data.end())
    {
        return found->second;
    }

    // Vector search: find the closest stored context via cosine similarity
    // over character trigram vectors instead of requiring an exact key match.
    const auto query_vector = buildTokenVector(query);
    if (query_vector.empty())
    {
        return std::nullopt;
    }

    double best_similarity = 0.0;
    const std::string *best_value = nullptr;
    for (const auto &[key, value] : memory_data)
    {
        const auto candidate_vector = buildTokenVector(key + " " + value);
        const double similarity = cosineSimilarity(query_vector, candidate_vector);
        if (similarity > best_similarity)
        {
            best_similarity = similarity;
            best_value = &value;
        }
    }

    if (best_value != nullptr && best_similarity >= kSimilarityThreshold)
    {
        return *best_value;
    }
    return std::nullopt;
}

void Memory::clear_memory()
{
    memory_data.clear();
}

void Memory::init()
{
    auto instance = std::make_shared<Memory>();
    ToolRegistry::getInstance().registerTool(instance);
}
