#include "gui/application/workbench_state.h"
#include "gui/commands/command_registry.h"
#include "gui/dialogs/new_file_dialog.h"
#include "gui/documents/map_workspace.h"
#include "gui/views/panel_registry.h"
#include <catch2/catch_test_macros.hpp>
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
}

TEST_CASE("commandRegistry_Should_DispatchRegisteredShortcuts")
{
    WorkbenchState state;

    REQUIRE(state.commands.executeShortcut("Ctrl+N", state));
    REQUIRE(state.menuBarState.fileDialog.mode == uo_mule::gui::MapFileDialogMode::newType);
    REQUIRE_FALSE(state.commands.executeShortcut("Ctrl+P", state));
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
