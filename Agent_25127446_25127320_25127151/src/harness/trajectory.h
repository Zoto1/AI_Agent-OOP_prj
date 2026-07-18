#pragma once 
#include <string>
#include <vector>
#include <variant> // c++ 17

struct ToolCall {
    std::string type = "tool_call";
    std::string tool;
    std::string args;



};

struct FinalAnswer {
    std::string type = "final_answer";
    std::string text;


};

// Tách Step ra ngoài để cấu trúc rõ ràng hơn
struct Step {
    int step_id = 0;
    std::string thought;
    
    std::variant<ToolCall, FinalAnswer> action; 
    
    std::string tool_result;
    int tokens_used = 0;
    long long latency_ms = 0;

};

struct Trajectory {
    std::string task_id;
    std::string model;
    bool success = false;
    int total_tokens = 0;
    int total_time_ms = 0; // Đổi tên để chuẩn format JSON
    
    // Đã thêm biến vector để thực sự lưu trữ danh sách các bước
    std::vector<Step> steps; 
    std::string toJson() const;
};