#include "gui/views/radarcol_panel.h"
#include "gui/assets/asset_window.h"
#include "gui/assets/asset_window_contracts.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/documents/document_identity.h"
#include "gui/documents/radarcol_document.h"
#include "gui/frontend/theme.h"
#include "gui/documents/document_contracts.h"
#include "gui/persistence/preferences.h"
#include "gui/profile/data_profile.h"
#include "gui/strings/ui_strings.h"
#include "gui/support/bounded_buffer.h"
#include "uomul/radarcol/radarcol.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
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
constexpr std::uint32_t radarColorChannelMaximum = 31U;
constexpr std::uint32_t radarColorByteMaximum = 255U;
constexpr std::uint32_t radarColorByteRounding = 15U;
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

[[nodiscard]] std::string colorToHex(uomul::RadarColor color)
{
    const auto packed = static_cast<std::uint32_t>(color);
    const auto toByte = [](std::uint32_t channel)
    { return ((channel * radarColorByteMaximum) + radarColorByteRounding) / radarColorChannelMaximum; };
    return std::format("#{:02X}{:02X}{:02X}",
                       toByte((packed >> redShift) & channelMask),
                       toByte((packed >> greenShift) & channelMask),
                       toByte(packed & channelMask));
}

void loadSource(RadarcolPanelState & state)
{
    auto reader = uomul::RadarColReader::open(state.sourcePath.data());
    if (!reader)
    {
        state.reader.reset();
        state.error = strings::formatError(reader.error());
        return;
    }
    state.reader.emplace(std::move(*reader));
    state.error.clear();
}

void drawEditControls(RadarcolDocument & document, FileBrowserState & browser)
{
    if (document.readOnly())
    {
        ImGui::TextUnformatted(strings::configuredMetadataReadOnly());
        return;
    }
    if (!document.editing())
    {
        ImGui::TextUnformatted(strings::readyToEdit());
        if (ImGui::Button(strings::beginEditing()))
        {
            document.requestIntent({
                .kind = RadarcolDocumentIntentKind::beginEdit,
                .colorIndex = 0,
                .color = 0,
                .outputPath = document.sourcePath(),
                .overwriteOutput = true,
            });
        }
        ImGui::SameLine();
        if (ImGui::Button(strings::saveAs()))
        {
            requestFileBrowser(browser,
                               {
                                   .mode = FileBrowserMode::saveFile,
                                   .target = FileBrowserTarget::radarcolOutput,
                                   .initialPath = document.sourcePath(),
                                   .extension = ".mul",
                                   .title = "Save Radarcol As",
                               });
        }
    }
    if (document.editing())
    {
        ImGui::TextUnformatted("Editing");
        if (ImGui::Button("Save"))
        {
            document.requestIntent({
                .kind = RadarcolDocumentIntentKind::save,
                .colorIndex = 0,
                .color = 0,
                .outputPath = {},
                .overwriteOutput = false,
            });
        }
        ImGui::SameLine();
        if (ImGui::Button(strings::discardChanges()))
        {
            document.requestIntent({
                .kind = RadarcolDocumentIntentKind::discard,
                .colorIndex = 0,
                .color = 0,
                .outputPath = {},
                .overwriteOutput = false,
            });
        }
    }
    if (!document.error().empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::error), "%s", document.error().c_str());
    }
}

void drawRadarcolItem(const RadarcolPanelState & state, const assets::AssetItem & item)
{
    const auto index = assets::itemIndex({.value = item.id.value});
    if (!index)
    {
        return;
    }
    const auto color = state.reader ? state.reader->readRawColor(*index).value_or(0) : uomul::RadarColor{};
    ImGui::ColorButton("##RadarcolColor", colorToImVec4(color), ImGuiColorEditFlags_NoTooltip, ImVec2(swatchSize, swatchSize));
}

[[nodiscard]] std::vector<assets::AssetItem>
loadRadarcolRange(const uomul::RadarColReader * reader, const assets::AssetRangeRequest & request, IdFormat idFormat)
{
    std::vector<assets::AssetItem> items;
    if (reader == nullptr)
    {
        return items;
    }
    const std::size_t end = std::min(request.first + request.count, static_cast<std::size_t>(reader->colorCount()));
    for (std::size_t index = request.first; index < end; index++)
    {
        items.push_back({
            .id = {.value = std::to_string(index)},
            .label = strings::formatId(static_cast<std::uint32_t>(index), idFormat),
            .state = reader->readRawColor(static_cast<std::uint32_t>(index)).has_value() ? assets::AssetItemState::available
                                                                                         : assets::AssetItemState::empty,
            .stateMessage = {},
        });
    }
    return items;
}

