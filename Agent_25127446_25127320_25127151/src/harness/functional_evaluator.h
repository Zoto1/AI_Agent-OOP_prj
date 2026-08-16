#pragma once
#include "evaluator.h"
#include <string>

class FunctionalEvaluator : public Evaluator {
private:
    std::string eval_script;
public:
    FunctionalEvaluator(const std::string& script);
    double evaluate(const Trajectory& trajectory) override;
};