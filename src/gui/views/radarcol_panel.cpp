#include "gui/views/radarcol_panel.h"
#include "gui/assets/asset_window.h"
#include "gui/assets/asset_window_contracts.h"
#include "gui/context/color_conversion.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/documents/document_identity.h"
#include "gui/documents/map_workspace.h"
#include "gui/documents/radarcol_document.h"
#include "gui/metadata/document_contracts.h"
#include "gui/profile/data_profile.h"
#include "gui/views/common.h"
#include "uomul/radarcol/radarcol.h"
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

constexpr float swatchSize = 36.0F;
constexpr float channelScale = 1.0F / 31.0F;
constexpr std::uint32_t channelMask = 0x1FU;
constexpr std::uint32_t redShift = 10U;
constexpr std::uint32_t greenShift = 5U;
constexpr std::size_t assetCellWidth = 72U;
constexpr std::size_t assetCellHeight = 44U;

[[nodiscard]] ImVec4 colorToImVec4(uomul::RadarColor color) noexcept
{
    const auto packed = static_cast<std::uint32_t>(color);
    return {static_cast<float>((packed >> redShift) & channelMask) * channelScale,
            static_cast<float>((packed >> greenShift) & channelMask) * channelScale,
            static_cast<float>(packed & channelMask) * channelScale,
            1.0F};
}

void setPath(std::array<char, radarcolPathBufferSize> & buffer, const std::filesystem::path & path)
{
    const std::string value = path.string();
    const std::size_t length = std::min(value.size(), buffer.size() - 1U);
    std::memcpy(buffer.data(), value.data(), length);
    buffer.at(length) = '\0';
}

void invalidateMapColorCache(RadarcolPanelState & state) noexcept
{
    state.mapColorCacheStatus.fill(0);
    ++state.colorRevision;
}

void loadSource(RadarcolPanelState & state)
{
    auto reader = uomul::RadarColReader::open(state.sourcePath.data());
    if (!reader)
    {
        state.reader.reset();
        invalidateMapColorCache(state);
        state.error = formatError(reader.error());
        return;
    }
    state.reader.emplace(std::move(*reader));
    invalidateMapColorCache(state);
    state.error.clear();
}

void beginEditing(RadarcolPanelState & state, bool overwriteOutput)
{
    auto editor = uomul::RadarColEditor::create(std::filesystem::path(state.sourcePath.data()),
                                                std::filesystem::path(state.outputPath.data()),
                                                {.overwriteOutput = overwriteOutput});
    if (!editor)
    {
        state.error = formatError(editor.error());
        return;
    }
    state.editor.emplace(std::move(*editor));
    state.error.clear();
}

void saveChanges(RadarcolPanelState & state)
{
    if (!state.editor)
    {
        return;
    }
    const std::filesystem::path output(state.outputPath.data());
    auto committed = state.editor->commit();
    if (!committed)
    {
        state.error = formatError(committed.error());
        return;
    }
    state.editor.reset();
    setPath(state.sourcePath, output);
    loadSource(state);
}

void discardChanges(RadarcolPanelState & state)
{
    if (state.editor)
    {
        state.editor->abort();
        state.editor.reset();
    }
    state.error.clear();
}

void drawSourceControls(RadarcolPanelState & state, FileBrowserState & browser, bool readOnly)
{
    ImGui::Text("Source: %s", state.sourcePath.data());
    if (readOnly)
    {
        ImGui::TextUnformatted("Configured metadata source (read-only)");
        return;
    }
    ImGui::BeginDisabled(state.editor.has_value());
    if (ImGui::Button("Load"))
    {
        requestFileBrowser(browser,
                           {.mode = FileBrowserMode::openFile,
                            .target = FileBrowserTarget::radarcolSource,
                            .initialPath = state.sourcePath.data(),
                            .extension = ".mul",
                            .title = "Open Radarcol",});
    }
    ImGui::EndDisabled();
}

