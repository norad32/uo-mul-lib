#pragma once

#include <cstdint>
#include <imgui.h>
#include <spdlog/common.h>

namespace uo_mule::gui::theme
{

struct Color
{
    float red;
    float green;
    float blue;
    float alpha;
};

inline constexpr float maximumColorChannel = 255.0F;
inline constexpr std::uint32_t colorChannelMask = 0xFFU;
inline constexpr int buttonColorStackDepth = 3;

inline constexpr std::uint32_t redChannelShift = 24U;
inline constexpr std::uint32_t greenChannelShift = 16U;
inline constexpr std::uint32_t blueChannelShift = 8U;

constexpr Color colorFromRgba(std::uint32_t rgba) noexcept
{
    return {
        .red = static_cast<float>((rgba >> redChannelShift) & colorChannelMask) / maximumColorChannel,
        .green = static_cast<float>((rgba >> greenChannelShift) & colorChannelMask) / maximumColorChannel,
        .blue = static_cast<float>((rgba >> blueChannelShift) & colorChannelMask) / maximumColorChannel,
        .alpha = static_cast<float>(rgba & colorChannelMask) / maximumColorChannel,
    };
}

inline constexpr Color traceLog = colorFromRgba(0x9399B2FFU);    // Overlay 2
inline constexpr Color debugLog = colorFromRgba(0x89B4FAFFU);    // Blue
inline constexpr Color infoLog = colorFromRgba(0x94E2D5FFU);     // Teal
inline constexpr Color warningLog = colorFromRgba(0xF9E2AFFFU);  // Yellow
inline constexpr Color errorLog = colorFromRgba(0xF38BA8FFU);    // Red
inline constexpr Color neutralLog = colorFromRgba(0xBAC2DEFFU);  // Subtext 1

inline constexpr Color base = colorFromRgba(0x1E1E2EFFU);
inline constexpr Color mantle = colorFromRgba(0x181825FFU);
inline constexpr Color crust = colorFromRgba(0x11111BFFU);
inline constexpr Color surface0 = colorFromRgba(0x313244FFU);
inline constexpr Color surface1 = colorFromRgba(0x45475AFFU);
inline constexpr Color surface2 = colorFromRgba(0x585B70FFU);
inline constexpr Color text = colorFromRgba(0xCDD6F4FFU);
inline constexpr Color subtext0 = colorFromRgba(0xA6ADC8FFU);
inline constexpr Color subtext1 = colorFromRgba(0xBAC2DEFFU);
inline constexpr Color overlay0 = colorFromRgba(0x6C7086FFU);
inline constexpr Color overlay1 = colorFromRgba(0x7F849CFFU);
inline constexpr Color overlay2 = colorFromRgba(0x9399B2FFU);
inline constexpr Color mauve = colorFromRgba(0xCBA6F7FFU);
inline constexpr Color lavender = colorFromRgba(0xB4BEFEFFU);
inline constexpr Color blue = colorFromRgba(0x89B4FAFFU);
inline constexpr Color sapphire = colorFromRgba(0x74C7ECFFU);
inline constexpr Color sky = colorFromRgba(0x89DCEBFFU);
inline constexpr Color teal = colorFromRgba(0x94E2D5FFU);
inline constexpr Color green = colorFromRgba(0xA6E3A1FFU);
inline constexpr Color yellow = colorFromRgba(0xF9E2AFFFU);
inline constexpr Color peach = colorFromRgba(0xFAB387FFU);
inline constexpr Color red = colorFromRgba(0xF38BA8FFU);
inline constexpr Color maroon = colorFromRgba(0xEBA0ACFFU);
inline constexpr Color flamingo = colorFromRgba(0xF2CDCDFFU);
inline constexpr Color rosewater = colorFromRgba(0xF5E0DCFFU);
inline constexpr Color pink = colorFromRgba(0xF5C2E7FFU);
inline constexpr Color transparent = colorFromRgba(0x00000000U);

// Semantic roles used by application UI code. Keep these mapped to the official palette above.
inline constexpr Color canvasBackground = base;
inline constexpr Color panelBackground = mantle;
inline constexpr Color popupBackground = mantle;
inline constexpr Color border = surface1;
inline constexpr Color selection = lavender;
inline constexpr Color accent = mauve;
inline constexpr Color primaryText = text;
inline constexpr Color greeting = lavender;
inline constexpr Color warning = peach;
inline constexpr Color error = red;
inline constexpr Color success = green;
inline constexpr Color info = sapphire;
inline constexpr Color mutedText = subtext0;

[[nodiscard]] constexpr Color withAlpha(Color color, float value) noexcept
{
    color.alpha = value;
    return color;
}

inline constexpr float alpha = 1.0F;
inline constexpr float disabledAlpha = 0.60F;
inline constexpr ImVec2 windowPadding{12.0F, 12.0F};
inline constexpr float windowRounding = 8.0F;
inline constexpr float windowBorderSize = 1.0F;
inline constexpr float windowBorderHoverPadding = 3.0F;
inline constexpr ImVec2 windowMinimumSize{160.0F, 100.0F};
inline constexpr ImVec2 windowTitleAlignment{0.0F, 0.5F};
inline constexpr float childRounding = 5.0F;
inline constexpr float childBorderSize = 1.0F;
inline constexpr float popupRounding = 5.0F;
inline constexpr float popupBorderSize = 1.0F;
inline constexpr ImVec2 framePadding{6.0F, 4.0F};
inline constexpr float frameRounding = 5.0F;
inline constexpr float frameBorderSize = 0.0F;
inline constexpr ImVec2 itemSpacing{8.0F, 6.0F};
inline constexpr ImVec2 itemInnerSpacing{6.0F, 4.0F};
inline constexpr float indentSpacing = 18.0F;
inline constexpr float scrollbarSize = 14.0F;
inline constexpr float scrollbarRounding = 12.0F;
inline constexpr float grabMinimumSize = 12.0F;
inline constexpr float grabRounding = 5.0F;
inline constexpr float imageBorderSize = 1.0F;
inline constexpr float tabRounding = 5.0F;
inline constexpr float tabBorderSize = 1.0F;
inline constexpr float tabBarBorderSize = 1.0F;
inline constexpr float dockingSeparatorSize = 2.0F;
inline constexpr float popupAlpha = 0.95F;
inline constexpr float collapsedTitleAlpha = 0.51F;
inline constexpr float resizeGripAlpha = 0.67F;
inline constexpr float resizeGripHoveredAlpha = 0.79F;
inline constexpr float tabDimmedAlpha = 0.63F;
inline constexpr float tabDimmedSelectedAlpha = 0.97F;
inline constexpr float tableAlternateAlpha = 0.06F;
inline constexpr float navHighlightAlpha = 0.70F;
inline constexpr float navDimAlpha = 0.20F;
inline constexpr float modalDimAlpha = 0.35F;

[[nodiscard]] inline ImVec4 imguiColor(const Color & color) noexcept
{
    ImVec4 result{};
    result.x = color.red;
    result.y = color.green;
    result.z = color.blue;
    result.w = color.alpha;
    return result;
}

[[nodiscard]] inline ImU32 packedColor(const Color & color) noexcept
{
    return ImGui::ColorConvertFloat4ToU32(imguiColor(color));
}

inline void applyImGuiStyle()
{
    ImGuiStyle & style = ImGui::GetStyle();
    style.Alpha = alpha;
    style.DisabledAlpha = disabledAlpha;
    style.WindowPadding = windowPadding;
    style.WindowRounding = windowRounding;
    style.WindowBorderSize = windowBorderSize;
    style.WindowBorderHoverPadding = windowBorderHoverPadding;
    style.WindowMinSize = windowMinimumSize;
    style.WindowTitleAlign = windowTitleAlignment;
    style.ChildRounding = childRounding;
    style.ChildBorderSize = childBorderSize;
    style.PopupRounding = popupRounding;
    style.PopupBorderSize = popupBorderSize;
    style.FramePadding = framePadding;
    style.FrameRounding = frameRounding;
    style.FrameBorderSize = frameBorderSize;
    style.ItemSpacing = itemSpacing;
    style.ItemInnerSpacing = itemInnerSpacing;
    style.IndentSpacing = indentSpacing;
    style.ScrollbarSize = scrollbarSize;
    style.ScrollbarRounding = scrollbarRounding;
    style.GrabMinSize = grabMinimumSize;
    style.GrabRounding = grabRounding;
    style.ImageBorderSize = imageBorderSize;
    style.TabRounding = tabRounding;
    style.TabBorderSize = tabBorderSize;
    style.TabBarBorderSize = tabBarBorderSize;
    style.DockingSeparatorSize = dockingSeparatorSize;
    style.Colors[ImGuiCol_Text] = imguiColor(text);
    style.Colors[ImGuiCol_TextDisabled] = imguiColor(subtext0);
    style.Colors[ImGuiCol_TextLink] = imguiColor(lavender);
    style.Colors[ImGuiCol_TextSelectedBg] = imguiColor(surface0);
    style.Colors[ImGuiCol_WindowBg] = imguiColor(base);
    style.Colors[ImGuiCol_ChildBg] = imguiColor(transparent);
    style.Colors[ImGuiCol_PopupBg] = imguiColor(withAlpha(mantle, popupAlpha));
    style.Colors[ImGuiCol_Border] = imguiColor(surface1);
    style.Colors[ImGuiCol_BorderShadow] = imguiColor(transparent);
    style.Colors[ImGuiCol_FrameBg] = imguiColor(surface0);
    style.Colors[ImGuiCol_FrameBgHovered] = imguiColor(surface1);
    style.Colors[ImGuiCol_FrameBgActive] = imguiColor(surface2);
    style.Colors[ImGuiCol_TitleBg] = imguiColor(mantle);
    style.Colors[ImGuiCol_TitleBgActive] = imguiColor(base);
    style.Colors[ImGuiCol_TitleBgCollapsed] = imguiColor(withAlpha(crust, collapsedTitleAlpha));
    style.Colors[ImGuiCol_MenuBarBg] = imguiColor(mantle);
    style.Colors[ImGuiCol_ScrollbarBg] = imguiColor(mantle);
    style.Colors[ImGuiCol_ScrollbarGrab] = imguiColor(surface2);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = imguiColor(overlay0);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = imguiColor(overlay2);
    style.Colors[ImGuiCol_CheckMark] = imguiColor(lavender);
    style.Colors[ImGuiCol_SliderGrab] = imguiColor(sapphire);
    style.Colors[ImGuiCol_SliderGrabActive] = imguiColor(sapphire);
    style.Colors[ImGuiCol_ResizeGrip] = imguiColor(withAlpha(mauve, resizeGripAlpha));
    style.Colors[ImGuiCol_ResizeGripHovered] = imguiColor(withAlpha(mauve, resizeGripHoveredAlpha));
    style.Colors[ImGuiCol_ResizeGripActive] = imguiColor(lavender);
    style.Colors[ImGuiCol_Button] = imguiColor(surface0);
    style.Colors[ImGuiCol_ButtonHovered] = imguiColor(surface1);
    style.Colors[ImGuiCol_ButtonActive] = imguiColor(surface2);
    style.Colors[ImGuiCol_Header] = imguiColor(surface0);
    style.Colors[ImGuiCol_HeaderHovered] = imguiColor(surface1);
    style.Colors[ImGuiCol_HeaderActive] = imguiColor(surface2);
    style.Colors[ImGuiCol_Separator] = imguiColor(surface1);
    style.Colors[ImGuiCol_SeparatorHovered] = imguiColor(mauve);
    style.Colors[ImGuiCol_SeparatorActive] = imguiColor(lavender);
    style.Colors[ImGuiCol_Tab] = imguiColor(surface0);
    style.Colors[ImGuiCol_TabSelected] = imguiColor(surface2);
    style.Colors[ImGuiCol_TabHovered] = imguiColor(surface1);
    style.Colors[ImGuiCol_TabDimmed] = imguiColor(withAlpha(surface0, tabDimmedAlpha));
    style.Colors[ImGuiCol_TabDimmedSelected] = imguiColor(withAlpha(surface2, tabDimmedSelectedAlpha));
    style.Colors[ImGuiCol_TabDimmedSelectedOverline] = imguiColor(transparent);
    style.Colors[ImGuiCol_TabSelectedOverline] = imguiColor(lavender);
    style.Colors[ImGuiCol_PlotLines] = imguiColor(overlay2);
    style.Colors[ImGuiCol_PlotLinesHovered] = imguiColor(red);
    style.Colors[ImGuiCol_PlotHistogram] = imguiColor(yellow);
    style.Colors[ImGuiCol_PlotHistogramHovered] = imguiColor(peach);
    style.Colors[ImGuiCol_TableHeaderBg] = imguiColor(surface1);
    style.Colors[ImGuiCol_TableBorderStrong] = imguiColor(overlay0);
    style.Colors[ImGuiCol_TableBorderLight] = imguiColor(surface2);
    style.Colors[ImGuiCol_TableRowBg] = imguiColor(transparent);
    style.Colors[ImGuiCol_TableRowBgAlt] = imguiColor(withAlpha(text, tableAlternateAlpha));
    style.Colors[ImGuiCol_NavCursor] = imguiColor(lavender);
    style.Colors[ImGuiCol_NavWindowingHighlight] = imguiColor(withAlpha(text, navHighlightAlpha));
    style.Colors[ImGuiCol_NavWindowingDimBg] = imguiColor(withAlpha(overlay2, navDimAlpha));
    style.Colors[ImGuiCol_ModalWindowDimBg] = imguiColor(withAlpha(overlay2, modalDimAlpha));
    style.Colors[ImGuiCol_DockingPreview] = imguiColor(withAlpha(mauve, resizeGripHoveredAlpha));
    style.Colors[ImGuiCol_DockingEmptyBg] = imguiColor(base);
}

[[nodiscard]] inline Color logLevelColor(spdlog::level::level_enum level) noexcept
{
    switch (level)
    {
        case spdlog::level::trace :
            return traceLog;
        case spdlog::level::debug :
            return debugLog;
        case spdlog::level::info :
            return infoLog;
        case spdlog::level::warn :
            return warningLog;
        case spdlog::level::err :
        case spdlog::level::critical :
            return errorLog;
        default :
            return neutralLog;
    }
}

} // namespace uo_mule::gui::theme
