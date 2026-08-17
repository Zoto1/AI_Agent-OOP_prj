#include "../tools/exec.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>

int main()
{
    ExecTool exec;

    const std::string success = exec.execute({{"command", "printf 'hello'"}});
    assert(success.find("exit_code: 0") != std::string::npos);
    assert(success.find("stdout:\nhello") != std::string::npos);

    const std::string failure = exec.execute({
        {"command", "printf 'bad input' >&2; exit 7"}
    });
    assert(failure.find("exit_code: 7") != std::string::npos);
    assert(failure.find("stderr:\nbad input") != std::string::npos);

    const auto started = std::chrono::steady_clock::now();
    const std::string timeout = exec.execute({
        {"command", "sleep 2"}, {"timeout_ms", "100"}
    });
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    assert(timeout.find("exit_code: 124") != std::string::npos);
    assert(timeout.find("timed_out: true") != std::string::npos);
    assert(elapsed.count() < 1500);

    // Background process vẫn giữ stdout/stderr pipe: tool phải dừng cả group.
    const auto background_started = std::chrono::steady_clock::now();
    const std::string background = exec.execute({
        {"command", "sleep 2 &"}, {"timeout_ms", "100"}
    });
    const auto background_elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - background_started);
    assert(background.find("timed_out: true") != std::string::npos);
    assert(background_elapsed.count() < 1500);

    assert(exec.execute({}).find("command") != std::string::npos);
    assert(exec.execute({{"command", ""}}).find("rỗng") != std::string::npos);
    assert(exec.execute({{"command", "true"}, {"timeout_ms", "abc"}})
               .find("timeout_ms") != std::string::npos);

    std::cout << "[PASS] ExecTool stdout/stderr/exit-code/timeout tests\n";
    return 0;
}
