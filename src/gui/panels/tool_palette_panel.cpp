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
#include <functional>
#include <optional>
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

void drawTileInput(WorkbenchState & state,
                   const char * label,
                   const char * idLabel,
                   const char * zLabel,
                   uomul::MapTile tile,
                   const std::function<ToolStateChange(std::int32_t)> & setId,
                   const std::function<ToolStateChange(std::int32_t)> & setZ)
{
    const bool hexadecimalIds = state.preferences.idFormat == IdFormat::hexadecimal;
    const char * idInputFormat = hexadecimalIds ? "%04X" : "%u";
    const ImGuiInputTextFlags idInputFlags = hexadecimalIds ? ImGuiInputTextFlags_CharsHexadecimal
                                                            : ImGuiInputTextFlags_CharsDecimal;
    std::uint16_t id = tile.id;
    int z = static_cast<int>(tile.z);
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    if (ImGui::InputScalar(idLabel, ImGuiDataType_U16, &id, nullptr, nullptr, idInputFormat, idInputFlags))
    {
        const auto result = setId(static_cast<std::int32_t>(id));
        if (!result.accepted)
        {
            ImGui::TextColored(theme::imguiColor(theme::warning), "%s", result.reason.c_str());
        }
    }
    if (ImGui::InputInt(zLabel, &z, 0, 0, ImGuiInputTextFlags_CharsDecimal))
    {
        const auto result = setZ(z);
        if (!result.accepted)
        {
            ImGui::TextColored(theme::imguiColor(theme::warning), "%s", result.reason.c_str());
        }
    }
    ImGui::PopID();
}

void drawPickButton(WorkbenchState & state, const char * label, TilePickTarget target)
{
    if (ImGui::Button(label))
    {
        state.toolState.requestTilePick(target);
    }
}

