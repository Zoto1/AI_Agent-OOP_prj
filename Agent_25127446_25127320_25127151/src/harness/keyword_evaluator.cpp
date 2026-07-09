#include "keyword_evaluator.h"
#include <variant>

KeywordEvaluator::KeywordEvaluator(const std::vector<std::string>& keywords)
    : required_keywords(keywords) {}

double KeywordEvaluator::evaluate(const Trajectory& trajectory) {
    // Duyệt qua dữ liệu lịch sử trong Trajectory
    for (const auto& step : trajectory.steps) {
        // Kiểm tra xem Agent đã đưa ra câu trả lời cuối cùng (FinalAnswer) chưa
        if (std::holds_alternative<FinalAnswer>(step.action)) {
            const auto& final_answer = std::get<FinalAnswer>(step.action);
            
            int match_count = 0;
            // Tìm kiếm xem các từ khóa bắt buộc có xuất hiện trong câu trả lời không
            for (const auto& keyword : required_keywords) {
                if (final_answer.text.find(keyword) != std::string::npos) {
                    match_count++;
                }
            }
            
            // Trả về tỷ lệ hoàn thành
            if (required_keywords.empty()) return 1.0;
            return static_cast<double>(match_count) / required_keywords.size();
        }
    }
    return 0.0; 
}