#include "gui/panels/texmap_panel.h"
#include "gui/presentation/strings/ui_strings.h"
#include "gui/state/workbench_state.h"
#include "gui/widgets/asset_window.h"
#include "gui/widgets/texture_preview.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <imgui.h>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace uo_mule::gui
{
namespace
{

constexpr float thumbnailSize = 64.0F;
constexpr float borderThickness = 2.0F;
constexpr std::size_t assetCellWidth = 72U;
constexpr std::size_t assetCellHeight = 72U;

[[nodiscard]] std::string sourceLabel(const ProviderStatus & status)
{
    for (const auto & [role, path] : status.sourceIdentity.roles)
    {
        static_cast<void>(role);
        if (!path.empty())
        {
            return path.string();
        }
    }
    return "Texmap Pair";
}

[[nodiscard]] const TexmapsTexturePreview &
texturePreview(TexmapsPanelState & state, const TextureProvider & provider, std::uint32_t id)
{
    const auto [iterator, inserted] = state.texturePreviews.try_emplace(id);
    if (inserted)
    {
        const auto texture = provider.texture(id);
        if (texture)
        {
            iterator->second.width = texture->width;
            iterator->second.height = texture->height;
            iterator->second.pixels.reserve(texture->pixels.size());
            for (const auto pixel : texture->pixels)
            {
                iterator->second.pixels.push_back(pixel);
            }
        }
    }
    return iterator->second;
}

[[nodiscard]] std::string formatTextureId(const assets::AssetItemId & item, IdFormat idFormat)
{
    return assets::itemIndex(item)
        .transform([idFormat](std::uint32_t id) { return strings::formatId(id, idFormat); })
        .value_or(item.value);
}

void drawTextureAsset(TexmapsPanelState & state, const TextureProvider & provider, const assets::AssetItemId & item, float size)
{
    const auto id = assets::itemIndex(item);
    if (!id)
    {
        return;
    }
    const auto & preview = texturePreview(state, provider, *id);
    drawTexturePreview({.width = preview.width, .height = preview.height, .pixels = preview.pixels}, size, borderThickness);
}

[[nodiscard]] std::vector<assets::AssetItem>
loadTextureRange(const TextureProvider & provider, const assets::AssetRangeRequest & request, IdFormat idFormat)
{
    std::vector<assets::AssetItem> items;
    const std::size_t end = std::min(request.first + request.count, static_cast<std::size_t>(provider.textureCount()));
    for (std::size_t id = request.first; id < end; id++)
    {
        const auto present = provider.hasTexture(static_cast<std::uint32_t>(id));
        items.push_back({
            .id = {.value = std::to_string(id)},
            .label = strings::formatId(static_cast<std::uint32_t>(id), idFormat),
            .state = present && *present ? assets::AssetItemState::available : assets::AssetItemState::empty,
            .stateMessage = {},
        });
    }
    return items;
}

[[nodiscard]] std::vector<assets::AssetSearchItem>
loadTextureSearch(const TexmapsPanelState & state, const TextureProvider & provider, IdFormat idFormat)
{
    std::vector<assets::AssetSearchItem> items;
    for (std::size_t id = 0; id < static_cast<std::size_t>(provider.textureCount()); id++)
    {
        const auto present = provider.hasTexture(static_cast<std::uint32_t>(id));
        if (state.hideUnused && (!present || !*present))
        {
            continue;
        }
        const auto label = strings::formatId(static_cast<std::uint32_t>(id), idFormat);
        items.push_back({
            .item =
                {
                    .id = {.value = std::to_string(id)},
                    .label = label,
                    .state = present && *present ? assets::AssetItemState::available : assets::AssetItemState::empty,
                    .stateMessage = {},
                },
            .values = {"ID: " + label},
        });
    }
    return items;
}

[[nodiscard]] std::optional<assets::AssetDetails>
loadTextureDetails(const TextureProvider & provider, const assets::AssetItemId & item, IdFormat idFormat)
{
    const auto id = assets::itemIndex(item);
    if (!id)
    {
        return std::nullopt;
    }
    const auto texture = provider.texture(*id);
    if (!texture)
    {
        return std::nullopt;
    }
    return assets::AssetDetails{
        .item = item,
        .title = "ID: " + strings::formatId(*id, idFormat),
        .fields =
            {
                std::string(strings::dimensions()) + ": " + std::to_string(texture->width) + "x" +
                    std::to_string(texture->height),
            },
        .fieldTooltips = {},
    };
}

[[nodiscard]] assets::AssetAdapter makeAdapter(TexmapsPanelState & state,
                                               const TextureProvider & provider,
                                               const Preferences & preferences,
                                               PendingPreferenceChanges & pendingChanges)
{
    const IdFormat idFormat = preferences.idFormat;
    const ProviderStatus status = provider.status();
    state.hideUnused = preferences.texmapsHideUnused;
    if (state.texturePreviewRevision != status.revision)
    {
        state.texturePreviews.clear();
        state.texturePreviewRevision = status.revision;
    }
    assets::AssetAdapter adapter;
    adapter.descriptor.viewId = "view.texmaps";
    adapter.descriptor.title = "Texmaps";
    adapter.descriptor.sourceTooltip = "Source: " + sourceLabel(status);
    adapter.descriptor.cacheContext = std::to_string(static_cast<int>(idFormat)) + "|" +
                                      std::to_string(static_cast<int>(state.hideUnused)) +
                                      "|revision=" + std::to_string(status.revision);
    adapter.descriptor.defaultPresentation =
        preferences.texmapsTablePresentation ? assets::AssetPresentation::table : assets::AssetPresentation::grid;
    adapter.descriptor.presentationChanged = [&pendingChanges](assets::AssetPresentation presentation)
    { pendingChanges.texmapsTablePresentation = presentation == assets::AssetPresentation::table; };
    adapter.descriptor.queryWhenEmpty = state.hideUnused;
    adapter.descriptor.formatSelectionId = [idFormat](const assets::AssetItemId & item)
    { return formatTextureId(item, idFormat); };
    adapter.descriptor.renderSelectionPreview = [&state, &provider](const assets::AssetItemId & item, float size)
    { drawTextureAsset(state, provider, item, size); };
    adapter.descriptor.selectionPreviewSize = thumbnailSize;
    adapter.descriptor.itemPicked = state.itemPicked;
    adapter.descriptor.inlinePresentationControl = [&state, &pendingChanges]
    {
        bool hideUnused = state.hideUnused;
        if (ImGui::Checkbox(strings::hideUnused(), &hideUnused))
        {
            state.hideUnused = hideUnused;
            pendingChanges.texmapsHideUnused = hideUnused;
        }
    };
    if (!status.message.empty())
    {
        adapter.descriptor.sourceTooltip += "\nStatus: " + status.message;
    }
    adapter.descriptor.source = {
        .kind = DocumentKind::texmapPair,
        .contextKind = assets::AssetSourceContext::Kind::metadataView,
        .source = status.sourceIdentity,
        .contextId = "view.texmaps",
    };
    adapter.descriptor.capabilities = {
        .presentations = static_cast<std::uint8_t>(assets::AssetPresentationCapability::grid) |
                         static_cast<std::uint8_t>(assets::AssetPresentationCapability::table),
        .queries = static_cast<std::uint8_t>(assets::AssetQueryCapability::filter),
        .selection = assets::AssetSelectionMode::multiple,
    };
    adapter.descriptor.itemCount = provider.textureCount();
    adapter.descriptor.fixedCellWidth = assetCellWidth;
    adapter.descriptor.fixedCellHeight = assetCellHeight;
    adapter.loadRange = [&provider, idFormat](const assets::AssetRangeRequest & request, const assets::AssetQuery &)
    { return loadTextureRange(provider, request, idFormat); };
    adapter.loadSearchItems = [&state, &provider, idFormat] { return loadTextureSearch(state, provider, idFormat); };
    adapter.loadDetails = [&provider, idFormat](const assets::AssetItemId & item)
    { return loadTextureDetails(provider, item, idFormat); };
    adapter.renderItem = [&state, &provider](const assets::AssetItem & item, bool)
    { drawTextureAsset(state, provider, item.id, thumbnailSize); };
    return adapter;
}

} // namespace

void drawTexmapsPanel(TexmapsPanelState & state,
                      const TextureProvider & provider,
                      bool & visible,
                      const Preferences & preferences,
                      PendingPreferenceChanges & pendingChanges)
{
    const auto adapter = makeAdapter(state, provider, preferences, pendingChanges);
    assets::drawAssetWindow(adapter, state.assetView, visible);
}

} // namespace uo_mule::gui
