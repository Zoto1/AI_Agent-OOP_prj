#pragma once
#include "evaluator.h"
#include<vector>
class KeywordEvaluator : public Evaluator {
    private:
    std::vector<std::string> required_keywords; // Danh sách các từ khóa cần thiết
    public:
    KeywordEvaluator(const std::vector<std::string>& keywords) : required_keywords(keywords) {}
    double evaluate(const Trajectory& trajectory) override ;
};