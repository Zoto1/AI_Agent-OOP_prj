#pragma once

#include "llm_client.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>
#include <stdexcept>

// ConfigLoader: đọc config.json (nếu có) + fallback sang biến môi trường
// để tạo ra LLMConfig cho từng provider ("gemini", "ollama", ...).
//
// Tách riêng thành 1 loader dùng chung, KHÔNG nhét logic đọc file vào
// bên trong GeminiClient/OllamaClient -> đúng nguyên tắc separation of
// concerns: LLMClient chỉ lo việc gọi API, không lo việc key lấy từ đâu.
class ConfigLoader
{
public:
    // provider: "gemini" hoặc "ollama" -> khớp key trong config.json
    // envVarName: tên biến môi trường dùng làm fallback cho api_key
    //             (vd "GEMINI_API_KEY"). Truyền "" nếu provider không cần key.
    static LLMConfig loadLLMConfig(const std::string &configPath,
                                    const std::string &provider,
                                    const std::string &envVarName = "")
    {
        LLMConfig cfg;

        std::ifstream file(configPath);
        if (!file.is_open())
        {
            throw std::runtime_error("ConfigLoader: khong mo duoc file " + configPath);
        }

        nlohmann::json root;
        file >> root;

        if (!root.contains(provider))
        {
            throw std::invalid_argument(
                "ConfigLoader: config.json thieu muc \"" + provider + "\"");
        }

        const auto &section = root.at(provider);
        cfg.base_url = section.value("base_url", "");
        cfg.model_name = section.value("model_name", "");
        cfg.temperature = section.value("temperature", 0.7f);
        cfg.max_tokens = section.value("max_tokens", 1024);
        cfg.timeout_ms = section.value("timeout_ms", 30000);

        // Ưu tiên api_key trong config.json; nếu không có, thử env var.
        // Cách này tiện khi demo trên máy khác nhau mà không muốn sửa file.
        if (section.contains("api_key") && !section["api_key"].get<std::string>().empty())
        {
            cfg.api_key = section["api_key"].get<std::string>();
        }
        else if (!envVarName.empty())
        {
            if (const char *envKey = std::getenv(envVarName.c_str()))
            {
                cfg.api_key = std::string(envKey);
            }
        }

        return cfg;
    }
};