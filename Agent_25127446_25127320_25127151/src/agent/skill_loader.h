#pragma once

#include <iostream>
#include <string>

#include <vector>
#include <filesystem> //c++17
/* sử dụng để quét toàn bộ thư mục SKills
=> đọc nội dung các file .md: gồm keywords*/
#include <optional>

/*
1. Khởi tạo + quét thư mục
2. Khớp keyword => chọn skill (người dùng yêu cầu 1 task-> skillloader xem và chọn skill)
*/

struct Skill
{
    std::string name;        // tên skill
    std::string keywords;    // keyword ->match
    std::string instruction; // tutorial
};

class SkillLoader
{

private:
    std::string skill_directory;
    std::vector<Skill> loaded_skill;
    std::string readFile(const std::filesystem::path& filepath); // hàm đọc file

public:
    SkillLoader(const std::string &directory = "skills/");
    void load_skills();
    std::optional<Skill> select_skill(const std::string &task_description);
    std::string inject_into_prompt(const std::string &original_system_prompt, const Skill &selected_skill);
};
