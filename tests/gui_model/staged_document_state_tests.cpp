#include "gui_model/documents/staged_document_state.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <string>

namespace
{

struct TestIntent
{
    std::uint32_t value = 0;
};

} // namespace

TEST_CASE("stagedDocumentState_Should_TrackDirtyChangesAndRevision")
{
    uo_mule::gui::StagedDocumentState state;

    REQUIRE_FALSE(state.dirty());
    REQUIRE(state.revision() == 0U);

    state.setDirty(true);
    state.touch();

    REQUIRE(state.dirty());
    REQUIRE(state.revision() == 1U);
}

TEST_CASE("stagedDocumentState_Should_ResetChangesAndErrors")
{
    uo_mule::gui::StagedDocumentState state;
    state.setDirty(true);
    state.setError("failure");
    state.touch();

    state.resetChanges();
    state.clearError();

    REQUIRE_FALSE(state.dirty());
    REQUIRE(state.error().empty());
    REQUIRE(state.revision() == 2U);
}

TEST_CASE("pendingDocumentIntent_Should_KeepOnlyTheLatestRequest")
{
    uo_mule::gui::PendingDocumentIntent<TestIntent> pending;
    pending.request({.value = 1U});
    pending.request({.value = 2U});

    const auto intent = pending.take();
    const TestIntent received = intent.value_or(TestIntent{});

    REQUIRE(intent.has_value());
    REQUIRE(received.value == 2U);
    REQUIRE_FALSE(pending.take());
}
