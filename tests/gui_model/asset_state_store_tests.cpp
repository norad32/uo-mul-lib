#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/asset_state_store.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <filesystem>
#include <system_error>
#include <vector>

namespace
{

class TestDirectory
{
public:
    TestDirectory() : path_(std::filesystem::temp_directory_path() / "uo-mule-asset-state-tests")
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_, error);
    }

    ~TestDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path & path() const noexcept { return path_; }

    TestDirectory(const TestDirectory &) = delete;
    TestDirectory & operator=(const TestDirectory &) = delete;
    TestDirectory(TestDirectory &&) = delete;
    TestDirectory & operator=(TestDirectory &&) = delete;

private:
    std::filesystem::path path_;
};

uo_mule::gui::assets::AssetSourceContext metadataContext()
{
    return {
        .kind = uo_mule::gui::DocumentKind::radarcol,
        .contextKind = uo_mule::gui::assets::AssetSourceContext::Kind::metadataView,
        .source = uo_mule::gui::makeDocumentSourceIdentity({"radarcol.mul"}),
        .contextId = "view.radarcol",
    };
}

} // namespace

TEST_CASE("assetStateStore_Should_RoundTripSemanticContextAndPresentationState")
{
    const TestDirectory directory;
    const auto path = directory.path() / "asset_state.ini";
    uo_mule::gui::assets::AssetWindowState state;
    state.presentation = uo_mule::gui::assets::AssetPresentation::table;
    state.query.filter = "stone";
    state.query.sortKey = "id";
    state.query.sortAscending = false;
    state.detailsCollapsed = true;
    state.scrollPosition = 42.0F; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    state.selection.selected = {{.value = "7"}, {.value = "9"}};
    state.selection.anchor = {.value = "9"};
    state.selection.anchorIndex = 3U;
    const auto context = metadataContext();
    const auto record = uo_mule::gui::assets::captureAssetState("profile-a", "asset-window-a", context, state);

    uo_mule::gui::assets::saveAssetState(path, {record});
    const auto loaded = uo_mule::gui::assets::loadAssetState(path);
    const auto * found = uo_mule::gui::assets::findAssetState(loaded, "profile-a", "asset-window-a", context);

    REQUIRE(found != nullptr);
    REQUIRE(found->context == context);
    REQUIRE(found->state.presentation == uo_mule::gui::assets::AssetPresentation::table);
    REQUIRE(found->state.query == state.query);
    REQUIRE(found->state.detailsCollapsed);
    REQUIRE(std::abs(found->state.scrollPosition - state.scrollPosition) <
            0.001F); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    REQUIRE(found->state.selection.selected == state.selection.selected);
    REQUIRE(found->state.selection.anchor == state.selection.anchor);
    REQUIRE(found->state.selection.anchorIndex == state.selection.anchorIndex);
}

TEST_CASE("assetStateStore_Should_IgnoreUnknownContexts_If_LookingUpState")
{
    const auto context = metadataContext();
    const std::vector<uo_mule::gui::assets::AssetStateRecord> records{
        {.profileIdentity = "profile-a", .assetWindowIdentity = "asset-window-a", .context = context, .state = {}},
    };
    auto unknown = context;
    unknown.contextId = "view.unknown";

    REQUIRE(uo_mule::gui::assets::findAssetState(records, "profile-a", "asset-window-a", unknown) == nullptr);
    REQUIRE(uo_mule::gui::assets::findAssetState(records, "profile-b", "asset-window-a", context) == nullptr);
}

TEST_CASE("assetStateStore_Should_ResetAssetStateWithoutChangingDocuments")
{
    std::vector<uo_mule::gui::assets::AssetStateRecord> records{
        {.profileIdentity = "profile-a", .assetWindowIdentity = "asset-window-a", .context = metadataContext(), .state = {}},
    };

    uo_mule::gui::assets::resetAssetState(records);

    REQUIRE(records.empty());
}
