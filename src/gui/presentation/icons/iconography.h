#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace uo_mule::gui
{

enum class IconId : std::uint8_t
{
    selection,
    pan,
    rectangleSelect,
    freeSelect,
    fuzzySelect,
    fill,
    replace,
    zoom,
    fitView,
    eyedropper,
    grid,
    save,
    settings,
    warning,
    ok,
    cancel,
    elevation,
    tileId,
    readOnly,
    preferences,
    fileBrowserDialog
};

struct IconMapping
{
    IconId id;
    std::uint32_t codepoint;
};

[[nodiscard]] std::span<const IconMapping> iconMappings() noexcept;
[[nodiscard]] std::string_view iconName(IconId id) noexcept;
[[nodiscard]] std::string iconGlyph(IconId id);
[[nodiscard]] std::span<const std::uint32_t> imguiIconGlyphRanges() noexcept;

} // namespace uo_mule::gui
