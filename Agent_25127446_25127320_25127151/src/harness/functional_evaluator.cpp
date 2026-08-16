#include "functional_evaluator.h"
#include <cstdlib>
#include <iostream>

FunctionalEvaluator::FunctionalEvaluator(const std::string& script) 
    : eval_script(script) {}

double FunctionalEvaluator::evaluate(const Trajectory& trajectory) {
    // Sử dụng lệnh gọi hệ thống (system call) để chạy kịch bản kiểm thử thực tế
    int result = std::system(eval_script.c_str());
    
    // Trả về 1.0 (Thành công) nếu script chạy hợp lệ, ngược lại 0.0 (Thất bại)
    if (result == 0) {
        return 1.0;
    }
    return 0.0;
}