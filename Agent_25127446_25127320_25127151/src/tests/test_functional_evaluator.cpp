#include "../src/harness/functional_evaluator.h"
#include "../src/harness/trajectory.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

class CurrentPathGuard
{
private:
    fs::path old_path;

public:
    explicit CurrentPathGuard(
        const fs::path& new_path
    )
        : old_path(fs::current_path())
    {
        fs::current_path(new_path);
    }

    ~CurrentPathGuard()
    {
        std::error_code error;
        fs::current_path(old_path, error);
    }
};

int main()
{
    const fs::path test_workspace =
        fs::path("test_workspace_functional");

    std::error_code error;
    fs::remove_all(test_workspace, error);
    fs::create_directories(test_workspace);

    Trajectory trajectory;
    trajectory.task_id = "functional_test";

    {
        CurrentPathGuard guard(test_workspace);

        // Tạo file để evaluator kiểm tra
        std::ofstream output("result.txt");
        output << "255";
        output.close();

#ifdef _WIN32
        FunctionalEvaluator evaluator(
            "findstr 255 result.txt > nul"
        );
#else
        FunctionalEvaluator evaluator(
            "test -f result.txt && grep -q 255 result.txt"
        );
#endif

        const double score =
            evaluator.evaluate(trajectory);

        assert(score == 1.0);
    }

    {
        CurrentPathGuard guard(test_workspace);

#ifdef _WIN32
        FunctionalEvaluator evaluator(
            "findstr 999 result.txt > nul"
        );
#else
        FunctionalEvaluator evaluator(
            "grep -q 999 result.txt"
        );
#endif

        const double score =
            evaluator.evaluate(trajectory);

        assert(score == 0.0);
    }

#ifndef _WIN32
    {
        CurrentPathGuard guard(test_workspace);
        const std::string vietnam_time_script =
            "test -s time.txt && grep -Eq '\\+0700$' time.txt && "
            "ts=$(date -d \"$(cat time.txt)\" +%s) && now=$(date +%s) && "
            "delta=$((now-ts)) && test ${delta#-} -le 300";

        // UTC có timestamp đúng nhưng phải fail vì sai múi giờ Việt Nam.
        {
            std::ofstream time_file("time.txt");
            time_file << "2026-08-17 03:53:01 +0000 UTC\n";
        }
        FunctionalEvaluator evaluator(vietnam_time_script);
        assert(evaluator.evaluate(trajectory) == 0.0);

        // Thời gian Việt Nam hiện tại, có offset +0700, phải pass.
        assert(std::system(
                   "TZ=Asia/Ho_Chi_Minh date '+%Y-%m-%d %H:%M:%S %z' "
                   "> time.txt") == 0);
        assert(evaluator.evaluate(trajectory) == 1.0);
    }
#endif

    fs::remove_all(test_workspace, error);

    std::cout
        << "[PASS] FunctionalEvaluator tests\n";

    return 0;
}
