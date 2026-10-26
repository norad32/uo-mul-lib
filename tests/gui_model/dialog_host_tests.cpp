#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/dialogs/dialog_host.h"
#include "gui_model/profile/data_profile.h"
#include <catch2/catch_test_macros.hpp>
#include <optional>

using uo_mule::gui::DataFileRole;
using uo_mule::gui::DialogAction;
using uo_mule::gui::DialogHostState;
using uo_mule::gui::DialogId;
using uo_mule::gui::FileBrowserResult;
using uo_mule::gui::FileBrowserTarget;
using uo_mule::gui::makeDialogRequest;

TEST_CASE("dialogContracts_Should_KeepStablePopupIdsSeparateFromDialogKinds")
{
    const auto request = makeDialogRequest(DialogId::preferences);

    REQUIRE(request.id == DialogId::preferences);
    REQUIRE(request.popupId == "###dialog.preferences");
    REQUIRE(request.popupId != "Preferences");
}

TEST_CASE("dialogHost_Should_RejectReplacementWithoutExplicitPermission")
{
    DialogHostState host;
    REQUIRE(host.request(makeDialogRequest(DialogId::preferences)));
    REQUIRE_FALSE(host.request(makeDialogRequest(DialogId::fileBrowser)));
    REQUIRE(host.owns(DialogId::preferences));
}

TEST_CASE("dialogHost_Should_ReplaceAndDeliverTypedResult")
{
    DialogHostState host;
    REQUIRE(host.request(makeDialogRequest(DialogId::preferences)));
    auto replacement = makeDialogRequest(DialogId::fileBrowser);
    replacement.replaceActive = true;
    REQUIRE(host.request(replacement));
    const auto generation = host.active.value_or(uo_mule::gui::DialogRequest{}).generation;
    REQUIRE(host.complete({.id = DialogId::fileBrowser,
                           .action = DialogAction::accept,
                           .payload = FileBrowserResult{.path = "selected.mul",
                                                        .mapCandidates = {},
                                                        .assetCandidates = {},
                                                        .assetCandidate = std::nullopt},
                           .generation = generation}));

    const auto result = host.consumeResult().value_or(uo_mule::gui::DialogResult{});
    REQUIRE(result.id == DialogId::fileBrowser);
    REQUIRE(result.action == DialogAction::accept);
    REQUIRE(std::get<FileBrowserResult>(result.payload).path == "selected.mul");
    REQUIRE_FALSE(host.active.has_value());
}

TEST_CASE("dialogHost_Should_PublishTypedCancellation")
{
    DialogHostState host;
    host.activate(makeDialogRequest(DialogId::mapWorkflow));
    host.cancel();

    const auto result = host.consumeResult().value_or(uo_mule::gui::DialogResult{});
    REQUIRE(result.id == DialogId::mapWorkflow);
    REQUIRE(result.action == DialogAction::cancel);
}

TEST_CASE("dialogHost_Should_RejectResultFromReplacedWorkflow")
{
    DialogHostState host;
    host.activate(makeDialogRequest(DialogId::preferences));
    const auto staleGeneration = host.active.value_or(uo_mule::gui::DialogRequest{}).generation;

    auto replacement = makeDialogRequest(DialogId::fileBrowser);
    replacement.replaceActive = true;
    REQUIRE(host.request(replacement));

    REQUIRE_FALSE(host.complete(
        {.id = DialogId::preferences, .action = DialogAction::accept, .payload = {}, .generation = staleGeneration}));
    REQUIRE_FALSE(host.consumeResult().has_value());
    REQUIRE(host.owns(DialogId::fileBrowser));
}

TEST_CASE("dialogHost_Should_RejectStaleResultForTheActiveDialog")
{
    DialogHostState host;
    REQUIRE(host.request(makeDialogRequest(DialogId::fileBrowser)));
    const auto generation = host.active.value_or(uo_mule::gui::DialogRequest{}).generation;

    REQUIRE_FALSE(host.complete(
        {.id = DialogId::fileBrowser, .action = DialogAction::accept, .payload = {}, .generation = generation - 1U}));
    REQUIRE_FALSE(host.consumeResult().has_value());
    REQUIRE(host.owns(DialogId::fileBrowser));
}

TEST_CASE("dialogHost_Should_PublishTypedFileBrowserAcceptance")
{
    DialogHostState host;
    REQUIRE(host.request(makeDialogRequest(DialogId::fileBrowser)));
    const auto generation = host.active.value_or(uo_mule::gui::DialogRequest{}).generation;
    const FileBrowserResult selected{
        .target = FileBrowserTarget::mapOpen,
        .role = DataFileRole::radarcol,
        .path = "map0.mul",
        .mapCandidates = {},
        .assetCandidates = {},
        .assetCandidate = std::nullopt,
    };

    REQUIRE(host.complete(
        {.id = DialogId::fileBrowser, .action = DialogAction::accept, .payload = selected, .generation = generation}));
    const auto result = host.consumeResult().value_or(uo_mule::gui::DialogResult{});
    REQUIRE(result.action == DialogAction::accept);
    REQUIRE(std::get<FileBrowserResult>(result.payload).path == "map0.mul");
}

TEST_CASE("dialogHost_Should_PublishFileBrowserCancellationOnlyOnce")
{
    DialogHostState host;
    REQUIRE(host.request(makeDialogRequest(DialogId::fileBrowser)));

    host.cancel();
    const auto result = host.consumeResult().value_or(uo_mule::gui::DialogResult{});
    REQUIRE(result.id == DialogId::fileBrowser);
    REQUIRE(result.action == DialogAction::cancel);
    REQUIRE_FALSE(host.consumeResult().has_value());
}
