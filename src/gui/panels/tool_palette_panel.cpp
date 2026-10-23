#include "gui/panels/tool_palette_panel.h"
#include "gui/commands/command_registry.h"
#include "gui/presentation/theme/theme.h"
#include "gui/state/workbench_state.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/session/workbench_session.h"
#include "gui_model/tools/tool_contracts.h"
#include "imgui.h"
#include <algorithm>
#include <cstddef>
#include <string>

namespace uo_mule::gui
{
namespace
{

ToolContext toolContext(const WorkbenchState & state)
{
    const DocumentRecord * active = state.session.activeDocumentRecord();
    const bool mapOpen = active != nullptr && active->summary.identity.kind == DocumentKind::map &&
                         state.workspace.reader() != nullptr;
    const bool mapEditable = mapOpen && active->summary.access == DocumentAccess::editable;
    return {
        .mapDocumentOpen = mapOpen,
        .mapEditable = mapEditable,
        .selectionGranularity = state.toolState.selectionGranularity(),
    };
}

void drawSectionHeading(const char * label)
{
    ImGui::SeparatorText(label);
}

void drawPlaceholder(const char * text)
{
    ImGui::TextColored(theme::imguiColor(theme::mutedText), "%s", text);
}

void drawToolButton(WorkbenchState & state, const ToolDescriptor & tool, const ToolContext & context, float width)
{
    const ToolAvailability availability = tool.availability(context);
    const bool active = state.toolState.activeTool() == tool.id;
    if (active)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, theme::imguiColor(theme::activeSurface));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::imguiColor(theme::activeSurface));
    }

    ImGui::PushID(static_cast<int>(tool.id));
    const bool clicked = ImGui::Button(tool.label.c_str(), ImVec2(width, 44.0F));
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip("%s\nShortcut: %s\n%s",
                          tool.label.c_str(),
                          tool.defaultShortcut.c_str(),
                          availability.enabled ? tool.description.c_str() : availability.disabledReason.c_str());
    }
    ImGui::PopID();

    if (clicked && availability.enabled)
    {
        static_cast<void>(state.commands.execute(toolCommandId(tool.id), state));
    }

    if (active)
    {
        ImGui::PopStyleColor(2);
    }
}

} // namespace

void drawToolPalettePanel(WorkbenchState & state, bool & visible)
{
    if (!ImGui::Begin("Tool Palette###view.tool-palette", &visible))
    {
        ImGui::End();
        return;
    }

    const ToolContext context = toolContext(state);
    const float availableWidth = std::max(1.0F, ImGui::GetContentRegionAvail().x);
    constexpr float minimumButtonWidth = 104.0F;
    constexpr float buttonSpacing = 8.0F;
    const int columns = std::max(1, static_cast<int>((availableWidth + buttonSpacing) /
                                                     (minimumButtonWidth + buttonSpacing)));
    const float buttonWidth = (availableWidth - (static_cast<float>(columns - 1) * buttonSpacing)) /
                              static_cast<float>(columns);

    drawSectionHeading("Tools");
    std::size_t index = 0;
    for (const ToolDescriptor & tool : state.toolRegistry.tools())
    {
        drawToolButton(state, tool, context, buttonWidth);
        index++;
        if (index % static_cast<std::size_t>(columns) != 0U)
        {
            ImGui::SameLine();
        }
    }

    drawSectionHeading("Tool Options");
    const ToolDescriptor * activeTool = state.toolRegistry.find(state.toolState.activeTool());
    if (activeTool != nullptr)
    {
        ImGui::TextUnformatted(activeTool->label.c_str());
        ImGui::SameLine();
        ImGui::TextColored(theme::imguiColor(theme::mutedText), "(%s)", activeTool->defaultShortcut.c_str());
        drawPlaceholder("Tool-specific options will appear here.");
    }

    drawSectionHeading("Active Tile");
    drawPlaceholder("ID and Z controls will appear here.");
    ImGui::End();
}

} // namespace uo_mule::gui