[[nodiscard]] std::vector<assets::AssetSearchItem> loadRadarcolSearchItems(const uomul::RadarColReader * reader,
                                                                           IdFormat idFormat)
{
    std::vector<assets::AssetSearchItem> items;
    if (reader == nullptr)
    {
        return items;
    }
    for (std::size_t index = 0; index < static_cast<std::size_t>(reader->colorCount()); index++)
    {
        const auto color = reader->readRawColor(static_cast<std::uint32_t>(index));
        const auto formattedId = strings::formatId(static_cast<std::uint32_t>(index), idFormat);
        std::vector<std::string> values{"ID: " + formattedId};
        if (color)
        {
            values.push_back(std::string(strings::raw()) + ": " +
                             strings::formatId(static_cast<std::uint32_t>(*color), idFormat));
            values.push_back(std::string(strings::hexadecimal()) + ": " + colorToHex(*color));
        }
        items.push_back({
            .item =
                {
                    .id = {.value = std::to_string(index)},
                    .label = formattedId,
                    .state = color ? assets::AssetItemState::available : assets::AssetItemState::empty,
                    .stateMessage = {},
                },
            .values = std::move(values),
        });
    }
    return items;
}

[[nodiscard]] std::optional<assets::AssetDetails>
loadRadarcolDetails(const uomul::RadarColReader * reader, const assets::AssetItemId & id, IdFormat idFormat)
{
    if (reader == nullptr)
    {
        return std::nullopt;
    }
    const auto index = assets::itemIndex(id);
    if (!index)
    {
        return std::nullopt;
    }
    const auto color = reader->readRawColor(*index);
    if (!color)
    {
        return std::nullopt;
    }
    return assets::AssetDetails{
        .item = id,
        .title = "ID: " + strings::formatId(*index, idFormat),
        .fields =
            {
                std::string(strings::raw()) + ": " + strings::formatId(static_cast<std::uint32_t>(*color), idFormat),
                std::string(strings::hexadecimal()) + ": " + colorToHex(*color),
            },
    };
}

