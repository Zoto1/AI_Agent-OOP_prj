#include "skill_loader.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

std::string SkillLoader::readFile(const fs::path &filepath)
{
    std::ifstream file(filepath);
    if (!file)
    {
        std::cerr << "[SkillLoader Error] Khong the mo file: " << filepath.string() << "\n";
        return "";
    }

    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

static std::string to_lowercase(const std::string &str)
{
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(),
                   [](unsigned char c)
                   { return std::tolower(c); });
    return lower_str;
}

static std::string trim(const std::string &value)
{
    const std::string whitespace = " \t\r\n";
    auto start = value.find_first_not_of(whitespace);
    if (start == std::string::npos)
        return "";
    auto end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

SkillLoader::SkillLoader(const std::string &directory)
{
    skill_directory = directory;
}
void SkillLoader::load_skills()
{
    loaded_skill.clear();
    if (!fs::exists(skill_directory) || !fs::is_directory(skill_directory))
    {
        std::cerr << "[SkillLoader Error] Thư mục '" << skill_directory << "' không tồn tại!\n";
        return;
    }

    for (const auto &entry : fs::directory_iterator(skill_directory))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".md")
        {
            Skill skill;
            skill.name = entry.path().stem().string();

            std::string content = readFile(entry.path());
            if (content.empty())
            {
                std::cerr << "[SkillLoader Warning] Nội dung skill trống: " << entry.path().string() << "\n";
                continue;
            }

            std::stringstream ss(content);
            std::string line;
            std::stringstream instruction_stream;

            while (std::getline(ss, line))
            {
                std::string trimmed = trim(line);
                if (trimmed.rfind("Keywords:", 0) == 0)
                {
                    skill.keywords = trim(trimmed.substr(9));
                }
                instruction_stream << line << "\n";
            }

            skill.instruction = instruction_stream.str();
            loaded_skill.push_back(skill);

            std::cout << "[SkillLoader] Đã tải skill: " << skill.name << "\n";
        }
    }
}

std::optional<Skill> SkillLoader::select_skill(const std::string &task_description)
{
    if (loaded_skill.empty())
    {
        return std::nullopt;
    }

    /*Keywords: tính, toán, calculator, phép tính, cộng, trừ, nhân, chia
=== INSTRUCTION ===
BẮT BUỘC dùng tool calculator cho mọi phép tính số học.
Không được tự nhẩm kết quả.*/

    std::string lower_task = to_lowercase(task_description);
    int max_score = 0;
    std::optional<Skill> best_skill = std::nullopt;

    // Duyệt qua từng skill để tính điểm khớp từ khóa
    for (const auto &skill : loaded_skill)
    {
        int current_score = 0;
        std::string lower_keywords = to_lowercase(skill.keywords);

        // Tách các keyword bằng dấu phẩy
        std::stringstream ss(lower_keywords);
        std::string kw;
        while (std::getline(ss, kw, ','))
        {
            kw.erase(0, kw.find_first_not_of(" \t\r\n"));
            kw.erase(kw.find_last_not_of(" \t\r\n") + 1);

            if (!kw.empty() && lower_task.find(kw) != std::string::npos)
            {
                current_score++;
            }
        }

        // update skill có điểm cao nhất
        if (current_score > max_score)
        {
            max_score = current_score;
            best_skill = skill;
        }
    }
    return best_skill;
}
std::string SkillLoader::inject_into_prompt(const std::string &original_system_prompt, const Skill &selected_skill)
{
    std::stringstream ss;

    ss << original_system_prompt << "\n\n";

    ss << "=== ACTIVE SKILL INSTRUCTION: " << selected_skill.name << " ===\n";
    ss << selected_skill.instruction << "\n";
    ss << "=========================================================\n";

    return ss.str();
}
