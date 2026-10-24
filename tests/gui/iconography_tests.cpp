#include "gui/presentation/icons/iconography.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <set>
#include <string>

TEST_CASE("iconography_ShouldHaveOneValidMappingPerSemanticId")
{
    const auto mappings = uo_mule::gui::iconMappings();
    REQUIRE(mappings.size() == 21);

    std::set<uo_mule::gui::IconId> ids;
    for (const auto & mapping : mappings)
    {
        REQUIRE(mapping.codepoint > 0U);
        REQUIRE(mapping.codepoint <= 0x10FFFFU);
        REQUIRE(!(mapping.codepoint >= 0xD800U && mapping.codepoint <= 0xDFFFU));
        REQUIRE(ids.insert(mapping.id).second);
        REQUIRE_FALSE(uo_mule::gui::iconName(mapping.id).empty());
    }
}

TEST_CASE("iconography_ShouldEncodeMappedCodepointsAsUtf8")
{
    for (const auto & mapping : uo_mule::gui::iconMappings())
    {
        const std::string glyph = uo_mule::gui::iconGlyph(mapping.id);
        REQUIRE(glyph.size() == 4);
        REQUIRE((static_cast<std::uint8_t>(glyph.at(0)) & 0xF8U) == 0xF0U);
        REQUIRE((static_cast<std::uint8_t>(glyph.at(1)) & 0xC0U) == 0x80U);
        REQUIRE((static_cast<std::uint8_t>(glyph.at(2)) & 0xC0U) == 0x80U);
        REQUIRE((static_cast<std::uint8_t>(glyph.at(3)) & 0xC0U) == 0x80U);
    }
}

TEST_CASE("iconography_ShouldDeriveImGuiRangesFromMappings")
{
    const auto mappings = uo_mule::gui::iconMappings();
    const auto ranges = uo_mule::gui::imguiIconGlyphRanges();
    REQUIRE(ranges.size() == (mappings.size() * 2U) + 1U);
    REQUIRE(ranges.back() == 0U);

    for (std::size_t index = 0; index < mappings.size(); index++)
    {
        REQUIRE(ranges[index * 2U] == mappings[index].codepoint);
        REQUIRE(ranges[(index * 2U) + 1U] == mappings[index].codepoint);
    }
}
