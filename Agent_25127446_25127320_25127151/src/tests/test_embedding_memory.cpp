#include "../tools/memory_tool.h"
#include "../client/embedding_client.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace
{

// Mock embedding: mã hoá văn bản thành vector cố định theo từ.
// Không cần Ollama chạy thật — đủ để test logic cosine similarity
// và nhánh embedding-based search của Memory tool.
class FakeEmbeddingClient : public EmbeddingClient
{
public:
    std::vector<float> embed(const std::string &text) override
    {
        std::vector<float> vec(32, 0.0f);
        for (const unsigned char ch : text)
        {
            vec[static_cast<std::size_t>(ch) % 32] += 1.0f;
        }
        return vec;
    }

    std::string getModelName() const override { return "fake-embed"; }
};

std::string remove_temp(const std::string &path)
{
    std::remove(path.c_str());
    return path;
}

} // namespace

int main()
{
    // ---- 1. Embedding search: query gần nghĩa với entry đã lưu ----
    {
        auto fake = std::make_shared<FakeEmbeddingClient>();
        const std::string store =
            remove_temp("memory_test_embedding.json");
        Memory memory(fake, store);
        memory.clear_memory();

        assert(memory.execute({
                   {"action", "save"},
                   {"key", "favorite_language"},
                   {"value", "Cpp"}
               }) == "True");

        // Query không trùng key chính xác nhưng cùng token "Cpp".
        const std::string result = memory.execute({
            {"action", "load"},
            {"query", "language I like Cpp"}
        });
        assert(result == "Cpp");

        std::cout << "[PASS] embedding-based vector search\n";
    }

    // ---- 2. Persist: entry sống sót sau khi tạo Memory mới ----
    {
        auto fake = std::make_shared<FakeEmbeddingClient>();
        const std::string store =
            remove_temp("memory_test_persist.json");

        {
            Memory writer(fake, store);
            writer.clear_memory();
            assert(writer.execute({
                       {"action", "save"},
                       {"key", "team_size"},
                       {"value", "3 members"}
                   }) == "True");
        }

        Memory reader(fake, store);
        assert(reader.execute({
                   {"action", "load"},
                   {"query", "team_size"}
               }) == "3 members");

        std::cout << "[PASS] persistent memory across instances\n";
    }

    // ---- 3. Fallback trigram khi embedder lỗi ----
    {
        Memory plain;
        plain.clear_memory();
        assert(plain.execute({
                   {"action", "save"},
                   {"key", "ngon_ngu_yeu_thich"},
                   {"value", "Cpp"}
               }) == "True");

        const std::string fuzzy = plain.execute({
            {"action", "load"},
            {"query", "ngon ngu yeu thich"}
        });
        assert(fuzzy == "Cpp");

        std::cout << "[PASS] trigram fallback without embedder\n";
    }

    std::cout << "[PASS] Memory embedding + persistence tests\n";
    return 0;
}
