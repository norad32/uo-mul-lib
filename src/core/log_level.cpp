#include "core/log_level.h"
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace im_cpp::core
{
namespace
{

[[nodiscard]] std::string normalize(std::string_view value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n\f\v");
    if (first == std::string_view::npos)
    {
        return {};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n\f\v");
    std::string normalized(value.substr(first, last - first + 1));
    for (char & character : normalized)
    {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
    return normalized;
}

} // namespace

std::optional<LogLevel> parseLogLevel(std::string_view value)
{
    const std::string normalized = normalize(value);
    if (normalized == "0" || normalized == "TRACE")
    {
        return LogLevel::trace;
    }
    if (normalized == "10" || normalized == "DEBUG")
    {
        return LogLevel::debug;
    }
    if (normalized == "20" || normalized == "INFO")
    {
        return LogLevel::info;
    }
    if (normalized == "30" || normalized == "WARNING" || normalized == "WARN")
    {
        return LogLevel::warning;
    }
    if (normalized == "40" || normalized == "ERROR")
    {
        return LogLevel::error;
    }
    if (normalized == "50" || normalized == "CRITICAL" || normalized == "FATAL")
    {
        return LogLevel::critical;
    }
    return std::nullopt;
}

std::string_view toString(LogLevel level) noexcept
{
    switch (level)
    {
        case LogLevel::trace :
            return "TRACE";
        case LogLevel::debug :
            return "DEBUG";
        case LogLevel::info :
            return "INFO";
        case LogLevel::warning :
            return "WARNING";
        case LogLevel::error :
            return "ERROR";
        case LogLevel::critical :
            return "CRITICAL";
    }
    return "UNKNOWN";
}

} // namespace im_cpp::core
