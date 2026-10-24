#include "gui/presentation/icons/iconography.h"
#include <array>

namespace uo_mule::gui
{
namespace
{

constexpr std::array<IconMapping, 21> mappings{{
    {IconId::selection, 0xF0489},
    {IconId::pan, 0xF01BE},
    {IconId::rectangleSelect, 0xF0E5F},
    {IconId::freeSelect, 0xF0D32},
    {IconId::fuzzySelect, 0xF1204},
    {IconId::fill, 0xF027C},
    {IconId::replace, 0xF06D4},
    {IconId::zoom, 0xF0349},
    {IconId::fitView, 0xF0EF5},
    {IconId::eyedropper, 0xF020A},
    {IconId::grid, 0xF02C1},
    {IconId::save, 0xF0193},
    {IconId::settings, 0xF0493},
    {IconId::warning, 0xF0026},
    {IconId::ok, 0xF012C},
    {IconId::cancel, 0xF0156},
    {IconId::elevation, 0xF0509},
    {IconId::tileId, 0xF0EFE},
    {IconId::readOnly, 0xF1C06},
    {IconId::preferences, 0xF062E},
    {IconId::fileBrowserDialog, 0xF0770},
}};

constexpr std::array<std::string_view, 21> names{{
    "selection",
    "pan",
    "rectangleSelect",
    "freeSelect",
    "fuzzySelect",
    "fill",
    "replace",
    "zoom",
    "fitView",
    "eyedropper",
    "grid",
    "save",
    "settings",
    "warning",
    "ok",
    "cancel",
    "elevation",
    "tileId",
    "readOnly",
    "preferences",
    "fileBrowserDialog",
}};

[[nodiscard]] const IconMapping * findMapping(IconId id) noexcept
{
    for (const IconMapping & mapping : mappings)
    {
        if (mapping.id == id)
        {
            return &mapping;
        }
    }
    return nullptr;
}

[[nodiscard]] std::string encodeUtf8(std::uint32_t codepoint)
{
    std::string result;
    if (codepoint <= 0x7F)
    {
        result.push_back(static_cast<char>(codepoint));
    }
    else if (codepoint <= 0x7FF)
    {
        result.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
        result.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
    else if (codepoint <= 0xFFFF)
    {
        result.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
        result.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        result.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
    else
    {
        result.push_back(static_cast<char>(0xF0U | (codepoint >> 18U)));
        result.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)));
        result.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        result.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
    return result;
}

[[nodiscard]] const std::array<std::uint32_t, (mappings.size() * 2U) + 1U> & glyphRanges() noexcept
{
    static const auto ranges = []
    {
        std::array<std::uint32_t, (mappings.size() * 2U) + 1U> result{};
        std::size_t index = 0;
        for (const IconMapping & mapping : mappings)
        {
            result.at(index++) = mapping.codepoint;
            result.at(index++) = mapping.codepoint;
        }
        return result;
    }();
    return ranges;
}

} // namespace

std::span<const IconMapping> iconMappings() noexcept
{
    return mappings;
}

std::string_view iconName(IconId id) noexcept
{
    const auto index = static_cast<std::size_t>(id);
    return index < names.size() ? names.at(index) : std::string_view{};
}

std::string iconGlyph(IconId id)
{
    const IconMapping * mapping = findMapping(id);
    return mapping == nullptr ? std::string{} : encodeUtf8(mapping->codepoint);
}

std::span<const std::uint32_t> imguiIconGlyphRanges() noexcept
{
    return glyphRanges();
}

} // namespace uo_mule::gui
