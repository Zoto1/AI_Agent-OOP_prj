#include "datetime_tool.h"

DateTimeTool::DateTimeTool()
    : Tool(
          "datetime",
          "Get current system date and time with custom format. Args: format (default '%Y-%m-%d %H:%M:%S').",
          {
              {"type", "OBJECT"},
              {"properties", {
                  {"format", {
                      {"type", "STRING"},
                      {"description", "Format string for strftime, e.g. %Y-%m-%d %H:%M:%S"}
                  }}
              }}
          }) {}

std::string DateTimeTool::execute(const std::map<std::string, std::string>& args) {
    std::string fmt = "%Y-%m-%d %H:%M:%S";
    if (auto it = args.find("format"); it != args.end() && !it->second.empty()) {
        fmt = it->second;
    }

    // Thiết lập múi giờ Việt Nam 
#if defined(_WIN32) || defined(_WIN64)
    _putenv("TZ=Asia/Ho_Chi_Minh");
    _tzset();
#else
    setenv("TZ", "Asia/Ho_Chi_Minh", 1);
    tzset();
#endif

    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    
    std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_buf, &now_c);
#else
    localtime_r(&now_c, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, fmt.c_str());
    return oss.str();
}
