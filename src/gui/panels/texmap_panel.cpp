#include "gui/panels/texmap_panel.h"
#include "gui/panels/asset_panel_helpers.h"
#include "gui/presentation/strings/ui_strings.h"
#include "gui/state/workbench_state.h"
#include "gui/widgets/asset_window.h"
#include "gui/widgets/texture_preview.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/texmaps_document.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/color_conversion.h"
#include "gui_model/providers/data_providers.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <imgui.h>
#include <optional>
#include <string>
#include <unordered_map>
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

[[nodiscard]] const TexmapsTexturePreview & texturePreview(std::unordered_map<std::uint32_t, TexmapsTexturePreview> & previews,
                                                           const TextureProvider & provider,
                                                           std::uint32_t id)
{
    const auto [iterator, inserted] = previews.try_emplace(id);
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

void drawTextureAsset(TexmapsPanelState & state, const TextureProvider & provider, const assets::AssetItemId & item, float size)
{
    const auto id = assets::itemIndex(item);
    if (!id)
    {
        return;
    }
    const auto & preview = texturePreview(state.texturePreviews, provider, *id);
    drawTexturePreview({.width = preview.width, .height = preview.height, .pixels = preview.pixels}, size, borderThickness);
}

void drawDocumentTextureAsset(std::unordered_map<std::uint32_t, TexmapsTexturePreview> & previews,
                              const TextureProvider & provider,
                              const assets::AssetItemId & item,
                              float size)
{
    const auto id = assets::itemIndex(item);
    if (!id)
    {
        return;
    }
    const auto & preview = texturePreview(previews, provider, *id);
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
    configureAssetAdapter(adapter.descriptor,
                          {
                              .viewId = "view.texmaps",
                              .windowIdentity = {},
                              .title = "Texmaps",
                              .sourceTooltip = "Source: " + sourceLabel(status),
                              .cacheContext = std::to_string(static_cast<int>(idFormat)) + "|" +
                                              std::to_string(static_cast<int>(state.hideUnused)) +
                                              "|revision=" + std::to_string(status.revision),
                              .source =
                                  {
                                      .kind = DocumentKind::texmapPair,
                                      .contextKind = assets::AssetSourceContext::Kind::metadataView,
                                      .source = status.sourceIdentity,
                                      .contextId = "view.texmaps",
                                  },
                              .itemPicked = state.itemPicked,
                              .hoveredContext = state.hoveredContext,
                          });
    adapter.descriptor.defaultPresentation =
        preferences.texmapsTablePresentation ? assets::AssetPresentation::table : assets::AssetPresentation::grid;
    adapter.descriptor.presentationChanged = [&pendingChanges](assets::AssetPresentation presentation)
    { pendingChanges.texmapsTablePresentation = presentation == assets::AssetPresentation::table; };
    adapter.descriptor.queryWhenEmpty = state.hideUnused;
    adapter.descriptor.formatSelectionId = [idFormat](const assets::AssetItemId & item)
    { return formatAssetItemId(item, idFormat); };
    adapter.descriptor.renderSelectionPreview = [&state, &provider](const assets::AssetItemId & item, float size)
    { drawTextureAsset(state, provider, item, size); };
    adapter.descriptor.selectionPreviewSize = thumbnailSize;
    adapter.descriptor.itemSourcePicked = [&state](const assets::AssetItemId & item)
    {
        const auto id = assets::itemIndex(item);
        return id && state.itemSourcePicked ? state.itemSourcePicked(*id) : false;
    };
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

class DocumentTextureProvider final : public TextureProvider
{
public:
    explicit DocumentTextureProvider(const TexmapsDocument & document) : document_(&document) {}

    [[nodiscard]] ProviderStatus status() const override
    {
        return {
            .state = document_->reader() ? ProviderAssetState::available : ProviderAssetState::invalid,
            .source = ProviderSourceKind::externalConfigured,
            .message = document_->error(),
            .sourceIdentity = document_->sourceIdentity(),
            .revision = document_->displayRevision(),
        };
    }

    [[nodiscard]] ProviderStatus status(std::uint32_t id) const override
    {
        auto result = status();
        const auto present = document_->hasTexture(id);
        if (!present)
        {
            result.state = ProviderAssetState::invalid;
            result.message = present.error().message;
        }
        else if (!*present)
        {
            result.state = ProviderAssetState::empty;
            result.message = "No texture entry";
        }
        return result;
    }

    [[nodiscard]] std::optional<ProviderTexture> texture(std::uint32_t id) const override
    {
        const auto result = document_->effectiveTexture(id);
        if (!result || !result->has_value())
        {
            return std::nullopt;
        }
        ProviderTexture texture{
            .width = result->value().dimension,
            .height = result->value().dimension,
            .pixels = {},
        };
        texture.pixels.reserve(result->value().pixels.size());
        for (const auto pixel : result->value().pixels)
        {
            texture.pixels.push_back(texturePixelToMapColor(pixel));
        }
        return texture;
    }

    [[nodiscard]] std::uint32_t textureCount() const override
    {
        return document_->reader() ? document_->reader()->textureCount() : 0U;
    }

    [[nodiscard]] std::optional<bool> hasTexture(std::uint32_t id) const override
    {
        const auto result = document_->hasTexture(id);
        return result ? std::optional<bool>(*result) : std::nullopt;
    }

private:
    const TexmapsDocument * document_ = nullptr;
};

[[nodiscard]] assets::AssetAdapter makeDocumentAdapter(TexmapsPanelState & state,
                                                       TexmapsDocument & document,
                                                       const TextureProvider & provider,
                                                       IdFormat idFormat,
                                                       std::string windowIdentity)
{
    const auto status = provider.status();
    auto & previews = state.documentTexturePreviews[document.id()];
    const auto revision = document.displayRevision();
    if (state.documentTexturePreviewRevisions[document.id()] != revision)
    {
        previews.clear();
        state.documentTexturePreviewRevisions[document.id()] = revision;
    }

    assets::AssetAdapter adapter;
    configureAssetAdapter(
        adapter.descriptor,
        {
            .viewId = "document.texmaps." + std::to_string(document.id()),
            .windowIdentity = std::move(windowIdentity),
            .title = "Texmaps",
            .sourceTooltip = "Source: " + document.sourceIndexPath().string(),
            .cacheContext = std::to_string(static_cast<int>(idFormat)) + "|revision=" + std::to_string(revision),
            .source =
                {
                    .kind = DocumentKind::texmapPair,
                    .contextKind = assets::AssetSourceContext::Kind::document,
                    .source = document.sourceIdentity(),
                    .contextId = "document.texmaps." + std::to_string(document.id()),
                },
            .itemPicked = state.itemPicked,
            .hoveredContext = state.hoveredContext,
        });
    adapter.descriptor.formatSelectionId = [idFormat](const assets::AssetItemId & item)
    { return formatAssetItemId(item, idFormat); };
    adapter.descriptor.renderSelectionPreview = [&previews, &provider](const assets::AssetItemId & item, float size)
    { drawDocumentTextureAsset(previews, provider, item, size); };
    adapter.descriptor.selectionPreviewSize = thumbnailSize;
    adapter.descriptor.capabilities = {
        .presentations = static_cast<std::uint8_t>(assets::AssetPresentationCapability::grid) |
                         static_cast<std::uint8_t>(assets::AssetPresentationCapability::table),
        .queries = static_cast<std::uint8_t>(assets::AssetQueryCapability::filter),
        .selection = assets::AssetSelectionMode::multiple,
    };
    adapter.descriptor.itemCount = provider.textureCount();
    adapter.descriptor.fixedCellWidth = assetCellWidth;
    adapter.descriptor.fixedCellHeight = assetCellHeight;
    adapter.descriptor.itemStaged = [&document](const assets::AssetItemId & item)
    {
        return assets::itemIndex(item)
            .transform([&document](std::uint32_t id) { return document.hasStagedTexture(id); })
            .value_or(false);
    };
    adapter.descriptor.actions.push_back({
        .id = "texmaps.revert",
        .label = "Revert",
        .enabled = !document.readOnly() && std::ranges::any_of(state.documentAssetViews[document.id()].selection.selected,
                                                               [&document](const assets::AssetItemId & item)
                                                               {
                                                                   return assets::itemIndex(item)
                                                                       .transform([&document](std::uint32_t id)
                                                                                  { return document.hasStagedTexture(id); })
                                                                       .value_or(false);
                                                               }),
        .disabledReason = "No selected staged textures",
        .invoke =
            [&document, &state]
        {
            std::vector<std::uint32_t> ids;
            for (const auto & item : state.documentAssetViews[document.id()].selection.selected)
            {
                if (const auto id = assets::itemIndex(item))
                {
                    ids.push_back(*id);
                }
            }
            TexmapsDocumentIntent intent;
            intent.kind = TexmapsDocumentIntentKind::revert;
            intent.textureIds = std::move(ids);
            document.requestIntent(std::move(intent));
        },
    });
    if (!status.message.empty())
    {
        adapter.descriptor.sourceTooltip += "\nStatus: " + status.message;
    }
    adapter.loadRange = [&provider, idFormat](const assets::AssetRangeRequest & request, const assets::AssetQuery &)
    { return loadTextureRange(provider, request, idFormat); };
    adapter.loadSearchItems = [&state, &provider, idFormat] { return loadTextureSearch(state, provider, idFormat); };
    adapter.loadDetails = [&provider, idFormat](const assets::AssetItemId & item)
    { return loadTextureDetails(provider, item, idFormat); };
    adapter.renderItem = [&previews, &provider](const assets::AssetItem & item, bool)
    { drawDocumentTextureAsset(previews, provider, item.id, thumbnailSize); };
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

void drawTexmapsDocumentPanel(TexmapsPanelState & state,
                              TexmapsDocument & document,
                              bool & visible,
                              IdFormat idFormat,
                              std::string windowIdentity,
                              const std::function<void()> & becameVisible)
{
    if (!document.reader())
    {
        if (ImGui::Begin(windowIdentity.c_str(), &visible))
        {
            ImGui::TextUnformatted(document.error().c_str());
        }
        ImGui::End();
        return;
    }

    const DocumentTextureProvider provider(document);
    const auto adapter = makeDocumentAdapter(state, document, provider, idFormat, std::move(windowIdentity));
    assets::drawAssetWindow(adapter, state.documentAssetViews[document.id()], visible, becameVisible);
}

} // namespace uo_mule::gui
