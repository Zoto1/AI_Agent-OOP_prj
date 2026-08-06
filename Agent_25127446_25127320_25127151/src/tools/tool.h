#pragma once
#include <map>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

class Tool {
protected:
    std::string name;
    std::string description;
    nlohmann::json parameters_schema;

public:
    Tool(const std::string& n,
         const std::string& d,
         nlohmann::json schema = {
             {"type", "OBJECT"},
             {"properties", nlohmann::json::object()}
         })
        : name(n), description(d), parameters_schema(std::move(schema)) {}
    virtual ~Tool() = default;

    virtual std::string execute(const std::map<std::string, std::string>& args) = 0;

    std::string getName() const { return name; }
    std::string getDescription() const { return description; }
    const nlohmann::json& getParametersSchema() const { return parameters_schema; }
};
