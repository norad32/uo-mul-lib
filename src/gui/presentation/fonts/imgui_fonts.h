#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace uo_mule::gui
{

struct ImGuiFontState
{
    bool iconFontAvailable = false;
    std::string diagnostic;
};

[[nodiscard]] ImGuiFontState configureImGuiFonts(const std::optional<std::filesystem::path> & iconFontPath);

} // namespace uo_mule::gui
