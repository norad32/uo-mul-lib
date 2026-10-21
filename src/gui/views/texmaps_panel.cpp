#include "gui/views/texmaps_panel.h"
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

void drawTexturePreview(const TexmapsTexturePreview & texture, std::uint32_t id, bool selected)
{
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max(min.x + thumbnailSize, min.y + thumbnailSize);
    ImDrawList * drawList = ImGui::GetWindowDrawList();
    constexpr float checkerSize = 4.0F;
    constexpr ImU32 checkerA = IM_COL32(64, 64, 64, 255);
    constexpr ImU32 checkerB = IM_COL32(128, 128, 128, 255);
    for (int y = 0; static_cast<float>(y) * checkerSize < thumbnailSize; y++)
    {
        for (int x = 0; static_cast<float>(x) * checkerSize < thumbnailSize; x++)
        {
            const ImU32 color = ((x % 2) != (y % 2)) ? checkerA : checkerB;
            const ImVec2 cellMin(min.x + (static_cast<float>(x) * checkerSize), min.y + (static_cast<float>(y) * checkerSize));
            drawList->AddRectFilled(cellMin, ImVec2(cellMin.x + checkerSize, cellMin.y + checkerSize), color);
        }
    }
    if (!texture.pixels.empty() && texture.width > 0U && texture.height > 0U)
    {
        const float scaleX = thumbnailSize / static_cast<float>(texture.width);
        const float scaleY = thumbnailSize / static_cast<float>(texture.height);
        for (std::uint32_t y = 0; y < texture.height; y++)
        {
            for (std::uint32_t x = 0; x < texture.width; x++)
            {
                const std::size_t index = (y * texture.width) + x;
                if (index < texture.pixels.size())
                {
                    const MapColor color = texture.pixels.at(index);
                    drawList->AddRectFilled(ImVec2(min.x + (static_cast<float>(x) * scaleX),
                                                    min.y + (static_cast<float>(y) * scaleY)),
                                                  ImVec2(min.x + (static_cast<float>(x + 1U) * scaleX),
                                                         min.y + (static_cast<float>(y + 1U) * scaleY)),
                                                  IM_COL32(color.red, color.green, color.blue, 255));
                }
            }
        }
    }
    drawList->AddRect(min, max, selected ? theme::packedColor(theme::brightBrass) : theme::packedColor(theme::stoneBorder), 0.0F,
                      ImDrawFlags_None, borderThickness);
    ImGui::Dummy(ImVec2(thumbnailSize, thumbnailSize));
    static_cast<void>(id);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
[[nodiscard]] assets::AssetAdapter makeAdapter(TexmapsPanelState & state, FileBrowserState & browser)
{
    assets::AssetAdapter adapter;
    adapter.descriptor.viewId = "view.texmaps";
    adapter.descriptor.title = "Texmaps";
    adapter.descriptor.source = {.kind = DocumentKind::texmapPair,
                                 .source = makeDocumentSourceIdentity({std::filesystem::path(state.sourceIndexPath.data()),
                                                                        std::filesystem::path(state.sourceDataPath.data()),}),
                                 .contextId = "view.texmaps",};
    adapter.descriptor.capabilities = {.presentations = static_cast<std::uint8_t>(assets::AssetPresentationCapability::grid),
                                       .queries = static_cast<std::uint8_t>(assets::AssetQueryCapability::filter),
                                       .selection = assets::AssetSelectionMode::single,};
    adapter.descriptor.itemCount = state.reader ? state.reader->textureCount() : 0U;
    adapter.descriptor.fixedCellWidth = assetCellWidth;
    adapter.descriptor.fixedCellHeight = assetCellHeight;
    adapter.descriptor.extensionSlots.push_back(assets::AssetExtensionSlot{
        .id = "source", .label = "Source", .draw = [&state, &browser] {
            ImGui::Text("Index: %s", state.sourceIndexPath.data());
            ImGui::Text("Data: %s", state.sourceDataPath.data());
            if (ImGui::Button("Load Textures"))
            {
                loadSource(state);
            }
            if (!state.error.empty())
            {
                ImGui::TextColored(theme::imguiColor(theme::coral), "Error: %s", state.error.c_str());
            }
            static_cast<void>(browser);
        },
    });
    adapter.loadRange = [&state](const assets::AssetRangeRequest & request, const assets::AssetQuery &) {
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
                             .label = std::to_string(id),
                             .state = present && *present ? assets::AssetItemState::available : assets::AssetItemState::empty,
                             .stateMessage = {},});
        }
        return items;
    };
    adapter.loadDetails = [&state](const assets::AssetItemId & item) -> std::optional<assets::AssetDetails> {
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
                                    .title = "Texture " + item.value,
                                    .fields = {"Dimensions: " + std::to_string(texture->dimension) + " x " +
                                                std::to_string(texture->dimension),},};
    };
    adapter.renderItem = [&state](const assets::AssetItem & item, bool selected) {
        const auto id = static_cast<std::uint32_t>(std::stoul(item.id.value));
        TexmapsTexturePreview preview;
        if (state.reader)
        {
            const auto texture = state.reader->readTexture(id);
            if (texture)
            {
                preview.width = texture->dimension;
                preview.height = texture->dimension;
                preview.pixels.reserve(texture->pixels.size());
                for (const auto pixel : texture->pixels)
                {
                    preview.pixels.push_back(texturePixelToMapColor(pixel));
                }
            }
        }
        drawTexturePreview(preview, id, selected);
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

void drawTexmapsPanel(TexmapsPanelState & state, bool & visible, FileBrowserState & browser)
{
    const auto adapter = makeAdapter(state, browser);
    assets::drawAssetWindow(adapter, state.assetView, visible);
}

} // namespace uo_mule::gui
