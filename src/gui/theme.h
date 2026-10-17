#pragma once

#include <cstdint>
#include <imgui.h>
#include <spdlog/common.h>

namespace im_cpp::gui::theme
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

inline constexpr Color traceLog{
    .red = 166.0F / maximumColorChannel,
    .green = 166.0F / maximumColorChannel,
    .blue = 166.0F / maximumColorChannel,
    .alpha = 1.0F,
};
inline constexpr Color debugLog{
    .red = 140.0F / maximumColorChannel,
    .green = 191.0F / maximumColorChannel,
    .blue = 1.0F,
    .alpha = 1.0F,
};
inline constexpr Color infoLog{
    .red = 166.0F / maximumColorChannel,
    .green = 217.0F / maximumColorChannel,
    .blue = 166.0F / maximumColorChannel,
    .alpha = 1.0F,
};
inline constexpr Color warningLog{
    .red = 1.0F,
    .green = 204.0F / maximumColorChannel,
    .blue = 89.0F / maximumColorChannel,
    .alpha = 1.0F,
};
inline constexpr Color errorLog{
    .red = 1.0F,
    .green = 102.0F / maximumColorChannel,
    .blue = 102.0F / maximumColorChannel,
    .alpha = 1.0F,
};
inline constexpr Color neutralLog{
    .red = 217.0F / maximumColorChannel,
    .green = 217.0F / maximumColorChannel,
    .blue = 217.0F / maximumColorChannel,
    .alpha = 1.0F,
};
constexpr Color colorFromRgba(std::uint32_t rgba) noexcept
{
    return {
        .red = static_cast<float>((rgba >> redChannelShift) & colorChannelMask) / maximumColorChannel,
        .green = static_cast<float>((rgba >> greenChannelShift) & colorChannelMask) / maximumColorChannel,
        .blue = static_cast<float>((rgba >> blueChannelShift) & colorChannelMask) / maximumColorChannel,
        .alpha = static_cast<float>(rgba & colorChannelMask) / maximumColorChannel,
    };
}

inline constexpr Color background = colorFromRgba(0x26262FFFU);
inline constexpr Color stoneSurface = colorFromRgba(0x1A1A21FFU);
inline constexpr Color stoneBorder = colorFromRgba(0x45454DFFU);
inline constexpr Color mutedGold = colorFromRgba(0x736333FFU);
inline constexpr Color brass = colorFromRgba(0x9E8C61FFU);
inline constexpr Color brightBrass = colorFromRgba(0xD9D4BDFFU);
inline constexpr Color parchmentText = colorFromRgba(0xBDBDB5FFU);
inline constexpr Color mutedText = colorFromRgba(0x828278FFU);
inline constexpr Color transparent = colorFromRgba(0x00000000U);
inline constexpr Color deepSlateVeil = colorFromRgba(0x1A1A21F2U);
inline constexpr Color blackVeil = colorFromRgba(0x00000082U);
inline constexpr Color brassWash = colorFromRgba(0x9E8C61ABU);
inline constexpr Color antiqueGoldHighlight = colorFromRgba(0x736333C9U);
inline constexpr Color antiqueGoldFade = colorFromRgba(0x736333A1U);
inline constexpr Color antiqueGoldSoft = colorFromRgba(0x736333F7U);
inline constexpr Color charcoalVeil = colorFromRgba(0x05050687U);
inline constexpr Color silverGray = colorFromRgba(0x9C9C9CFFU);
inline constexpr Color coral = colorFromRgba(0xFF6E59FFU);
inline constexpr Color amber = colorFromRgba(0xE6B300FFU);
inline constexpr Color orange = colorFromRgba(0xFF9900FFU);
inline constexpr Color stoneGray = colorFromRgba(0x474749FFU);
inline constexpr Color steelGray = colorFromRgba(0x666673FFU);
inline constexpr Color slateGray = colorFromRgba(0x525257FFU);
inline constexpr Color whiteTrace = colorFromRgba(0xFFFFFF0FU);
inline constexpr Color whiteVeil = colorFromRgba(0xFFFFFFB3U);
inline constexpr Color grayVeil = colorFromRgba(0xCCCCCC33U);
inline constexpr Color grayWash = colorFromRgba(0xCCCCCC59U);
inline constexpr Color invisibleGray = colorFromRgba(0x80808000U);
inline constexpr Color oxblood = colorFromRgba(0x631F1FFFU);
inline constexpr Color brickRed = colorFromRgba(0x843E37FFU);
inline constexpr Color dustyRose = colorFromRgba(0xA75D52FFU);
inline constexpr Color forestGreen = colorFromRgba(0x174638FFU);
inline constexpr Color leafGreen = colorFromRgba(0x286853FFU);
inline constexpr Color sageGreen = colorFromRgba(0x3A8068FFU);

