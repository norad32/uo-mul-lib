#include "gui/dialogs/dialog_presentation.h"
#include <cfloat>
#include <imgui.h>

namespace uo_mule::gui
{

namespace
{

constexpr float standardWidth = 520.0F;
constexpr float standardHeight = 320.0F;
constexpr float standardMinimumWidth = 360.0F;
constexpr float standardMinimumHeight = 220.0F;
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

[[nodiscard]] ImGuiWindowFlags modalFlags(ImGuiWindowFlags extraFlags) noexcept
{
    return static_cast<ImGuiWindowFlags>(static_cast<unsigned int>(ImGuiWindowFlags_NoCollapse) |
                                         static_cast<unsigned int>(extraFlags));
}

} // namespace

DialogWindowSpec standardDialogSpec() noexcept
{
    return {.initialSize = ImVec2(standardWidth, standardHeight), .minimumSize = ImVec2(standardMinimumWidth, standardMinimumHeight)};
}

DialogWindowSpec largeDialogSpec() noexcept
{
    return {.initialSize = ImVec2(largeWidth, largeHeight), .minimumSize = ImVec2(largeMinimumWidth, largeMinimumHeight)};
}

DialogWindowSpec browserDialogSpec() noexcept
{
    return {.initialSize = ImVec2(browserWidth, browserHeight), .minimumSize = ImVec2(browserMinimumWidth, browserMinimumHeight)};
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

void endDialog()
{
    ImGui::EndPopup();
}

void beginDialogFooter()
{
    ImGui::Separator();
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
