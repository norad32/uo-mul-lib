#include "gui/application/welcome_view.h"
#include "gui/workbench/workbench_state.h"
#include "gui_model/documents/document_contracts.h"
#include "imgui_test_support.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <uomul/map/map.h>

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
    uo_mule::gui::WorkbenchState state;
    state.session = uo_mule::gui::WorkbenchSession(recentPath);
    state.session.recordRecentFile("custom-map.mul",
                                   uomul::Dimensions{.widthTiles = recentMapWidth, .heightTiles = recentMapHeight});
    state.session.recordRecentFile("radarcol.mul", uo_mule::gui::DocumentKind::radarcol, {}, recentAssetItemCount);
    state.session.refreshRecentFiles();

    const uo_mule::gui::test::ImGuiFrame frame;
    uo_mule::gui::drawWelcomeView(state);

    REQUIRE(state.session.recentFiles().size() == 2U);
    std::filesystem::remove(recentPath, error);
}