inline constexpr float alpha = 1.0F;
inline constexpr float disabledAlpha = 0.60F;
inline constexpr ImVec2 windowPadding{10.0F, 10.0F};
inline constexpr float windowRounding = 4.0F;
inline constexpr float windowBorderSize = 1.0F;
inline constexpr float windowBorderHoverPadding = 3.0F;
inline constexpr ImVec2 windowMinimumSize{160.0F, 100.0F};
inline constexpr ImVec2 windowTitleAlignment{0.0F, 0.5F};
inline constexpr float childRounding = 3.0F;
inline constexpr float childBorderSize = 1.0F;
inline constexpr float popupRounding = 4.0F;
inline constexpr float popupBorderSize = 1.0F;
inline constexpr ImVec2 framePadding{7.0F, 5.0F};
inline constexpr float frameRounding = 3.0F;
inline constexpr float frameBorderSize = 1.0F;
inline constexpr ImVec2 itemSpacing{8.0F, 6.0F};
inline constexpr ImVec2 itemInnerSpacing{6.0F, 4.0F};
inline constexpr float indentSpacing = 18.0F;
inline constexpr float scrollbarSize = 14.0F;
inline constexpr float scrollbarRounding = 7.0F;
inline constexpr float grabMinimumSize = 12.0F;
inline constexpr float grabRounding = 3.0F;
inline constexpr float imageBorderSize = 1.0F;
inline constexpr float tabRounding = 3.0F;
inline constexpr float tabBorderSize = 1.0F;
inline constexpr float tabBarBorderSize = 1.0F;
inline constexpr float dockingSeparatorSize = 2.0F;

struct ButtonColors
{
    Color normal;
    Color hovered;
    Color active;
};

inline constexpr ButtonColors alternateButtonBurgundy{
    .normal = oxblood,
    .hovered = brickRed,
    .active = dustyRose,
};
inline constexpr ButtonColors alternateButtonGreen{
    .normal = forestGreen,
    .hovered = leafGreen,
    .active = sageGreen,
};

[[nodiscard]] inline ImVec4 imguiColor(const Color & color) noexcept
{
    ImVec4 result{};
    result.x = color.red;
    result.y = color.green;
    result.z = color.blue;
    result.w = color.alpha;
    return result;
}

inline void pushButtonColors(const ButtonColors & colors)
{
    ImGui::PushStyleColor(ImGuiCol_Button, imguiColor(colors.normal));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, imguiColor(colors.hovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, imguiColor(colors.active));
}

