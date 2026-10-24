#include "gui/panels/tiledata_panel.h"
#include "gui/presentation/strings/ui_strings.h"
#include "gui/presentation/theme/theme.h"
#include "gui/widgets/asset_window.h"
#include "gui/widgets/texture_preview.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/tiledata_document.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/color_conversion.h"
#include "gui_model/providers/data_providers.h"
#include "gui_model/source/source_identity.h"
#include "uomul/radarcol/radarcol.h"
#include "uomul/tiledata/tiledata.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <imgui.h>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace uo_mule::gui
{
namespace
{

constexpr std::size_t assetCellWidth = 72U;
constexpr std::size_t assetCellHeight = 72U;
constexpr float previewSize = 64.0F;
constexpr std::uint32_t radarStaticOffset = uomul::radarStaticColorOffset;

[[nodiscard]] std::string rawFlagsText(uomul::TiledataFlag flags)
{
    return std::format("0x{:016X}", static_cast<std::uint64_t>(flags));
}

[[nodiscard]] std::string flagsText(uomul::TiledataFlag flags)
{
    static constexpr std::array flagsByName{
        std::pair{uomul::TiledataFlag::background, "background"},
        std::pair{uomul::TiledataFlag::weapon, "weapon"},
        std::pair{uomul::TiledataFlag::transparent, "transparent"},
        std::pair{uomul::TiledataFlag::translucent, "translucent"},
        std::pair{uomul::TiledataFlag::wall, "wall"},
        std::pair{uomul::TiledataFlag::damaging, "damaging"},
        std::pair{uomul::TiledataFlag::impassable, "impassable"},
        std::pair{uomul::TiledataFlag::wet, "wet"},
        std::pair{uomul::TiledataFlag::surface, "surface"},
        std::pair{uomul::TiledataFlag::bridge, "bridge"},
        std::pair{uomul::TiledataFlag::generic, "generic"},
        std::pair{uomul::TiledataFlag::window, "window"},
        std::pair{uomul::TiledataFlag::noShoot, "no shoot"},
        std::pair{uomul::TiledataFlag::foliage, "foliage"},
        std::pair{uomul::TiledataFlag::partialHue, "partial hue"},
        std::pair{uomul::TiledataFlag::noHouse, "no house"},
        std::pair{uomul::TiledataFlag::map, "map"},
        std::pair{uomul::TiledataFlag::container, "container"},
        std::pair{uomul::TiledataFlag::wearable, "wearable"},
        std::pair{uomul::TiledataFlag::lightSource, "light source"},
        std::pair{uomul::TiledataFlag::animation, "animation"},
        std::pair{uomul::TiledataFlag::hoverOver, "hover over"},
        std::pair{uomul::TiledataFlag::noDiagonal, "no diagonal"},
        std::pair{uomul::TiledataFlag::armor, "armor"},
        std::pair{uomul::TiledataFlag::roof, "roof"},
        std::pair{uomul::TiledataFlag::door, "door"},
        std::pair{uomul::TiledataFlag::stairBack, "stair back"},
        std::pair{uomul::TiledataFlag::stairRight, "stair right"},
        std::pair{uomul::TiledataFlag::alphaBlend, "alpha blend"},
        std::pair{uomul::TiledataFlag::useNewArt, "use new art"},
        std::pair{uomul::TiledataFlag::artUsed, "art used"},
        std::pair{uomul::TiledataFlag::noShadow, "no shadow"},
        std::pair{uomul::TiledataFlag::pixelBleed, "pixel bleed"},
        std::pair{uomul::TiledataFlag::playAnimOnce, "play animation once"},
        std::pair{uomul::TiledataFlag::multiMovable, "multi movable"},
    };
    std::string result;
    for (const auto & [flag, name] : flagsByName)
    {
        if (!uomul::hasTiledataFlag(flags, flag))
        {
            continue;
        }
        if (!result.empty())
        {
            result += ", ";
        }
        result += name;
    }
    return result.empty() ? "none" : result;
}

[[nodiscard]] std::optional<std::uint32_t> idOf(const assets::AssetItemId & item)
{
    return assets::itemIndex(item);
}

[[nodiscard]] std::size_t itemCount(TiledataBrowserKind kind)
{
    return kind == TiledataBrowserKind::map ? uomul::tiledataMapTileCount : uomul::tiledataStaticTileCount;
}

[[nodiscard]] const ProviderTexture *
texturePreview(TiledataPanelState & state, const TextureProvider & textures, std::uint32_t id)
{
    const auto [iterator, inserted] = state.texturePreviews.try_emplace(id);
    if (inserted)
    {
        if (const auto texture = textures.texture(id))
        {
            iterator->second = *texture;
        }
        else
        {
            state.texturePreviews.erase(iterator);
            return nullptr;
        }
    }
    return &iterator->second;
}

[[nodiscard]] std::vector<std::string> mapValues(const ProviderMapTile & tile, IdFormat idFormat)
{
    return {
        "ID: " + strings::formatId(tile.id, idFormat),
        "Name: " + tile.record.name,
        "Flags: " + flagsText(tile.record.flags),
        "Texture: " + strings::formatId(tile.record.textureId, idFormat),
    };
}

[[nodiscard]] std::vector<std::string> staticValues(const ProviderStaticTile & tile, IdFormat idFormat)
{
    return {
        "ID: " + strings::formatId(tile.id, idFormat),
        "Name: " + tile.record.name,
        "Flags: " + flagsText(tile.record.flags),
        "Weight: " + std::to_string(tile.record.weight),
        "Quality: " + std::to_string(tile.record.quality),
        "Animation: " + std::to_string(tile.record.animation),
        "Hue: " + std::to_string(tile.record.hue),
        "Height: " + std::to_string(tile.record.height),
    };
}

// The adapter contract requires composing all virtualization callbacks in one descriptor.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
[[nodiscard]] assets::AssetAdapter makeAdapter(TiledataPanelState & state,
                                               const TiledataProvider & provider,
                                               const TextureProvider & textures,
                                               const RadarColorProvider & radarColors,
                                               IdFormat idFormat)
{
    const auto status = provider.status();
    if (state.texturePreviewRevision != textures.status().revision)
    {
        state.texturePreviews.clear();
        state.texturePreviewRevision = textures.status().revision;
    }
    assets::AssetAdapter adapter;
    adapter.descriptor.viewId = "view.tiledata";
    adapter.descriptor.title = "Tiledata";
    adapter.descriptor.sourceTooltip = "Source: " + (status.sourceIdentity.path(SourceRole::primary) == nullptr
                                                         ? std::string("Unavailable")
                                                         : status.sourceIdentity.path(SourceRole::primary)->string());
    adapter.descriptor.cacheContext = std::to_string(static_cast<int>(state.browserKind)) + "|" +
                                      std::to_string(static_cast<int>(idFormat)) +
                                      "|revision=" + std::to_string(status.revision);
    adapter.descriptor.defaultPresentation = assets::AssetPresentation::table;
    adapter.descriptor.formatSelectionId = [idFormat](const assets::AssetItemId & item)
    {
        return idOf(item)
            .transform([idFormat](std::uint32_t id) { return strings::formatId(id, idFormat); })
            .value_or(item.value);
    };
    adapter.descriptor.source = {
        .kind = DocumentKind::tiledata,
        .contextKind = assets::AssetSourceContext::Kind::metadataView,
        .source = status.sourceIdentity,
        .contextId = "view.tiledata",
    };
    adapter.descriptor.capabilities = {
        .presentations = static_cast<std::uint8_t>(assets::AssetPresentationCapability::grid) |
                         static_cast<std::uint8_t>(assets::AssetPresentationCapability::table),
        .queries = static_cast<std::uint8_t>(assets::AssetQueryCapability::filter),
        .selection = assets::AssetSelectionMode::multiple,
    };
    adapter.descriptor.itemCount = itemCount(state.browserKind);
    adapter.descriptor.fixedCellWidth = assetCellWidth;
    adapter.descriptor.fixedCellHeight = assetCellHeight;
    adapter.descriptor.extensionSlots.push_back({
        .id = "tiledata-kind",
        .label = "Tiledata kind",
        .draw =
            [&state]
        {
            if (ImGui::RadioButton("Map", state.browserKind == TiledataBrowserKind::map))
            {
                state.browserKind = TiledataBrowserKind::map;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Static", state.browserKind == TiledataBrowserKind::statics))
            {
                state.browserKind = TiledataBrowserKind::statics;
            }
        },
    });
    adapter.loadRange = [&provider, &state, idFormat](const assets::AssetRangeRequest & request, const assets::AssetQuery &)
    {
        std::vector<assets::AssetItem> items;
        const std::size_t end = std::min(request.first + request.count, itemCount(state.browserKind));
        for (std::size_t index = request.first; index < end; index++)
        {
            const auto id = static_cast<std::uint32_t>(index);
            const bool available = state.browserKind == TiledataBrowserKind::map
                                       ? provider.mapTile(static_cast<std::uint16_t>(id)).has_value()
                                       : provider.staticTile(id).has_value();
            items.push_back({
                .id = {.value = std::to_string(id)},
                .label = strings::formatId(id, idFormat),
                .state = available ? assets::AssetItemState::available : assets::AssetItemState::empty,
                .stateMessage = available ? std::string{} : "Record unavailable",
            });
        }
        return items;
    };
    adapter.loadSearchItems = [&provider, &state, idFormat]
    {
        std::vector<assets::AssetSearchItem> items;
        for (std::size_t index = 0; index < itemCount(state.browserKind); index++)
        {
            const auto id = static_cast<std::uint32_t>(index);
            std::vector<std::string> values;
            bool available = false;
            if (state.browserKind == TiledataBrowserKind::map)
            {
                if (const auto record = provider.mapTile(static_cast<std::uint16_t>(id)))
                {
                    values = mapValues(*record, idFormat);
                    available = true;
                }
            }
            else if (const auto record = provider.staticTile(id))
            {
                values = staticValues(*record, idFormat);
                available = true;
            }
            if (!available)
            {
                continue;
            }
            items.push_back({
                .item =
                    {
                        .id = {.value = std::to_string(id)},
                        .label = strings::formatId(id, idFormat),
                        .state = assets::AssetItemState::available,
                        .stateMessage = {},
                    },
                .values = std::move(values),
            });
        }
        return items;
    };
    adapter.loadDetails = [&provider, &state, idFormat](const assets::AssetItemId & item) -> std::optional<assets::AssetDetails>
    {
        const auto id = idOf(item);
        if (!id)
        {
            return std::nullopt;
        }
        if (state.browserKind == TiledataBrowserKind::map)
        {
            const auto record = provider.mapTile(static_cast<std::uint16_t>(*id));
            if (!record)
            {
                return std::nullopt;
            }
            return assets::AssetDetails{
                .item = item,
                .title = {},
                .fields = mapValues(*record, idFormat),
                .fieldTooltips = {"", "", rawFlagsText(record->record.flags), ""},
            };
        }
        const auto record = provider.staticTile(*id);
        if (!record)
        {
            return std::nullopt;
        }
        return assets::AssetDetails{
            .item = item,
            .title = {},
            .fields = staticValues(*record, idFormat),
            .fieldTooltips = {"", "", rawFlagsText(record->record.flags), "", "", "", "", ""},
        };
    };
    adapter.renderItem = [&state, &provider, &textures, &radarColors](const assets::AssetItem & item, bool)
    {
        const auto id = idOf(item.id);
        if (!id)
        {
            ImGui::TextUnformatted(item.label.c_str());
            return;
        }
        const ProviderTexture * texture = nullptr;
        if (state.browserKind == TiledataBrowserKind::map)
        {
            if (const auto tile = provider.mapTile(static_cast<std::uint16_t>(*id)))
            {
                texture = texturePreview(state, textures, tile->record.textureId);
                if (texture == nullptr)
                {
                    if (const auto color = radarColors.mapColor(tile->record.textureId))
                    {
                        state.texturePreviews.insert_or_assign(tile->record.textureId,
                                                               ProviderTexture{.width = 1, .height = 1, .pixels = {*color}});
                        texture = &state.texturePreviews.at(tile->record.textureId);
                    }
                }
            }
        }
        else if (const auto color = radarColors.rawColor(static_cast<std::uint32_t>(*id) + radarStaticOffset))
        {
            state.texturePreviews.insert_or_assign(
                *id, ProviderTexture{.width = 1, .height = 1, .pixels = {radarColorToMapColor(*color)}});
            texture = &state.texturePreviews.at(*id);
        }
        if (texture != nullptr)
        {
            drawTexturePreview({.width = texture->width, .height = texture->height, .pixels = texture->pixels}, previewSize);
        }
        else
        {
            ImGui::TextUnformatted(item.label.c_str());
        }
    };
    return adapter;
}

} // namespace

void drawTiledataPanel(TiledataPanelState & state,
                       const TiledataProvider & provider,
                       const TextureProvider & textures,
                       const RadarColorProvider & radarColors,
                       bool & visible,
                       IdFormat idFormat)
{
    const auto adapter = makeAdapter(state, provider, textures, radarColors, idFormat);
    assets::drawAssetWindow(adapter, state.assetView, visible);
}

void drawTiledataDocumentPanel(TiledataPanelState & state,
                               TiledataDocument & document,
                               const TextureProvider & textures,
                               const RadarColorProvider & radarColors,
                               bool & visible,
                               IdFormat idFormat,
                               std::string windowIdentity)
{
    if (!document.reader())
    {
        if (ImGui::Begin(windowIdentity.c_str(), &visible))
        {
            ImGui::TextColored(theme::imguiColor(theme::error), "%s", document.error().c_str());
        }
        ImGui::End();
        return;
    }

    class ReaderProvider final : public TiledataProvider
    {
    public:
        explicit ReaderProvider(TiledataDocument & document) : document_(&document) {}
        [[nodiscard]] ProviderStatus status() const override
        {
            return {
                .state = ProviderAssetState::available,
                .source = ProviderSourceKind::externalConfigured,
                .message = {},
                .sourceIdentity = makeDocumentSourceIdentity({document_->sourcePath()}),
                .revision = 1,
            };
        }
        [[nodiscard]] ProviderStatus status(std::uint16_t id) const override { return recordStatus(mapTile(id).has_value()); }
        [[nodiscard]] ProviderStatus staticStatus(std::uint32_t id) const override
        {
            return recordStatus(staticTile(id).has_value());
        }
        [[nodiscard]] std::optional<uomul::TiledataLayout> layout() const override { return document_->layout(); }
        [[nodiscard]] std::optional<ProviderMapTile> mapTile(std::uint16_t id) const override
        {
            const auto * reader = document_->reader() ? &document_->reader().value() : nullptr;
            if (reader == nullptr)
            {
                return std::nullopt;
            }
            const auto record = reader->readMapTile(id);
            return record ? std::optional<ProviderMapTile>(ProviderMapTile{.id = id, .record = *record}) : std::nullopt;
        }
        [[nodiscard]] std::optional<ProviderStaticTile> staticTile(std::uint32_t id) const override
        {
            const auto * reader = document_->reader() ? &document_->reader().value() : nullptr;
            if (reader == nullptr)
            {
                return std::nullopt;
            }
            const auto record = reader->readStaticTile(id);
            return record ? std::optional<ProviderStaticTile>(ProviderStaticTile{.id = id, .record = *record}) : std::nullopt;
        }

    private:
        [[nodiscard]] ProviderStatus recordStatus(bool available) const
        {
            auto result = status();
            if (!available)
            {
                result.state = ProviderAssetState::empty;
                result.message = "Record unavailable";
            }
            return result;
        }
        TiledataDocument * document_ = nullptr;
    };
    const ReaderProvider provider(document);

    const auto adapter = makeAdapter(state, provider, textures, radarColors, idFormat);
    auto documentAdapter = adapter;
    documentAdapter.descriptor.windowIdentity = std::move(windowIdentity);
    documentAdapter.descriptor.source.contextKind = assets::AssetSourceContext::Kind::document;
    documentAdapter.descriptor.source.contextId = "document.tiledata";
    assets::drawAssetWindow(documentAdapter, state.assetView, visible);
}

} // namespace uo_mule::gui
