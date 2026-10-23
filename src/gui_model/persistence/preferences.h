#pragma once

#include <bit>
#include <cstdint>
#include <string>

namespace uo_mule::gui
{

enum class IdFormat : unsigned char
{
    hexadecimal,
    decimal,
};

enum class MapViewTileSource : unsigned char
{
    textures,
    radarcol,
};

struct Preferences
{
    bool showLogPanel = false;
    bool autoScroll = true;
    bool logPanelCollapsed = false;
    float logPanelHeight = 0.0F;
    std::string activeDataProfile;
    bool showWelcomeOnStartup = true;
    IdFormat idFormat = IdFormat::hexadecimal;
    bool texmapsHideUnused = true;
    bool texmapsTablePresentation = false;
    bool workspaceFullscreen = false;
    MapViewTileSource mapViewTileSource = MapViewTileSource::textures;

    [[nodiscard]] bool operator==(const Preferences & other) const noexcept
    {
        return showLogPanel == other.showLogPanel && autoScroll == other.autoScroll &&
               logPanelCollapsed == other.logPanelCollapsed &&
               std::bit_cast<std::uint32_t>(logPanelHeight) == std::bit_cast<std::uint32_t>(other.logPanelHeight) &&
               activeDataProfile == other.activeDataProfile && showWelcomeOnStartup == other.showWelcomeOnStartup &&
               idFormat == other.idFormat && texmapsHideUnused == other.texmapsHideUnused &&
                texmapsTablePresentation == other.texmapsTablePresentation && workspaceFullscreen == other.workspaceFullscreen &&
                mapViewTileSource == other.mapViewTileSource;
    }
};

[[nodiscard]] Preferences loadPreferences();
void savePreferences(const Preferences & preferences);

} // namespace uo_mule::gui
