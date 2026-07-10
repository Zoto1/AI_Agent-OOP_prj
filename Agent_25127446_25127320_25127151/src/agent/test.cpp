#include <iostream>
#include <vector>
#include "trajectory.h"
#include "loop_detector.h"

// Hàm tiện ích để tạo nhanh một Step chứa ToolCall giúp code gọn hơn
Step makeToolStep(int id, const std::string& tool_name, const std::string& args) {
    Step s;
    s.step_id = id;
    s.thought = "Thinking about " + tool_name;
    // Gán ToolCall vào variant
    s.action = ToolCall{"tool_call", tool_name, args}; 
    s.tool_result = "Result of " + tool_name;
    return s;
}

// Hàm tiện ích in kết quả
void printResult(const std::string& test_name, const LoopResult& result) {
    std::cout << "=== " << test_name << " ===\n";
    std::cout << "Message: " << result.message << "\n\n";
}

int main() {
    // Khởi tạo detector với cấu hình: 
    // - 2 lần lặp là Warning
    // - 4 lần lặp là Critical
    LoopDetector detector(2, 4); 

    // ---------------------------------------------------------
    // KỊCH BẢN 1: Agent hoạt động bình thường
    // ---------------------------------------------------------
    std::vector<Step> normal_history = {
        makeToolStep(1, "web_search", "cách học C++"),
        makeToolStep(2, "read_file", "tutorial.txt"),
        makeToolStep(3, "calculator", "1+1")
    };
    LoopResult res1 = detector.detect(normal_history);
    printResult("Test 1 - Normal Execution", res1);


    // ---------------------------------------------------------
    // KỊCH BẢN 2: Generic Repeat (Lặp 1 màu)
    // Agent bị kẹt, gọi calculator("1+1") liên tục 4 lần
    // ---------------------------------------------------------
    std::vector<Step> repeat_history = {
        makeToolStep(1, "web_search", "thời tiết"),
        makeToolStep(2, "calculator", "1+1"),
        makeToolStep(3, "calculator", "1+1"),
        makeToolStep(4, "calculator", "1+1"),
        makeToolStep(5, "calculator", "1+1") // Đạt ngưỡng Critical (4 lần)
    };
    LoopResult res2 = detector.detect(repeat_history);
    printResult("Test 2 - Generic Repeat", res2);


    // ---------------------------------------------------------
    // KỊCH BẢN 3: Ping-Pong (Lặp bóng bàn)
    // Agent nhảy qua lại: read_file -> web_search -> read_file -> web_search
    // ---------------------------------------------------------
    std::vector<Step> pingpong_history = {
        makeToolStep(1, "calculator", "5*5"),
        // Bắt đầu chuỗi ping-pong (cần 4 bước để thành 2 cặp A-B)
        makeToolStep(2, "read_file", "data.txt"),   // A
        makeToolStep(3, "web_search", "lỗi file"),  // B
        makeToolStep(4, "read_file", "data.txt"),   // A
        makeToolStep(5, "web_search", "lỗi file")   // B 
    };
    LoopResult res3 = detector.detect(pingpong_history);
    printResult("Test 3 - Ping-Pong Loop", res3);

    return 0;
}