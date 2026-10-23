#include "gui/dialogs/dialog_contracts.h"
#include "gui/dialogs/dialog_host.h"
#include <catch2/catch_test_macros.hpp>

using namespace uo_mule::gui;

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
    REQUIRE(host.request(std::move(replacement)));
    host.complete(
        {.id = DialogId::fileBrowser, .action = DialogAction::accept, .payload = DialogPathResult{.path = "selected.mul"}});

    const auto result = host.consumeResult();
    REQUIRE(result.has_value());
    REQUIRE(result->id == DialogId::fileBrowser);
    REQUIRE(result->action == DialogAction::accept);
    REQUIRE(std::get<DialogPathResult>(result->payload).path == "selected.mul");
    REQUIRE_FALSE(host.active.has_value());
}

TEST_CASE("dialogHost_Should_PublishTypedCancellation")
{
    DialogHostState host;
    host.activate(makeDialogRequest(DialogId::mapWorkflow));
    host.cancel();

    const auto result = host.consumeResult();
    REQUIRE(result.has_value());
    REQUIRE(result->id == DialogId::mapWorkflow);
    REQUIRE(result->action == DialogAction::cancel);
}
