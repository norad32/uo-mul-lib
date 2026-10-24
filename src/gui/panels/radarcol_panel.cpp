#include "gui/panels/radarcol_panel.h"
#include "gui/dialogs/file_browser_dialog.h"
#include "gui/presentation/icons/iconography.h"
#include "gui/presentation/strings/ui_strings.h"
#include "gui/presentation/theme/theme.h"
#include "gui/widgets/asset_window.h"
#include "gui_model/dialogs/dialog_contracts.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/radarcol_document.h"
#include "gui_model/input/radarcol_color_input.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include "uomul/radarcol/radarcol.h"
#include <algorithm>
#include <cmath>
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
constexpr float colorByteScale = 1.0F / 255.0F;

[[nodiscard]] const char * pickerFormatName(RadarcolColorPickerFormat format) noexcept
{
    switch (format)
    {
        case RadarcolColorPickerFormat::rgb :
            return "RGB";
        case RadarcolColorPickerFormat::rgbHex :
            return "RGB hex";
        case RadarcolColorPickerFormat::raw :
            return "Raw RGB555";
    }
    return "RGB";
}

template <std::size_t Size>
void setPickerBuffer(std::array<char, Size> & buffer, const std::string & value)
{
    buffer.fill('\0');
    const std::size_t length = std::min(value.size(), buffer.size() - 1U);
    std::copy_n(value.data(), length, buffer.data());
}

void syncPickerBuffers(RadarcolColorPickerState & picker)
{
    const auto rgb = radarcolRgb888(picker.value);
    setPickerBuffer(picker.red, std::to_string(rgb.red));
    setPickerBuffer(picker.green, std::to_string(rgb.green));
    setPickerBuffer(picker.blue, std::to_string(rgb.blue));
    setPickerBuffer(picker.rgbHex, formatRadarcolRgbHex(picker.value));
    setPickerBuffer(picker.raw, formatRadarcolRaw(picker.value));
}

[[nodiscard]] bool validHexCharacter(char value) noexcept
{
    return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') || (value >= 'A' && value <= 'F');
}

int pickerHexFilter(ImGuiInputTextCallbackData * data)
{
    if (data->EventChar == '#' && data->CursorPos == 0 && data->BufTextLen == 0)
    {
        return 0;
    }
    return validHexCharacter(static_cast<char>(data->EventChar)) ? 0 : 1;
}

void drawPickerError(const RadarcolColorPickerState & picker)
{
    if (!picker.error.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::error), "%s", picker.error.c_str());
    }
}

void updatePickerFromRgbFields(RadarcolColorPickerState & picker, std::uint8_t red, std::uint8_t green, std::uint8_t blue)
{
    picker.value = uomul::RadarColorConverter::fromRgb888(red, green, blue);
    picker.error.clear();
    syncPickerBuffers(picker);
}

void drawPickerRgbFields(RadarcolColorPickerState & picker)
{
    auto rgb = radarcolRgb888(picker.value);
    std::uint8_t red = rgb.red;
    std::uint8_t green = rgb.green;
    std::uint8_t blue = rgb.blue;
    bool changed = false;
    changed =
        ImGui::InputScalar("Red", ImGuiDataType_U8, &red, nullptr, nullptr, "%u", ImGuiInputTextFlags_CharsDecimal) || changed;
    changed = ImGui::InputScalar("Green", ImGuiDataType_U8, &green, nullptr, nullptr, "%u", ImGuiInputTextFlags_CharsDecimal) ||
              changed;
    changed = ImGui::InputScalar("Blue", ImGuiDataType_U8, &blue, nullptr, nullptr, "%u", ImGuiInputTextFlags_CharsDecimal) ||
              changed;
    if (changed)
    {
        updatePickerFromRgbFields(picker, red, green, blue);
    }
}

