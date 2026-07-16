#include "tool_registry.h"
#include "memory_tool.h"
#include <iostream>
#include <map>
#include <string>

int main() {
    std::cout << "=== CHUONG TRINH TEST NOI BO MEMORY TOOL ===\n\n";

    // Kích hoạt: Gọi hàm static init() của bạn để tự động nạp vào Registry Singleton
    MemoryTool::init();

    // ==========================================
    // KỊCH BẢN TEST 1: LƯU DỮ LIỆU (ACTION = SAVE)
    // ==========================================
    std::cout << "--- Test 1: Them du lieu vao bo nho ---\n";
    std::map<std::string, std::string> saveArgs1 = {
        {"action", "save"},
        {"key", "user_profile"},
        {"value", "Seonghyeon (CORTIS)"}
    };
    
    // Gọi thông qua Registry
    std::string saveResult1 = ToolRegistry::getInstance().executeTool("memory", saveArgs1);
    std::cout << "Ket qua Save 1: " << saveResult1 << "\n\n";


    // ==========================================
    // KỊCH BẢN TEST 2: ĐỌC DỮ LIỆU THÀNH CÔNG (ACTION = LOAD)
    // ==========================================
    std::cout << "--- Test 2: Lay du lieu da ton tai (Co Optional Value) ---\n";
    std::map<std::string, std::string> loadArgs1 = {
        {"action", "load"},
        {"query", "user_profile"}
    };

    std::string loadResult1 = ToolRegistry::getInstance().executeTool("memory", loadArgs1);
    std::cout << "Ket qua Load 1: " << loadResult1 << "\n\n";


    // ==========================================
    // KỊCH BẢN TEST 3: ĐỌC DỮ LIỆU THẤT BẠI (ACTION = LOAD nhưng sai Key)
    // ==========================================
    std::cout << "--- Test 3: Lay du lieu KHONG ton tai (Tra ve Nullopt) ---\n";
    std::map<std::string, std::string> loadArgs2 = {
        {"action", "load"},
        {"query", "random_key_123"}
    };

    std::string loadResult2 = ToolRegistry::getInstance().executeTool("memory", loadArgs2);
    std::cout << "Ket qua Load 2: " << loadResult2 << "\n\n";


    // ==========================================
    // KỊCH BẢN TEST 4: BÁO LỖI THIẾU THAM SỐ
    // ==========================================
    std::cout << "--- Test 4: Goi tool nhung quen truyen tham so 'action' ---\n";
    std::map<std::string, std::string> badArgs = {
        {"chao_hoi", "hello_agent"}
    };

    std::string badResult = ToolRegistry::getInstance().executeTool("memory", badArgs);
    std::cout << "Ket qua khi thieu tham so: " << badResult << "\n\n";

    std::cout << "===========================================\n";
    std::cout << "Test hoan thanh! Neu moi thu hien thi dung mong muon,\n";
    std::cout << "ban co the tu tin push file memory_tool len Git nhom.\n";
    
    return 0;
}