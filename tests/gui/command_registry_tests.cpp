#include "gui/commands/command_registry.h"
#include "gui/dialogs/map_workflow_dialog.h"
#include "gui/state/workbench_state.h"
#include "gui_model/layout/panel_registry.h"
#include "gui_model/workspaces/map_workspace.h"
#include "gui_model/tools/tool_contracts.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string>

using uo_mule::gui::CommandId;
using uo_mule::gui::WorkbenchState;

TEST_CASE("defaultCommands_Should_ExposeStableIdsAndMetadata")
{
    const WorkbenchState state;

    const auto * open = state.commands.find(CommandId::fileOpen);
    REQUIRE(open != nullptr);
    REQUIRE(open->stableId == "file.open");
    REQUIRE(open->label == "Open...");
    REQUIRE(open->category == "File");
    REQUIRE(open->defaultShortcut == "Ctrl+O");
    REQUIRE_FALSE(open->description.empty());
    REQUIRE(state.commands.find("data.paths") != nullptr);

    const auto * save = state.commands.find(CommandId::fileSave);
    REQUIRE(save != nullptr);
    REQUIRE(save->stableId == "file.save");
    REQUIRE(save->label == "Save");
    REQUIRE(save->defaultShortcut == "Ctrl+S");

    const auto * saveAs = state.commands.find(CommandId::fileSaveAs);
    REQUIRE(saveAs != nullptr);
    REQUIRE(saveAs->stableId == "file.save-as");
    REQUIRE(saveAs->label == "Save As...");
    REQUIRE(saveAs->defaultShortcut == "Ctrl+Shift+S");
}

TEST_CASE("commandRegistry_Should_CentralizeEnablementAndDispatch")
{
    WorkbenchState state;

    REQUIRE(state.commands.execute(CommandId::fileOpen, state));
    REQUIRE(state.menuBarState.fileDialog.mode == uo_mule::gui::MapFileDialogMode::openMap);

    REQUIRE(state.commands.execute(CommandId::viewCombined, state));
    REQUIRE(state.workspace.displayMode() == uo_mule::gui::MapDisplayMode::combined);

    REQUIRE(state.commands.execute(CommandId::panelTexmaps, state));
    REQUIRE_FALSE(state.panelRegistry.find(uo_mule::gui::PanelId::texmaps)->visible);
    REQUIRE(state.commands.execute(CommandId::panelTexmaps, state));
    REQUIRE(state.panelRegistry.find(uo_mule::gui::PanelId::texmaps)->visible);

    REQUIRE(state.commands.execute(CommandId::panelToolPalette, state));
    REQUIRE_FALSE(state.panelRegistry.find(uo_mule::gui::PanelId::toolPalette)->visible);
    REQUIRE(state.commands.execute(CommandId::panelToolPalette, state));
    REQUIRE(state.panelRegistry.find(uo_mule::gui::PanelId::toolPalette)->visible);
}

TEST_CASE("commandRegistry_Should_DispatchRegisteredShortcuts")
{
    WorkbenchState state;

    REQUIRE(state.commands.executeShortcut("Ctrl+N", state));
    REQUIRE(state.menuBarState.fileDialog.mode == uo_mule::gui::MapFileDialogMode::newType);
    REQUIRE_FALSE(state.commands.executeShortcut("Ctrl+P", state));
}

TEST_CASE("toolCommands_Should_ExposeSharedActivationMetadata")
{
    const WorkbenchState state;

    const auto * move = state.commands.find(CommandId::toolMove);
    REQUIRE(move != nullptr);
    REQUIRE(move->stableId == "tool.move");
    REQUIRE(move->defaultShortcut == "M");
    REQUIRE(state.commands.find(CommandId::toolReplace)->defaultShortcut == "Shift+R");
}

TEST_CASE("toolCommands_Should_ActivateThroughRegistryWhenMapIsOpen")
{
    WorkbenchState state;
    const uomul::Dimensions dimensions{.widthTiles = 8, .heightTiles = 8};
    const auto mapPath = std::filesystem::temp_directory_path() / "uo-mule-tool-command-map.mul";
    std::filesystem::remove(mapPath);
    REQUIRE(state.workspace.createEmptyMap(mapPath, dimensions));
    state.session.syncDocument(state.workspace);

    REQUIRE(state.commands.execute(CommandId::toolRectangleSelect, state));
    REQUIRE(state.toolState.activeTool() == uo_mule::gui::ToolId::rectangleSelect);
    REQUIRE(state.commands.executeShortcut("M", state));
    REQUIRE(state.toolState.activeTool() == uo_mule::gui::ToolId::move);

    static_cast<void>(state.workspace.closeMap());
    std::filesystem::remove(mapPath);
}

TEST_CASE("toolCommands_Should_RejectEditingToolWithoutEditableMap")
{
    WorkbenchState state;

    REQUIRE_FALSE(state.commands.execute(CommandId::toolFill, state));
}

TEST_CASE("selectionCommands_Should_ExposeStandardSelectionShortcuts")
{
    const WorkbenchState state;

    REQUIRE(state.commands.find(CommandId::selectionAll)->defaultShortcut == "Ctrl+A");
    REQUIRE(state.commands.find(CommandId::selectionNone)->defaultShortcut == "Ctrl+Shift+A");
    REQUIRE(state.commands.find(CommandId::selectionInvert)->defaultShortcut == "Ctrl+I");
}

TEST_CASE("selectionCommands_Should_OperateOnTheActiveMapMask")
{
    WorkbenchState state;
    const uomul::Dimensions dimensions{.widthTiles = 8, .heightTiles = 8};
    const auto mapPath = std::filesystem::temp_directory_path() / "uo-mule-selection-command-map.mul";
    std::filesystem::remove(mapPath);
    REQUIRE(state.workspace.createEmptyMap(mapPath, dimensions));
    state.session.syncDocument(state.workspace);

    REQUIRE(state.commands.execute(CommandId::selectionAll, state));
    REQUIRE(state.workspace.selectionMask().size() == 64);
    REQUIRE(state.commands.execute(CommandId::selectionInvert, state));
    REQUIRE(state.workspace.selectionMask().empty());
    REQUIRE(state.commands.execute(CommandId::selectionAll, state));
    REQUIRE(state.commands.execute(CommandId::selectionNone, state));
    REQUIRE(state.workspace.selectionMask().empty());

    static_cast<void>(state.workspace.closeMap());
    std::filesystem::remove(mapPath);
}

TEST_CASE("defaultCommands_Should_ExposeSeparateTileAndBlockNavigationCommands")
{
    const WorkbenchState state;

    const auto * tile = state.commands.find(CommandId::mapGoToCoordinate);
    REQUIRE(tile != nullptr);
    REQUIRE(tile->label == "Go to Coordinate...");
    REQUIRE(tile->defaultShortcut == "Ctrl+G");

    const auto * block = state.commands.find(CommandId::mapGoToBlockCoordinate);
    REQUIRE(block != nullptr);
    REQUIRE(block->label == "Go to Block...");
    REQUIRE(block->defaultShortcut.empty());
}

TEST_CASE("copyLocationCommands_Should_RemainDisabledWithoutActiveLocation")
{
    const WorkbenchState state;

    REQUIRE_FALSE(state.commands.availability(CommandId::mapCopyCoordinate, state).enabled);
    REQUIRE_FALSE(state.commands.availability(CommandId::mapCopyTileData, state).enabled);
}
