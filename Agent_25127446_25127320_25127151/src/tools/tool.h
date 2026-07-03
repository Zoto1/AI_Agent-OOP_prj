#pragma once
#include <string>
#include <map>

class Tool {
protected:
    std::string name;
    std::string description;

public:
    Tool(const std::string& n, const std::string& d) : name(n), description(d) {}
    virtual ~Tool() = default; 
s
    virtual std::string execute(const std::map<std::string, std::string>& args) = 0;

    std::string getName() const { return name; }
    std::string getDescription() const { return description; }
};