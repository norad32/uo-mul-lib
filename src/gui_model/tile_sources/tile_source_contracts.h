#pragma once

#include "gui_model/documents/document_identity.h"
#include "gui_model/providers/data_providers.h"
#include <cstdint>
#include <optional>
#include <string>

namespace uo_mule::gui
{

enum class TileSourceSlot : std::uint8_t
{
    map,
    statics,
};

struct TileSourceSelection
{
    TileSourceSlot slot = TileSourceSlot::map;
    DocumentSourceIdentity source;
    std::uint32_t id = 0;
    std::string name;
    bool unresolved = false;
};

enum class TileSourceValidationError : std::uint8_t
{
    unavailable,
    sourceMismatch,
    missingRecord,
    staticArtSourceDeferred,
};

struct TileSourceValidation
{
    bool accepted = false;
    TileSourceValidationError error = TileSourceValidationError::unavailable;
    std::string diagnostic;
};

[[nodiscard]] TileSourceValidation
validateMapTileSource(const TiledataProvider & provider, const DocumentSourceIdentity & source, std::uint16_t id);
[[nodiscard]] TileSourceValidation
validateStaticTileSource(const TiledataProvider & provider, const DocumentSourceIdentity & source, std::uint32_t id);

struct TileSourceRequest
{
    TileSourceSelection selection;
};

struct TiledataPromotionRequest
{
    DocumentSourceIdentity source;
    std::optional<uomul::TiledataLayout> layout;
};

} // namespace uo_mule::gui
