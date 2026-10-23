#include "gui_model/documents/document_contracts.h"
#include "gui_model/session/workbench_session.h"
#include "gui_model/source/source_identity.h"
#include "gui_model/source/source_locks.h"
#include <catch2/catch_test_macros.hpp>
#include <optional>

using uo_mule::gui::DocumentAccess;
using uo_mule::gui::DocumentKind;
using uo_mule::gui::makeRoleAwareSourceIdentity;
using uo_mule::gui::makeSourceIdentity;
using uo_mule::gui::SourceLockRegistry;
using uo_mule::gui::SourceRole;
using uo_mule::gui::WorkbenchSession;

TEST_CASE("sourceLocks_Should_KeepExactRoleAwareIdentityOwnedByOneDocument")
{
    SourceLockRegistry locks;
    const auto identity = makeRoleAwareSourceIdentity({
        {.role = SourceRole::index, .path = "./texidx.mul"},
        {.role = SourceRole::data, .path = "./texmaps.mul"},
    });

    REQUIRE(locks.acquire(7U, identity));       // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(locks.owner(identity) == 7U);       // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE_FALSE(locks.acquire(8U, identity)); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

TEST_CASE("sourceLocks_Should_AllowAlternatePathsAndReleaseOnClose")
{
    SourceLockRegistry locks;
    const auto first = makeSourceIdentity({"first/map0.mul"});
    const auto alternate = makeSourceIdentity({"second/map0.mul"});

    REQUIRE(locks.acquire(7U, first));     // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(locks.acquire(8U, alternate)); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(locks.owner(alternate) == 8U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    locks.release(7U);                     // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE_FALSE(locks.owner(first).has_value());
    REQUIRE(locks.owner(alternate) == 8U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

TEST_CASE("workbenchSession_Should_FocusDocumentForAlreadyLockedExactSource")
{
    WorkbenchSession session;
    const auto identity = makeSourceIdentity({"locked-radarcol.mul"});
    const auto first = session.openDocument({
        .identity = {.kind = DocumentKind::radarcol, .source = identity},
        .displayName = "Locked",
        .access = DocumentAccess::editable,
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    });
    REQUIRE(first != 0);

    const auto focused = session.openDocument({
        .identity = {.kind = DocumentKind::radarcol, .source = identity},
        .displayName = "Duplicate",
        .access = DocumentAccess::editable,
        .facetKey = std::nullopt,
        .dimensions = std::nullopt,
    });
    REQUIRE(focused == first);
    REQUIRE(session.activeDocumentId() == first);
    REQUIRE(session.documents().size() == 1);
}
