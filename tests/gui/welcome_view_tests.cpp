#include "gui/application/welcome_view.h"
#include "gui/dialogs/map_workflow_dialog.h"
#include "gui/workbench/workbench_state.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/persistence/recent_files.h"
#include "imgui_test_support.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <imgui_internal.h>
#include <optional>
#include <string>
#include <system_error>
#include <uomul/map/map.h>
#include <utility>

namespace
{

constexpr std::uint32_t recentMapWidth = 64U;
constexpr std::uint32_t recentMapHeight = 64U;
constexpr std::uint32_t recentAssetItemCount = 512U;

class InvalidWelcomeBanner
{
public:
    InvalidWelcomeBanner() : path_(std::filesystem::temp_directory_path() / "uo-mule-invalid-welcome-banner")
    {
        std::ofstream(path_) << "not an image";
        if (const char * previous = std::getenv("IM_CPP_WELCOME_BANNER_PATH"); previous != nullptr)
        {
            previous_ = previous;
        }
        setenv("IM_CPP_WELCOME_BANNER_PATH", path_.c_str(), 1); // NOLINT(misc-include-cleaner): POSIX environment API.
    }

    ~InvalidWelcomeBanner()
    {
        if (previous_.has_value())
        {
            setenv("IM_CPP_WELCOME_BANNER_PATH", previous_->c_str(), 1); // NOLINT(misc-include-cleaner): POSIX environment API.
        }
        else
        {
            unsetenv("IM_CPP_WELCOME_BANNER_PATH"); // NOLINT(misc-include-cleaner): POSIX environment API.
        }
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    InvalidWelcomeBanner(const InvalidWelcomeBanner &) = delete;
    InvalidWelcomeBanner & operator=(const InvalidWelcomeBanner &) = delete;
    InvalidWelcomeBanner(InvalidWelcomeBanner &&) = delete;
    InvalidWelcomeBanner & operator=(InvalidWelcomeBanner &&) = delete;

private:
    std::filesystem::path path_;
    std::optional<std::string> previous_;
};

ImGuiWindow * findWelcomeRecentRow()
{
    for (ImGuiWindow * window : ImGui::GetCurrentContext()->Windows)
    {
        if (std::string(window->Name).contains("welcome.recent-row"))
        {
            return window;
        }
    }
    return nullptr;
}

void drawWelcomeMouseFrame(uo_mule::gui::WorkbenchState & state, ImGuiIO & io, ImVec2 position, bool pressed)
{
    ImGui::EndFrame();
    io.AddMousePosEvent(position.x, position.y);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, pressed);
    ImGui::NewFrame();
    uo_mule::gui::drawWelcomeView(state);
}

void activateWelcomeRecentWithKeyboard(uo_mule::gui::WorkbenchState & state)
{
    auto & io = ImGui::GetIO();
    io.ConfigFlags = static_cast<ImGuiConfigFlags>(static_cast<unsigned int>(io.ConfigFlags) |
                                                   static_cast<unsigned int>(ImGuiConfigFlags_NavEnableKeyboard));
    uo_mule::gui::drawWelcomeView(state);

    ImGui::EndFrame();
    io.AddKeyEvent(ImGuiKey_Tab, true);
    ImGui::NewFrame();
    uo_mule::gui::drawWelcomeView(state);

    ImGui::EndFrame();
    io.AddKeyEvent(ImGuiKey_Enter, true);
    ImGui::NewFrame();
    uo_mule::gui::drawWelcomeView(state);
}

} // namespace

TEST_CASE("welcomeView_Should_RenderEmptyProfileAndSelectGreeting")
{
    const InvalidWelcomeBanner banner;
    uo_mule::gui::WorkbenchState state;
    state.welcomeGreeting = "Welcome";

    const uo_mule::gui::test::ImGuiFrame frame;
    uo_mule::gui::drawWelcomeView(state);

    REQUIRE(state.welcomeGreetingSelected);
}

TEST_CASE("welcomeView_Should_RenderRecentMapAndAssetRows")
{
    const InvalidWelcomeBanner banner;
    const auto recentPath = std::filesystem::temp_directory_path() / "uo-mule-welcome-recent-files";
    std::error_code error;
    std::filesystem::remove(recentPath, error);
    const auto mapPath = recentPath.parent_path() / "uo-mule-welcome-map.mul";
    const auto radarcolPath = recentPath.parent_path() / "uo-mule-welcome-radarcol.mul";
    std::ofstream(mapPath) << "map";
    std::ofstream(radarcolPath) << "radarcol";
    uo_mule::gui::WorkbenchState state;
    state.session = uo_mule::gui::WorkbenchSession(recentPath);
    state.session.recordRecentFile(mapPath, uomul::Dimensions{.widthTiles = recentMapWidth, .heightTiles = recentMapHeight});
    state.session.recordRecentFile(radarcolPath, uo_mule::gui::DocumentKind::radarcol, {}, recentAssetItemCount);
    state.session.refreshRecentFiles();

    const uo_mule::gui::test::ImGuiFrame frame;
    uo_mule::gui::drawWelcomeView(state);

    REQUIRE(state.session.recentFiles().size() == 2U);
    std::filesystem::remove(recentPath, error);
    std::filesystem::remove(mapPath, error);
    std::filesystem::remove(radarcolPath, error);
}

