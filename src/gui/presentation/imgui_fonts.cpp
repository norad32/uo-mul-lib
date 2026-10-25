#include "gui/presentation/imgui_fonts.h"
#include "gui/presentation/iconography.h"
#include <cstdint>
#include <filesystem>
#include <imgui.h>
#include <optional>
#include <string>
#include <vector>

namespace uo_mule::gui
{
namespace
{

constexpr float defaultFontSize = 13.0F;
ImFont * largeIconFont = nullptr;

[[nodiscard]] const std::vector<ImWchar> & imguiGlyphRanges()
{
    static_assert(sizeof(ImWchar) >= sizeof(std::uint32_t), "ImGui must use 32-bit glyph codepoints");
    static const std::vector<ImWchar> ranges = []
    {
        std::vector<ImWchar> result;
        for (const std::uint32_t codepoint : imguiIconGlyphRanges())
        {
            result.push_back(static_cast<ImWchar>(codepoint));
        }
        return result;
    }();
    return ranges;
}

} // namespace

ImGuiFontState configureImGuiFonts(const std::optional<std::filesystem::path> & iconFontPath)
{
    largeIconFont = nullptr;
    setIconFontAvailable(false);
    const ImGuiIO & io = ImGui::GetIO();
    ImFontConfig defaultConfig;
    defaultConfig.SizePixels = defaultFontSize;
    const ImFont * defaultFont = io.Fonts->AddFontDefault(&defaultConfig);
    if (defaultFont == nullptr)
    {
        return {.iconFontAvailable = false, .diagnostic = "ImGui default font could not be initialized"};
    }
    if (!iconFontPath.has_value())
    {
        return {.iconFontAvailable = false, .diagnostic = "Material Design Icons font resource was not found"};
    }

    ImFontConfig config;
    config.MergeMode = true;
    config.SizePixels = defaultFontSize;
    const ImFont * mergedFont =
        io.Fonts->AddFontFromFileTTF(iconFontPath->string().c_str(), defaultFontSize, &config, imguiGlyphRanges().data());
    if (mergedFont == nullptr)
    {
        return {
            .iconFontAvailable = false,
            .diagnostic = "Material Design Icons font could not be loaded from " + iconFontPath->string(),
        };
    }

    ImFontConfig largeConfig;
    constexpr float largeFontSize = defaultFontSize * 2.0F;
    largeConfig.SizePixels = largeFontSize;
    largeIconFont =
        io.Fonts->AddFontFromFileTTF(iconFontPath->string().c_str(), largeFontSize, &largeConfig, imguiGlyphRanges().data());
    setIconFontAvailable(true);
    return {.iconFontAvailable = true, .diagnostic = {}};
}

ImFont * iconFontLarge() noexcept
{
    return largeIconFont;
}

} // namespace uo_mule::gui
