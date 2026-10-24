#include "gui/presentation/icons/iconography.h"
#include <array>

namespace uo_mule::gui
{
namespace
{

constexpr std::array<IconMapping, 81> mappings{{
    {IconId::selection, 0xF0489, 0xF0489},
    {IconId::pan, 0xF01BE, 0xF01BE},
    {IconId::rectangleSelect, 0xF0E5F, 0xF0E5F},
    {IconId::freeSelect, 0xF0D32, 0xF0D32},
    {IconId::lasso, 0xF0A6D, 0xF0A6D},
    {IconId::fuzzySelect, 0xF1204, 0xF1204},
    {IconId::fill, 0xF027C, 0xF0266},
    {IconId::replace, 0xF06D4, 0xF06D4},
    {IconId::zoom, 0xF0349, 0xF0349},
    {IconId::fitView, 0xF0EF5, 0xF0EF6},
    {IconId::eyedropper, 0xF020A, 0xF020B},
    {IconId::grid, 0xF02C1, 0xF02C2},
    {IconId::save, 0xF0193, 0xF0818},
    {IconId::settings, 0xF0493, 0xF08BB},
    {IconId::warning, 0xF0026, 0xF002A},
    {IconId::ok, 0xF012C, 0xF0855},
    {IconId::cancel, 0xF073A, 0xF073A},
    {IconId::elevation, 0xF0509, 0xF0509},
    {IconId::tileId, 0xF0EFE, 0xF0EFE},
    {IconId::readOnly, 0xF1C06, 0xF1C09},
    {IconId::preferences, 0xF062E, 0xF1542},
    {IconId::fileBrowserDialog, 0xF0770, 0xF0DCF},
    {IconId::alertBox, 0xF0027, 0xF0CE4},
    {IconId::folderOpen, 0xF0770, 0xF0DCF},
    {IconId::information, 0xF02FC, 0xF02FD},
    {IconId::pineTreeVariant, 0xF1C73, 0xF1C74},
    {IconId::redo, 0xF044E, 0xF044F},
    {IconId::undo, 0xF054C, 0xF054D},
    {IconId::rename, 0xF1C18, 0xF1C1A},
    {IconId::selectAll, 0xF0486, 0xF0486},
    {IconId::shore, 0xF14F9, 0xF14F9},
    {IconId::spellcheck, 0xF04C6, 0xF04C6},
    {IconId::square, 0xF0764, 0xF0763},
    {IconId::squareMedium, 0xF0A13, 0xF0A14},
    {IconId::formatColorFill, 0xF0266, 0xF0266},
    {IconId::selectDrag, 0xF0A6C, 0xF0A6C},
    {IconId::select, 0xF0485, 0xF0485},
    {IconId::selectInverse, 0xF0487, 0xF0487},
    {IconId::palette, 0xF03D8, 0xF0E0C},
    {IconId::layers, 0xF0328, 0xF09FE},
    {IconId::image, 0xF02E9, 0xF0976},
    {IconId::folderSearch, 0xF0968, 0xF0969},
    {IconId::folder, 0xF024B, 0xF0256},
    {IconId::fitToScreen, 0xF18F4, 0xF18F5},
    {IconId::filter, 0xF0232, 0xF0233},
    {IconId::exportIcon, 0xF0207, 0xF0B93},
    {IconId::deleteIcon, 0xF01B4, 0xF09E7},
    {IconId::cursorMove, 0xF01BE, 0xF01BE},
    {IconId::contentSaveMove, 0xF0E27, 0xF0E28},
    {IconId::contentSave, 0xF0193, 0xF0818},
    {IconId::abacus, 0xF16E0, 0xF16E0},
    {IconId::account, 0xF0004, 0xF0013},
    {IconId::alert, 0xF0026, 0xF002A},
    {IconId::apps, 0xF003B, 0xF0D46},
    {IconId::autoFix, 0xF0068, 0xF0068},
    {IconId::axisZArrow, 0xF0D55, 0xF0D56},
    {IconId::binoculars, 0xF00A5, 0xF00A5},
    {IconId::bookOpen, 0xF00BD, 0xF0B63},
    {IconId::bookmark, 0xF00C0, 0xF00C3},
    {IconId::brush, 0xF00E3, 0xF1A0D},
    {IconId::bug, 0xF00E4, 0xF0A30},
    {IconId::check, 0xF012C, 0xF0855},
    {IconId::close, 0xF0156, 0xF06C9},
    {IconId::cog, 0xF0493, 0xF08BB},
    {IconId::contentCopy, 0xF018F, 0xF018F},
    {IconId::contentCut, 0xF0190, 0xF0190},
    {IconId::contentPaste, 0xF0192, 0xF0192},
    {IconId::identifier, 0xF0EFE, 0xF0EFE},
    {IconId::hexadecimal, 0xF12A7, 0xF12A7},
    {IconId::formatListBulleted, 0xF0279, 0xF0279},
    {IconId::refresh, 0xF0450, 0xF0450},
    {IconId::eyeOff, 0xF0209, 0xF06D1},
    {IconId::arrowUp, 0xF005D, 0xF005D},
    {IconId::arrowDown, 0xF0045, 0xF0045},
    {IconId::arrowLeft, 0xF004D, 0xF004D},
    {IconId::arrowRight, 0xF0054, 0xF0054},
    {IconId::ellipse, 0xF0EA0, 0xF0EA1},
    {IconId::fileDocumentPlus, 0xF1A9D, 0xF1A9E},
    {IconId::eyeClosed, 0xF1CA3, 0xF1CA3},
    {IconId::eye, 0xF0208, 0xF06D0},
    {IconId::selectRemove, 0xF17C1, 0xF17C1},
}};

constexpr std::array<std::string_view, 81> names{{
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
}};

constexpr std::array<std::string_view, 81> mdiNames{{
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
}};

constexpr std::array<std::string_view, 81> mdiOutlineNames{{
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
}};

bool fontAvailable = false;

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