void drawFillOptions(WorkbenchState & state)
{
    ImGui::TextUnformatted("Fill mode");
    const FillMode mode = state.toolState.fillMode();
    if (ImGui::RadioButton("ID##fill.mode.id", mode == FillMode::id))
    {
        state.toolState.setFillMode(FillMode::id);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Z##fill.mode.z", mode == FillMode::z))
    {
        state.toolState.setFillMode(FillMode::z);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("ID + Z##fill.mode.id-z", mode == FillMode::idAndZ))
    {
        state.toolState.setFillMode(FillMode::idAndZ);
    }
    drawPlaceholder("Target uses Active Tile.");
}

void drawReplaceOptions(WorkbenchState & state)
{
    ImGui::TextUnformatted("Replace mode");
    const ReplaceMode mode = state.toolState.replaceMode();
    if (ImGui::RadioButton("ID##replace.mode.id", mode == ReplaceMode::id))
    {
        state.toolState.setReplaceMode(ReplaceMode::id);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Z##replace.mode.z", mode == ReplaceMode::z))
    {
        state.toolState.setReplaceMode(ReplaceMode::z);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("ID + Z##replace.mode.id-z", mode == ReplaceMode::idAndZ))
    {
        state.toolState.setReplaceMode(ReplaceMode::idAndZ);
    }
    drawTileInput(state,
                  "Source",
                  "ID##replace.source.id",
                  "Z##replace.source.z",
                  state.toolState.replaceSource(),
                  [&state](std::int32_t id) { return state.toolState.setReplaceSourceId(id); },
                  [&state](std::int32_t z) { return state.toolState.setReplaceSourceZ(z); });
    drawPickButton(state, "Pick Source##replace.pick-source", TilePickTarget::replaceSource);
    ImGui::SameLine();
    if (ImGui::Button("From Active Tile##replace.source.active"))
    {
        const auto result = state.toolState.setReplaceSourceFromActiveTile();
        if (!result.accepted)
        {
            drawPlaceholder(result.reason.c_str());
        }
    }
    drawTileInput(state,
                  "Target",
                  "ID##replace.target.id",
                  "Z##replace.target.z",
                  state.toolState.replaceTarget(),
                  [&state](std::int32_t id) { return state.toolState.setReplaceTargetId(id); },
                  [&state](std::int32_t z) { return state.toolState.setReplaceTargetZ(z); });
    drawPickButton(state, "Pick Target##replace.pick-target", TilePickTarget::replaceTarget);
    ImGui::SameLine();
    if (ImGui::Button("From Active Tile##replace.target.active"))
    {
        const auto result = state.toolState.setReplaceTargetFromActiveTile();
        if (!result.accepted)
        {
            drawPlaceholder(result.reason.c_str());
        }
    }
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
        if (state.toolState.activeTool() == ToolId::fill)
        {
            drawFillOptions(state);
        }
        else if (state.toolState.activeTool() == ToolId::replace)
        {
            drawReplaceOptions(state);
        }
        else
        {
            drawPlaceholder("Tool-specific options will appear here.");
        }
    }

    if (state.toolState.activeTool() == ToolId::rectangleSelect || state.toolState.activeTool() == ToolId::lassoSelect)
    {
        const bool blocks = state.toolState.selectionGranularity() == ToolSelectionGranularity::blocks;
        ImGui::TextUnformatted("Selection granularity");
        if (ImGui::RadioButton("Tiles", !blocks))
        {
            const auto result = state.toolState.setSelectionGranularity(ToolSelectionGranularity::tiles,
                                                                          context,
                                                                          state.toolRegistry);
            if (result.accepted)
            {
                if (state.workspace.selectionMode() != MapSelectionMode::tiles)
                {
                    state.workspace.setSelectionMode(MapSelectionMode::tiles);
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Blocks", blocks))
        {
            const auto result = state.toolState.setSelectionGranularity(ToolSelectionGranularity::blocks,
                                                                          context,
                                                                          state.toolRegistry);
            if (result.accepted)
            {
                if (state.workspace.selectionMode() != MapSelectionMode::blocks)
                {
                    state.workspace.setSelectionMode(MapSelectionMode::blocks);
                }
            }
        }
    }

    drawSectionHeading("Active Tile");
    const uomul::MapTile activeTile = state.toolState.activeTile().value_or(uomul::MapTile{});
    std::uint16_t tileId = activeTile.id;
    int tileZ = static_cast<int>(activeTile.z);
    const bool hexadecimalIds = state.preferences.idFormat == IdFormat::hexadecimal;
    const char * idInputFormat = hexadecimalIds ? "%04X" : "%u";
    const ImGuiInputTextFlags idInputFlags = hexadecimalIds ? ImGuiInputTextFlags_CharsHexadecimal
                                                            : ImGuiInputTextFlags_CharsDecimal;
    if (ImGui::InputScalar("ID", ImGuiDataType_U16, &tileId, nullptr, nullptr, idInputFormat, idInputFlags))
    {
        const auto result = state.toolState.setActiveTileId(static_cast<std::int32_t>(tileId));
        if (!result.accepted)
        {
            ImGui::TextColored(theme::imguiColor(theme::warning), "%s", result.reason.c_str());
        }
    }
    if (ImGui::InputInt("Z", &tileZ, 0, 0, ImGuiInputTextFlags_CharsDecimal))
    {
        const auto result = state.toolState.setActiveTileZ(tileZ);
        if (!result.accepted)
        {
            ImGui::TextColored(theme::imguiColor(theme::warning), "%s", result.reason.c_str());
        }
    }
    if (ImGui::Button("Pick"))
    {
        state.toolState.requestTilePick(TilePickTarget::activeTile);
    }
    if (state.toolState.tilePickRequested())
    {
        drawPlaceholder("Click a tile or item in Canvas, Radarcol, Texmaps, or Tiledata.");
    }
    ImGui::End();
}

} // namespace uo_mule::gui
