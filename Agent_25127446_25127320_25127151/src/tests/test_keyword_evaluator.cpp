#include "../src/harness/keyword_evaluator.h"
#include "../src/harness/trajectory.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

int main()
{
    {
        // Có đầy đủ keyword
        Trajectory trajectory;
        trajectory.task_id = "keyword_full";

        Step step;
        step.step_id = 0;
        step.thought = "Da hoan thanh";
        step.action = FinalAnswer{
            "Ket qua phep tinh la 255 va da luu vao result.txt"
        };

        trajectory.steps.push_back(step);
        trajectory.final_answer =
            "Ket qua phep tinh la 255 va da luu vao result.txt";

        KeywordEvaluator evaluator(
            std::vector<std::string>{
                "255",
                "result.txt"
            }
        );

        const double score =
            evaluator.evaluate(trajectory);

        assert(score == 1.0);
    }

    {
        // Chỉ có một trong hai keyword
        Trajectory trajectory;
        trajectory.task_id = "keyword_partial";
        trajectory.final_answer =
            "Ket qua phep tinh la 255";

        Step step;
        step.step_id = 0;
        step.action = FinalAnswer{
            "Ket qua phep tinh la 255"
        };

        trajectory.steps.push_back(step);

        KeywordEvaluator evaluator(
            std::vector<std::string>{
                "255",
                "result.txt"
            }
        );

        const double score =
            evaluator.evaluate(trajectory);

        assert(score == 0.5);
    }

    {
        // Không có keyword nào
        Trajectory trajectory;
        trajectory.task_id = "keyword_none";
        trajectory.final_answer =
            "Khong tim thay ket qua";

        Step step;
        step.step_id = 0;
        step.action = FinalAnswer{
            "Khong tim thay ket qua"
        };

        trajectory.steps.push_back(step);

        KeywordEvaluator evaluator(
            std::vector<std::string>{
                "255",
                "result.txt"
            }
        );

        const double score =
            evaluator.evaluate(trajectory);

        assert(score == 0.0);
    }

    std::cout
        << "[PASS] KeywordEvaluator tests\n";

    return 0;
}