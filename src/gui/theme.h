#pragma once

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
inline constexpr Color background{.red = 0.08F, .green = 0.08F, .blue = 0.09F, .alpha = 1.0F};

inline constexpr float alpha = 1.0F;
inline constexpr float disabledAlpha = 0.60F;
inline constexpr ImVec2 windowPadding{8.0F, 8.0F};
inline constexpr float windowRounding = 0.0F;
inline constexpr float windowBorderSize = 1.0F;
inline constexpr float windowBorderHoverPadding = 4.0F;
inline constexpr ImVec2 windowMinimumSize{32.0F, 32.0F};
inline constexpr ImVec2 windowTitleAlignment{0.0F, 0.5F};
inline constexpr float childRounding = 0.0F;
inline constexpr float childBorderSize = 1.0F;
inline constexpr float popupRounding = 0.0F;
inline constexpr float popupBorderSize = 1.0F;
inline constexpr ImVec2 framePadding{4.0F, 3.0F};
inline constexpr float frameRounding = 0.0F;
inline constexpr float frameBorderSize = 0.0F;
inline constexpr ImVec2 itemSpacing{8.0F, 4.0F};
inline constexpr ImVec2 itemInnerSpacing{4.0F, 4.0F};
inline constexpr float indentSpacing = 21.0F;
inline constexpr float scrollbarSize = 14.0F;
inline constexpr float scrollbarRounding = 9.0F;
inline constexpr float grabMinimumSize = 12.0F;
inline constexpr float grabRounding = 0.0F;
inline constexpr float imageBorderSize = 0.0F;
inline constexpr float tabRounding = 5.0F;
inline constexpr float tabBorderSize = 0.0F;
inline constexpr float tabBarBorderSize = 1.0F;
inline constexpr float dockingSeparatorSize = 2.0F;
inline constexpr Color panelBackground = background;
inline constexpr Color childBackground{.red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 0.0F};
inline constexpr Color popupBackground{.red = 0.08F, .green = 0.08F, .blue = 0.08F, .alpha = 0.94F};
inline constexpr Color panelBorder{.red = 0.43F, .green = 0.43F, .blue = 0.50F, .alpha = 0.50F};
inline constexpr Color panelBorderShadow{.red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 0.0F};
inline constexpr Color panelTitle{.red = 0.04F, .green = 0.04F, .blue = 0.04F, .alpha = 1.0F};
inline constexpr Color panelTitleActive{.red = 0.16F, .green = 0.29F, .blue = 0.48F, .alpha = 1.0F};
inline constexpr Color panelTitleCollapsed{.red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 0.51F};
inline constexpr Color panelTab{.red = 0.18F, .green = 0.35F, .blue = 0.58F, .alpha = 0.862F};
inline constexpr Color panelTabActive{.red = 0.20F, .green = 0.41F, .blue = 0.68F, .alpha = 1.0F};
inline constexpr Color panelTabHovered{.red = 0.26F, .green = 0.59F, .blue = 0.98F, .alpha = 0.80F};
inline constexpr Color panelTabDimmed{.red = 0.068F, .green = 0.102F, .blue = 0.148F, .alpha = 0.9724F};
inline constexpr Color panelTabDimmedActive{.red = 0.136F, .green = 0.262F, .blue = 0.424F, .alpha = 1.0F};
inline constexpr Color panelTabOverline{.red = 0.26F, .green = 0.59F, .blue = 0.98F, .alpha = 1.0F};
inline constexpr Color panelText{.red = 1.0F, .green = 1.0F, .blue = 1.0F, .alpha = 1.0F};
inline constexpr Color panelDisabledText{.red = 0.50F, .green = 0.50F, .blue = 0.50F, .alpha = 1.0F};
inline constexpr Color panelFrame{.red = 0.16F, .green = 0.29F, .blue = 0.48F, .alpha = 0.54F};
inline constexpr Color panelFrameHovered{.red = 0.26F, .green = 0.59F, .blue = 0.98F, .alpha = 0.40F};
inline constexpr Color panelFrameActive{.red = 0.26F, .green = 0.59F, .blue = 0.98F, .alpha = 0.67F};
inline constexpr Color panelButton{.red = 0.26F, .green = 0.59F, .blue = 0.98F, .alpha = 0.40F};
inline constexpr Color panelButtonHovered{.red = 0.26F, .green = 0.59F, .blue = 0.98F, .alpha = 1.0F};
inline constexpr Color panelButtonActive{.red = 0.06F, .green = 0.53F, .blue = 0.98F, .alpha = 1.0F};
inline constexpr Color panelHeader{.red = 0.26F, .green = 0.59F, .blue = 0.98F, .alpha = 0.31F};
inline constexpr Color panelHeaderHovered{.red = 0.26F, .green = 0.59F, .blue = 0.98F, .alpha = 0.80F};
inline constexpr Color panelHeaderActive{.red = 0.26F, .green = 0.59F, .blue = 0.98F, .alpha = 1.0F};
inline constexpr Color panelSeparatorHovered{.red = 0.10F, .green = 0.40F, .blue = 0.75F, .alpha = 0.78F};
inline constexpr Color panelSeparatorActive{.red = 0.10F, .green = 0.40F, .blue = 0.75F, .alpha = 1.0F};
inline constexpr Color dockPreview{.red = 0.26F, .green = 0.59F, .blue = 0.98F, .alpha = 0.70F};
inline constexpr Color dockBackground{.red = 0.20F, .green = 0.20F, .blue = 0.20F, .alpha = 1.0F};

