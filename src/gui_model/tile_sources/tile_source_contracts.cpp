#include "gui_model/tile_sources/tile_source_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/providers/data_providers.h"
#include <cstdint>

namespace uo_mule::gui
{

TileSourceValidation
validateMapTileSource(const TiledataProvider & provider, const DocumentSourceIdentity & source, std::uint16_t id)
{
    if (provider.status().state != ProviderAssetState::available)
    {
        return {.accepted = false, .error = TileSourceValidationError::unavailable, .diagnostic = provider.status().message};
    }
    if (provider.status().sourceIdentity != source)
    {
        return {
            .accepted = false,
            .error = TileSourceValidationError::sourceMismatch,
            .diagnostic = "Tiledata source no longer matches the selected metadata source",
        };
    }
    if (!provider.mapTile(id))
    {
        return {
            .accepted = false,
            .error = TileSourceValidationError::missingRecord,
            .diagnostic = "MapTileRecord is unavailable",
        };
    }
    return {.accepted = true, .error = TileSourceValidationError::unavailable, .diagnostic = {}};
}

TileSourceValidation
validateStaticTileSource(const TiledataProvider & provider, const DocumentSourceIdentity & source, std::uint32_t id)
{
    static_cast<void>(provider);
    static_cast<void>(source);
    static_cast<void>(id);
    return {
        .accepted = false,
        .error = TileSourceValidationError::staticArtSourceDeferred,
        .diagnostic = "Static Tile Source validation requires the Art source contract",
    };
}

} // namespace uo_mule::gui
