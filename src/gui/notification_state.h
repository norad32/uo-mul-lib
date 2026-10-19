#pragma once

#include <string>
#include <cstdint>
#include <utility>
#include <vector>

namespace uo_mule::gui
{

enum class NotificationSeverity : std::uint8_t
{
    info,
    success,
    warning,
    error,
};

struct Notification
{
    NotificationSeverity severity = NotificationSeverity::info;
    std::string message;
};

struct NotificationState
{
    void add(NotificationSeverity severity, std::string message)
    {
        entries.push_back({.severity = severity, .message = std::move(message)});
    }

    std::vector<Notification> entries;
};

} // namespace uo_mule::gui
