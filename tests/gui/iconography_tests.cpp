#include "gui/presentation/icons/iconography.h"
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers,readability-function-cognitive-complexity)

TEST_CASE("iconography_ShouldHaveOneValidMappingPerSemanticId")
{
    const auto mappings = uo_mule::gui::iconMappings();
    REQUIRE(mappings.size() == 81);

    std::set<uo_mule::gui::IconId> ids;
    for (const auto & mapping : mappings)
    {
        REQUIRE(mapping.codepoint > 0U);
        REQUIRE(mapping.codepoint <= 0x10FFFFU);
        REQUIRE(mapping.outlineCodepoint > 0U);
        REQUIRE(mapping.outlineCodepoint <= 0x10FFFFU);
        REQUIRE((mapping.codepoint < 0xD800U || mapping.codepoint > 0xDFFFU));
        REQUIRE((mapping.outlineCodepoint < 0xD800U || mapping.outlineCodepoint > 0xDFFFU));
        REQUIRE(ids.insert(mapping.id).second);
        REQUIRE_FALSE(uo_mule::gui::iconName(mapping.id).empty());
        REQUIRE_FALSE(uo_mule::gui::iconName(mapping.id, uo_mule::gui::IconVariant::outline).empty());
        REQUIRE_FALSE(uo_mule::gui::iconSemanticName(mapping.id).empty());
    }
}

TEST_CASE("iconography_ShouldExposeCanonicalMdiNamesSeparatelyFromSemanticNames")
{
    REQUIRE(uo_mule::gui::iconSemanticName(uo_mule::gui::IconId::pan) == "pan");
    REQUIRE(uo_mule::gui::iconName(uo_mule::gui::IconId::pan) == "cursor-move");
    REQUIRE(uo_mule::gui::iconName(uo_mule::gui::IconId::settings, uo_mule::gui::IconVariant::outline) == "cog-outline");
    REQUIRE(uo_mule::gui::iconName(uo_mule::gui::IconId::lasso) == "selection-drag");
    REQUIRE(uo_mule::gui::iconName(uo_mule::gui::IconId::fileDocumentPlus) == "file-document-plus");
    REQUIRE(uo_mule::gui::iconName(uo_mule::gui::IconId::eyeClosed) == "eye-closed");
    REQUIRE(uo_mule::gui::iconName(uo_mule::gui::IconId::eye) == "eye");
    REQUIRE(uo_mule::gui::iconName(uo_mule::gui::IconId::selectRemove) == "select-remove");
}

TEST_CASE("iconography_ShouldEncodeMappedCodepointsAsUtf8")
{
    for (const auto & mapping : uo_mule::gui::iconMappings())
    {
        for (const auto variant : {uo_mule::gui::IconVariant::filled, uo_mule::gui::IconVariant::outline})
        {
            const std::string glyph = uo_mule::gui::iconGlyph(mapping.id, variant);
            REQUIRE(glyph.size() == 4);
            REQUIRE((static_cast<std::uint8_t>(glyph.at(0)) & 0xF8U) == 0xF0U);
            REQUIRE((static_cast<std::uint8_t>(glyph.at(1)) & 0xC0U) == 0x80U);
            REQUIRE((static_cast<std::uint8_t>(glyph.at(2)) & 0xC0U) == 0x80U);
            REQUIRE((static_cast<std::uint8_t>(glyph.at(3)) & 0xC0U) == 0x80U);
        }
    }
}

TEST_CASE("iconography_ShouldDeriveImGuiRangesFromMappings")
{
    const auto mappings = uo_mule::gui::iconMappings();
    const auto ranges = uo_mule::gui::imguiIconGlyphRanges();
    REQUIRE(ranges.size() == (mappings.size() * 4U) + 1U);
    REQUIRE(ranges.back() == 0U);

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    for (std::size_t index = 0; index < mappings.size(); index++)
    {
        REQUIRE(ranges[index * 4U] ==
                mappings[index].codepoint); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        REQUIRE(ranges[(index * 4U) + 1U] ==
                mappings[index].codepoint); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        REQUIRE(ranges[(index * 4U) + 2U] ==
                mappings[index].outlineCodepoint); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        REQUIRE(ranges[(index * 4U) + 3U] ==
                mappings[index].outlineCodepoint); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
}

TEST_CASE("iconography_ShouldPreserveTextWhenFontIsUnavailable")
{
    uo_mule::gui::setIconFontAvailable(false);
    REQUIRE(uo_mule::gui::iconLabel(uo_mule::gui::IconId::save, "Save", false) == "Save");
    REQUIRE(uo_mule::gui::iconLabel(uo_mule::gui::IconId::save, "Save") == "Save");
    const std::string available = uo_mule::gui::iconLabel(uo_mule::gui::IconId::save, "Save", true);
    REQUIRE(available.ends_with(" Save"));
    REQUIRE(available.size() > std::string(" Save").size());
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers,readability-function-cognitive-complexity)
