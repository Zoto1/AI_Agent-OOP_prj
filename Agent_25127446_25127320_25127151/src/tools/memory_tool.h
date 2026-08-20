#pragma once
#include "tool.h"
#include "client/embedding_client.h"
#include <map>
#include <string>
#include <optional>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>

struct sqlite3;

class Memory : public Tool {
private:
    struct Entry {
        std::string value;
        std::vector<float> embedding; // rỗng nếu không dùng embedding
    };

    std::unordered_map<std::string, Entry> memory_data;
    std::shared_ptr<EmbeddingClient> embedder_;
    std::string persist_path_;
    sqlite3 *db_ = nullptr; // handle SQLite nếu có persist_path_

    // Bảo vệ memory_data + db_ khi nhiều sub-agent (10.3) gọi chung tool này.
    mutable std::mutex mtx_;

    bool save_context(const std::string &key, const std::string &value);
    std::optional<std::string> load_context(const std::string &query) const;
    std::optional<std::string> load_by_embedding(const std::string &query) const;
    bool persist_entry(const std::string &key, const Entry &entry) const;
    bool load_all_from_db();
    void exec_simple(const char *sql);

public:
    Memory();
    explicit Memory(std::shared_ptr<EmbeddingClient> embedder,
                    std::string persist_path = "");
    ~Memory() override;
    void clear_memory();
    void resetState() override { clear_memory(); }
    std::string execute(const std::map<std::string, std::string> &args) override;
    static void init();
};
