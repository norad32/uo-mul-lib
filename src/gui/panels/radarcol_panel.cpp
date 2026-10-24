#include "gui/panels/radarcol_panel.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/presentation/strings/ui_strings.h"
#include "gui/presentation/theme/theme.h"
#include "gui/widgets/asset_window.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/radarcol_document.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include "uomul/radarcol/radarcol.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
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

void drawRadarcolItem(const RadarColorProvider & provider, const assets::AssetItem & item)
{
    const auto index = assets::itemIndex({.value = item.id.value});
    if (!index)
    {
        return;
    }
    const auto color = provider.rawColor(*index).value_or(0);
    ImGui::ColorButton("##RadarcolColor", colorToImVec4(color), ImGuiColorEditFlags_NoTooltip, ImVec2(swatchSize, swatchSize));
}

[[nodiscard]] std::vector<assets::AssetItem>
loadRadarcolRange(const RadarColorProvider & provider, const assets::AssetRangeRequest & request, IdFormat idFormat)
{
    std::vector<assets::AssetItem> items;
    const std::size_t end = std::min(request.first + request.count, static_cast<std::size_t>(provider.colorCount()));
    for (std::size_t index = request.first; index < end; index++)
    {
        items.push_back({
            .id = {.value = std::to_string(index)},
            .label = strings::formatId(static_cast<std::uint32_t>(index), idFormat),
            .state = provider.rawColor(static_cast<std::uint32_t>(index)).has_value() ? assets::AssetItemState::available
                                                                                      : assets::AssetItemState::empty,
            .stateMessage = {},
        });
    }
    return items;
}

[[nodiscard]] std::vector<assets::AssetSearchItem> loadRadarcolSearchItems(const RadarColorProvider & provider,
                                                                           IdFormat idFormat)
{
    std::vector<assets::AssetSearchItem> items;
    for (std::size_t index = 0; index < static_cast<std::size_t>(provider.colorCount()); index++)
    {
        const auto color = provider.rawColor(static_cast<std::uint32_t>(index));
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
loadRadarcolDetails(const RadarColorProvider & provider, const assets::AssetItemId & id, IdFormat idFormat)
{
    const auto index = assets::itemIndex(id);
    if (!index)
    {
        return std::nullopt;
    }
    const auto color = provider.rawColor(*index);
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
        .fieldTooltips = {},
    };
}

void drawRadarcolPreview(const RadarColorProvider & provider, const assets::AssetItemId & item, float size, const char * id)
{
    const auto index = assets::itemIndex(item);
    const auto color = index ? provider.rawColor(*index).value_or(0) : uomul::RadarColor{};
    ImGui::ColorButton(id, colorToImVec4(color), ImGuiColorEditFlags_NoTooltip, ImVec2(size, size));
}

void drawRadarcolPreview(const uomul::RadarColReader * reader, const assets::AssetItemId & item, float size, const char * id)
{
    const auto index = assets::itemIndex(item);
    const auto color = reader != nullptr && index ? reader->readRawColor(*index).value_or(0) : uomul::RadarColor{};
    ImGui::ColorButton(id, colorToImVec4(color), ImGuiColorEditFlags_NoTooltip, ImVec2(size, size));
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
        items.push_back({
            .item =
                {
                    .id = {.value = std::to_string(index)},
                    .label = formattedId,
                    .state = color ? assets::AssetItemState::available : assets::AssetItemState::empty,
                    .stateMessage = {},
                },
            .values = {"ID: " + formattedId},
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
        .fieldTooltips = {},
    };
}

[[nodiscard]] assets::AssetAdapter makeAdapter(const RadarColorProvider & provider, IdFormat idFormat, std::string viewId)
{
    const ProviderStatus status = provider.status();
    assets::AssetAdapter adapter;
    adapter.descriptor.viewId = std::move(viewId);
    adapter.descriptor.title = "Radarcol";
    adapter.descriptor.sourceTooltip = "Source: " + std::string(providerSourceName(status.source));
    adapter.descriptor.cacheContext =
        std::to_string(static_cast<int>(idFormat)) + "|revision=" + std::to_string(status.revision);
    adapter.descriptor.formatSelectionId = [idFormat](const assets::AssetItemId & item)
    {
        return assets::itemIndex(item)
            .transform([idFormat](std::uint32_t id) { return strings::formatId(id, idFormat); })
            .value_or(item.value);
    };
    adapter.descriptor.renderSelectionPreview = [&provider](const assets::AssetItemId & item, float size)
    { drawRadarcolPreview(provider, item, size, "##RadarcolSelectionColor"); };
    adapter.descriptor.selectionPreviewSize = swatchSize;
    if (!status.message.empty())
    {
        adapter.descriptor.sourceTooltip += "\nStatus: " + status.message;
    }
    adapter.descriptor.source = {
        .kind = DocumentKind::radarcol,
        .contextKind = assets::AssetSourceContext::Kind::metadataView,
        .source = status.sourceIdentity,
        .contextId = adapter.descriptor.viewId,
    };
    adapter.descriptor.capabilities = {
        .presentations = static_cast<std::uint8_t>(assets::AssetPresentationCapability::grid) |
                         static_cast<std::uint8_t>(assets::AssetPresentationCapability::table),
        .queries = static_cast<std::uint8_t>(assets::AssetQueryCapability::filter),
        .selection = assets::AssetSelectionMode::multiple,
    };
    adapter.descriptor.itemCount = provider.colorCount();
    adapter.descriptor.fixedCellWidth = assetCellWidth;
    adapter.descriptor.fixedCellHeight = assetCellHeight;
    adapter.loadRange = [&provider, idFormat](const assets::AssetRangeRequest & request, const assets::AssetQuery &)
    { return loadRadarcolRange(provider, request, idFormat); };
    adapter.loadSearchItems = [&provider, idFormat] { return loadRadarcolSearchItems(provider, idFormat); };
    adapter.loadDetails = [&provider, idFormat](const assets::AssetItemId & id)
    { return loadRadarcolDetails(provider, id, idFormat); };
    adapter.renderItem = [&provider](const assets::AssetItem & item, bool) { drawRadarcolItem(provider, item); };
    return adapter;
}

void drawRadarcolDocumentView(RadarcolDocument & document,
                              assets::AssetWindowState & assetView,
                              bool & visible,
                              const std::function<void()> & drawEditing,
                              IdFormat idFormat,
                              std::string viewId,
                              std::string windowIdentity,
                              const std::function<void()> & becameVisible)
{
    assets::AssetAdapter adapter;
    adapter.descriptor.viewId = std::move(viewId);
    adapter.descriptor.windowIdentity = std::move(windowIdentity);
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
        .contextKind = assets::AssetSourceContext::Kind::document,
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

void drawRadarcolPanel(RadarcolPanelState & state, const RadarColorProvider & provider, bool & visible, IdFormat idFormat)
{
    const auto adapter = makeAdapter(provider, idFormat, "view.radarcol");
    assets::drawAssetWindow(adapter, state.assetView, visible);
}

void drawRadarcolDocumentPanel(RadarcolPanelState & state,
                               RadarcolDocument & document,
                               bool & visible,
                               FileBrowserState & browser,
                               IdFormat idFormat,
                               std::string windowIdentity,
                               const std::function<void()> & becameVisible)
{
    drawRadarcolDocumentView(
        document,
        state.documentAssetViews[document.id()],
        visible,
        [&document, &browser] { drawEditControls(document, browser); },
        idFormat,
        "document.radarcol." + std::to_string(document.id()),
        std::move(windowIdentity),
        becameVisible);
}

} // namespace uo_mule::gui
