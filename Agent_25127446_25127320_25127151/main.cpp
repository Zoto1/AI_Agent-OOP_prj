#include "gemini_client.h"
#include "config_loader.h"

#include <iostream>
#include <vector>

// Demo nhỏ để kiểm tra GeminiClient hoạt động đúng, tương tự cách
// bạn hay viết test trong main() cho Rectangle/BankAccount trước đây:
// tạo object -> gọi hàm -> in kết quả -> bắt lỗi nếu có.
int main()
{
    try
    {
        // 1) Load config từ file, key lấy từ config.json hoặc
        //    biến môi trường GEMINI_API_KEY nếu config.json không có.
        LLMConfig cfg = ConfigLoader::loadLLMConfig(
            "config.json", "gemini", "GEMINI_API_KEY");

        // 2) Tạo client thông qua con trỏ tới lớp cha LLMClient
        //    -> thể hiện đúng tinh thần "AgentLoop chỉ biết LLMClient,
        //    không biết cụ thể là Gemini hay Ollama" (mục 4.4 đề bài).
        LLMClient *client = new GeminiClient(cfg);

        std::cout << "Dang test GeminiClient voi model: "
                  << client->getModelName() << "\n\n";

        // ---- Test 1: chat() text-only ----
        std::vector<Message> messages = {
            {"system", "Ban la mot tro ly AI tra loi ngan gon."},
            {"user", "1 + 1 bang may?"}
        };

        std::cout << "[Test 1] Goi chat() text-only...\n";
        std::string reply = client->chat(messages);
        std::cout << "Gemini tra loi: " << reply << "\n\n";

        // ---- Test 2: multi-turn (giữ history) ----
        messages.push_back({"assistant", reply});
        messages.push_back({"user", "Nhan doi ket qua do len."});

        std::cout << "[Test 2] Goi chat() voi conversation history...\n";
        std::string reply2 = client->chat(messages);
        std::cout << "Gemini tra loi: " << reply2 << "\n\n";

        // ---- Test 3: xu ly loi - model_name sai ----
        std::cout << "[Test 3] Test truong hop loi (model khong ton tai)...\n";
        LLMConfig badCfg = cfg;
        badCfg.model_name = "model-khong-ton-tai-xyz";
        LLMClient *badClient = new GeminiClient(badCfg);

        try
        {
            badClient->chat(messages);
            std::cout << "[FAIL] Le ra phai nem exception nhung khong thay!\n";
        }
        catch (const std::exception &e)
        {
            std::cout << "[OK] Bat duoc loi nhu mong doi: " << e.what() << "\n";
        }

        delete badClient;
        delete client;
    }
    catch (const std::exception &e)
    {
        // Bắt lỗi ở tầng ngoài cùng: config.json thiếu, thiếu api_key,
        // lỗi mạng không mong đợi... In ra và thoát với mã lỗi khác 0.
        std::cerr << "Loi khong mong doi: " << e.what() << "\n";
        return 1;
    }

    return 0;
}