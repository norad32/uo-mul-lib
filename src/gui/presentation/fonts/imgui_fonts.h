#pragma once

#include <filesystem>
#include <optional>
#include <string>

struct ImFont;

namespace uo_mule::gui
{

struct ImGuiFontState
{
    bool iconFontAvailable = false;
    std::string diagnostic;
};

[[nodiscard]] ImGuiFontState configureImGuiFonts(const std::optional<std::filesystem::path> & iconFontPath);
[[nodiscard]] ImFont * iconFontLarge() noexcept;

} // namespace uo_mule::gui
