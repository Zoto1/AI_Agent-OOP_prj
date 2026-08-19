#include "exec.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
constexpr int kDefaultTimeoutMs = 10000;
constexpr int kMaxTimeoutMs = 60000;
constexpr std::size_t kMaxOutputBytes = 1024 * 1024;

void closeFd(int &fd)
{
    if (fd >= 0) { close(fd); fd = -1; }
}

void appendAvailable(int &fd, std::string &output, bool &truncated)
{
    std::array<char, 4096> buffer{};
    while (fd >= 0)
    {
        const ssize_t count = read(fd, buffer.data(), buffer.size());
        if (count > 0)
        {
            const std::size_t remaining = output.size() < kMaxOutputBytes
                                              ? kMaxOutputBytes - output.size()
                                              : 0;
            const auto accepted = std::min(
                remaining, static_cast<std::size_t>(count));
            output.append(buffer.data(), accepted);
            truncated = truncated || accepted < static_cast<std::size_t>(count);
        }
        else if (count == 0)
        {
            closeFd(fd);
        }
        else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        {
            break;
        }
        else
        {
            closeFd(fd);
        }
    }
}

int parseTimeout(const std::map<std::string, std::string> &args)
{
    const auto it = args.find("timeout_ms");
    if (it == args.end() || it->second.empty()) return kDefaultTimeoutMs;
    try
    {
        const int value = std::stoi(it->second);
        return value >= 1 && value <= kMaxTimeoutMs ? value : -1;
    }
    catch (const std::exception &) { return -1; }
}

std::string formatResult(int exit_code, const std::string &out,
                         const std::string &err, bool timed_out,
                         bool truncated)
{
    std::ostringstream result;
    result << "exit_code: " << exit_code << '\n';
    if (timed_out) result << "timed_out: true\n";
    result << "stdout:\n" << out;
    if (!out.empty() && out.back() != '\n') result << '\n';
    result << "stderr:\n" << err;
    if (!err.empty() && err.back() != '\n') result << '\n';
    if (truncated)
        result << "[output truncated at " << kMaxOutputBytes << " bytes]\n";
    return result.str();
}
} // namespace

ExecTool::ExecTool()
    : Tool(
          "exec",
          "Run a shell command in the current task workspace and return its "
          "exit code, stdout and stderr. Args: command, timeout_ms (optional).",
          {
              {"type", "OBJECT"},
              {"properties", {
                  {"command", {
                      {"type", "STRING"},
                      {"description", "Shell command to execute"}
                  }},
                  {"timeout_ms", {
                      {"type", "STRING"},
                      {"description", "Timeout from 1 to 60000 ms; default 10000"}
                  }}
              }},
              {"required", {"command"}}
          }) {}

std::string ExecTool::execute(const std::map<std::string, std::string> &args)
{
    const auto command = args.find("command");
    if (command == args.end()) return "Lỗi: Thiếu tham số 'command'.";
    if (command->second.empty()) return "Lỗi: Tham số 'command' không được rỗng.";

    const int timeout_ms = parseTimeout(args);
    if (timeout_ms < 0)
        return "Lỗi: 'timeout_ms' phải là số nguyên từ 1 đến 60000.";

    int out_pipe[2] = {-1, -1};
    int err_pipe[2] = {-1, -1};
    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0)
    {
        closeFd(out_pipe[0]); closeFd(out_pipe[1]);
        closeFd(err_pipe[0]); closeFd(err_pipe[1]);
        return "Lỗi: Không thể tạo pipe cho tiến trình.";
    }

    const pid_t pid = fork();
    if (pid < 0)
    {
        closeFd(out_pipe[0]); closeFd(out_pipe[1]);
        closeFd(err_pipe[0]); closeFd(err_pipe[1]);
        return "Lỗi: Không thể tạo tiến trình.";
    }
    if (pid == 0)
    {
        setpgid(0, 0); // Cho phép parent dừng cả cây process khi timeout.
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        execl("/bin/sh", "sh", "-c", command->second.c_str(),
              static_cast<char *>(nullptr));
        _exit(127);
    }

    // Thu hẹp race với setpgid() trong child trước khi timeout cần kill group.
    setpgid(pid, pid);
    closeFd(out_pipe[1]); closeFd(err_pipe[1]);
    fcntl(out_pipe[0], F_SETFL, fcntl(out_pipe[0], F_GETFL) | O_NONBLOCK);
    fcntl(err_pipe[0], F_SETFL, fcntl(err_pipe[0], F_GETFL) | O_NONBLOCK);

    std::string out, err;
    bool truncated = false, timed_out = false, child_done = false;
    int status = 0;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);

    while (!child_done || out_pipe[0] >= 0 || err_pipe[0] >= 0)
    {
        appendAvailable(out_pipe[0], out, truncated);
        appendAvailable(err_pipe[0], err, truncated);

        if (!child_done) child_done = waitpid(pid, &status, WNOHANG) == pid;
        const bool pipes_open = out_pipe[0] >= 0 || err_pipe[0] >= 0;
        if ((!child_done || pipes_open) &&
            std::chrono::steady_clock::now() >= deadline)
        {
            timed_out = true;
            kill(-pid, SIGKILL);
            kill(pid, SIGKILL);
            if (!child_done) waitpid(pid, &status, 0);
            child_done = true;
        }

        if (!child_done || out_pipe[0] >= 0 || err_pipe[0] >= 0)
        {
            pollfd descriptors[2] = {
                {out_pipe[0], POLLIN | POLLHUP, 0},
                {err_pipe[0], POLLIN | POLLHUP, 0}
            };
            poll(descriptors, 2, 10);
        }
    }

    const int exit_code = timed_out ? 124
        : WIFEXITED(status) ? WEXITSTATUS(status)
        : WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1;
    return formatResult(exit_code, out, err, timed_out, truncated);
}