[[nodiscard]] inline ImVec4 imguiColor(const Color & color) noexcept
{
    ImVec4 result{};
    result.x = color.red;
    result.y = color.green;
    result.z = color.blue;
    result.w = color.alpha;
    return result;
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
    style.Colors[ImGuiCol_Text] = imguiColor(panelText);
    style.Colors[ImGuiCol_TextDisabled] = imguiColor(panelDisabledText);
    style.Colors[ImGuiCol_WindowBg] = imguiColor(panelBackground);
    style.Colors[ImGuiCol_ChildBg] = imguiColor(childBackground);
    style.Colors[ImGuiCol_PopupBg] = imguiColor(popupBackground);
    style.Colors[ImGuiCol_Border] = imguiColor(panelBorder);
    style.Colors[ImGuiCol_BorderShadow] = imguiColor(panelBorderShadow);
    style.Colors[ImGuiCol_FrameBg] = imguiColor(panelFrame);
    style.Colors[ImGuiCol_FrameBgHovered] = imguiColor(panelFrameHovered);
    style.Colors[ImGuiCol_FrameBgActive] = imguiColor(panelFrameActive);
    style.Colors[ImGuiCol_TitleBg] = imguiColor(panelTitle);
    style.Colors[ImGuiCol_TitleBgActive] = imguiColor(panelTitleActive);
    style.Colors[ImGuiCol_TitleBgCollapsed] = imguiColor(panelTitleCollapsed);
    style.Colors[ImGuiCol_Button] = imguiColor(panelButton);
    style.Colors[ImGuiCol_ButtonHovered] = imguiColor(panelButtonHovered);
    style.Colors[ImGuiCol_ButtonActive] = imguiColor(panelButtonActive);
    style.Colors[ImGuiCol_Header] = imguiColor(panelHeader);
    style.Colors[ImGuiCol_HeaderHovered] = imguiColor(panelHeaderHovered);
    style.Colors[ImGuiCol_HeaderActive] = imguiColor(panelHeaderActive);
    style.Colors[ImGuiCol_Separator] = imguiColor(panelBorder);
    style.Colors[ImGuiCol_SeparatorHovered] = imguiColor(panelSeparatorHovered);
    style.Colors[ImGuiCol_SeparatorActive] = imguiColor(panelSeparatorActive);
    style.Colors[ImGuiCol_Tab] = imguiColor(panelTab);
    style.Colors[ImGuiCol_TabSelected] = imguiColor(panelTabActive);
    style.Colors[ImGuiCol_TabHovered] = imguiColor(panelTabHovered);
    style.Colors[ImGuiCol_TabDimmed] = imguiColor(panelTabDimmed);
    style.Colors[ImGuiCol_TabDimmedSelected] = imguiColor(panelTabDimmedActive);
    style.Colors[ImGuiCol_TabSelectedOverline] = imguiColor(panelTabOverline);
    style.Colors[ImGuiCol_DockingPreview] = imguiColor(dockPreview);
    style.Colors[ImGuiCol_DockingEmptyBg] = imguiColor(dockBackground);
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
