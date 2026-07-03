#pragma once
#include <string>
#include <map>

class Tool {
protected:// chỉ cho lớp con kế thừa sd 
    std::string name;
    std::string description;

public:
    Tool(const std::string& n, const std::string& d) : name(n), description(d) {}
    virtual ~Tool() = default; // Luôn cần destructor ảo cho abstract class

    // Hàm thuần ảo (Pure Virtual Function) - tránh memory leak
    virtual std::string execute(const std::map<std::string, std::string>& args) = 0;

    std::string getName() const { return name; }
    std::string getDescription() const { return description; }
};