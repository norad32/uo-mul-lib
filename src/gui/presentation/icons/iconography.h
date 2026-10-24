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
    lasso,
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
    fileBrowserDialog,
    alertBox,
    folderOpen,
    information,
    pineTreeVariant,
    redo,
    undo,
    rename,
    selectAll,
    shore,
    spellcheck,
    square,
    squareMedium,
    formatColorFill,
    selectDrag,
    select,
    selectInverse,
    palette,
    layers,
    image,
    folderSearch,
    folder,
    fitToScreen,
    filter,
    exportIcon,
    deleteIcon,
    cursorMove,
    contentSaveMove,
    contentSave,
    abacus,
    account,
    alert,
    apps,
    autoFix,
    axisZArrow,
    binoculars,
    bookOpen,
    bookmark,
    brush,
    bug,
    check,
    close,
    cog,
    contentCopy,
    contentCut,
    contentPaste,
    identifier,
    hexadecimal,
    formatListBulleted,
    refresh,
    eyeOff,
    arrowUp,
    arrowDown,
    arrowLeft,
    arrowRight,
    ellipse,
    fileDocumentPlus,
    eyeClosed,
    eye,
    selectRemove
};

enum class IconVariant : std::uint8_t
{
    filled,
    outline
};

struct IconMapping
{
    IconId id;
    std::uint32_t codepoint;
    std::uint32_t outlineCodepoint;
};

[[nodiscard]] std::span<const IconMapping> iconMappings() noexcept;
[[nodiscard]] std::string_view iconName(IconId id) noexcept;
[[nodiscard]] std::string_view iconName(IconId id, IconVariant variant) noexcept;
[[nodiscard]] std::string_view iconSemanticName(IconId id) noexcept;
[[nodiscard]] std::string iconGlyph(IconId id, IconVariant variant = IconVariant::filled);
[[nodiscard]] bool iconFontAvailable() noexcept;
void setIconFontAvailable(bool available) noexcept;
[[nodiscard]] std::string iconLabel(IconId id, std::string_view label, bool iconFontAvailable);
[[nodiscard]] std::string iconLabel(IconId id, std::string_view label);
[[nodiscard]] std::string iconLabel(IconId id,
                                    IconVariant variant,
                                    std::string_view label,
                                    bool iconFontAvailable);
[[nodiscard]] std::string iconLabel(IconId id, IconVariant variant, std::string_view label);
[[nodiscard]] std::span<const std::uint32_t> imguiIconGlyphRanges() noexcept;

} // namespace uo_mule::gui
