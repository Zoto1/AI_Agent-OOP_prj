#include "datetime_tool.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

DateTimeTool::DateTimeTool()
    : Tool(
          "datetime",
          "Get the current system date and time. Args: format (optional, "
          "strftime format string, default: %Y-%m-%d %H:%M:%S).",
          {
              {"type", "OBJECT"},
              {"properties", {
                  {"format", {
                      {"type", "STRING"},
                      {"description", "strftime format, e.g. %Y-%m-%d %H:%M:%S"}
                  }}
              }},
              {"required", nlohmann::json::array()}
          }) {}

std::string DateTimeTool::execute(const std::map<std::string, std::string>& args)
{
    std::string format = "%Y-%m-%d %H:%M:%S";
    auto format_it = args.find("format");
    if (format_it != args.end() && !format_it->second.empty())
    {
        format = format_it->second;
    }

    const std::time_t now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());

    std::tm local_time{};
    if (localtime_r(&now, &local_time) == nullptr)
    {
        return "Error: Failed to read local system time.";
    }

    std::ostringstream output;
    output << std::put_time(&local_time, format.c_str());
    if (output.fail())
    {
        return "Error: Invalid format string: " + format;
    }
    return output.str();
}
