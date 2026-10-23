#include "gui/views/texmaps_panel.h"
#include "gui/assets/asset_window.h"
#include "gui/assets/asset_window_contracts.h"
#include "gui/context/color_conversion.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/documents/document_identity.h"
#include "gui/documents/document_contracts.h"
#include "gui/persistence/preferences.h"
#include "gui/profile/data_profile.h"
#include "gui/strings/ui_strings.h"
#include "gui/support/bounded_buffer.h"
#include "gui/views/texture_preview.h"
#include "uomul/texmaps/texmaps.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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

void loadSource(TexmapsPanelState & state)
{
    state.reader.reset();
    state.texturePreviews.clear();
    state.error.clear();
    const std::string indexPath(state.sourceIndexPath.data());
    const std::string dataPath(state.sourceDataPath.data());
    if (indexPath.empty() || dataPath.empty())
    {
        return;
    }
    auto result = uomul::TexMapsReader::open(indexPath, dataPath);
    if (!result)
    {
        state.error = result.error().message;
        return;
    }
    state.reader.emplace(std::move(*result));
}

[[nodiscard]] const TexmapsTexturePreview & texturePreview(TexmapsPanelState & state, std::uint32_t id)
{
    const auto [iterator, inserted] = state.texturePreviews.try_emplace(id);
    if (inserted && state.reader)
    {
        const auto texture = state.reader->readTexture(id);
        if (texture)
        {
            iterator->second.width = texture->dimension;
            iterator->second.height = texture->dimension;
            iterator->second.pixels.reserve(texture->pixels.size());
            for (const auto pixel : texture->pixels)
            {
                iterator->second.pixels.push_back(texturePixelToMapColor(pixel));
            }
        }
    }
    return iterator->second;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
[[nodiscard]] assets::AssetAdapter makeAdapter(TexmapsPanelState & state, Preferences & preferences)
{
    const IdFormat idFormat = preferences.idFormat;
    state.hideUnused = preferences.texmapsHideUnused;
    assets::AssetAdapter adapter;
    adapter.descriptor.viewId = "view.texmaps";
    adapter.descriptor.title = "Texmaps";
    adapter.descriptor.sourceTooltip =
        "Index: " + std::string(state.sourceIndexPath.data()) + "\nData: " + std::string(state.sourceDataPath.data());
    adapter.descriptor.cacheContext =
        std::to_string(static_cast<int>(idFormat)) + "|" + std::to_string(static_cast<int>(state.hideUnused));
    adapter.descriptor.defaultPresentation =
        preferences.texmapsTablePresentation ? assets::AssetPresentation::table : assets::AssetPresentation::grid;
    adapter.descriptor.presentationChanged = [&preferences](assets::AssetPresentation presentation)
    {
        preferences.texmapsTablePresentation = presentation == assets::AssetPresentation::table;
        savePreferences(preferences);
    };
    adapter.descriptor.queryWhenEmpty = state.hideUnused;
    adapter.descriptor.formatSelectionId = [idFormat](const assets::AssetItemId & item)
    {
        return assets::itemIndex(item)
            .transform([idFormat](std::uint32_t id) { return strings::formatId(id, idFormat); })
            .value_or(item.value);
    };
    adapter.descriptor.renderSelectionPreview = [&state](const assets::AssetItemId & item, float size)
    {
        const auto id = assets::itemIndex(item);
        if (!id)
        {
            return;
        }
        const auto & preview = texturePreview(state, *id);
        drawTexturePreview({.width = preview.width, .height = preview.height, .pixels = preview.pixels}, size, borderThickness);
    };
    adapter.descriptor.selectionPreviewSize = thumbnailSize;
    adapter.descriptor.inlinePresentationControl = [&state, &preferences]
    {
        bool hideUnused = preferences.texmapsHideUnused;
        if (ImGui::Checkbox(strings::hideUnused(), &hideUnused) && hideUnused != preferences.texmapsHideUnused)
        {
            preferences.texmapsHideUnused = hideUnused;
            state.hideUnused = hideUnused;
            savePreferences(preferences);
        }
    };
    if (!state.error.empty())
    {
        adapter.descriptor.sourceTooltip += "\nError: " + state.error;
    }
    adapter.descriptor.source = {
        .kind = DocumentKind::texmapPair,
        .source = makeDocumentSourceIdentity({
            std::filesystem::path(state.sourceIndexPath.data()),
            std::filesystem::path(state.sourceDataPath.data()),
        }),
        .contextId = "view.texmaps",
    };
    adapter.descriptor.capabilities = {
        .presentations = static_cast<std::uint8_t>(assets::AssetPresentationCapability::grid) |
                         static_cast<std::uint8_t>(assets::AssetPresentationCapability::table),
        .queries = static_cast<std::uint8_t>(assets::AssetQueryCapability::filter),
        .selection = assets::AssetSelectionMode::multiple,
    };
    adapter.descriptor.itemCount = state.reader ? state.reader->textureCount() : 0U;
    adapter.descriptor.fixedCellWidth = assetCellWidth;
    adapter.descriptor.fixedCellHeight = assetCellHeight;
    adapter.loadRange = [&state, idFormat](const assets::AssetRangeRequest & request, const assets::AssetQuery &)
    {
        std::vector<assets::AssetItem> items;
        if (!state.reader)
        {
            return items;
        }
        const std::size_t end = std::min(request.first + request.count, static_cast<std::size_t>(state.reader->textureCount()));
        for (std::size_t id = request.first; id < end; id++)
        {
            const auto present = state.reader->hasTexture(static_cast<std::uint32_t>(id));
            items.push_back({
                .id = {.value = std::to_string(id)},
                .label = strings::formatId(static_cast<std::uint32_t>(id), idFormat),
                .state = present && *present ? assets::AssetItemState::available : assets::AssetItemState::empty,
                .stateMessage = {},
            });
        }
        return items;
    };
    adapter.loadSearchItems = [&state, idFormat]
    {
        std::vector<assets::AssetSearchItem> items;
        if (!state.reader)
        {
            return items;
        }
        const auto count = static_cast<std::size_t>(state.reader->textureCount());
        for (std::size_t id = 0U; id < count; id++)
        {
            const auto present = state.reader->hasTexture(static_cast<std::uint32_t>(id));
            const auto formattedId = strings::formatId(static_cast<std::uint32_t>(id), idFormat);
            std::vector<std::string> values{"ID: " + formattedId};
            if (present && *present)
            {
                const auto texture = state.reader->readTexture(static_cast<std::uint32_t>(id));
                if (texture)
                {
                    values.push_back(std::string(strings::dimensions()) + ": " + std::to_string(texture->dimension) + "x" +
                                     std::to_string(texture->dimension));
                }
            }
            if (state.hideUnused && (!present || !*present))
            {
                continue;
            }
            items.push_back({
                .item =
                    {
                        .id = {.value = std::to_string(id)},
                        .label = formattedId,
                        .state = present && *present ? assets::AssetItemState::available : assets::AssetItemState::empty,
                        .stateMessage = {},
                    },
                .values = std::move(values),
            });
        }
        return items;
    };
    adapter.loadDetails = [&state, idFormat](const assets::AssetItemId & item) -> std::optional<assets::AssetDetails>
    {
        if (!state.reader)
        {
            return std::nullopt;
        }
        const auto id = assets::itemIndex(item);
        if (!id)
        {
            return std::nullopt;
        }
        const auto texture = state.reader->readTexture(*id);
        if (!texture)
        {
            return std::nullopt;
        }
        return assets::AssetDetails{
            .item = item,
            .title = "ID: " + strings::formatId(*id, idFormat),
            .fields =
                {
                    std::string(strings::dimensions()) + ": " + std::to_string(texture->dimension) + "x" +
                        std::to_string(texture->dimension),
                },
        };
    };
    adapter.renderItem = [&state](const assets::AssetItem & item, bool)
    {
        const auto id = assets::itemIndex(item.id);
        if (!id)
        {
            return;
        }
        const auto & preview = texturePreview(state, *id);
        drawTexturePreview(
            {.width = preview.width, .height = preview.height, .pixels = preview.pixels}, thumbnailSize, borderThickness);
    };
    return adapter;
}

} // namespace

void syncTexmapsPanel(TexmapsPanelState & state, const DataProfile & profile)
{
    const auto index = resolvedDataFilePath(profile, DataFileRole::texidx);
    const auto data = resolvedDataFilePath(profile, DataFileRole::texmaps);
    if (index.string() == state.sourceIndexPath.data() && data.string() == state.sourceDataPath.data())
    {
        return;
    }
    copyToBuffer(state.sourceIndexPath, index);
    copyToBuffer(state.sourceDataPath, data);
    loadSource(state);
}

void applyFileBrowserResult(TexmapsPanelState & state, const FileBrowserResult & result)
{
    if (result.target == FileBrowserTarget::texmapsIndex)
    {
        copyToBuffer(state.sourceIndexPath, result.path);
    }
    else if (result.target == FileBrowserTarget::texmapsData)
    {
        copyToBuffer(state.sourceDataPath, result.path);
    }
}

void drawTexmapsPanel(TexmapsPanelState & state, bool & visible, Preferences & preferences)
{
    const auto adapter = makeAdapter(state, preferences);
    assets::drawAssetWindow(adapter, state.assetView, visible);
}

} // namespace uo_mule::gui
