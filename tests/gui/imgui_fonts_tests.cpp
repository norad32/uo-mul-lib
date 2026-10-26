#include "gui/presentation/imgui_fonts.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <imgui.h>
#include <optional>

namespace
{

class ImGuiContextScope
{
public:
    ImGuiContextScope() { ImGui::CreateContext(); }
    ~ImGuiContextScope() { ImGui::DestroyContext(); }

    ImGuiContextScope(const ImGuiContextScope &) = delete;
    ImGuiContextScope & operator=(const ImGuiContextScope &) = delete;
    ImGuiContextScope(ImGuiContextScope &&) = delete;
    ImGuiContextScope & operator=(ImGuiContextScope &&) = delete;
};

} // namespace

TEST_CASE("imguiFonts_Should_KeepDefaultFont_If_IconFontIsMissing")
{
    const ImGuiContextScope context;
    const auto state = uo_mule::gui::configureImGuiFonts(std::nullopt);

    REQUIRE_FALSE(state.iconFontAvailable);
    REQUIRE_FALSE(state.diagnostic.empty());
    REQUIRE(ImGui::GetIO().Fonts->Fonts.Size == 1);
    REQUIRE(uo_mule::gui::iconFontLarge() == nullptr);
}

TEST_CASE("imguiFonts_Should_MergeVendoredIconFontIntoDefaultFont")
{
    const ImGuiContextScope context;
    const std::filesystem::path fontPath =
        std::filesystem::path(UOMUL_SOURCE_DIR) / "assets/fonts/materialdesignicons-webfont.ttf";
    const auto state = uo_mule::gui::configureImGuiFonts(fontPath);

    REQUIRE(state.iconFontAvailable);
    REQUIRE(state.diagnostic.empty());
    REQUIRE(ImGui::GetIO().Fonts->Fonts.Size == 2);
    REQUIRE(uo_mule::gui::iconFontLarge() != nullptr);
}