void drawRadarcolPreview(const uomul::RadarColReader * reader, const assets::AssetItemId & item, float size, const char * id)
{
    const auto index = assets::itemIndex(item);
    const auto color = reader != nullptr && index ? reader->readRawColor(*index).value_or(0) : uomul::RadarColor{};
    ImGui::ColorButton(id, colorToImVec4(color), ImGuiColorEditFlags_NoTooltip, ImVec2(size, size));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
[[nodiscard]] assets::AssetAdapter makeAdapter(RadarcolPanelState & state, IdFormat idFormat, std::string viewId)
{
    assets::AssetAdapter adapter;
    adapter.descriptor.viewId = std::move(viewId);
    adapter.descriptor.title = "Radarcol";
    adapter.descriptor.sourceTooltip = "Source: " + std::string(state.sourcePath.data());
    adapter.descriptor.cacheContext = std::to_string(static_cast<int>(idFormat));
    adapter.descriptor.formatSelectionId = [idFormat](const assets::AssetItemId & item)
    {
        return assets::itemIndex(item)
            .transform([idFormat](std::uint32_t id) { return strings::formatId(id, idFormat); })
            .value_or(item.value);
    };
    adapter.descriptor.renderSelectionPreview = [&state](const assets::AssetItemId & item, float size)
    { drawRadarcolPreview(state.reader ? &*state.reader : nullptr, item, size, "##RadarcolSelectionColor"); };
    adapter.descriptor.selectionPreviewSize = swatchSize;
    if (!state.error.empty())
    {
        adapter.descriptor.sourceTooltip += "\nError: " + state.error;
    }
    adapter.descriptor.source = {
        .kind = DocumentKind::radarcol,
        .source = makeDocumentSourceIdentity({std::filesystem::path(state.sourcePath.data())}),
        .contextId = adapter.descriptor.viewId,
    };
    adapter.descriptor.capabilities = {
        .presentations = static_cast<std::uint8_t>(assets::AssetPresentationCapability::grid) |
                         static_cast<std::uint8_t>(assets::AssetPresentationCapability::table),
        .queries = static_cast<std::uint8_t>(assets::AssetQueryCapability::filter),
        .selection = assets::AssetSelectionMode::multiple,
    };
    adapter.descriptor.itemCount = state.reader ? state.reader->colorCount() : 0U;
    adapter.descriptor.fixedCellWidth = assetCellWidth;
    adapter.descriptor.fixedCellHeight = assetCellHeight;
    adapter.loadRange = [&state, idFormat](const assets::AssetRangeRequest & request, const assets::AssetQuery &)
    { return loadRadarcolRange(state.reader ? &*state.reader : nullptr, request, idFormat); };
    adapter.loadSearchItems = [&state, idFormat]
    { return loadRadarcolSearchItems(state.reader ? &*state.reader : nullptr, idFormat); };
    adapter.loadDetails = [&state, idFormat](const assets::AssetItemId & id)
    { return loadRadarcolDetails(state.reader ? &*state.reader : nullptr, id, idFormat); };
    adapter.renderItem = [&state](const assets::AssetItem & item, bool) { drawRadarcolItem(state, item); };
    return adapter;
}

void drawRadarcolDocumentView(RadarcolDocument & document,
                              assets::AssetWindowState & assetView,
                              bool & visible,
                              const std::function<void()> & drawEditing,
                              IdFormat idFormat,
                              std::string viewId,
                              const std::function<void()> & becameVisible)
{
    assets::AssetAdapter adapter;
    adapter.descriptor.viewId = std::move(viewId);
    adapter.descriptor.title = "Radarcol";
    adapter.descriptor.sourceTooltip = "Source: " + document.sourcePath().string();
    adapter.descriptor.cacheContext = std::to_string(static_cast<int>(idFormat));
    adapter.descriptor.formatSelectionId = [idFormat](const assets::AssetItemId & item)
    {
        return assets::itemIndex(item)
            .transform([idFormat](std::uint32_t id) { return strings::formatId(id, idFormat); })
            .value_or(item.value);
    };
    adapter.descriptor.renderSelectionPreview = [&document](const assets::AssetItemId & item, float size)
    { drawRadarcolPreview(document.reader() ? &*document.reader() : nullptr, item, size, "##RadarcolSelectionColor"); };
    adapter.descriptor.selectionPreviewSize = swatchSize;
    adapter.descriptor.source = {
        .kind = DocumentKind::radarcol,
        .source = makeDocumentSourceIdentity({document.sourcePath()}),
        .contextId = adapter.descriptor.viewId,
    };
    adapter.descriptor.capabilities = {
        .presentations = static_cast<std::uint8_t>(assets::AssetPresentationCapability::grid) |
                         static_cast<std::uint8_t>(assets::AssetPresentationCapability::table),
        .queries = static_cast<std::uint8_t>(assets::AssetQueryCapability::filter),
        .selection = assets::AssetSelectionMode::multiple,
    };
    adapter.descriptor.itemCount = document.reader() ? document.reader()->colorCount() : 0U;
    adapter.descriptor.fixedCellWidth = assetCellWidth;
    adapter.descriptor.fixedCellHeight = assetCellHeight;
    adapter.descriptor.extensionSlots.push_back(assets::AssetExtensionSlot{
        .id = "editing",
        .label = "Editing",
        .draw = drawEditing,
    });
    adapter.loadRange = [&document, idFormat](const assets::AssetRangeRequest & request, const assets::AssetQuery &)
    { return loadRadarcolRange(document.reader() ? &*document.reader() : nullptr, request, idFormat); };
    adapter.loadSearchItems = [&document, idFormat]
    { return loadRadarcolSearchItems(document.reader() ? &*document.reader() : nullptr, idFormat); };
    adapter.loadDetails = [&document, idFormat](const assets::AssetItemId & id)
    { return loadRadarcolDetails(document.reader() ? &*document.reader() : nullptr, id, idFormat); };
    adapter.renderItem = [&document](const assets::AssetItem & item, bool)
    { drawRadarcolPreview(document.reader() ? &*document.reader() : nullptr, item.id, swatchSize, "##RadarcolColor"); };
    assets::drawAssetWindow(adapter, assetView, visible, becameVisible);
}

} // namespace

void syncRadarcolPanel(RadarcolPanelState & state, const DataProfile & profile)
{
    const auto path = resolvedDataFilePath(profile, DataFileRole::radarcol);
    if (path.string() == state.sourcePath.data())
    {
        return;
    }
    copyToBuffer(state.sourcePath, path);
    state.reader.reset();
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
    copyToBuffer(state.sourcePath, result.path);
    loadSource(state);
}

void drawRadarcolPanel(RadarcolPanelState & state, bool & visible, IdFormat idFormat)
{
    const auto adapter = makeAdapter(state, idFormat, "view.radarcol");
    assets::drawAssetWindow(adapter, state.assetView, visible);
}

void drawRadarcolDocumentPanel(RadarcolPanelState & state,
                               RadarcolDocument & document,
                               bool & visible,
                               FileBrowserState & browser,
                               IdFormat idFormat,
                               const std::function<void()> & becameVisible)
{
    drawRadarcolDocumentView(
        document,
        state.documentAssetViews[document.id()],
        visible,
        [&document, &browser] { drawEditControls(document, browser); },
        idFormat,
        "document.radarcol." + std::to_string(document.id()),
        becameVisible);
}

} // namespace uo_mule::gui