inline void popButtonColors()
{
    ImGui::PopStyleColor(buttonColorStackDepth);
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
    style.Colors[ImGuiCol_Text] = imguiColor(parchmentText);
    style.Colors[ImGuiCol_TextDisabled] = imguiColor(mutedText);
    style.Colors[ImGuiCol_TextLink] = imguiColor(brass);
    style.Colors[ImGuiCol_TextSelectedBg] = imguiColor(mutedGold);
    style.Colors[ImGuiCol_WindowBg] = imguiColor(background);
    style.Colors[ImGuiCol_ChildBg] = imguiColor(transparent);
    style.Colors[ImGuiCol_PopupBg] = imguiColor(deepSlateVeil);
    style.Colors[ImGuiCol_Border] = imguiColor(stoneBorder);
    style.Colors[ImGuiCol_BorderShadow] = imguiColor(transparent);
    style.Colors[ImGuiCol_FrameBg] = imguiColor(stoneSurface);
    style.Colors[ImGuiCol_FrameBgHovered] = imguiColor(stoneBorder);
    style.Colors[ImGuiCol_FrameBgActive] = imguiColor(mutedGold);
    style.Colors[ImGuiCol_TitleBg] = imguiColor(stoneSurface);
    style.Colors[ImGuiCol_TitleBgActive] = imguiColor(mutedGold);
    style.Colors[ImGuiCol_TitleBgCollapsed] = imguiColor(blackVeil);
    style.Colors[ImGuiCol_MenuBarBg] = imguiColor(stoneSurface);
    style.Colors[ImGuiCol_ScrollbarBg] = imguiColor(charcoalVeil);
    style.Colors[ImGuiCol_ScrollbarGrab] = imguiColor(brass);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = imguiColor(antiqueGoldHighlight);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = imguiColor(brightBrass);
    style.Colors[ImGuiCol_CheckMark] = imguiColor(brass);
    style.Colors[ImGuiCol_SliderGrab] = imguiColor(brass);
    style.Colors[ImGuiCol_SliderGrabActive] = imguiColor(brightBrass);
    style.Colors[ImGuiCol_ResizeGrip] = imguiColor(brassWash);
    style.Colors[ImGuiCol_ResizeGripHovered] = imguiColor(antiqueGoldHighlight);
    style.Colors[ImGuiCol_ResizeGripActive] = imguiColor(brightBrass);
    style.Colors[ImGuiCol_Button] = imguiColor(brassWash);
    style.Colors[ImGuiCol_ButtonHovered] = imguiColor(antiqueGoldHighlight);
    style.Colors[ImGuiCol_ButtonActive] = imguiColor(brightBrass);
    style.Colors[ImGuiCol_Header] = imguiColor(brassWash);
    style.Colors[ImGuiCol_HeaderHovered] = imguiColor(antiqueGoldHighlight);
    style.Colors[ImGuiCol_HeaderActive] = imguiColor(brightBrass);
    style.Colors[ImGuiCol_Separator] = imguiColor(stoneBorder);
    style.Colors[ImGuiCol_SeparatorHovered] = imguiColor(antiqueGoldHighlight);
    style.Colors[ImGuiCol_SeparatorActive] = imguiColor(brightBrass);
    style.Colors[ImGuiCol_Tab] = imguiColor(brassWash);
    style.Colors[ImGuiCol_TabSelected] = imguiColor(mutedGold);
    style.Colors[ImGuiCol_TabHovered] = imguiColor(antiqueGoldHighlight);
    style.Colors[ImGuiCol_TabDimmed] = imguiColor(antiqueGoldFade);
    style.Colors[ImGuiCol_TabDimmedSelected] = imguiColor(antiqueGoldSoft);
    style.Colors[ImGuiCol_TabDimmedSelectedOverline] = imguiColor(invisibleGray);
    style.Colors[ImGuiCol_TabSelectedOverline] = imguiColor(brightBrass);
    style.Colors[ImGuiCol_PlotLines] = imguiColor(silverGray);
    style.Colors[ImGuiCol_PlotLinesHovered] = imguiColor(coral);
    style.Colors[ImGuiCol_PlotHistogram] = imguiColor(amber);
    style.Colors[ImGuiCol_PlotHistogramHovered] = imguiColor(orange);
    style.Colors[ImGuiCol_TableHeaderBg] = imguiColor(stoneGray);
    style.Colors[ImGuiCol_TableBorderStrong] = imguiColor(steelGray);
    style.Colors[ImGuiCol_TableBorderLight] = imguiColor(slateGray);
    style.Colors[ImGuiCol_TableRowBg] = imguiColor(transparent);
    style.Colors[ImGuiCol_TableRowBgAlt] = imguiColor(whiteTrace);
    style.Colors[ImGuiCol_NavCursor] = imguiColor(brass);
    style.Colors[ImGuiCol_NavWindowingHighlight] = imguiColor(whiteVeil);
    style.Colors[ImGuiCol_NavWindowingDimBg] = imguiColor(grayVeil);
    style.Colors[ImGuiCol_ModalWindowDimBg] = imguiColor(grayWash);
    style.Colors[ImGuiCol_DockingPreview] = imguiColor(antiqueGoldHighlight);
    style.Colors[ImGuiCol_DockingEmptyBg] = imguiColor(background);
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

} // namespace im_cpp::gui::theme
