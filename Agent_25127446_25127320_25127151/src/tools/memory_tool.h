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

class Memory : public Tool {
private:
    struct Entry {
        std::string value;
        std::vector<float> embedding; // rỗng nếu không dùng embedding
    };

    std::unordered_map<std::string, Entry> memory_data;
    std::shared_ptr<EmbeddingClient> embedder_;
    std::string persist_path_;

    // Bảo vệ memory_data khi nhiều sub-agent (10.3) gọi chung tool này.
    mutable std::mutex mtx_;

    bool save_context(const std::string &key, const std::string &value);
    std::optional<std::string> load_context(const std::string &query) const;
    std::optional<std::string> load_by_embedding(const std::string &query) const;
    void persist() const;
    void load_persisted();

public:
    Memory();
    explicit Memory(std::shared_ptr<EmbeddingClient> embedder,
                    std::string persist_path = "");
    ~Memory() override = default;
    void clear_memory();
    std::string execute(const std::map<std::string, std::string> &args) override;
    static void init();
};