void drawPickerTextInput(RadarcolColorPickerState & picker)
{
    if (picker.format == RadarcolColorPickerFormat::rgb)
    {
        drawPickerRgbFields(picker);
        return;
    }

    const std::string id = picker.format == RadarcolColorPickerFormat::rgbHex ? "RGB hex" : "Raw RGB555";
    char * buffer = picker.format == RadarcolColorPickerFormat::rgbHex ? picker.rgbHex.data() : picker.raw.data();
    const std::size_t bufferSize =
        picker.format == RadarcolColorPickerFormat::rgbHex ? picker.rgbHex.size() : picker.raw.size();
    if (ImGui::InputText(id.c_str(), buffer, bufferSize, ImGuiInputTextFlags_CallbackCharFilter, pickerHexFilter))
    {
        const auto parsed =
            picker.format == RadarcolColorPickerFormat::rgbHex ? parseRadarcolRgbHex(buffer) : parseRadarcolRaw(buffer);
        if (parsed)
        {
            picker.value = *parsed;
            picker.error.clear();
            syncPickerBuffers(picker);
        }
        else
        {
            picker.error = parsed.error();
        }
    }
}

void drawRadarcolColorPicker(RadarcolPanelState & state, RadarcolDocument & document)
{
    auto & picker = state.colorPicker;
    if (!picker.open || picker.documentId != document.id())
    {
        return;
    }

    if (picker.positionPending)
    {
        const ImGuiViewport * viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
        picker.positionPending = false;
    }
    ImGui::SetNextWindowSize(ImVec2(430.0F, 520.0F), ImGuiCond_Always);
    if (!ImGui::Begin("Radarcol Color Picker###dialog.radarcol-color-picker", &picker.open, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Editing %zu color%s", picker.targetIndices.size(), picker.targetIndices.size() == 1U ? "" : "s");
    if (picker.mixed)
    {
        ImGui::TextUnformatted("Selected colors have different values; Apply assigns one value to all.");
    }

    auto rgb = radarcolRgb888(picker.value);
    float color[3]{static_cast<float>(rgb.red) * colorByteScale,
                   static_cast<float>(rgb.green) * colorByteScale,
                   static_cast<float>(rgb.blue) * colorByteScale};
    if (ImGui::ColorPicker3("##RadarcolPicker", color, ImGuiColorEditFlags_DisplayRGB))
    {
        const auto toByte = [](float value)
        { return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F)); };
        updatePickerFromRgbFields(picker, toByte(color[0]), toByte(color[1]), toByte(color[2]));
    }

    const char * formatName = pickerFormatName(picker.format);
    if (ImGui::BeginCombo("Format", formatName))
    {
        for (const auto format :
             {RadarcolColorPickerFormat::rgb, RadarcolColorPickerFormat::rgbHex, RadarcolColorPickerFormat::raw})
        {
            const bool selected = picker.format == format;
            if (ImGui::Selectable(pickerFormatName(format), selected))
            {
                picker.format = format;
                picker.error.clear();
                syncPickerBuffers(picker);
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    drawPickerTextInput(picker);
    drawPickerError(picker);

    if (ImGui::Button("Use active tile"))
    {
        picker.value = state.activeTileColor ? state.activeTileColor().value_or(0) : 0;
        picker.error.clear();
        syncPickerBuffers(picker);
    }
    ImGui::SameLine();

    if (ImGui::Button("Pick source"))
    {
        picker.sourceMode = true;
        picker.open = false;
        if (state.statusMessage)
        {
            state.statusMessage("Pick a map tile or asset. Press Esc to cancel.");
        }
    }

    if (ImGui::Button("Apply") && picker.error.empty())
    {
        std::vector<RadarcolColorEdit> edits;
        edits.reserve(picker.targetIndices.size());
        for (const auto index : picker.targetIndices)
        {
            edits.push_back({.colorIndex = index, .color = picker.value});
        }
        document.requestIntent({
            .kind = RadarcolDocumentIntentKind::editColors,
            .colorEdits = std::move(edits),
            .outputPath = {},
            .overwriteOutput = false,
        });
        picker.open = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        picker.open = false;
    }
    ImGui::End();
}

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
        ImGui::TextUnformatted(iconLabel(IconId::readOnly, strings::configuredMetadataReadOnly()).c_str());
        return;
    }
    ImGui::TextUnformatted(document.dirty() ? "Editing" : strings::readyToEdit());
    ImGui::BeginDisabled(!document.editing() || !document.dirty());
    if (ImGui::Button(iconLabel(IconId::save, "Save").c_str()))
    {
        document.requestIntent({
            .kind = RadarcolDocumentIntentKind::save,
            .colorEdits = {},
            .outputPath = {},
            .overwriteOutput = false,
        });
    }
    ImGui::SameLine();
    if (ImGui::Button(iconLabel(IconId::cancel, strings::discardChanges()).c_str()))
    {
        document.requestIntent({
            .kind = RadarcolDocumentIntentKind::discard,
            .colorEdits = {},
            .outputPath = {},
            .overwriteOutput = false,
        });
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(iconLabel(IconId::save, strings::saveAs()).c_str()))
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

void drawRadarcolPreview(const RadarcolDocument & document, const assets::AssetItemId & item, float size, const char * id)
{
    const auto index = assets::itemIndex(item);
    const auto color = index ? document.rawColor(*index).value_or(0) : uomul::RadarColor{};
    ImGui::ColorButton(id, colorToImVec4(color), ImGuiColorEditFlags_NoTooltip, ImVec2(size, size));
}

[[nodiscard]] std::vector<assets::AssetItem>
loadRadarcolRange(const RadarcolDocument & document, const assets::AssetRangeRequest & request, IdFormat idFormat)
{
    std::vector<assets::AssetItem> items;
    if (!document.reader())
    {
        return items;
    }
    const std::size_t end = std::min(request.first + request.count, static_cast<std::size_t>(document.reader()->colorCount()));
    for (std::size_t index = request.first; index < end; index++)
    {
        const auto color = document.rawColor(static_cast<std::uint32_t>(index));
        items.push_back({
            .id = {.value = std::to_string(index)},
            .label = strings::formatId(static_cast<std::uint32_t>(index), idFormat),
            .state = color.has_value() ? assets::AssetItemState::available : assets::AssetItemState::empty,
            .stateMessage = {},
        });
    }
    return items;
}

[[nodiscard]] std::vector<assets::AssetSearchItem> loadRadarcolSearchItems(const RadarcolDocument & document, IdFormat idFormat)
{
    std::vector<assets::AssetSearchItem> items;
    if (!document.reader())
    {
        return items;
    }
    for (std::size_t index = 0; index < static_cast<std::size_t>(document.reader()->colorCount()); index++)
    {
        const auto color = document.rawColor(static_cast<std::uint32_t>(index));
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
loadRadarcolDetails(const RadarcolDocument & document, const assets::AssetItemId & id, IdFormat idFormat)
{
    if (!document.reader())
    {
        return std::nullopt;
    }
    const auto index = assets::itemIndex(id);
    if (!index)
    {
        return std::nullopt;
    }
    const auto color = document.rawColor(*index);
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
                document.hasStagedColor(*index) ? "Staged: yes" : "Staged: no",
            },
        .fieldTooltips = {},
    };
}

[[nodiscard]] assets::AssetAdapter
makeAdapter(RadarcolPanelState & state, const RadarColorProvider & provider, IdFormat idFormat, std::string viewId)
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
    adapter.descriptor.itemPicked = state.itemPicked;
    adapter.descriptor.itemSourcePicked = [&state, &provider](const assets::AssetItemId & item)
    {
        const auto index = assets::itemIndex(item);
        return index ? acceptRadarcolColorSource(state, provider.rawColor(*index).value_or(0)) : false;
    };
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

void drawDocumentContextMenu(RadarcolPanelState & state, RadarcolDocument & document)
{
    const auto & selection = state.documentAssetViews.at(document.id()).selection;
    if (!document.readOnly() && ImGui::MenuItem("Edit selected colors..."))
    {
        openRadarcolColorPicker(state, document, selection);
    }
    bool hasStaged = false;
    for (const auto & item : selection.selected)
    {
        if (const auto index = assets::itemIndex(item); index && document.hasStagedColor(*index))
        {
            hasStaged = true;
            break;
        }
    }
    if (ImGui::MenuItem("Revert selected staged colors", nullptr, false, hasStaged))
    {
        std::vector<RadarcolColorEdit> edits;
        edits.reserve(selection.selected.size());
        for (const auto & item : selection.selected)
        {
            if (const auto index = assets::itemIndex(item))
            {
                edits.push_back({.colorIndex = *index, .color = 0});
            }
        }
        document.requestIntent({
            .kind = RadarcolDocumentIntentKind::revertColors,
            .colorEdits = std::move(edits),
            .outputPath = {},
            .overwriteOutput = false,
        });
    }
}

void drawRadarcolDocumentView(RadarcolPanelState & state,
                              RadarcolDocument & document,
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
    adapter.descriptor.cacheContext =
        std::to_string(static_cast<int>(idFormat)) + "|revision=" + std::to_string(document.displayRevision());
    adapter.descriptor.formatSelectionId = [idFormat](const assets::AssetItemId & item)
    {
        return assets::itemIndex(item)
            .transform([idFormat](std::uint32_t id) { return strings::formatId(id, idFormat); })
            .value_or(item.value);
    };
    adapter.descriptor.renderSelectionPreview = [&document](const assets::AssetItemId & item, float size)
    { drawRadarcolPreview(document, item, size, "##RadarcolSelectionColor"); };
    adapter.descriptor.selectionPreviewSize = swatchSize;
    adapter.descriptor.itemSourcePicked = [&state, &document](const assets::AssetItemId & item)
    {
        const auto index = assets::itemIndex(item);
        return index ? acceptRadarcolColorSource(state, document.rawColor(*index).value_or(0)) : false;
    };
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
    adapter.descriptor.itemContextMenu = [&state, &document](const assets::AssetItemId &)
    { drawDocumentContextMenu(state, document); };
    adapter.loadRange = [&document, idFormat](const assets::AssetRangeRequest & request, const assets::AssetQuery &)
    { return loadRadarcolRange(document, request, idFormat); };
    adapter.loadSearchItems = [&document, idFormat] { return loadRadarcolSearchItems(document, idFormat); };
    adapter.loadDetails = [&document, idFormat](const assets::AssetItemId & id)
    { return loadRadarcolDetails(document, id, idFormat); };
    adapter.descriptor.itemStaged = [&document](const assets::AssetItemId & item)
    {
        return assets::itemIndex(item)
            .transform([&document](std::uint32_t index) { return document.hasStagedColor(index); })
            .value_or(false);
    };
    adapter.renderItem = [&document](const assets::AssetItem & item, bool)
    { drawRadarcolPreview(document, item.id, swatchSize, "##RadarcolColor"); };
    assets::drawAssetWindow(adapter, assetView, visible, becameVisible);
    drawRadarcolColorPicker(state, document);
}

} // namespace

void openRadarcolColorPicker(RadarcolPanelState & state, RadarcolDocument & document, const assets::AssetSelection & selection)
{
    if (document.readOnly() || !document.reader() || selection.selected.empty())
    {
        return;
    }

    RadarcolColorPickerState picker;
    picker.documentId = document.id();
    for (const auto & item : selection.selected)
    {
        if (const auto index = assets::itemIndex(item))
        {
            picker.targetIndices.push_back(*index);
        }
    }
    if (picker.targetIndices.empty())
    {
        return;
    }

    picker.value = document.rawColor(picker.targetIndices.front()).value_or(0);
    for (const auto index : picker.targetIndices)
    {
        if (document.rawColor(index).value_or(0) != picker.value)
        {
            picker.mixed = true;
            break;
        }
    }
    picker.open = true;
    picker.sourceMode = false;
    picker.positionPending = true;
    syncPickerBuffers(picker);
    state.colorPicker = std::move(picker);
}

bool acceptRadarcolColorSource(RadarcolPanelState & state, uomul::RadarColor color)
{
    if (!state.colorPicker.sourceMode)
    {
        return false;
    }
    state.colorPicker.value = color;
    state.colorPicker.sourceMode = false;
    state.colorPicker.open = true;
    state.colorPicker.positionPending = true;
    state.colorPicker.error.clear();
    syncPickerBuffers(state.colorPicker);
    return true;
}

void cancelRadarcolColorSource(RadarcolPanelState & state)
{
    if (!state.colorPicker.sourceMode)
    {
        return;
    }
    state.colorPicker.sourceMode = false;
    state.colorPicker.open = true;
    state.colorPicker.positionPending = true;
}

void drawRadarcolPanel(RadarcolPanelState & state, const RadarColorProvider & provider, bool & visible, IdFormat idFormat)
{
    const auto adapter = makeAdapter(state, provider, idFormat, "view.radarcol");
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
        state,
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
