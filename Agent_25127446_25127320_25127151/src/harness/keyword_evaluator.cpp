#include "keyword_evaluator.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <variant>

namespace
{
    std::string toLower(std::string text)
    {
        std::transform(
            text.begin(),
            text.end(),
            text.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            }
        );

        return text;
    }
}

double KeywordEvaluator::evaluate(const Trajectory& trajectory)
{
    // Không có keyword nghĩa là cấu hình task không hợp lệ.
    if (required_keywords.empty())
    {
        return 0.0;
    }

    // Duyệt ngược để lấy FinalAnswer cuối cùng.
    for (auto it = trajectory.steps.rbegin();
         it != trajectory.steps.rend();
         ++it)
    {
        if (!std::holds_alternative<FinalAnswer>(it->action))
        {
            continue;
        }

        const FinalAnswer& finalAnswer =
            std::get<FinalAnswer>(it->action);

        const std::string normalizedAnswer =
            toLower(finalAnswer.text);

        int matchedKeywords = 0;

        for (const std::string& keyword : required_keywords)
        {
            if (keyword.empty())
            {
                continue;
            }

            const std::string normalizedKeyword =
                toLower(keyword);

            if (normalizedAnswer.find(normalizedKeyword)
                != std::string::npos)
            {
                ++matchedKeywords;
            }
        }

        return static_cast<double>(matchedKeywords)
             / static_cast<double>(required_keywords.size());
    }

    // Agent không tạo FinalAnswer.
    return 0.0;
}