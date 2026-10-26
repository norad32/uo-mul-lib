#include "gui/presentation/dialog_presentation.h"
#include "gui/presentation/theme.h"
#include <algorithm>
#include <cfloat>
#include <imgui.h>
#include <initializer_list>

namespace uo_mule::gui
{

namespace
{

constexpr float framePaddingMultiplier = 2.0F;
constexpr float dialogCenterPivot = 0.5F;

constexpr float largeWidth = 860.0F;
constexpr float largeHeight = 720.0F;
constexpr float largeMinimumWidth = 640.0F;
constexpr float largeMinimumHeight = 480.0F;
constexpr float browserWidth = 980.0F;
constexpr float browserHeight = 620.0F;
constexpr float browserMinimumWidth = 720.0F;
constexpr float browserMinimumHeight = 440.0F;
constexpr float mapWidth = 620.0F;
constexpr float mapHeight = 420.0F;
constexpr float mapMinimumWidth = 440.0F;
constexpr float mapMinimumHeight = 260.0F;
constexpr float dialogFieldLabelWidth = 240.0F;

[[nodiscard]] ImGuiWindowFlags modalFlags(ImGuiWindowFlags extraFlags) noexcept
{
    return static_cast<ImGuiWindowFlags>(
        static_cast<unsigned int>(ImGuiWindowFlags_NoCollapse) | static_cast<unsigned int>(ImGuiWindowFlags_NoResize) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoScrollbar) |
        static_cast<unsigned int>(ImGuiWindowFlags_NoScrollWithMouse) | static_cast<unsigned int>(extraFlags));
}

} // namespace

DialogWindowSpec largeDialogSpec() noexcept
{
    return {.initialSize = ImVec2(largeWidth, largeHeight), .minimumSize = ImVec2(largeMinimumWidth, largeMinimumHeight)};
}

DialogWindowSpec browserDialogSpec() noexcept
{
    return {
        .initialSize = ImVec2(browserWidth, browserHeight),
        .minimumSize = ImVec2(browserMinimumWidth, browserMinimumHeight),
    };
}

DialogWindowSpec mapDialogSpec() noexcept
{
    return {.initialSize = ImVec2(mapWidth, mapHeight), .minimumSize = ImVec2(mapMinimumWidth, mapMinimumHeight)};
}

bool beginDialog(const char * popupId, bool * visible, DialogWindowSpec spec, ImGuiWindowFlags extraFlags)
{
    ImGui::OpenPopup(popupId);
    ImGui::SetNextWindowSize(spec.initialSize, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(spec.minimumSize, ImVec2(FLT_MAX, FLT_MAX));
    return ImGui::BeginPopupModal(popupId, visible, modalFlags(extraFlags));
}

bool beginAutoSizeDialog(const char * popupId, bool * visible, ImGuiWindowFlags extraFlags)
{
    ImGui::OpenPopup(popupId);
    const ImGuiViewport * viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(dialogCenterPivot, dialogCenterPivot));
    const auto flags = static_cast<ImGuiWindowFlags>(static_cast<unsigned int>(extraFlags) |
                                                     static_cast<unsigned int>(ImGuiWindowFlags_AlwaysAutoResize));
    return ImGui::BeginPopupModal(popupId, visible, modalFlags(flags));
}

void endDialog()
{
    ImGui::EndPopup();
}

void dialogHeading(const char * text)
{
    ImGui::TextColored(theme::imguiColor(theme::heading), "%s", text);
}

void dialogSubheading(const char * text)
{
    ImGui::TextColored(theme::imguiColor(theme::subheading), "%s", text);
}

void dialogSeparatorText(const char * text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, theme::imguiColor(theme::subheading));
    ImGui::SeparatorText(text);
    ImGui::PopStyleColor();
}

void dialogFieldLabel(const char * text)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(text);
    ImGui::SameLine(dialogFieldLabelWidth);
}

void dialogFieldWidth(float width)
{
    ImGui::SetNextItemWidth(width);
}

void beginDialogFooter(std::initializer_list<const char *> labels, bool alignRight)
{
    if (!alignRight)
    {
        ImGui::BeginGroup();
        return;
    }

    float buttonWidth = 0.0F;
    for (const char * label : labels)
    {
        buttonWidth += ImGui::CalcTextSize(label).x + (ImGui::GetStyle().FramePadding.x * framePaddingMultiplier);
    }
    if (labels.size() > 1U)
    {
        buttonWidth += static_cast<float>(labels.size() - 1U) * ImGui::GetStyle().ItemSpacing.x;
    }
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0F, ImGui::GetContentRegionAvail().x - buttonWidth));
    ImGui::BeginGroup();
}

void endDialogFooter()
{
    ImGui::EndGroup();
}

bool dialogButton(const char * label, bool enabled)
{
    ImGui::BeginDisabled(!enabled);
    const bool clicked = ImGui::Button(label);
    ImGui::EndDisabled();
    return clicked;
}

} // namespace uo_mule::gui