TEST_CASE("welcomeView_Should_RenderEmptyStateAfterMissingRecentFilesArePruned")
{
    const InvalidWelcomeBanner banner;
    const auto recentPath = std::filesystem::temp_directory_path() / "uo-mule-welcome-pruned-recent-files";
    const auto missingPath = recentPath.parent_path() / "uo-mule-welcome-missing.mul";
    std::error_code error;
    std::filesystem::remove(recentPath, error);
    std::filesystem::remove(missingPath, error);

    uo_mule::gui::RecentFileStore store(recentPath);
    store.record(missingPath, uomul::Dimensions{.widthTiles = recentMapWidth, .heightTiles = recentMapHeight});

    uo_mule::gui::WorkbenchState state;
    state.session = uo_mule::gui::WorkbenchSession(recentPath);
    state.session.refreshRecentFiles();

    const uo_mule::gui::test::ImGuiFrame frame;
    uo_mule::gui::drawWelcomeView(state);

    REQUIRE(state.session.recentFiles().empty());
    REQUIRE(findWelcomeRecentRow() == nullptr);
    std::filesystem::remove(recentPath, error);
}

TEST_CASE("welcomeView_Should_SelectRecentRowWithTab")
{
    const InvalidWelcomeBanner banner;
    uo_mule::gui::WorkbenchState state;
    state.session.recordRecentFile("keyboard-map.mul",
                                   uomul::Dimensions{.widthTiles = recentMapWidth, .heightTiles = recentMapHeight});

    const uo_mule::gui::test::ImGuiFrame frame;
    auto & io = ImGui::GetIO();
    io.ConfigFlags = static_cast<ImGuiConfigFlags>(static_cast<unsigned int>(io.ConfigFlags) |
                                                   static_cast<unsigned int>(ImGuiConfigFlags_NavEnableKeyboard));
    uo_mule::gui::drawWelcomeView(state);

    ImGui::EndFrame();
    io.AddKeyEvent(ImGuiKey_Tab, true);
    ImGui::NewFrame();
    uo_mule::gui::drawWelcomeView(state);

    REQUIRE(state.welcomeRecentKeyboardSelection == 0U);
}

TEST_CASE("welcomeView_Should_DeferRecentMapActivationUntilAfterRendering")
{
    const InvalidWelcomeBanner banner;
    const auto recentPath = std::filesystem::temp_directory_path() / "uo-mule-welcome-deferred-map.mul";
    std::error_code error;
    REQUIRE(
        uomul::MapEditor::createEmpty(recentPath, {.widthTiles = recentMapWidth, .heightTiles = recentMapHeight}).has_value());
    uo_mule::gui::WorkbenchState state;
    state.session.recordRecentFile(recentPath, uomul::Dimensions{.widthTiles = recentMapWidth, .heightTiles = recentMapHeight});
    state.session.refreshRecentFiles();

    const uo_mule::gui::test::ImGuiFrame frame;
    auto & io = ImGui::GetIO();
    io.ConfigFlags = static_cast<ImGuiConfigFlags>(static_cast<unsigned int>(io.ConfigFlags) |
                                                   static_cast<unsigned int>(ImGuiConfigFlags_NavEnableKeyboard));
    uo_mule::gui::drawWelcomeView(state);

    ImGui::EndFrame();
    io.AddKeyEvent(ImGuiKey_Tab, true);
    ImGui::NewFrame();
    uo_mule::gui::drawWelcomeView(state);

    ImGui::EndFrame();
    io.AddKeyEvent(ImGuiKey_Enter, true);
    ImGui::NewFrame();
    uo_mule::gui::drawWelcomeView(state);

    REQUIRE(state.menuBarState.fileDialog.mode == uo_mule::gui::MapFileDialogMode::none);
    REQUIRE(state.deferredWorkflowActions.size() == 1U);

    const auto action = std::move(state.deferredWorkflowActions.front());
    state.deferredWorkflowActions.clear();
    action();

    REQUIRE(state.menuBarState.fileDialog.mode == uo_mule::gui::MapFileDialogMode::openMap);
    REQUIRE(state.menuBarState.fileDialog.openWithoutReview);
    std::filesystem::remove(recentPath, error);
}

