#include "gui/views/radarcol_panel.h"
#include "gui/persistence/preferences.h"
#include "gui/strings/ui_strings.h"
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
#include <functional>
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

void drawSourceControls(RadarcolPanelState & state, FileBrowserState & browser, bool readOnly)
{
    ImGui::Text("Source: %s", state.sourcePath.data());
    if (readOnly)
    {
        ImGui::TextUnformatted("Configured metadata source (read-only)");
        return;
    }
    if (ImGui::Button("Load"))
    {
        requestFileBrowser(browser,
                           {.mode = FileBrowserMode::openFile,
                            .target = FileBrowserTarget::radarcolSource,
                            .initialPath = state.sourcePath.data(),
                            .extension = ".mul",
                            .title = "Open Radarcol",});
    }
}

void drawEditControls(RadarcolDocument & document, FileBrowserState & browser)
{
    if (document.readOnly())
    {
        ImGui::TextUnformatted("Configured metadata source (read-only)");
        return;
    }
    if (!document.editing())
    {
        ImGui::Text("Output: %s", document.outputPath().empty() ? "Not selected" : document.outputPath().string().c_str());
        if (ImGui::Button("Choose output"))
        {
            requestFileBrowser(browser,
                               {.mode = FileBrowserMode::saveFile,
                                .target = FileBrowserTarget::radarcolOutput,
                                .initialPath = document.outputPath(),
                                .extension = ".mul",
                                .title = "Save Radarcol As",});
        }
        ImGui::SameLine();
        bool overwriteOutput = document.overwriteOutput();
        if (ImGui::Checkbox("Overwrite output", &overwriteOutput))
        {
            document.setOverwriteOutput(overwriteOutput);
        }
        ImGui::BeginDisabled(document.outputPath().empty());
        if (ImGui::Button("Begin editing"))
        {
            document.requestIntent({.kind = RadarcolDocumentIntentKind::beginEdit,
                                    .colorIndex = 0,
                                    .color = 0,
                                    .outputPath = document.outputPath(),
                                    .overwriteOutput = document.overwriteOutput(),});
        }
        ImGui::EndDisabled();
    }
    if (document.editing())
    {
        ImGui::Text("Editing: %s", document.outputPath().string().c_str());
        if (ImGui::Button("Save changes"))
        {
            document.requestIntent({.kind = RadarcolDocumentIntentKind::save,
                                    .colorIndex = 0,
                                    .color = 0,
                                    .outputPath = {},
                                    .overwriteOutput = false,});
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard changes"))
        {
            document.requestIntent({.kind = RadarcolDocumentIntentKind::discard,
                                    .colorIndex = 0,
                                    .color = 0,
                                    .outputPath = {},
                                    .overwriteOutput = false,});
        }
    }
    if (!document.error().empty())
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "%s", document.error().c_str());
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
                                                 IdFormat idFormat,
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
    adapter.loadRange = [&state, idFormat](const assets::AssetRangeRequest & request, const assets::AssetQuery &) {
        std::vector<assets::AssetItem> items;
        if (!state.reader)
        {
            return items;
        }
        const std::size_t end = std::min(request.first + request.count, static_cast<std::size_t>(state.reader->colorCount()));
        for (std::size_t index = request.first; index < end; index++)
        {
            items.push_back({.id = {.value = std::to_string(index)}, .label = strings::formatId(static_cast<std::uint32_t>(index), idFormat),
                             .state = state.reader->readRawColor(static_cast<std::uint32_t>(index)).has_value()
                                          ? assets::AssetItemState::available
                                          : assets::AssetItemState::empty,
                             .stateMessage = {},});
        }
        return items;
    };
    adapter.loadDetails = [&state, idFormat](const assets::AssetItemId & id) -> std::optional<assets::AssetDetails> {
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
                                     .title = "Radarcol color " + strings::formatId(index, idFormat),
                                     .fields = std::vector<std::string>{"Raw value: " +
                                                                       strings::formatId(static_cast<std::uint32_t>(*color), idFormat),},};
    };
    adapter.renderItem = [&state](const assets::AssetItem & item, bool selected) { drawRadarcolItem(state, item, selected); };
    return adapter;
}