void drawEditControls(RadarcolPanelState & state, bool readOnly)
{
    if (readOnly)
    {
        return;
    }
    if (!state.editor && state.reader)
    {
        ImGui::Text("Output: %s", state.outputPath.data());
        if (ImGui::Button("Begin editing"))
        {
            beginEditing(state, state.overwriteOutput);
        }
        ImGui::SameLine();
        ImGui::Checkbox("Overwrite output", &state.overwriteOutput);
    }
    if (state.editor)
    {
        ImGui::Text("Editing: %s", state.outputPath.data());
        if (ImGui::Button("Save changes"))
        {
            saveChanges(state);
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard changes"))
        {
            discardChanges(state);
        }
    }
}

void drawRadarcolItem(const RadarcolPanelState & state, const assets::AssetItem & item, bool selected)
{
    const auto index = static_cast<std::uint32_t>(std::stoul(item.id.value));
    const auto color = state.reader ? state.reader->readRawColor(index).value_or(0) : uomul::RadarColor{};
    ImGui::ColorButton("##RadarcolColor", colorToImVec4(color), ImGuiColorEditFlags_NoTooltip, ImVec2(swatchSize, swatchSize));
    static_cast<void>(selected);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
[[nodiscard]] assets::AssetAdapter makeAdapter(RadarcolPanelState & state,
                                                 FileBrowserState & browser,
                                                 bool readOnly,
                                                 std::string viewId)
{
    assets::AssetAdapter adapter;
    adapter.descriptor.viewId = std::move(viewId);
    adapter.descriptor.title = "Radarcol";
    adapter.descriptor.source = {.kind = DocumentKind::radarcol,
                                 .source = makeDocumentSourceIdentity({std::filesystem::path(state.sourcePath.data())}),
                                 .contextId = adapter.descriptor.viewId,};
    adapter.descriptor.capabilities = {.presentations = static_cast<std::uint8_t>(assets::AssetPresentationCapability::grid) |
                                                       static_cast<std::uint8_t>(assets::AssetPresentationCapability::table),
                                       .queries = static_cast<std::uint8_t>(assets::AssetQueryCapability::filter),
                                       .selection = assets::AssetSelectionMode::single,};
    adapter.descriptor.itemCount = state.reader ? state.reader->colorCount() : 0U;
    adapter.descriptor.fixedCellWidth = assetCellWidth;
    adapter.descriptor.fixedCellHeight = assetCellHeight;
    adapter.descriptor.extensionSlots.push_back(assets::AssetExtensionSlot{
        .id = "source", .label = "Source", .draw = [&state, &browser, readOnly] {
            drawSourceControls(state, browser, readOnly);
        },
    });
    adapter.descriptor.extensionSlots.push_back(assets::AssetExtensionSlot{
        .id = "editing", .label = "Editing", .draw = [&state, readOnly] {
            drawEditControls(state, readOnly);
        },
    });
    adapter.loadRange = [&state](const assets::AssetRangeRequest & request, const assets::AssetQuery &) {
        std::vector<assets::AssetItem> items;
        if (!state.reader)
        {
            return items;
        }
        const std::size_t end = std::min(request.first + request.count, static_cast<std::size_t>(state.reader->colorCount()));
        for (std::size_t index = request.first; index < end; index++)
        {
            items.push_back({.id = {.value = std::to_string(index)}, .label = std::to_string(index),
                             .state = state.reader->readRawColor(static_cast<std::uint32_t>(index)).has_value()
                                          ? assets::AssetItemState::available
                                          : assets::AssetItemState::empty,
                             .stateMessage = {},});
        }
        return items;
    };
    adapter.loadDetails = [&state](const assets::AssetItemId & id) -> std::optional<assets::AssetDetails> {
        if (!state.reader)
        {
            return std::nullopt;
        }
        const auto index = static_cast<std::uint32_t>(std::stoul(id.value));
        const auto color = state.reader->readRawColor(index);
        if (!color)
        {
            return std::nullopt;
        }
        return assets::AssetDetails{.item = id,
                                    .title = "Radarcol color " + id.value,
                                    .fields = std::vector<std::string>{"Raw value: " +
                                                                       std::to_string(static_cast<std::uint32_t>(*color)),},};
    };
    adapter.renderItem = [&state](const assets::AssetItem & item, bool selected) { drawRadarcolItem(state, item, selected); };
    return adapter;
}

void drawRadarcolView(RadarcolPanelState & state,
                      bool & visible,
                      FileBrowserState & browser,
                      bool readOnly,
                      std::string viewId)
{
    const auto adapter = makeAdapter(state, browser, readOnly, std::move(viewId));
    assets::drawAssetWindow(adapter, state.assetView, visible);
}

} // namespace

std::optional<MapColor> radarMapDisplayColor(RadarcolPanelState & state, std::uint16_t id)
{
    if (id >= uomul::radarMapColorCount)
    {
        return std::nullopt;
    }
    const std::size_t index = id;
    if (state.mapColorCacheStatus.at(index) != 0U)
    {
        return state.mapColorCacheStatus.at(index) == 1U ? std::optional<MapColor>(state.mapColorCache.at(index)) : std::nullopt;
    }
    if (!state.reader)
    {
        return std::nullopt;
    }
    const auto radarColor = state.reader->readMapColor(id);
    if (!radarColor)
    {
        state.mapColorCacheStatus.at(index) = 2;
        return std::nullopt;
    }
    const MapColor color = radarColorToMapColor(*radarColor);
    state.mapColorCache.at(index) = color;
    state.mapColorCacheStatus.at(index) = 1;
    return color;
}

void syncRadarcolPanel(RadarcolPanelState & state, const DataProfile & profile)
{
    const auto path = resolvedDataFilePath(profile, DataFileRole::radarcol);
    if (path.string() == state.sourcePath.data())
    {
        return;
    }
    setPath(state.sourcePath, path);
    state.reader.reset();
    state.editor.reset();
    invalidateMapColorCache(state);
    state.error.clear();
    if (!path.empty())
    {
        loadSource(state);
    }
}

void applyFileBrowserResult(RadarcolPanelState & state, const FileBrowserResult & result)
{
    if (result.target != FileBrowserTarget::radarcolSource && result.target != FileBrowserTarget::radarcolOutput)
    {
        return;
    }
    setPath(result.target == FileBrowserTarget::radarcolSource ? state.sourcePath : state.outputPath, result.path);
    if (result.target == FileBrowserTarget::radarcolSource)
    {
        loadSource(state);
    }
}

void drawRadarcolPanel(RadarcolPanelState & state,
                       bool & visible,
                       FileBrowserState & browser,
                       bool readOnly)
{
    drawRadarcolView(state, visible, browser, readOnly, "view.radarcol");
}

void drawRadarcolDocumentPanel(RadarcolPanelState & state,
                               const RadarcolDocument & document,
                               DocumentAccess access,
                               bool & visible,
                               FileBrowserState & browser)
{
    const bool readOnly = access == DocumentAccess::readOnly;
    if (state.documentId != document.id || state.documentReadOnly != readOnly)
    {
        if (state.editor)
        {
            state.editor->abort();
            state.editor.reset();
        }
        state.documentId = document.id;
        state.documentReadOnly = readOnly;
        setPath(state.sourcePath, document.sourcePath);
        state.reader.reset();
        invalidateMapColorCache(state);
        state.error = document.error;
        if (document.reader)
        {
            loadSource(state);
        }
    }
    drawRadarcolView(state, visible, browser, readOnly, "document.radarcol");
}

} // namespace uo_mule::gui
