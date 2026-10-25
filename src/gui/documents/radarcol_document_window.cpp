#include "gui/documents/radarcol_document_window.h"
#include "gui/presentation/asset_browser.h"
#include "gui/presentation/asset_browser_helpers.h"
#include "gui/presentation/dialog_presentation.h"
#include "gui/presentation/iconography.h"
#include "gui/presentation/strings.h"
#include "gui/presentation/theme.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/radarcol_document.h"
#include "gui_model/input/radarcol_color_input.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include "gui_model/persistence/preferences.h"
#include "gui_model/providers/data_providers.h"
#include "uomul/radarcol/radarcol.h"
#include <algorithm>
#include <array>
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
constexpr std::size_t assetCellWidth = 44U;
constexpr std::size_t assetCellHeight = 44U;
constexpr float colorByteScale = 1.0F / static_cast<float>(radarColorByteMaximum);
constexpr float pickerColorMinimum = 0.0F;
constexpr float pickerColorMaximum = 1.0F;
constexpr float pickerWindowWidth = 430.0F;
constexpr float pickerWindowHeight = 520.0F;

[[nodiscard]] std::uint32_t radarcolRawIndex(RadarcolBrowserKind kind, std::uint32_t id) noexcept
{
    return kind == RadarcolBrowserKind::statics ? uomul::radarStaticColorOffset + id : id;
}

[[nodiscard]] std::size_t radarcolItemCount(RadarcolBrowserKind kind, std::uint32_t colorCount) noexcept
{
    if (kind == RadarcolBrowserKind::map)
    {
        return std::min(static_cast<std::size_t>(colorCount), static_cast<std::size_t>(uomul::radarMapColorCount));
    }
    if (colorCount <= uomul::radarStaticColorOffset)
    {
        return 0U;
    }
    return static_cast<std::size_t>(colorCount - uomul::radarStaticColorOffset);
}

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

int pickerHexFilter(ImGuiInputTextCallbackData * data) // NOLINT(misc-const-correctness)
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
    const auto rgb = radarcolRgb888(picker.value);
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

void drawPickerDescription(const RadarcolColorPickerState & picker)
{
    const std::string editingLabel = strings::formatEditingColors(picker.targetIndices.size());
    ImGui::TextUnformatted(editingLabel.c_str());
    if (picker.targetIndices.size() > 1U)
    {
        const char * hint = picker.mixed ? strings::mixedColorValues() : strings::multipleColorValues();
        ImGui::TextColored(theme::imguiColor(theme::mutedText), "%s", hint);
    }
}

void drawPickerColorControl(RadarcolColorPickerState & picker)
{
    const auto rgb = radarcolRgb888(picker.value);
    std::array<float, 3> color{
        static_cast<float>(rgb.red) * colorByteScale,
        static_cast<float>(rgb.green) * colorByteScale,
        static_cast<float>(rgb.blue) * colorByteScale,
    };
    if (ImGui::ColorPicker3("##RadarcolPicker", color.data(), ImGuiColorEditFlags_DisplayRGB))
    {
        const auto toByte = [](float value)
        {
            return static_cast<std::uint8_t>(std::lround(std::clamp(value, pickerColorMinimum, pickerColorMaximum) *
                                                         static_cast<float>(radarColorByteMaximum)));
        };
        updatePickerFromRgbFields(picker, toByte(color.at(0)), toByte(color.at(1)), toByte(color.at(2)));
    }
}

void drawPickerFormatControl(RadarcolColorPickerState & picker)
{
    if (!ImGui::BeginCombo(strings::formatLabel(), pickerFormatName(picker.format)))
    {
        return;
    }
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

void showPickerButtonTooltip()
{
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", strings::pick());
    }
}

void drawPickerSourceControls(const RadarcolDocumentWindowState & state, RadarcolColorPickerState & picker)
{
    const std::string pickSourceLabel = iconFontAvailable() ? iconGlyph(IconId::eyedropper) + "##RadarcolPickSource"
                                                             : strings::pickSource();
    if (ImGui::Button(pickSourceLabel.c_str()))
    {
        picker.sourceMode = true;
        picker.open = false;
    }
    showPickerButtonTooltip();
    ImGui::SameLine();
    if (ImGui::Button(strings::fromActiveTile()))
    {
        picker.value = state.activeTileColor ? state.activeTileColor().value_or(0) : 0;
        picker.error.clear();
        syncPickerBuffers(picker);
    }
}

