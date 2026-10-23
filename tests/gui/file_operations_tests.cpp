#include "gui/dialogs/map_workflow_dialog.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <optional>

TEST_CASE("fileBrowserResult_Should_ApplyMetadataSearchFolder")
{
    uo_mule::gui::PreferencesDialogState state;
    const auto root = std::filesystem::temp_directory_path() / "metadata-root";
    const uo_mule::gui::FileBrowserResult result{
        .target = uo_mule::gui::FileBrowserTarget::dataSearchRoot,
        .path = root,
        .mapCandidates = {},
        .assetCandidates = {},
        .assetCandidate = std::nullopt,
    };

    uo_mule::gui::applyFileBrowserResult(state, result);
    REQUIRE(state.workingProfile.sourceRoot == root);
}

TEST_CASE("fileBrowserResult_Should_TransitionMapOpenToFacetSelection")
{
    uo_mule::gui::NewFileDialogState state;
    const auto mapPath = std::filesystem::temp_directory_path() / "map0.mul";
    const uo_mule::gui::FileBrowserResult result{
        .target = uo_mule::gui::FileBrowserTarget::mapOpen,
        .path = mapPath,
        .mapCandidates = {},
        .assetCandidates = {},
        .assetCandidate = std::nullopt,
    };

    uo_mule::gui::applyFileBrowserResult(state, result);
    REQUIRE(state.mode == uo_mule::gui::MapFileDialogMode::selectMapFacet);
    REQUIRE(std::filesystem::path(state.openPath.data()) == mapPath);
}
