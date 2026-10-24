#include "gui/panels/tool_palette_panel.h"
#include "gui/commands/command_registry.h"
#include "gui/presentation/theme/theme.h"
#include "gui/state/workbench_state.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/map_document.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/session/workbench_session.h"
#include "gui_model/tools/tool_contracts.h"
#include "gui_model/workspaces/map_workspace.h"
#include "imgui.h"
#include "uomul/map/map.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace uo_mule::gui
{
namespace
{

constexpr std::size_t fuzzyNameBufferSize = 128U;

ToolContext toolContext(const WorkbenchState & state)
{
    const DocumentRecord * active = state.session.activeDocumentRecord();
    const bool mapOpen =
        active != nullptr && active->summary.identity.kind == DocumentKind::map && state.workspace.reader() != nullptr;
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

void drawTileInput(const WorkbenchState & state,
                   const char * label,
                   const char * idLabel,
                   const char * zLabel,
                   uomul::MapTile tile,
                   const std::function<ToolStateChange(std::int32_t)> & setId,
                   const std::function<ToolStateChange(std::int32_t)> & setZ)
{
    const bool hexadecimalIds = state.preferences.idFormat == IdFormat::hexadecimal;
    const char * idInputFormat = hexadecimalIds ? "%04X" : "%u";
    const ImGuiInputTextFlags idInputFlags =
        hexadecimalIds ? ImGuiInputTextFlags_CharsHexadecimal : ImGuiInputTextFlags_CharsDecimal;
    std::uint16_t id = tile.id;
    int z = static_cast<int>(static_cast<std::int16_t>(tile.z));
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
    ImGui::TextUnformatted("Mode");
    bool idEnabled = state.toolState.fillIdEnabled();
    if (ImGui::Checkbox("ID##fill.mode.id", &idEnabled))
    {
        const auto result = state.toolState.setFillIdEnabled(idEnabled);
        if (!result.accepted)
        {
            drawPlaceholder(result.reason.c_str());
        }
    }
    ImGui::SameLine();
    bool zEnabled = state.toolState.fillZEnabled();
    if (ImGui::Checkbox("Z##fill.mode.z", &zEnabled))
    {
        const auto result = state.toolState.setFillZEnabled(zEnabled);
        if (!result.accepted)
        {
            drawPlaceholder(result.reason.c_str());
        }
    }
    drawPlaceholder("Target uses Active Tile.");
}

void drawReplaceOptions(WorkbenchState & state)
{
    ImGui::TextUnformatted("Mode");
    bool idEnabled = state.toolState.replaceIdEnabled();
    if (ImGui::Checkbox("ID##replace.mode.id", &idEnabled))
    {
        const auto result = state.toolState.setReplaceIdEnabled(idEnabled);
        if (!result.accepted)
        {
            drawPlaceholder(result.reason.c_str());
        }
    }
    ImGui::SameLine();
    bool zEnabled = state.toolState.replaceZEnabled();
    if (ImGui::Checkbox("Z##replace.mode.z", &zEnabled))
    {
        const auto result = state.toolState.setReplaceZEnabled(zEnabled);
        if (!result.accepted)
        {
            drawPlaceholder(result.reason.c_str());
        }
    }
    drawTileInput(
        state,
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
    drawTileInput(
        state,
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

void drawFuzzyOptions(WorkbenchState & state)
{
    const FuzzySelectionOptions & options = state.toolState.fuzzyOptions();
    ImGui::TextUnformatted("Scope");
    if (ImGui::RadioButton("Contiguous##fuzzy.scope.contiguous", options.scope == FuzzyScope::contiguous))
    {
        state.toolState.setFuzzyScope(FuzzyScope::contiguous);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Whole map##fuzzy.scope.whole", options.scope == FuzzyScope::wholeMap))
    {
        state.toolState.setFuzzyScope(FuzzyScope::wholeMap);
    }

    bool idEnabled = options.idEnabled;
    if (ImGui::Checkbox("ID##fuzzy.id.enabled", &idEnabled))
    {
        state.toolState.setFuzzyIdEnabled(idEnabled);
    }
    ImGui::SameLine();
    std::uint16_t id = options.id;
    const bool hexadecimalIds = state.preferences.idFormat == IdFormat::hexadecimal;
    const char * idInputFormat = hexadecimalIds ? "%04X" : "%u";
    const ImGuiInputTextFlags idInputFlags =
        hexadecimalIds ? ImGuiInputTextFlags_CharsHexadecimal : ImGuiInputTextFlags_CharsDecimal;
    if (ImGui::InputScalar("##fuzzy.id.value", ImGuiDataType_U16, &id, nullptr, nullptr, idInputFormat, idInputFlags))
    {
        static_cast<void>(state.toolState.setFuzzyId(static_cast<std::int32_t>(id)));
    }
    ImGui::SameLine();
    drawPickButton(state, "Pick##fuzzy.id.pick", TilePickTarget::fuzzyId);

    bool zEnabled = options.zEnabled;
    if (ImGui::Checkbox("Z##fuzzy.z.enabled", &zEnabled))
    {
        state.toolState.setFuzzyZEnabled(zEnabled);
    }
    ImGui::SameLine();
    int z = static_cast<int>(static_cast<std::int16_t>(options.z));
    if (ImGui::InputInt("##fuzzy.z.value", &z, 0, 0, ImGuiInputTextFlags_CharsDecimal))
    {
        static_cast<void>(state.toolState.setFuzzyZ(z));
    }
    ImGui::SameLine();
    drawPickButton(state, "Pick##fuzzy.z.pick", TilePickTarget::fuzzyZ);

    const bool tiledataAvailable = state.providers.tiledata().status().state == ProviderAssetState::available;
    ImGui::BeginDisabled(!tiledataAvailable);
    bool nameEnabled = options.nameEnabled;
    if (ImGui::Checkbox("Tiledata name##fuzzy.name.enabled", &nameEnabled))
    {
        state.toolState.setFuzzyNameEnabled(nameEnabled);
    }
    ImGui::SameLine();
    std::array<char, fuzzyNameBufferSize> name{};
    const auto copyLength = std::min(options.name.size(), name.size() - 1U);
    std::copy_n(options.name.begin(), static_cast<std::ptrdiff_t>(copyLength), name.begin());
    if (ImGui::InputText("##fuzzy.name.value", name.data(), name.size()))
    {
        state.toolState.setFuzzyName(std::string(name.data()));
    }
    ImGui::SameLine();
    drawPickButton(state, "Pick##fuzzy.name.pick", TilePickTarget::fuzzyName);
    ImGui::EndDisabled();
    if (!tiledataAvailable)
    {
        drawPlaceholder("Tiledata names unavailable.");
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

void drawToolGrid(WorkbenchState & state, const ToolContext & context)
{
    const float availableWidth = std::max(1.0F, ImGui::GetContentRegionAvail().x);
    constexpr float minimumButtonWidth = 104.0F;
    constexpr float buttonSpacing = 8.0F;
    const int columns = std::max(1, static_cast<int>((availableWidth + buttonSpacing) / (minimumButtonWidth + buttonSpacing)));
    const float buttonWidth =
        (availableWidth - (static_cast<float>(columns - 1) * buttonSpacing)) / static_cast<float>(columns);

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
}

void drawSelectionGranularity(WorkbenchState & state, const ToolContext & context)
{
    const ToolId activeTool = state.toolState.activeTool();
    if (activeTool != ToolId::rectangleSelect && activeTool != ToolId::lassoSelect)
    {
        return;
    }

    const bool blocks = state.toolState.selectionGranularity() == ToolSelectionGranularity::blocks;
    ImGui::TextUnformatted("Region");
    if (ImGui::RadioButton("Tiles", !blocks))
    {
        const auto result =
            state.toolState.setSelectionGranularity(ToolSelectionGranularity::tiles, context, state.toolRegistry);
        if (result.accepted && state.workspace.selectionMode() != MapSelectionMode::tiles)
        {
            state.workspace.setSelectionMode(MapSelectionMode::tiles);
        }
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Blocks", blocks))
    {
        const auto result =
            state.toolState.setSelectionGranularity(ToolSelectionGranularity::blocks, context, state.toolRegistry);
        if (result.accepted && state.workspace.selectionMode() != MapSelectionMode::blocks)
        {
            state.workspace.setSelectionMode(MapSelectionMode::blocks);
        }
    }
}

void drawActiveToolOptions(WorkbenchState & state, const ToolContext & context)
{
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
        else if (state.toolState.activeTool() == ToolId::fuzzySelect)
        {
            drawFuzzyOptions(state);
        }
    }

    drawSelectionGranularity(state, context);
}

void drawActiveTile(WorkbenchState & state)
{
    drawSectionHeading("Active Tile");
    const uomul::MapTile activeTile = state.toolState.activeTile().value_or(uomul::MapTile{});
    std::uint16_t tileId = activeTile.id;
    int tileZ = static_cast<int>(static_cast<std::int16_t>(activeTile.z));
    const bool hexadecimalIds = state.preferences.idFormat == IdFormat::hexadecimal;
    const char * idInputFormat = hexadecimalIds ? "%04X" : "%u";
    const ImGuiInputTextFlags idInputFlags =
        hexadecimalIds ? ImGuiInputTextFlags_CharsHexadecimal : ImGuiInputTextFlags_CharsDecimal;
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
    drawToolGrid(state, context);
    ImGui::NewLine();
    drawActiveToolOptions(state, context);
    drawActiveTile(state);
    ImGui::End();
}

} // namespace uo_mule::gui