void applyPickerChanges(RadarcolColorPickerState & picker, RadarcolDocument & document)
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

void drawPickerFooter(RadarcolColorPickerState & picker, RadarcolDocument & document)
{
    const std::string applyLabel = iconLabel(IconId::ok, "Apply");
    const std::string cancelLabel = iconLabel(IconId::cancel, "Cancel");
    beginDialogFooter({applyLabel.c_str(), cancelLabel.c_str()});
    if (dialogButton(applyLabel.c_str(), picker.error.empty()))
    {
        applyPickerChanges(picker, document);
    }
    ImGui::SameLine();
    if (dialogButton(cancelLabel.c_str()))
    {
        picker.open = false;
    }
    endDialogFooter();
}

void drawRadarcolColorPicker(RadarcolDocumentWindowState & state, RadarcolDocument & document)
{
    auto & picker = state.colorPicker;
    if (!picker.open || picker.documentId != document.id())
    {
        return;
    }

    const char * title = "Edit Radarcol###dialog.radarcol-color-picker";
    const DialogWindowSpec pickerSpec{
        .initialSize = ImVec2(pickerWindowWidth, pickerWindowHeight),
        .minimumSize = ImVec2(360.0F, 220.0F),
    };
    if (!beginDialog(title, &picker.open, pickerSpec, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    drawPickerDescription(picker);
    drawPickerColorControl(picker);
    drawPickerFormatControl(picker);
    drawPickerTextInput(picker);
    drawPickerError(picker);
    drawPickerSourceControls(state, picker);
    drawPickerFooter(picker, document);
    endDialog();
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

void drawRadarcolItem(const RadarColorProvider & provider, RadarcolBrowserKind kind, const assets::AssetItem & item)
{
    const auto index = assets::itemIndex({.value = item.id.value});
    if (!index)
    {
        return;
    }
    const auto color = provider.rawColor(radarcolRawIndex(kind, *index)).value_or(0);
    ImGui::ColorButton("##RadarcolColor",
                       colorToImVec4(color),
                       ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
                       ImVec2(swatchSize, swatchSize));
}

[[nodiscard]] std::vector<assets::AssetItem> loadRadarcolRange(const RadarColorProvider & provider,
                                                               RadarcolBrowserKind kind,
                                                               const assets::AssetRangeRequest & request,
                                                               IdFormat idFormat)
{
    std::vector<assets::AssetItem> items;
    const std::size_t end = std::min(request.first + request.count, radarcolItemCount(kind, provider.colorCount()));
    for (std::size_t index = request.first; index < end; index++)
    {
        const auto rawIndex = radarcolRawIndex(kind, static_cast<std::uint32_t>(index));
        items.push_back({
            .id = {.value = std::to_string(index)},
            .label = strings::formatId(static_cast<std::uint32_t>(index), idFormat),
            .state =
                provider.rawColor(rawIndex).has_value() ? assets::AssetItemState::available : assets::AssetItemState::empty,
            .stateMessage = {},
        });
    }
    return items;
}

[[nodiscard]] std::vector<assets::AssetSearchItem>
loadRadarcolSearchItems(const RadarColorProvider & provider, RadarcolBrowserKind kind, IdFormat idFormat)
{
    std::vector<assets::AssetSearchItem> items;
    for (std::size_t index = 0; index < radarcolItemCount(kind, provider.colorCount()); index++)
    {
        const auto color = provider.rawColor(radarcolRawIndex(kind, static_cast<std::uint32_t>(index)));
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

[[nodiscard]] std::optional<assets::AssetDetails> loadRadarcolDetails(const RadarColorProvider & provider,
                                                                      RadarcolBrowserKind kind,
                                                                      const assets::AssetItemId & id,
                                                                      IdFormat idFormat)
{
    const auto index = assets::itemIndex(id);
    if (!index)
    {
        return std::nullopt;
    }
    const auto color = provider.rawColor(radarcolRawIndex(kind, *index));
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

void drawRadarcolPreview(const RadarColorProvider & provider,
                         RadarcolBrowserKind kind,
                         const assets::AssetItemId & item,
                         float size,
                         const char * id)
{
    const auto index = assets::itemIndex(item);
    const auto color = index ? provider.rawColor(radarcolRawIndex(kind, *index)).value_or(0) : uomul::RadarColor{};
    ImGui::ColorButton(
        id, colorToImVec4(color), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(size, size));
}

void drawRadarcolPreview(const RadarcolDocument & document,
                         RadarcolBrowserKind kind,
                         const assets::AssetItemId & item,
                         float size,
                         const char * id)
{
    const auto index = assets::itemIndex(item);
    const auto color = index ? document.rawColor(radarcolRawIndex(kind, *index)).value_or(0) : uomul::RadarColor{};
    ImGui::ColorButton(
        id, colorToImVec4(color), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(size, size));
}

[[nodiscard]] std::vector<assets::AssetItem> loadRadarcolRange(const RadarcolDocument & document,
                                                               RadarcolBrowserKind kind,
                                                               const assets::AssetRangeRequest & request,
                                                               IdFormat idFormat)
{
    std::vector<assets::AssetItem> items;
    if (!document.reader())
    {
        return items;
    }
    const std::size_t end = std::min(request.first + request.count, radarcolItemCount(kind, document.reader()->colorCount()));
    for (std::size_t index = request.first; index < end; index++)
    {
        const auto color = document.rawColor(radarcolRawIndex(kind, static_cast<std::uint32_t>(index)));
        items.push_back({
            .id = {.value = std::to_string(index)},
            .label = strings::formatId(static_cast<std::uint32_t>(index), idFormat),
            .state = color.has_value() ? assets::AssetItemState::available : assets::AssetItemState::empty,
            .stateMessage = {},
        });
    }
    return items;
}

[[nodiscard]] std::vector<assets::AssetSearchItem>
loadRadarcolSearchItems(const RadarcolDocument & document, RadarcolBrowserKind kind, IdFormat idFormat)
{
    std::vector<assets::AssetSearchItem> items;
    if (!document.reader())
    {
        return items;
    }
    for (std::size_t index = 0; index < radarcolItemCount(kind, document.reader()->colorCount()); index++)
    {
        const auto color = document.rawColor(radarcolRawIndex(kind, static_cast<std::uint32_t>(index)));
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

[[nodiscard]] std::optional<assets::AssetDetails> loadRadarcolDetails(const RadarcolDocument & document,
                                                                      RadarcolBrowserKind kind,
                                                                      const assets::AssetItemId & id,
                                                                      IdFormat idFormat)
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
    const auto color = document.rawColor(radarcolRawIndex(kind, *index));
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

void drawProviderContextMenu(const RadarcolDocumentWindowState & state,
                             const RadarColorProvider & provider,
                             const assets::AssetItemId & item);

void drawRadarcolKindControl(RadarcolBrowserKind & browserKind)
{
    const auto drawKindOption = [&browserKind](IconId icon, const char * tooltip, RadarcolBrowserKind kind, const char * id)
    {
        const std::string symbol = iconFontAvailable() ? iconGlyph(icon) : tooltip;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(symbol.c_str());
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", tooltip);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton(id, browserKind == kind))
        {
            browserKind = kind;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", tooltip);
        }
    };

    drawKindOption(IconId::elevation, "Map", RadarcolBrowserKind::map, "##RadarcolMap");
    ImGui::SameLine();
    drawKindOption(IconId::pineTreeVariant, "Static", RadarcolBrowserKind::statics, "##RadarcolStatic");
}

[[nodiscard]] assets::AssetAdapter makeAdapter(RadarcolDocumentWindowState & state,
                                               const RadarColorProvider & provider,
                                               IdFormat idFormat,
                                               std::string_view viewId)
{
    const ProviderStatus status = provider.status();
    const RadarcolBrowserKind kind = state.browserKind;
    assets::AssetAdapter adapter;
    configureAssetAdapter(adapter.descriptor,
                          {
                              .viewId = std::string(viewId),
                              .windowIdentity = {},
                              .title = "Radarcol",
                              .sourceTooltip = "Source: " + std::string(providerSourceName(status.source)),
                              .cacheContext = std::to_string(static_cast<int>(kind)) + "|" +
                                              std::to_string(static_cast<int>(idFormat)) +
                                              "|revision=" + std::to_string(status.revision),
                              .source =
                                  {
                                      .kind = DocumentKind::radarcol,
                                      .contextKind = assets::AssetSourceContext::Kind::metadataView,
                                      .source = status.sourceIdentity,
                                      .contextId = std::string(viewId),
                                  },
                              .itemPicked = state.itemPicked,
                              .hoveredContext = state.hoveredContext,
                          });
    adapter.descriptor.formatSelectionId = [idFormat](const assets::AssetItemId & item)
    { return formatAssetItemId(item, idFormat); };
    adapter.descriptor.renderSelectionPreview = [&provider, kind](const assets::AssetItemId & item, float size)
    { drawRadarcolPreview(provider, kind, item, size, "##RadarcolSelectionColor"); };
    adapter.descriptor.selectionPreviewSize = swatchSize;
    adapter.descriptor.itemSourcePicked = [&state, &provider, kind](const assets::AssetItemId & item)
    {
        const auto index = assets::itemIndex(item);
        return index ? acceptRadarcolColorSource(state, provider.rawColor(radarcolRawIndex(kind, *index)).value_or(0)) : false;
    };
    if (!status.message.empty())
    {
        adapter.descriptor.sourceTooltip += "\nStatus: " + status.message;
    }
    const auto source = adapter.descriptor.source;
    adapter.makeDragPayload =
        [&provider, kind, source](const assets::AssetSelection & selection) -> std::optional<assets::AssetDragPayload>
    {
        if (selection.selected.size() != 1U)
        {
            return std::nullopt;
        }
        const auto index = assets::itemIndex(selection.selected.front());
        const auto color = index ? provider.rawColor(radarcolRawIndex(kind, *index)) : std::nullopt;
        if (!color)
        {
            return std::nullopt;
        }
        return assets::AssetDragPayload{
            .type = std::string(assets::assetValueDragPayloadType),
            .label = "Radarcol color",
            .source = source,
            .items = selection.selected,
            .values = {{.item = *index, .value = static_cast<std::uint32_t>(*color)}},
        };
    };
    adapter.descriptor.capabilities = {
        .presentations = static_cast<std::uint8_t>(assets::AssetPresentationCapability::grid) |
                         static_cast<std::uint8_t>(assets::AssetPresentationCapability::table),
        .queries = static_cast<std::uint8_t>(assets::AssetQueryCapability::filter),
        .selection = assets::AssetSelectionMode::multiple,
    };
    adapter.descriptor.fixedCellWidth = assetCellWidth;
    adapter.descriptor.fixedCellHeight = assetCellHeight;
    adapter.descriptor.gridItemSpacing = ImGui::GetStyle().ItemSpacing.y;
    adapter.descriptor.presentationTrailingControl = [&state] { drawRadarcolKindControl(state.browserKind); };
    adapter.descriptor.itemContextMenu = [&state, &provider](const assets::AssetItemId & item)
    { drawProviderContextMenu(state, provider, item); };
    adapter.descriptor.itemCount = radarcolItemCount(kind, provider.colorCount());
    adapter.loadRange = [&provider, kind, idFormat](const assets::AssetRangeRequest & request, const assets::AssetQuery &)
    { return loadRadarcolRange(provider, kind, request, idFormat); };
    adapter.loadSearchItems = [&provider, kind, idFormat] { return loadRadarcolSearchItems(provider, kind, idFormat); };
    adapter.loadDetails = [&provider, kind, idFormat](const assets::AssetItemId & id)
    { return loadRadarcolDetails(provider, kind, id, idFormat); };
    adapter.renderItem = [&provider, kind](const assets::AssetItem & item, bool) { drawRadarcolItem(provider, kind, item); };
    return adapter;
}

void copyRadarcolSelection(const RadarcolDocumentWindowState & state,
                           const assets::AssetSelection & selection,
                           const std::function<std::optional<uomul::RadarColor>(std::uint32_t)> & colorAt)
{
    if (!state.copyToClipboard || selection.selected.empty())
    {
        return;
    }

    std::string csv = "ID,RGBHex,Raw\n";
    for (const auto & item : selection.selected)
    {
        const auto index = assets::itemIndex(item);
        if (!index)
        {
            continue;
        }
        const auto color = colorAt(*index);
        if (!color)
        {
            continue;
        }
        csv += std::to_string(*index) + "," + formatRadarcolRgbHex(*color) + "," + formatRadarcolRaw(*color) + "\n";
    }
    state.copyToClipboard(csv);
}

void drawRadarcolShowMenu(const RadarcolDocumentWindowState & state,
                          const std::optional<std::uint32_t> & index,
                          RadarcolBrowserKind kind)
{
    const bool canShowTiledata = index.has_value() && static_cast<bool>(state.showInTiledata);
    if (ImGui::MenuItem("Show in Tiledata", nullptr, false, canShowTiledata))
    {
        if (index)
        {
            state.showInTiledata(*index, kind == RadarcolBrowserKind::statics);
        }
    }
    const bool canShowTexmaps = index.has_value() && static_cast<bool>(state.showInTexmaps);
    if (ImGui::MenuItem("Show in Texmaps", nullptr, false, canShowTexmaps))
    {
        if (index)
        {
            state.showInTexmaps(*index);
        }
    }
}

void drawProviderContextMenu(const RadarcolDocumentWindowState & state,
                             const RadarColorProvider & provider,
                             const assets::AssetItemId & item)
{
    const auto & selection = state.assetView.selection;
    if (ImGui::MenuItem("Copy (CSV)", "Ctrl+C", false, !selection.selected.empty()))
    {
        copyRadarcolSelection(state,
                              selection,
                              [&provider, &state](std::uint32_t index)
                              { return provider.rawColor(radarcolRawIndex(state.browserKind, index)); });
    }

    const auto index = assets::itemIndex(item);
    std::uint32_t activeTileId = 0;
    const bool canUseAsActiveTile =
        state.browserKind == RadarcolBrowserKind::map && index && *index < uomul::radarMapColorCount && state.useAsActiveTile;
    if (index)
    {
        activeTileId = *index;
    }
    if (ImGui::MenuItem("Set Active Tile", nullptr, false, canUseAsActiveTile))
    {
        state.useAsActiveTile(activeTileId);
    }
    drawRadarcolShowMenu(state, index, state.browserKind);
}

void drawDocumentContextMenu(RadarcolDocumentWindowState & state,
                             RadarcolDocument & document,
                             RadarcolBrowserKind kind,
                             const assets::AssetItemId & contextItem)
{
    const auto & selection = state.documentAssetViews.at(document.id()).selection;
    if (ImGui::MenuItem("Edit...", "Ctrl+E", false, !selection.selected.empty()))
    {
        openRadarcolColorPicker(state, document, selection);
    }

    if (ImGui::MenuItem("Copy (CSV)", "Ctrl+C", false, !selection.selected.empty()))
    {
        copyRadarcolSelection(state,
                              selection,
                              [&document, kind](std::uint32_t index)
                              { return document.rawColor(radarcolRawIndex(kind, index)); });
    }

    const auto contextIndex = assets::itemIndex(contextItem);
    std::uint32_t activeTileId = 0;
    const bool canUseAsActiveTile =
        kind == RadarcolBrowserKind::map && contextIndex && *contextIndex < uomul::radarMapColorCount && state.useAsActiveTile;
    if (contextIndex)
    {
        activeTileId = *contextIndex;
    }
    if (ImGui::MenuItem("Set Active Tile", nullptr, false, canUseAsActiveTile))
    {
        state.useAsActiveTile(activeTileId);
    }
    drawRadarcolShowMenu(state, contextIndex, kind);
}

void drawRadarcolAssetWindow(RadarcolDocumentWindowState & state,
                             RadarcolDocument & document,
                             assets::AssetWindowState & assetView,
                             bool & visible,
                             IdFormat idFormat,
                             std::string viewId,
                             std::string windowIdentity,
                             const std::function<void()> & becameVisible)
{
    const RadarcolBrowserKind kind = state.documentBrowserKinds[document.id()];
    assets::AssetAdapter adapter;
    const std::string documentViewId = std::move(viewId);
    configureAssetAdapter(adapter.descriptor,
                          {
                              .viewId = documentViewId,
                              .windowIdentity = std::move(windowIdentity),
                              .title = "Radarcol",
                              .sourceTooltip = "Source: " + document.sourcePath().string(),
                              .cacheContext = std::to_string(static_cast<int>(kind)) + "|" +
                                              std::to_string(static_cast<int>(idFormat)) +
                                              "|revision=" + std::to_string(document.displayRevision()),
                              .source =
                                  {
                                      .kind = DocumentKind::radarcol,
                                      .contextKind = assets::AssetSourceContext::Kind::document,
                                      .source = makeDocumentSourceIdentity({document.sourcePath()}),
                                      .contextId = documentViewId,
                                  },
                              .itemPicked = state.itemPicked,
                              .hoveredContext = state.hoveredContext,
                          });
    adapter.descriptor.formatSelectionId = [idFormat](const assets::AssetItemId & item)
    { return formatAssetItemId(item, idFormat); };
    adapter.descriptor.renderSelectionPreview = [&document, kind](const assets::AssetItemId & item, float size)
    { drawRadarcolPreview(document, kind, item, size, "##RadarcolSelectionColor"); };
    adapter.descriptor.selectionPreviewSize = swatchSize;
    adapter.descriptor.itemSourcePicked = [&state, &document, kind](const assets::AssetItemId & item)
    {
        const auto index = assets::itemIndex(item);
        return index ? acceptRadarcolColorSource(state, document.rawColor(radarcolRawIndex(kind, *index)).value_or(0)) : false;
    };
    const auto source = adapter.descriptor.source;
    adapter.makeDragPayload =
        [&document, kind, source](const assets::AssetSelection & selection) -> std::optional<assets::AssetDragPayload>
    {
        if (selection.selected.size() != 1U)
        {
            return std::nullopt;
        }
        const auto index = assets::itemIndex(selection.selected.front());
        const auto color = index ? document.rawColor(radarcolRawIndex(kind, *index)) : std::nullopt;
        if (!color)
        {
            return std::nullopt;
        }
        return assets::AssetDragPayload{
            .type = std::string(assets::assetValueDragPayloadType),
            .label = "Radarcol color",
            .source = source,
            .items = selection.selected,
            .values = {{.item = *index, .value = static_cast<std::uint32_t>(*color)}},
        };
    };
    adapter.descriptor.capabilities = {
        .presentations = static_cast<std::uint8_t>(assets::AssetPresentationCapability::grid) |
                         static_cast<std::uint8_t>(assets::AssetPresentationCapability::table),
        .queries = static_cast<std::uint8_t>(assets::AssetQueryCapability::filter),
        .selection = assets::AssetSelectionMode::multiple,
    };
    adapter.descriptor.itemCount = document.reader() ? radarcolItemCount(kind, document.reader()->colorCount()) : 0U;
    adapter.descriptor.fixedCellWidth = assetCellWidth;
    adapter.descriptor.fixedCellHeight = assetCellHeight;
    adapter.descriptor.gridItemSpacing = ImGui::GetStyle().ItemSpacing.y;
    adapter.descriptor.presentationTrailingControl = [&state, documentId = document.id()]
    { drawRadarcolKindControl(state.documentBrowserKinds.at(documentId)); };
    adapter.descriptor.itemContextMenu = [&state, &document, kind](const assets::AssetItemId & item)
    { drawDocumentContextMenu(state, document, kind, item); };
    adapter.descriptor.acceptedDragPayloadType = std::string(assets::assetValueDragPayloadType);
    adapter.descriptor.itemValueDropped =
        [&state, &document, kind](const assets::AssetItemId & item, const assets::AssetDragPayloadValue & payload)
    {
        if (const auto index = assets::itemIndex(item))
        {
            if (state.clearStatus)
            {
                state.clearStatus();
            }
            document.requestIntent({
                .kind = RadarcolDocumentIntentKind::editColors,
                .colorEdits =
                    {
                        {
                            .colorIndex = radarcolRawIndex(kind, *index),
                            .color = static_cast<uomul::RadarColor>(payload.value),
                        },
                    },
                .outputPath = {},
                .overwriteOutput = false,
            });
        }
    };
    adapter.descriptor.itemDoubleClicked = [&state, &document](const assets::AssetItemId & item)
    {
        assets::AssetSelection selection;
        assets::selectSingle(selection, item);
        openRadarcolColorPicker(state, document, selection);
    };
    adapter.loadRange = [&document, kind, idFormat](const assets::AssetRangeRequest & request, const assets::AssetQuery &)
    { return loadRadarcolRange(document, kind, request, idFormat); };
    adapter.loadSearchItems = [&document, kind, idFormat] { return loadRadarcolSearchItems(document, kind, idFormat); };
    adapter.loadDetails = [&document, kind, idFormat](const assets::AssetItemId & id)
    { return loadRadarcolDetails(document, kind, id, idFormat); };
    adapter.descriptor.itemStaged = [&document, kind](const assets::AssetItemId & item)
    {
        return assets::itemIndex(item)
            .transform([&document, kind](std::uint32_t index)
                       { return document.hasStagedColor(radarcolRawIndex(kind, index)); })
            .value_or(false);
    };
    adapter.descriptor.itemRevert = [&document, kind](const assets::AssetSelection & selection)
    {
        std::vector<RadarcolColorEdit> edits;
        edits.reserve(selection.selected.size());
        for (const auto & selectedItem : selection.selected)
        {
            if (const auto selectedIndex = assets::itemIndex(selectedItem))
            {
                edits.push_back({
                    .colorIndex = radarcolRawIndex(kind, *selectedIndex),
                    .color = 0,
                });
            }
        }
        document.requestIntent({
            .kind = RadarcolDocumentIntentKind::revertColors,
            .colorEdits = std::move(edits),
            .outputPath = {},
            .overwriteOutput = false,
        });
    };
    adapter.renderItem = [&document, kind](const assets::AssetItem & item, bool)
    { drawRadarcolPreview(document, kind, item.id, swatchSize, "##RadarcolColor"); };
    assets::drawAssetBrowser(adapter, assetView, visible, becameVisible);
    drawRadarcolColorPicker(state, document);
}

} // namespace

void openRadarcolColorPicker(RadarcolDocumentWindowState & state,
                             const RadarcolDocument & document,
                             const assets::AssetSelection & selection)
{
    if (!document.reader() || selection.selected.empty())
    {
        return;
    }
    if (state.clearStatus)
    {
        state.clearStatus();
    }

    RadarcolColorPickerState picker;
    picker.documentId = document.id();
    for (const auto & item : selection.selected)
    {
        if (const auto index = assets::itemIndex(item))
        {
            const auto kind = state.documentBrowserKinds[document.id()];
            picker.targetIndices.push_back(radarcolRawIndex(kind, *index));
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
    syncPickerBuffers(picker);
    state.colorPicker = std::move(picker);
}

bool acceptRadarcolColorSource(RadarcolDocumentWindowState & state, uomul::RadarColor color)
{
    if (!state.colorPicker.sourceMode)
    {
        return false;
    }
    state.colorPicker.value = color;
    state.colorPicker.sourceMode = false;
    state.colorPicker.open = true;
    state.colorPicker.error.clear();
    syncPickerBuffers(state.colorPicker);
    return true;
}

void cancelRadarcolColorSource(RadarcolDocumentWindowState & state)
{
    if (!state.colorPicker.sourceMode)
    {
        return;
    }
    state.colorPicker.sourceMode = false;
    state.colorPicker.open = true;
}

void drawRadarcolPanel(RadarcolDocumentWindowState & state,
                       const RadarColorProvider & provider,
                       bool & visible,
                       IdFormat idFormat)
{
    const auto adapter = makeAdapter(state, provider, idFormat, "view.radarcol");
    assets::drawAssetBrowser(adapter, state.assetView, visible);
}

void drawRadarcolDocumentWindow(RadarcolDocumentWindowState & state,
                                RadarcolDocument & document,
                                bool & visible,
                                IdFormat idFormat,
                                std::string windowIdentity,
                                const std::function<void()> & becameVisible)
{
    drawRadarcolAssetWindow(state,
                            document,
                            state.documentAssetViews[document.id()],
                            visible,
                            idFormat,
                            "document.radarcol." + std::to_string(document.id()),
                            std::move(windowIdentity),
                            becameVisible);
}

} // namespace uo_mule::gui
