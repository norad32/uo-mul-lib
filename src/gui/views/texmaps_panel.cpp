#include "gui/views/texmaps_panel.h"
#include "gui/persistence/preferences.h"
#include "gui/strings/ui_strings.h"
#include "gui/assets/asset_window.h"
#include "gui/assets/asset_window_contracts.h"
#include "gui/context/color_conversion.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/documents/document_identity.h"
#include "gui/frontend/theme.h"
#include "gui/metadata/document_contracts.h"
#include "gui/profile/data_profile.h"
#include "uomul/texmaps/texmaps.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
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

void setPath(std::array<char, texmapsPathBufferSize> & buffer, const std::filesystem::path & path)
{
    const std::string value = path.string();
    const std::size_t length = std::min(value.size(), buffer.size() - 1U);
    std::memcpy(buffer.data(), value.data(), length);
    buffer.at(length) = '\0';
}

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

void drawTexturePreview(const TexmapsTexturePreview & texture, float size)
{
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max(min.x + size, min.y + size);
    ImDrawList * drawList = ImGui::GetWindowDrawList();
    constexpr float checkerSize = 4.0F;
    constexpr ImU32 checkerA = IM_COL32(64, 64, 64, 255);
    constexpr ImU32 checkerB = IM_COL32(128, 128, 128, 255);
    for (int y = 0; static_cast<float>(y) * checkerSize < size; y++)
    {
        for (int x = 0; static_cast<float>(x) * checkerSize < size; x++)
        {
            const ImU32 color = ((x % 2) != (y % 2)) ? checkerA : checkerB;
            const ImVec2 cellMin(min.x + (static_cast<float>(x) * checkerSize), min.y + (static_cast<float>(y) * checkerSize));
            const ImVec2 cellMax(std::min(cellMin.x + checkerSize, max.x), std::min(cellMin.y + checkerSize, max.y));
            drawList->AddRectFilled(cellMin, cellMax, color);
        }
    }
    if (!texture.pixels.empty() && texture.width > 0U && texture.height > 0U)
    {
        const float scaleX = size / static_cast<float>(texture.width);
        const float scaleY = size / static_cast<float>(texture.height);
        for (std::uint32_t y = 0; y < texture.height; y++)
        {
            for (std::uint32_t x = 0; x < texture.width; x++)
            {
                const std::size_t index = (y * texture.width) + x;
                if (index < texture.pixels.size())
                {
                    const MapColor color = texture.pixels.at(index);
                    const float pixelMinX = min.x + std::round(static_cast<float>(x) * scaleX);
                    const float pixelMinY = min.y + std::round(static_cast<float>(y) * scaleY);
                    const float pixelMaxX = x + 1U == texture.width ? max.x : min.x + std::round(static_cast<float>(x + 1U) * scaleX);
                    const float pixelMaxY = y + 1U == texture.height ? max.y : min.y + std::round(static_cast<float>(y + 1U) * scaleY);
                    drawList->AddRectFilled(ImVec2(pixelMinX, pixelMinY),
                                                  ImVec2(pixelMaxX, pixelMaxY),
                                                  IM_COL32(color.red, color.green, color.blue, 255));
                }
            }
        }
    }
    drawList->AddRect(min, max, theme::packedColor(theme::border), 0.0F, ImDrawFlags_None, borderThickness);
    ImGui::Dummy(ImVec2(size, size));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
[[nodiscard]] assets::AssetAdapter makeAdapter(TexmapsPanelState & state, Preferences & preferences)
{
    const IdFormat idFormat = preferences.idFormat;
    state.hideUnused = preferences.texmapsHideUnused;
    assets::AssetAdapter adapter;
    adapter.descriptor.viewId = "view.texmaps";
    adapter.descriptor.title = "Texmaps";
    adapter.descriptor.sourceTooltip = "Index: " + std::string(state.sourceIndexPath.data()) +
                                      "\nData: " + std::string(state.sourceDataPath.data());
    adapter.descriptor.cacheContext = std::to_string(static_cast<int>(idFormat)) + "|" +
                                      std::to_string(static_cast<int>(state.hideUnused));
    adapter.descriptor.defaultPresentation = preferences.texmapsTablePresentation ? assets::AssetPresentation::table
                                                                                   : assets::AssetPresentation::grid;
    adapter.descriptor.presentationChanged = [&preferences](assets::AssetPresentation presentation) {
        preferences.texmapsTablePresentation = presentation == assets::AssetPresentation::table;
        savePreferences(preferences);
    };
    adapter.descriptor.queryWhenEmpty = state.hideUnused;
    adapter.descriptor.formatSelectionId = [idFormat](const assets::AssetItemId & item) {
        return strings::formatId(static_cast<std::uint32_t>(std::stoul(item.value)), idFormat);
    };
    adapter.descriptor.renderSelectionPreview = [&state](const assets::AssetItemId & item, float size) {
        drawTexturePreview(texturePreview(state, static_cast<std::uint32_t>(std::stoul(item.value))), size);
    };
    adapter.descriptor.selectionPreviewSize = thumbnailSize;
    adapter.descriptor.inlinePresentationControl = [&state, &preferences] {
        bool hideUnused = preferences.texmapsHideUnused;
        if (ImGui::Checkbox("Hide unused", &hideUnused) && hideUnused != preferences.texmapsHideUnused)
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
    adapter.descriptor.source = {.kind = DocumentKind::texmapPair,
                                 .source = makeDocumentSourceIdentity({std::filesystem::path(state.sourceIndexPath.data()),
                                                                        std::filesystem::path(state.sourceDataPath.data()),}),
                                 .contextId = "view.texmaps",};
    adapter.descriptor.capabilities = {.presentations = static_cast<std::uint8_t>(assets::AssetPresentationCapability::grid) |
                                                       static_cast<std::uint8_t>(assets::AssetPresentationCapability::table),
                                       .queries = static_cast<std::uint8_t>(assets::AssetQueryCapability::filter),
                                        .selection = assets::AssetSelectionMode::multiple,};
    adapter.descriptor.itemCount = state.reader ? state.reader->textureCount() : 0U;
    adapter.descriptor.fixedCellWidth = assetCellWidth;
    adapter.descriptor.fixedCellHeight = assetCellHeight;
    adapter.loadRange = [&state, idFormat](const assets::AssetRangeRequest & request, const assets::AssetQuery &) {
        std::vector<assets::AssetItem> items;
        if (!state.reader)
        {
            return items;
        }
        const std::size_t end = std::min(request.first + request.count, static_cast<std::size_t>(state.reader->textureCount()));
        for (std::size_t id = request.first; id < end; id++)
        {
            const auto present = state.reader->hasTexture(static_cast<std::uint32_t>(id));
            items.push_back({.id = {.value = std::to_string(id)},
                             .label = strings::formatId(static_cast<std::uint32_t>(id), idFormat),
                             .state = present && *present ? assets::AssetItemState::available : assets::AssetItemState::empty,
                             .stateMessage = {},});
        }
        return items;
    };
    adapter.loadSearchItems = [&state, idFormat]() {
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
                    values.push_back("Dimensions: " + std::to_string(texture->dimension) + "x" +
                                     std::to_string(texture->dimension));
                }
            }
            if (state.hideUnused && (!present || !*present))
            {
                continue;
            }
            items.push_back({.item = {.id = {.value = std::to_string(id)},
                                      .label = formattedId,
                                      .state = present && *present ? assets::AssetItemState::available : assets::AssetItemState::empty,
                                      .stateMessage = {},},
                             .values = std::move(values),});
        }
        return items;
    };
    adapter.loadDetails = [&state, idFormat](const assets::AssetItemId & item) -> std::optional<assets::AssetDetails> {
        if (!state.reader)
        {
            return std::nullopt;
        }
        const auto id = static_cast<std::uint32_t>(std::stoul(item.value));
        const auto texture = state.reader->readTexture(id);
        if (!texture)
        {
            return std::nullopt;
        }
        return assets::AssetDetails{.item = item,
                                     .title = "ID: " + strings::formatId(id, idFormat),
                                      .fields = {"Dimensions: " + std::to_string(texture->dimension) + "x" +
                                                 std::to_string(texture->dimension),},};
    };
    adapter.renderItem = [&state](const assets::AssetItem & item, bool) {
        const auto id = static_cast<std::uint32_t>(std::stoul(item.id.value));
        drawTexturePreview(texturePreview(state, id), thumbnailSize);
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
    setPath(state.sourceIndexPath, index);
    setPath(state.sourceDataPath, data);
    loadSource(state);
}

void applyFileBrowserResult(TexmapsPanelState & state, const FileBrowserResult & result)
{
    if (result.target == FileBrowserTarget::texmapsIndex)
    {
        setPath(state.sourceIndexPath, result.path);
    }
    else if (result.target == FileBrowserTarget::texmapsData)
    {
        setPath(state.sourceDataPath, result.path);
    }
}

void drawTexmapsPanel(TexmapsPanelState & state, bool & visible, Preferences & preferences)
{
    const auto adapter = makeAdapter(state, preferences);
    assets::drawAssetWindow(adapter, state.assetView, visible);
}

} // namespace uo_mule::gui
