#pragma once

#include "gui/map_document.h"
#include <optional>

namespace uo_mule::gui
{

struct MapContextSnapshot
{
    DocumentId documentId = 0;
    bool hasDocument = false;
    std::filesystem::path sourcePath;
    uomul::Dimensions dimensions{};
    bool dirty = false;
    std::optional<uomul::TileCoord> hoveredTile;
    std::optional<uomul::MapTile> hoveredValue;
    std::optional<uomul::TileCoord> selectedTile;
    std::optional<uomul::MapTile> selectedValue;
};

[[nodiscard]] MapContextSnapshot makeMapContextSnapshot(const MapWorkspace & workspace,
                                                        const std::optional<MapDocument> & document);

} // namespace uo_mule::gui
