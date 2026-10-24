#pragma once

#include "gui_model/tools/tool_contracts.h"
#include "uomul/map/map.h"
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

struct ToolPreferences
{
    ToolId activeTool = ToolId::move;
    ToolSelectionGranularity selectionGranularity = ToolSelectionGranularity::tiles;
    bool activeTileSet = false;
    uomul::MapTile activeTile{};
    bool fillIdEnabled = true;
    bool fillZEnabled = true;
    bool replaceIdEnabled = true;
    bool replaceZEnabled = true;
    uomul::MapTile replaceSource{};
    uomul::MapTile replaceTarget{};
    FuzzySelectionOptions fuzzy{};
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
    ToolPreferences tools{};

    [[nodiscard]] bool operator==(const Preferences & other) const noexcept
    {
        return showLogPanel == other.showLogPanel && autoScroll == other.autoScroll &&
               logPanelCollapsed == other.logPanelCollapsed &&
               std::bit_cast<std::uint32_t>(logPanelHeight) == std::bit_cast<std::uint32_t>(other.logPanelHeight) &&
               activeDataProfile == other.activeDataProfile && showWelcomeOnStartup == other.showWelcomeOnStartup &&
               idFormat == other.idFormat && texmapsHideUnused == other.texmapsHideUnused &&
               texmapsTablePresentation == other.texmapsTablePresentation && workspaceFullscreen == other.workspaceFullscreen &&
                 mapViewTileSource == other.mapViewTileSource && tools.activeTool == other.tools.activeTool &&
                 tools.selectionGranularity == other.tools.selectionGranularity && tools.activeTileSet == other.tools.activeTileSet &&
                 tools.activeTile == other.tools.activeTile && tools.fillIdEnabled == other.tools.fillIdEnabled &&
                 tools.fillZEnabled == other.tools.fillZEnabled && tools.replaceIdEnabled == other.tools.replaceIdEnabled &&
                 tools.replaceZEnabled == other.tools.replaceZEnabled && tools.replaceSource == other.tools.replaceSource &&
                 tools.replaceTarget == other.tools.replaceTarget && tools.fuzzy.scope == other.tools.fuzzy.scope &&
                 tools.fuzzy.idEnabled == other.tools.fuzzy.idEnabled && tools.fuzzy.id == other.tools.fuzzy.id &&
                 tools.fuzzy.zEnabled == other.tools.fuzzy.zEnabled && tools.fuzzy.z == other.tools.fuzzy.z &&
                 tools.fuzzy.nameEnabled == other.tools.fuzzy.nameEnabled && tools.fuzzy.name == other.tools.fuzzy.name;
    }
};

[[nodiscard]] Preferences loadPreferences();
void savePreferences(const Preferences & preferences);

} // namespace uo_mule::gui
