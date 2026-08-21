#include "memory_tool.h"
#include "tool_registry.h"

#include <sqlite3.h>

#include <cmath>
#include <cctype>
#include <map>
#include <filesystem>

namespace
{
constexpr double kSimilarityThreshold = 0.2;
constexpr double kEmbeddingThreshold = 0.55;

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

double cosineSimilarityVectors(const std::vector<float> &a,
                               const std::vector<float> &b)
{
    if (a.empty() || a.size() != b.size())
    {
        return 0.0;
    }
    double dot = 0.0, norm_a = 0.0, norm_b = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        dot += static_cast<double>(a[i]) * b[i];
        norm_a += static_cast<double>(a[i]) * a[i];
        norm_b += static_cast<double>(b[i]) * b[i];
    }
    if (norm_a == 0.0 || norm_b == 0.0)
    {
        return 0.0;
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

Memory::Memory(std::shared_ptr<EmbeddingClient> embedder,
               std::string persist_path)
    : Tool(
          "memory",
          "Save or load a persistent memory entry with embedding-based vector "
          "search. For save use action, key, value. For load use action, "
          "query.",
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
          }),
      embedder_(std::move(embedder)),
      persist_path_(std::move(persist_path))
{
    // 10.2 Persistent Memory: khóa đường dẫn lưu trữ thành tuyệt đối ngay lúc
    // khởi tạo. HarnessRunner (CurrentPathGuard) chdir vào workspace riêng của
    // từng task khi chạy benchmark; nếu giữ đường dẫn tương đối thì save() ghi
    // vào thư mục workspace task còn load() đọc từ CWD cũ => mất persistence.
    if (!persist_path_.empty())
    {
        persist_path_ = std::filesystem::absolute(persist_path_).lexically_normal().string();
        const std::filesystem::path parent =
            std::filesystem::path(persist_path_).parent_path();
        if (!parent.empty())
        {
            std::error_code error;
            std::filesystem::create_directories(parent, error);
        }

        const char *create_sql =
            "CREATE TABLE IF NOT EXISTS memory("
            "key TEXT PRIMARY KEY,"
            "value TEXT NOT NULL,"
            "embedding BLOB)";

        // III.3.2: memory lưu qua SQLite thay vì JSON.
        if (sqlite3_open(persist_path_.c_str(), &db_) == SQLITE_OK)
        {
            char *error_message = nullptr;
            int result = sqlite3_exec(db_, create_sql, nullptr, nullptr, &error_message);
            if (result != SQLITE_OK)
            {
                if (error_message != nullptr)
                {
                    sqlite3_free(error_message);
                }
                // File cũ là JSON (phiên bản trước) -> xóa và tạo DB mới.
                sqlite3_close(db_);
                db_ = nullptr;
                std::error_code remove_error;
                std::filesystem::remove(persist_path_, remove_error);
                if (sqlite3_open(persist_path_.c_str(), &db_) == SQLITE_OK)
                {
                    sqlite3_exec(db_, create_sql, nullptr, nullptr, nullptr);
                }
            }
        }

        load_all_from_db();
    }
}

Memory::~Memory()
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (db_ != nullptr)
    {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

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
    std::lock_guard<std::mutex> lock(mtx_);
    if (key.empty())
    {
        return false;
    }

    Entry entry;
    entry.value = value;

    if (embedder_)
    {
        try
        {
            entry.embedding = embedder_->embed(key + ": " + value);
        }
        catch (const std::exception &error)
        {
            // Embedding lỗi (Ollama chưa chạy) -> lưu không có embedding,
            // load_context sẽ fallback về trigram search.
            entry.embedding.clear();
        }
    }

    memory_data[key] = std::move(entry);
    if (!persist_entry(key, memory_data.at(key)))
    {
        // DB không khả dụng (Ollama/đĩa lỗi) -> vẫn giữ entry trong RAM.
    }
    return true;
}

std::optional<std::string> Memory::load_context(const std::string &query) const
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto found = memory_data.find(query);
    if (found != memory_data.end())
    {
        return found->second.value;
    }

