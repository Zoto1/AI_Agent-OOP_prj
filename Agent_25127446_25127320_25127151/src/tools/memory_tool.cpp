#include "memory_tool.h"
#include "tool_registry.h"

Memory::Memory() 
    : Tool("memory", "Luu va lay ngu canh bo nho nguoi dung"), storage_path(path) {}
std::string Memory::execute(const std::map<std::string, std::string>& args) {
    auto it = args.find("action");
    if (it == args.end()) {
        return "Loi: Thieu tham so, yeu cau 'save' hoac 'load'!";
    }
    std::string action = it->second;
    if (action == "save") {
        auto keyIt = args.find("key");
        auto valueIt = args.find("value");
        if (keyIt == args.end() || valueIt == args.end()) {
            return "Loi: Thieu tham so 'key' hoac 'value' de thuc hien save!";
        }
        
        bool success = save_context(keyIt->second, valueIt->second);
        return success ? "True" : "False";
    }
    if (action == "load") {
        auto queryIt = args.find("query");
        if (queryIt == args.end()) {
            return "Loi: Thieu tham so 'query' de thuc hien load!";
        }
    return load_context(queryIt->second).value_or("Loi: Khong tim thay ngu canh phu hop trong bo nho!");}
    return "Loi: Hanh dong '" + action + "' khong hop le!";
}
bool Memory::save_context(const std::string& key, const std::string& value) {
    if (key.empty()) return false;
    memory_data [key] = value;
    return true; 
}
std::optional<std::string> Memory::load_context(const std::string& query) { // check lai ham nay
    auto found = memory_data.find(query);
    if (found != memory_data.end()) {
        return found->second;
    }
    return std::nullopt;
}
void clear_memory() {
    memory_data.clear();
}

void Memory::init(){
    auto instance = std::make_shared<Memory>();
    ToolRegistry::getInstance().registerTool(instance);}