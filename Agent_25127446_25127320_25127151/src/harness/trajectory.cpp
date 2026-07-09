#include "trajectory.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::string Trajectory::toJson() const {
    // 1. Tạo "chiếc hộp to" chứa thông tin chung của Trajectory
    json j_trajectory;
    j_trajectory["task_id"] = task_id;
    j_trajectory["model"] = model;
    j_trajectory["success"] = success;
    j_trajectory["total_tokens"] = total_tokens;
    j_trajectory["total_time_ms"] = total_time_ms;

    // 2. Tạo "ngăn kéo" mảng để chứa danh sách các bước (Đã thêm đúng dòng bạn từng bị thiếu)
    json j_steps = json::array(); 

    // 3. Duyệt qua từng bước trong biến vector
    for (const auto& step : steps) {
        json j_step;
        j_step["step_id"] = step.step_id;
        j_step["thought"] = step.thought;
        
        // 4. Xử lý std::variant cho biến action bằng std::visit
        json j_action;
        std::visit([&j_action](auto&& arg) {
            //const ToolCall || &FinalAnswer
            using T = std::decay_t<decltype(arg)>;
            //Arg ToolCall  
            
            // Nếu action đang chứa ToolCall
            if constexpr (std::is_same_v<T, ToolCall>) {
                j_action["type"] = arg.type;
                j_action["tool"] = arg.tool;
                j_action["args"] = arg.args;
            } 
            // Nếu action đang chứa FinalAnswer
            else if constexpr (std::is_same_v<T, FinalAnswer>) {
                j_action["type"] = arg.type;
                j_action["text"] = arg.text;
            }
        }, step.action);
        
        // Bỏ action vào trong step
        j_step["action"] = j_action;
        
        j_step["tool_result"] = step.tool_result;
        j_step["tokens_used"] = step.tokens_used;
        j_step["latency_ms"] = step.latency_ms;
        
        // Nhét step này vào danh sách j_steps
        j_steps.push_back(j_step);
    }
    
    // 5. Gắn mảng steps vào trong hộp to trajectory
    j_trajectory["steps"] = j_steps;
    
    // 6. Chuyển đổi toàn bộ thành chuỗi text, thụt lề 4 dấu cách cho đẹp
    return j_trajectory.dump(4);
}