    // Ưu tiên embedding search nếu có dữ liệu embedding.
    if (embedder_)
    {
        auto embedded = load_by_embedding(query);
        if (embedded.has_value())
        {
            return embedded;
        }
    }

    // Fallback: trigram cosine similarity (chạy khi không có Ollama).
    const auto query_vector = buildTokenVector(query);
    if (query_vector.empty())
    {
        return std::nullopt;
    }

    double best_similarity = 0.0;
    const std::string *best_value = nullptr;
    for (const auto &[key, entry] : memory_data)
    {
        const auto candidate_vector = buildTokenVector(key + " " + entry.value);
        const double similarity = cosineSimilarity(query_vector, candidate_vector);
        if (similarity > best_similarity)
        {
            best_similarity = similarity;
            best_value = &entry.value;
        }
    }

    if (best_value != nullptr && best_similarity >= kSimilarityThreshold)
    {
        return *best_value;
    }
    return std::nullopt;
}

std::optional<std::string> Memory::load_by_embedding(const std::string &query) const
{
    std::vector<float> query_embedding;
    try
    {
        query_embedding = embedder_->embed(query);
    }
    catch (const std::exception &)
    {
        return std::nullopt;
    }

    if (query_embedding.empty())
    {
        return std::nullopt;
    }

    double best_similarity = 0.0;
    const std::string *best_value = nullptr;
    for (const auto &[key, entry] : memory_data)
    {
        if (entry.embedding.empty())
        {
            continue;
        }
        const double similarity =
            cosineSimilarityVectors(query_embedding, entry.embedding);
        if (similarity > best_similarity)
        {
            best_similarity = similarity;
            best_value = &entry.value;
        }
    }

    if (best_value != nullptr && best_similarity >= kEmbeddingThreshold)
    {
        return *best_value;
    }
    return std::nullopt;
}

void Memory::exec_simple(const char *sql)
{
    if (db_ == nullptr)
    {
        return;
    }
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) == SQLITE_OK)
    {
        sqlite3_step(statement);
        sqlite3_finalize(statement);
    }
}

bool Memory::persist_entry(const std::string &key, const Entry &entry) const
{
    if (db_ == nullptr)
    {
        return false;
    }

    const char *sql =
        "INSERT OR REPLACE INTO memory(key, value, embedding) VALUES(?,?,?)";
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(statement, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, entry.value.c_str(), -1, SQLITE_TRANSIENT);
    if (entry.embedding.empty())
    {
        sqlite3_bind_null(statement, 3);
    }
    else
    {
        sqlite3_bind_blob(
            statement, 3, entry.embedding.data(),
            static_cast<int>(entry.embedding.size() * sizeof(float)),
            SQLITE_TRANSIENT);
    }

    const bool ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return ok;
}

bool Memory::load_all_from_db()
{
    if (db_ == nullptr)
    {
        return false;
    }

    const char *sql = "SELECT key, value, embedding FROM memory";
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        return false;
    }

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        const auto *key_text = sqlite3_column_text(statement, 0);
        const auto *value_text = sqlite3_column_text(statement, 1);
        if (key_text == nullptr || value_text == nullptr)
        {
            continue;
        }

        Entry entry;
        entry.value = reinterpret_cast<const char *>(value_text);
        if (sqlite3_column_type(statement, 2) == SQLITE_BLOB)
        {
            const void *blob = sqlite3_column_blob(statement, 2);
            const int byte_count = sqlite3_column_bytes(statement, 2);
            if (blob != nullptr && byte_count > 0 &&
                byte_count % static_cast<int>(sizeof(float)) == 0)
            {
                const auto *begin = static_cast<const float *>(blob);
                const auto *end = begin + byte_count / static_cast<int>(sizeof(float));
                entry.embedding.assign(begin, end);
            }
        }
        memory_data[reinterpret_cast<const char *>(key_text)] = std::move(entry);
    }

    sqlite3_finalize(statement);
    return true;
}

void Memory::clear_memory()
{
    std::lock_guard<std::mutex> lock(mtx_);
    memory_data.clear();
    exec_simple("DELETE FROM memory");
}

void Memory::init()
{
    auto instance = std::make_shared<Memory>();
    ToolRegistry::getInstance().registerTool(instance);
}
