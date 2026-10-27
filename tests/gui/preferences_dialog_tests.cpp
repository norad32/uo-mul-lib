#include "gui/dialogs/file_browser_dialog.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui/workbench/workbench_state.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/dialogs/dialog_host.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/profile/data_profile.h"
#include "imgui_test_support.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>

using uo_mule::gui::applyFileBrowserResult;
using uo_mule::gui::DataFileRole;
using uo_mule::gui::DataProfile;
using uo_mule::gui::DialogHostState;
using uo_mule::gui::DialogId;
using uo_mule::gui::drawPreferencesDialog;
using uo_mule::gui::FileBrowserResult;
using uo_mule::gui::FileBrowserState;
using uo_mule::gui::FileBrowserTarget;
using uo_mule::gui::IdFormat;
using uo_mule::gui::knownDataFileRoles;
using uo_mule::gui::MapViewTileSource;
using uo_mule::gui::Preferences;
using uo_mule::gui::PreferencesDialogState;
using uo_mule::gui::requestPreferencesDialog;
using uo_mule::gui::WorkbenchState;
using uo_mule::gui::test::ImGuiFrame;

namespace
{

class TemporaryProfileDirectory
{
public:
    TemporaryProfileDirectory() : path_(std::filesystem::temp_directory_path() / "uo-mule-preferences-dialog-tests")
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_, error);
    }

    ~TemporaryProfileDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryProfileDirectory(const TemporaryProfileDirectory &) = delete;
    TemporaryProfileDirectory & operator=(const TemporaryProfileDirectory &) = delete;
    TemporaryProfileDirectory(TemporaryProfileDirectory &&) = delete;
    TemporaryProfileDirectory & operator=(TemporaryProfileDirectory &&) = delete;

    [[nodiscard]] const std::filesystem::path & path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("preferencesDialog_Should_InitializeProfileFieldsAndMetadataOverrides")
{
    DataProfile profile;
    profile.name = "Test profile";
    profile.sourceRoot = "/tmp/data";
    profile.files[DataFileRole::radarcol] = {.path = "radarcol.mul", .userOverride = true};
    Preferences preferences;
    preferences.showWelcomeOnStartup = false;
    preferences.idFormat = IdFormat::decimal;
    preferences.mapViewTileSource = MapViewTileSource::radarcol;

    PreferencesDialogState state;
    requestPreferencesDialog(state, profile, preferences);

    REQUIRE(state.visible);
    REQUIRE(std::string(state.profileName.data()) == "Test profile");
    REQUIRE(std::string(state.sourceRoot.data()) == "/tmp/data");
    REQUIRE_FALSE(state.showWelcomeOnStartup);
    REQUIRE(state.idFormat == IdFormat::decimal);
    REQUIRE(state.mapViewTileSource == MapViewTileSource::radarcol);
    REQUIRE(state.overrides.size() == knownDataFileRoles().size());
    REQUIRE(state.metadataOverrides.at(DataFileRole::radarcol));
    REQUIRE(state.validation.find(DataFileRole::tiledata) != nullptr);
}

TEST_CASE("preferencesDialog_Should_ActivateDialogHostWhenRequested")
{
    WorkbenchState state;
    requestPreferencesDialog(state.menuBarState.preferencesDialog, state.activeProfile, state.preferences);

    REQUIRE(state.dialogHost.owns(DialogId::preferences));
}

TEST_CASE("preferencesDialog_Should_ApplySearchRootAndOverrideResults")
{
    const TemporaryProfileDirectory directory;
    PreferencesDialogState state;
    const DataProfile profile{};
    const Preferences preferences{};
    requestPreferencesDialog(state, profile, preferences);

    applyFileBrowserResult(state,
                           FileBrowserResult{
                               .target = FileBrowserTarget::chooseDataSearchRoot,
                               .path = directory.path(),
                               .mapCandidates = {},
                               .assetCandidates = {},
                               .assetCandidate = std::nullopt,
                           });
    REQUIRE(state.workingProfile.sourceRoot == directory.path());

    const auto overridePath = directory.path() / "radarcol.mul";
    std::ofstream(overridePath) << "invalid";
    applyFileBrowserResult(state,
                           FileBrowserResult{
                               .target = FileBrowserTarget::chooseDataOverride,
                               .role = DataFileRole::radarcol,
                               .path = overridePath,
                               .mapCandidates = {},
                               .assetCandidates = {},
                               .assetCandidate = std::nullopt,
                           });
    REQUIRE(state.metadataOverrides.at(DataFileRole::radarcol));
    REQUIRE(state.overrides.at(DataFileRole::radarcol).data() == overridePath.string());

    applyFileBrowserResult(state,
                           FileBrowserResult{
                               .target = FileBrowserTarget::openGeneric,
                               .path = "ignored.mul",
                               .mapCandidates = {},
                               .assetCandidates = {},
                               .assetCandidate = std::nullopt,
                           });
    REQUIRE(state.workingProfile.sourceRoot == directory.path());
}

TEST_CASE("preferencesDialog_Should_RenderWithoutSelection")
{
    PreferencesDialogState state;
    FileBrowserState browser;
    const DataProfile profile{};
    const Preferences preferences{};
    requestPreferencesDialog(state, profile, preferences);
    const ImGuiFrame frame;

    REQUIRE_FALSE(drawPreferencesDialog(state, browser).has_value());
    REQUIRE(state.visible);
}

TEST_CASE("preferencesDialog_Should_ReturnNothingWhenClosed")
{
    PreferencesDialogState state;
    FileBrowserState browser;
    const ImGuiFrame frame;

    REQUIRE_FALSE(drawPreferencesDialog(state, browser).has_value());
}

TEST_CASE("preferencesDialog_Should_ClearCloseRequestAfterClosingPopup")
{
    PreferencesDialogState state;
    FileBrowserState browser;
    state.closeRequested = true;
    const ImGuiFrame frame;

    REQUIRE_FALSE(drawPreferencesDialog(state, browser).has_value());
    REQUIRE_FALSE(state.closeRequested);
}

TEST_CASE("preferencesDialog_Should_CancelHostAfterTitleBarClose")
{
    PreferencesDialogState state;
    FileBrowserState browser;
    DialogHostState host;
    host.activate(makeDialogRequest(DialogId::preferences));
    state.dialogHost = &host;
    state.closeRequested = true;
    const ImGuiFrame frame;

    REQUIRE_FALSE(drawPreferencesDialog(state, browser).has_value());
    const auto result = host.consumeResult().value_or(uo_mule::gui::DialogResult{});
    REQUIRE(result.id == DialogId::preferences);
    REQUIRE(result.action == uo_mule::gui::DialogAction::cancel);
    REQUIRE_FALSE(host.active.has_value());
    REQUIRE_FALSE(state.closeRequested);
}