TEST_CASE("welcomeView_Should_RequestReviewForRecentMapWithoutDimensions")
{
    const InvalidWelcomeBanner banner;
    const auto recentPath = std::filesystem::temp_directory_path() / "uo-mule-welcome-review-map.mul";
    std::error_code error;
    std::ofstream(recentPath) << "map";
    uo_mule::gui::WorkbenchState state;
    state.session.recordRecentFile(recentPath, uo_mule::gui::DocumentKind::map);
    state.session.refreshRecentFiles();

    const uo_mule::gui::test::ImGuiFrame frame;
    auto & io = ImGui::GetIO();
    io.ConfigFlags = static_cast<ImGuiConfigFlags>(static_cast<unsigned int>(io.ConfigFlags) |
                                                   static_cast<unsigned int>(ImGuiConfigFlags_NavEnableKeyboard));
    uo_mule::gui::drawWelcomeView(state);
    ImGui::EndFrame();
    io.AddKeyEvent(ImGuiKey_Tab, true);
    ImGui::NewFrame();
    uo_mule::gui::drawWelcomeView(state);

    ImGui::EndFrame();
    io.AddKeyEvent(ImGuiKey_Enter, true);
    ImGui::NewFrame();
    uo_mule::gui::drawWelcomeView(state);

    REQUIRE_FALSE(state.deferredWorkflowActions.empty());
    const auto action = std::move(state.deferredWorkflowActions.front());
    state.deferredWorkflowActions.clear();
    action();

    REQUIRE(state.menuBarState.fileDialog.mode == uo_mule::gui::MapFileDialogMode::openMap);
    REQUIRE_FALSE(state.menuBarState.fileDialog.openWithoutReview);
    std::filesystem::remove(recentPath, error);
}

TEST_CASE("welcomeView_Should_ReviewRecentMapWhenCachedDimensionsAreStale")
{
    const InvalidWelcomeBanner banner;
    const auto recentPath = std::filesystem::temp_directory_path() / "uo-mule-welcome-stale-map.mul";
    std::error_code error;
    const uomul::Dimensions cachedDimensions{.widthTiles = recentMapWidth, .heightTiles = recentMapHeight};
    const uomul::Dimensions currentDimensions{.widthTiles = 128U, .heightTiles = 128U};
    std::filesystem::remove(recentPath, error);
    REQUIRE(uomul::MapEditor::createEmpty(recentPath, cachedDimensions).has_value());
    uo_mule::gui::WorkbenchState state;
    state.session.recordRecentFile(recentPath, cachedDimensions);
    state.session.refreshRecentFiles();

    std::filesystem::remove(recentPath, error);
    REQUIRE(uomul::MapEditor::createEmpty(recentPath, currentDimensions).has_value());

    const uo_mule::gui::test::ImGuiFrame frame;
    activateWelcomeRecentWithKeyboard(state);

    REQUIRE_FALSE(state.deferredWorkflowActions.empty());
    const auto action = std::move(state.deferredWorkflowActions.front());
    state.deferredWorkflowActions.clear();
    action();

    REQUIRE(state.menuBarState.fileDialog.mode == uo_mule::gui::MapFileDialogMode::selectMapFacet);
    REQUIRE_FALSE(state.menuBarState.fileDialog.openWithoutReview);
    std::filesystem::remove(recentPath, error);
}

TEST_CASE("welcomeView_Should_DeferRecentMapActivationOnDoubleClick")
{
    const InvalidWelcomeBanner banner;
    const auto recentPath = std::filesystem::temp_directory_path() / "uo-mule-welcome-double-click-map.mul";
    std::error_code error;
    REQUIRE(
        uomul::MapEditor::createEmpty(recentPath, {.widthTiles = recentMapWidth, .heightTiles = recentMapHeight}).has_value());
    uo_mule::gui::WorkbenchState state;
    state.session.recordRecentFile(recentPath, uomul::Dimensions{.widthTiles = recentMapWidth, .heightTiles = recentMapHeight});
    state.session.refreshRecentFiles();

    const uo_mule::gui::test::ImGuiFrame frame;
    uo_mule::gui::drawWelcomeView(state);
    ImGuiWindow * recentRow = findWelcomeRecentRow();
    REQUIRE(recentRow != nullptr);
    const ImVec2 mousePosition(recentRow->Pos.x + (recentRow->Size.x * 0.5F), recentRow->Pos.y + (recentRow->Size.y * 0.5F));
    auto & io = ImGui::GetIO();

    drawWelcomeMouseFrame(state, io, mousePosition, true);
    drawWelcomeMouseFrame(state, io, mousePosition, false);
    drawWelcomeMouseFrame(state, io, mousePosition, true);

    REQUIRE(io.MouseClickedCount[ImGuiMouseButton_Left] == 2);
    REQUIRE(state.deferredWorkflowActions.size() == 1U);
    std::filesystem::remove(recentPath, error);
}