void drawRadarcolDocumentView(RadarcolDocument & document,
                              assets::AssetWindowState & assetView,
                              bool & visible,
                               const std::function<void()> & drawSource,
                               const std::function<void()> & drawEditing,
                               IdFormat idFormat,
                               std::string viewId,
                              const std::function<void()> & becameVisible)
{
    assets::AssetAdapter adapter;
    adapter.descriptor.viewId = std::move(viewId);
    adapter.descriptor.title = "Radarcol";
    adapter.descriptor.source = {.kind = DocumentKind::radarcol,
                                 .source = makeDocumentSourceIdentity({document.sourcePath()}),
                                 .contextId = adapter.descriptor.viewId,};
    adapter.descriptor.capabilities = {.presentations = static_cast<std::uint8_t>(assets::AssetPresentationCapability::grid) |
                                                       static_cast<std::uint8_t>(assets::AssetPresentationCapability::table),
                                       .queries = static_cast<std::uint8_t>(assets::AssetQueryCapability::filter),
                                       .selection = assets::AssetSelectionMode::single,};
    adapter.descriptor.itemCount = document.reader() ? document.reader()->colorCount() : 0U;
    adapter.descriptor.fixedCellWidth = assetCellWidth;
    adapter.descriptor.fixedCellHeight = assetCellHeight;
    adapter.descriptor.extensionSlots.push_back(assets::AssetExtensionSlot{
        .id = "source", .label = "Source", .draw = drawSource,
    });
    adapter.descriptor.extensionSlots.push_back(assets::AssetExtensionSlot{
        .id = "editing", .label = "Editing", .draw = drawEditing,
    });
    adapter.loadRange = [&document, idFormat](const assets::AssetRangeRequest & request, const assets::AssetQuery &) {
        std::vector<assets::AssetItem> items;
        if (!document.reader())
        {
            return items;
        }
        const std::size_t end = std::min(request.first + request.count, static_cast<std::size_t>(document.reader()->colorCount()));
        for (std::size_t index = request.first; index < end; index++)
        {
            items.push_back({.id = {.value = std::to_string(index)}, .label = strings::formatId(static_cast<std::uint32_t>(index), idFormat),
                             .state = document.reader()->readRawColor(static_cast<std::uint32_t>(index)).has_value()
                                          ? assets::AssetItemState::available
                                          : assets::AssetItemState::empty,
                             .stateMessage = {},});
        }
        return items;
    };
    adapter.loadDetails = [&document, idFormat](const assets::AssetItemId & id) -> std::optional<assets::AssetDetails> {
        if (!document.reader())
        {
            return std::nullopt;
        }
        const auto index = static_cast<std::uint32_t>(std::stoul(id.value));
        const auto color = document.reader()->readRawColor(index);
        if (!color)
        {
            return std::nullopt;
        }
        return assets::AssetDetails{.item = id,
                                      .title = "Radarcol color " + strings::formatId(index, idFormat),
                                      .fields = std::vector<std::string>{"Raw value: " +
                                                                        strings::formatId(static_cast<std::uint32_t>(*color), idFormat),},};
    };
    adapter.renderItem = [&document](const assets::AssetItem & item, bool selected) {
        const auto index = static_cast<std::uint32_t>(std::stoul(item.id.value));
        const auto color = document.reader() ? document.reader()->readRawColor(index).value_or(0) : uomul::RadarColor{};
        ImGui::ColorButton("##RadarcolColor", colorToImVec4(color), ImGuiColorEditFlags_NoTooltip, ImVec2(swatchSize, swatchSize));
        static_cast<void>(selected);
    };
    assets::drawAssetWindow(adapter, assetView, visible, becameVisible);
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
    invalidateMapColorCache(state);
    state.error.clear();
    if (!path.empty())
    {
        loadSource(state);
    }
}

void applyFileBrowserResult(RadarcolPanelState & state, const FileBrowserResult & result)
{
    if (result.target != FileBrowserTarget::radarcolSource)
    {
        return;
    }
    setPath(state.sourcePath, result.path);
    loadSource(state);
}

void drawRadarcolPanel(RadarcolPanelState & state,
                       bool & visible,
                       FileBrowserState & browser,
                       IdFormat idFormat,
                       bool readOnly)
{
    const auto adapter = makeAdapter(state, browser, readOnly, idFormat, "view.radarcol");
    assets::drawAssetWindow(adapter, state.assetView, visible);
}

void drawRadarcolDocumentPanel(RadarcolPanelState & state,
                               RadarcolDocument & document,
                                bool & visible,
                                FileBrowserState & browser,
                                IdFormat idFormat,
                                const std::function<void()> & becameVisible)
{
    drawRadarcolDocumentView(document,
                             state.documentAssetViews[document.id()],
                             visible,
                             [&document] {
                                 ImGui::Text("Source: %s", document.sourcePath().string().c_str());
                                 if (document.readOnly())
                                 {
                                     ImGui::TextUnformatted("Configured metadata source (read-only)");
                                 }
                             },
                              [&document, &browser] { drawEditControls(document, browser); },
                              idFormat,
                              "document.radarcol." + std::to_string(document.id()),
                              becameVisible);
}

} // namespace uo_mule::gui
