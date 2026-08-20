#include "../tools/memory_tool.h"
#include "../client/embedding_client.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>

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

    // ---- 4. Persistence bền khi CWD thay đổi (harness chdir từng task) ----
    // Mô phỏng flow thật: Memory khởi tạo ở CWD gốc (startup), sau đó harness
    // chdir vào workspace task để save. Không có fix, save() sẽ ghi vào file
    // trong thư mục workspace task => mất persistence.
    {
        namespace fs = std::filesystem;
        auto fake = std::make_shared<FakeEmbeddingClient>();
        const std::string store = remove_temp("memory_test_cwd.json");

        std::string save_result;
        {
            Memory writer(fake, store);  // khóa đường dẫn tuyệt đối tại đây
            writer.clear_memory();

            const std::string old_cwd = fs::current_path().string();
            fs::create_directories("memory_tmp_ws");
            fs::current_path("memory_tmp_ws");

            save_result = writer.execute({
                {"action", "save"},
                {"key", "location"},
                {"value", "HCM"}
            });

            fs::current_path(old_cwd);
            fs::remove_all("memory_tmp_ws");
        }
        assert(save_result == "True");

        Memory reader(fake, store);
        const std::string result = reader.execute({
            {"action", "load"},
            {"query", "location"}
        });
        assert(result == "HCM");

        std::cout << "[PASS] persistence stable across CWD change (harness chdir)\n";
    }

    // ---- 5. An toàn khi nhiều sub-agent (10.3) gọi chung Memory ----
    {
        auto fake = std::make_shared<FakeEmbeddingClient>();
        const std::string store = remove_temp("memory_test_thread.json");
        Memory memory(fake, store);
        memory.clear_memory();

        std::vector<std::jthread> threads;
        for (int t = 0; t < 4; ++t)
        {
            threads.emplace_back([&memory, t] {
                for (int i = 0; i < 50; ++i)
                {
                    const std::string key =
                        "key_" + std::to_string(t) + "_" + std::to_string(i);
                    memory.execute({
                        {"action", "save"},
                        {"key", key},
                        {"value", "v" + std::to_string(t)}
                    });
                    memory.execute({
                        {"action", "load"},
                        {"query", key}
                    });
                }
            });
        }
        threads.clear();  // join tất cả thread

        // Mọi entry phải còn nguyên vẹn sau khi chạy song song.
        for (int t = 0; t < 4; ++t)
        {
            const std::string key = "key_" + std::to_string(t) + "_49";
            assert(memory.execute({
                       {"action", "load"},
                       {"query", key}
                   }) == "v" + std::to_string(t));
        }

        std::cout << "[PASS] thread-safe concurrent memory access\n";
    }

    std::cout << "[PASS] Memory embedding + persistence tests\n";
    return 0;
}
