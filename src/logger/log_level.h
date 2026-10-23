#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace uo_mule::logger
{

enum class LogLevel : std::uint8_t
{
    trace,
    debug,
    info,
    warning,
    error,
    critical,
};

[[nodiscard]] constexpr LogLevel defaultLogLevel() noexcept
{
#ifdef DEBUG_BUILD
    return LogLevel::trace;
#else
    return LogLevel::info;
#endif
}

[[nodiscard]] std::optional<LogLevel> parseLogLevel(std::string_view value);
[[nodiscard]] std::string_view toString(LogLevel level) noexcept;

} // namespace uo_mule::logger
