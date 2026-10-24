#include "gui/presentation/icons/iconography.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace uo_mule::gui
{
namespace
{

constexpr std::array<IconMapping, 81> mappings{
    {
        {.id = IconId::selection, .codepoint = 0xF0489, .outlineCodepoint = 0xF0489},
        {.id = IconId::pan, .codepoint = 0xF01BE, .outlineCodepoint = 0xF01BE},
        {.id = IconId::rectangleSelect, .codepoint = 0xF0E5F, .outlineCodepoint = 0xF0E5F},
        {.id = IconId::freeSelect, .codepoint = 0xF0D32, .outlineCodepoint = 0xF0D32},
        {.id = IconId::lasso, .codepoint = 0xF0A6D, .outlineCodepoint = 0xF0A6D},
        {.id = IconId::fuzzySelect, .codepoint = 0xF1204, .outlineCodepoint = 0xF1204},
        {.id = IconId::fill, .codepoint = 0xF027C, .outlineCodepoint = 0xF0266},
        {.id = IconId::replace, .codepoint = 0xF06D4, .outlineCodepoint = 0xF06D4},
        {.id = IconId::zoom, .codepoint = 0xF0349, .outlineCodepoint = 0xF0349},
        {.id = IconId::fitView, .codepoint = 0xF0EF5, .outlineCodepoint = 0xF0EF6},
        {.id = IconId::eyedropper, .codepoint = 0xF020A, .outlineCodepoint = 0xF020B},
        {.id = IconId::grid, .codepoint = 0xF02C1, .outlineCodepoint = 0xF02C2},
        {.id = IconId::save, .codepoint = 0xF0193, .outlineCodepoint = 0xF0818},
        {.id = IconId::settings, .codepoint = 0xF0493, .outlineCodepoint = 0xF08BB},
        {.id = IconId::warning, .codepoint = 0xF0026, .outlineCodepoint = 0xF002A},
        {.id = IconId::ok, .codepoint = 0xF012C, .outlineCodepoint = 0xF0855},
        {.id = IconId::cancel, .codepoint = 0xF073A, .outlineCodepoint = 0xF073A},
        {.id = IconId::elevation, .codepoint = 0xF0509, .outlineCodepoint = 0xF0509},
        {.id = IconId::tileId, .codepoint = 0xF0EFE, .outlineCodepoint = 0xF0EFE},
        {.id = IconId::readOnly, .codepoint = 0xF1C06, .outlineCodepoint = 0xF1C09},
        {.id = IconId::preferences, .codepoint = 0xF062E, .outlineCodepoint = 0xF1542},
        {.id = IconId::fileBrowserDialog, .codepoint = 0xF0770, .outlineCodepoint = 0xF0DCF},
        {.id = IconId::alertBox, .codepoint = 0xF0027, .outlineCodepoint = 0xF0CE4},
        {.id = IconId::folderOpen, .codepoint = 0xF0770, .outlineCodepoint = 0xF0DCF},
        {.id = IconId::information, .codepoint = 0xF02FC, .outlineCodepoint = 0xF02FD},
        {.id = IconId::pineTreeVariant, .codepoint = 0xF1C73, .outlineCodepoint = 0xF1C74},
        {.id = IconId::redo, .codepoint = 0xF044E, .outlineCodepoint = 0xF044F},
        {.id = IconId::undo, .codepoint = 0xF054C, .outlineCodepoint = 0xF054D},
        {.id = IconId::rename, .codepoint = 0xF1C18, .outlineCodepoint = 0xF1C1A},
        {.id = IconId::selectAll, .codepoint = 0xF0486, .outlineCodepoint = 0xF0486},
        {.id = IconId::shore, .codepoint = 0xF14F9, .outlineCodepoint = 0xF14F9},
        {.id = IconId::spellcheck, .codepoint = 0xF04C6, .outlineCodepoint = 0xF04C6},
        {.id = IconId::square, .codepoint = 0xF0764, .outlineCodepoint = 0xF0763},
        {.id = IconId::squareMedium, .codepoint = 0xF0A13, .outlineCodepoint = 0xF0A14},
        {.id = IconId::formatColorFill, .codepoint = 0xF0266, .outlineCodepoint = 0xF0266},
        {.id = IconId::selectDrag, .codepoint = 0xF0A6C, .outlineCodepoint = 0xF0A6C},
        {.id = IconId::select, .codepoint = 0xF0485, .outlineCodepoint = 0xF0485},
        {.id = IconId::selectInverse, .codepoint = 0xF0487, .outlineCodepoint = 0xF0487},
        {.id = IconId::palette, .codepoint = 0xF03D8, .outlineCodepoint = 0xF0E0C},
        {.id = IconId::layers, .codepoint = 0xF0328, .outlineCodepoint = 0xF09FE},
        {.id = IconId::image, .codepoint = 0xF02E9, .outlineCodepoint = 0xF0976},
        {.id = IconId::folderSearch, .codepoint = 0xF0968, .outlineCodepoint = 0xF0969},
        {.id = IconId::folder, .codepoint = 0xF024B, .outlineCodepoint = 0xF0256},
        {.id = IconId::fitToScreen, .codepoint = 0xF18F4, .outlineCodepoint = 0xF18F5},
        {.id = IconId::filter, .codepoint = 0xF0232, .outlineCodepoint = 0xF0233},
        {.id = IconId::exportIcon, .codepoint = 0xF0207, .outlineCodepoint = 0xF0B93},
        {.id = IconId::deleteIcon, .codepoint = 0xF01B4, .outlineCodepoint = 0xF09E7},
        {.id = IconId::cursorMove, .codepoint = 0xF01BE, .outlineCodepoint = 0xF01BE},
        {.id = IconId::contentSaveMove, .codepoint = 0xF0E27, .outlineCodepoint = 0xF0E28},
        {.id = IconId::contentSave, .codepoint = 0xF0193, .outlineCodepoint = 0xF0818},
        {.id = IconId::abacus, .codepoint = 0xF16E0, .outlineCodepoint = 0xF16E0},
        {.id = IconId::account, .codepoint = 0xF0004, .outlineCodepoint = 0xF0013},
        {.id = IconId::alert, .codepoint = 0xF0026, .outlineCodepoint = 0xF002A},
        {.id = IconId::apps, .codepoint = 0xF003B, .outlineCodepoint = 0xF0D46},
        {.id = IconId::autoFix, .codepoint = 0xF0068, .outlineCodepoint = 0xF0068},
        {.id = IconId::axisZArrow, .codepoint = 0xF0D55, .outlineCodepoint = 0xF0D56},
        {.id = IconId::binoculars, .codepoint = 0xF00A5, .outlineCodepoint = 0xF00A5},
        {.id = IconId::bookOpen, .codepoint = 0xF00BD, .outlineCodepoint = 0xF0B63},
        {.id = IconId::bookmark, .codepoint = 0xF00C0, .outlineCodepoint = 0xF00C3},
        {.id = IconId::brush, .codepoint = 0xF00E3, .outlineCodepoint = 0xF1A0D},
        {.id = IconId::bug, .codepoint = 0xF00E4, .outlineCodepoint = 0xF0A30},
        {.id = IconId::check, .codepoint = 0xF012C, .outlineCodepoint = 0xF0855},
        {.id = IconId::close, .codepoint = 0xF0156, .outlineCodepoint = 0xF06C9},
        {.id = IconId::cog, .codepoint = 0xF0493, .outlineCodepoint = 0xF08BB},
        {.id = IconId::contentCopy, .codepoint = 0xF018F, .outlineCodepoint = 0xF018F},
        {.id = IconId::contentCut, .codepoint = 0xF0190, .outlineCodepoint = 0xF0190},
        {.id = IconId::contentPaste, .codepoint = 0xF0192, .outlineCodepoint = 0xF0192},
        {.id = IconId::identifier, .codepoint = 0xF0EFE, .outlineCodepoint = 0xF0EFE},
        {.id = IconId::hexadecimal, .codepoint = 0xF12A7, .outlineCodepoint = 0xF12A7},
        {.id = IconId::formatListBulleted, .codepoint = 0xF0279, .outlineCodepoint = 0xF0279},
        {.id = IconId::refresh, .codepoint = 0xF0450, .outlineCodepoint = 0xF0450},
        {.id = IconId::eyeOff, .codepoint = 0xF0209, .outlineCodepoint = 0xF06D1},
        {.id = IconId::arrowUp, .codepoint = 0xF005D, .outlineCodepoint = 0xF005D},
        {.id = IconId::arrowDown, .codepoint = 0xF0045, .outlineCodepoint = 0xF0045},
        {.id = IconId::arrowLeft, .codepoint = 0xF004D, .outlineCodepoint = 0xF004D},
        {.id = IconId::arrowRight, .codepoint = 0xF0054, .outlineCodepoint = 0xF0054},
        {.id = IconId::ellipse, .codepoint = 0xF0EA0, .outlineCodepoint = 0xF0EA1},
        {.id = IconId::fileDocumentPlus, .codepoint = 0xF1A9D, .outlineCodepoint = 0xF1A9E},
        {.id = IconId::eyeClosed, .codepoint = 0xF1CA3, .outlineCodepoint = 0xF1CA3},
        {.id = IconId::eye, .codepoint = 0xF0208, .outlineCodepoint = 0xF06D0},
        {.id = IconId::selectRemove, .codepoint = 0xF17C1, .outlineCodepoint = 0xF17C1},
    },
};

constexpr std::array<std::string_view, 81> names{
    {
        "selection",
        "pan",
        "rectangleSelect",
        "freeSelect",
        "lasso",
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
        "alertBox",
        "folderOpen",
        "information",
        "pineTreeVariant",
        "redo",
        "undo",
        "rename",
        "selectAll",
        "shore",
        "spellcheck",
        "square",
        "squareMedium",
        "formatColorFill",
        "selectDrag",
        "select",
        "selectInverse",
        "palette",
        "layers",
        "image",
        "folderSearch",
        "folder",
        "fitToScreen",
        "filter",
        "export",
        "delete",
        "cursorMove",
        "contentSaveMove",
        "contentSave",
        "abacus",
        "account",
        "alert",
        "apps",
        "autoFix",
        "axisZArrow",
        "binoculars",
        "bookOpen",
        "bookmark",
        "brush",
        "bug",
        "check",
        "close",
        "cog",
        "contentCopy",
        "contentCut",
        "contentPaste",
        "identifier",
        "hexadecimal",
        "formatListBulleted",
        "refresh",
        "eyeOff",
        "arrowUp",
        "arrowDown",
        "arrowLeft",
        "arrowRight",
        "ellipse",
        "fileDocumentPlus",
        "eyeClosed",
        "eye",
        "selectRemove",
    },
};

constexpr std::array<std::string_view, 81> mdiNames{
    {
        "selection",
        "cursor-move",
        "rectangle-outline",
        "selection-ellipse",
        "selection-drag",
        "select-search",
        "format-paint",
        "find-replace",
        "magnify",
        "fit-to-page",
        "eyedropper",
        "grid",
        "content-save",
        "cog",
        "alert",
        "check",
        "cancel",
        "terrain",
        "identifier",
        "eye-lock",
        "tune",
        "folder-open",
        "alert-box",
        "folder-open",
        "information",
        "pine-tree-variant",
        "redo",
        "undo",
        "rename",
        "select-all",
        "shore",
        "spellcheck",
        "square",
        "square-medium",
        "format-color-fill",
        "select-drag",
        "select",
        "select-inverse",
        "palette",
        "layers",
        "image",
        "folder-search",
        "folder",
        "fit-to-screen",
        "filter",
        "export",
        "delete",
        "cursor-move",
        "content-save-move",
        "content-save",
        "abacus",
        "account",
        "alert",
        "apps",
        "auto-fix",
        "axis-z-arrow",
        "binoculars",
        "book-open",
        "bookmark",
        "brush",
        "bug",
        "check",
        "close",
        "cog",
        "content-copy",
        "content-cut",
        "content-paste",
        "identifier",
        "hexadecimal",
        "format-list-bulleted",
        "refresh",
        "eye-off",
        "arrow-up",
        "arrow-down",
        "arrow-left",
        "arrow-right",
        "ellipse",
        "file-document-plus",
        "eye-closed",
        "eye",
        "select-remove",
    },
};

constexpr std::array<std::string_view, 81> mdiOutlineNames{
    {
        "selection",
        "cursor-move",
        "rectangle-outline",
        "selection-ellipse",
        "selection-drag",
        "select-search",
        "format-color-fill",
        "find-replace",
        "magnify",
        "fit-to-page-outline",
        "eyedropper-variant",
        "grid-off",
        "content-save-outline",
        "cog-outline",
        "alert-outline",
        "check-outline",
        "cancel",
        "terrain",
        "identifier",
        "eye-lock-outline",
        "tune-variant",
        "folder-open-outline",
        "alert-box-outline",
        "folder-open-outline",
        "information-outline",
        "pine-tree-variant-outline",
        "redo-variant",
        "undo-variant",
        "rename-outline",
        "select-all",
        "shore",
        "spellcheck",
        "square-outline",
        "square-medium-outline",
        "format-color-fill",
        "select-drag",
        "select",
        "select-inverse",
        "palette-outline",
        "layers-outline",
        "image-outline",
        "folder-search-outline",
        "folder-outline",
        "fit-to-screen-outline",
        "filter-outline",
        "export-variant",
        "delete-outline",
        "cursor-move",
        "content-save-move-outline",
        "content-save-outline",
        "abacus",
        "account-outline",
        "alert-outline",
        "apps-box",
        "auto-fix",
        "axis-z-arrow-lock",
        "binoculars",
        "book-open-outline",
        "bookmark-outline",
        "brush-outline",
        "bug-outline",
        "check-outline",
        "close-outline",
        "cog-outline",
        "content-copy",
        "content-cut",
        "content-paste",
        "identifier",
        "hexadecimal",
        "format-list-bulleted",
        "refresh",
        "eye-off-outline",
        "arrow-up",
        "arrow-down",
        "arrow-left",
        "arrow-right",
        "ellipse-outline",
        "file-document-plus-outline",
        "eye-closed",
        "eye-outline",
        "select-remove",
    },
};

bool fontAvailable = false;

constexpr std::uint32_t utf8OneByteMaximum = 0x7FU;
constexpr std::uint32_t utf8TwoByteMaximum = 0x7FFU;
constexpr std::uint32_t utf8ThreeByteMaximum = 0xFFFFU;
constexpr std::uint32_t utf8ContinuationPrefix = 0x80U;
constexpr std::uint32_t utf8ContinuationMask = 0x3FU;
constexpr std::uint32_t utf8TwoBytePrefix = 0xC0U;
constexpr std::uint32_t utf8ThreeBytePrefix = 0xE0U;
constexpr std::uint32_t utf8FourBytePrefix = 0xF0U;
constexpr std::uint32_t utf8ShiftSix = 6U;
constexpr std::uint32_t utf8ShiftTwelve = 12U;
constexpr std::uint32_t utf8ShiftEighteen = 18U;

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
    if (codepoint <= utf8OneByteMaximum)
    {
        result.push_back(static_cast<char>(codepoint));
    }
    else if (codepoint <= utf8TwoByteMaximum)
    {
        result.push_back(static_cast<char>(utf8TwoBytePrefix | (codepoint >> utf8ShiftSix)));
        result.push_back(static_cast<char>(utf8ContinuationPrefix | (codepoint & utf8ContinuationMask)));
    }
    else if (codepoint <= utf8ThreeByteMaximum)
    {
        result.push_back(static_cast<char>(utf8ThreeBytePrefix | (codepoint >> utf8ShiftTwelve)));
        result.push_back(static_cast<char>(utf8ContinuationPrefix | ((codepoint >> utf8ShiftSix) & utf8ContinuationMask)));
        result.push_back(static_cast<char>(utf8ContinuationPrefix | (codepoint & utf8ContinuationMask)));
    }
    else
    {
        result.push_back(static_cast<char>(utf8FourBytePrefix | (codepoint >> utf8ShiftEighteen)));
        result.push_back(static_cast<char>(utf8ContinuationPrefix | ((codepoint >> utf8ShiftTwelve) & utf8ContinuationMask)));
        result.push_back(static_cast<char>(utf8ContinuationPrefix | ((codepoint >> utf8ShiftSix) & utf8ContinuationMask)));
        result.push_back(static_cast<char>(utf8ContinuationPrefix | (codepoint & utf8ContinuationMask)));
    }
    return result;
}

[[nodiscard]] const std::array<std::uint32_t, (mappings.size() * 4U) + 1U> & glyphRanges() noexcept
{
    static const auto ranges = []
    {
        std::array<std::uint32_t, (mappings.size() * 4U) + 1U> result{};
        std::size_t index = 0;
        for (const IconMapping & mapping : mappings)
        {
            result.at(index++) = mapping.codepoint;
            result.at(index++) = mapping.codepoint;
            result.at(index++) = mapping.outlineCodepoint;
            result.at(index++) = mapping.outlineCodepoint;
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
    return iconName(id, IconVariant::filled);
}

std::string_view iconName(IconId id, IconVariant variant) noexcept
{
    const auto index = static_cast<std::size_t>(id);
    const auto & namesForVariant = variant == IconVariant::outline ? mdiOutlineNames : mdiNames;
    return index < namesForVariant.size() ? namesForVariant.at(index) : std::string_view{};
}

std::string_view iconSemanticName(IconId id) noexcept
{
    const auto index = static_cast<std::size_t>(id);
    return index < names.size() ? names.at(index) : std::string_view{};
}

std::string iconGlyph(IconId id, IconVariant variant)
{
    const IconMapping * mapping = findMapping(id);
    if (mapping == nullptr)
    {
        return {};
    }
    return encodeUtf8(variant == IconVariant::outline ? mapping->outlineCodepoint : mapping->codepoint);
}

bool iconFontAvailable() noexcept
{
    return fontAvailable;
}

void setIconFontAvailable(bool available) noexcept
{
    fontAvailable = available;
}

std::string iconLabel(IconId id, std::string_view label, bool iconFontAvailable)
{
    return iconLabel(id, IconVariant::filled, label, iconFontAvailable);
}

std::string iconLabel(IconId id, IconVariant variant, std::string_view label, bool iconFontAvailable)
{
    if (!iconFontAvailable)
    {
        return std::string(label);
    }
    return iconGlyph(id, variant) + " " + std::string(label);
}

std::string iconLabel(IconId id, std::string_view label)
{
    return iconLabel(id, label, iconFontAvailable());
}

std::string iconLabel(IconId id, IconVariant variant, std::string_view label)
{
    return iconLabel(id, variant, label, iconFontAvailable());
}

std::span<const std::uint32_t> imguiIconGlyphRanges() noexcept
{
    return glyphRanges();
}

} // namespace uo_mule::gui
