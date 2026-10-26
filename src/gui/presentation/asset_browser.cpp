#include "gui/presentation/asset_browser.h"
#include "gui/presentation/iconography.h"
#include "gui/presentation/strings.h"
#include "gui/presentation/theme.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <imgui.h>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace uo_mule::gui::assets
{
namespace
{

constexpr std::size_t filterBufferSize = 256;
constexpr float detailsHeight = 140.0F;
constexpr float detailsPadding = 12.0F;
constexpr float halfWidth = 0.5F;
constexpr float selectedBorderThickness = 2.0F;
constexpr float stagedMarkerPadding = 5.0F;
constexpr float stagedMarkerRadius = 3.0F;
constexpr std::size_t maximumSelectionPreviews = 9U;
constexpr double assetHighlightDuration = 1.5;
constexpr float assetHighlightMinimumAlpha = 0.35F;
constexpr float assetHighlightBorderThickness = 3.0F;
constexpr float assetScrollCenter = 0.5F;

[[nodiscard]] std::string windowName(const AssetAdapterDescriptor & descriptor)
{
    return descriptor.windowIdentity.empty() ? descriptor.title + "###" + descriptor.viewId : descriptor.windowIdentity;
}

void drawQueryControls(const AssetAdapter & adapter, AssetWindowState & state)
{
    if (adapter.descriptor.capabilities.supports(AssetQueryCapability::filter))
    {
        std::array<char, filterBufferSize> filterBuffer{};
        const std::size_t length = std::min(state.query.filter.size(), filterBuffer.size() - 1U);
        std::copy_n(state.query.filter.data(), length, filterBuffer.data());
        const std::string filterLabel = iconFontAvailable() ? iconGlyph(IconId::filter) : strings::filter();
        ImGui::TextUnformatted(filterLabel.c_str());
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", strings::filter());
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * halfWidth);
        if (ImGui::InputText("##AssetFilter", filterBuffer.data(), filterBuffer.size()))
        {
            state.query.filter.assign(filterBuffer.data());
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", strings::filterSyntaxTooltip());
        }
    }
}

[[nodiscard]] std::string presentationContextKey(const AssetAdapterDescriptor & descriptor)
{
    return ::uo_mule::gui::assets::presentationContextKey(descriptor.source);
}

[[nodiscard]] AssetPresentation defaultPresentation(const AssetCapabilitySet & capabilities) noexcept
{
    if (capabilities.supports(AssetPresentationCapability::grid))
    {
        return AssetPresentation::grid;
    }
    return AssetPresentation::table;
}

[[nodiscard]] bool supportsPresentation(const AssetAdapter & adapter, AssetPresentation presentation) noexcept
{
    return adapter.descriptor.capabilities.supports(
        presentation == AssetPresentation::grid ? AssetPresentationCapability::grid : AssetPresentationCapability::table);
}

void syncPresentation(const AssetAdapter & adapter, AssetWindowState & state)
{
    const std::string key = presentationContextKey(adapter.descriptor);
    if (state.presentationContext != key)
    {
        state.presentationContext = key;
        const auto requestedItem = state.requestedItem;
        resetContext(state);
        state.requestedItem = requestedItem;
        const auto stored = state.presentations.find(key);
        state.presentation = stored == state.presentations.end() ? adapter.descriptor.defaultPresentation : stored->second;
    }
    if (!supportsPresentation(adapter, state.presentation))
    {
        state.presentation = defaultPresentation(adapter.descriptor.capabilities);
    }
    state.presentations[key] = state.presentation;
}

void setPresentation(const AssetAdapter & adapter, AssetWindowState & state, AssetPresentation presentation)
{
    if (supportsPresentation(adapter, presentation))
    {
        state.presentation = presentation;
        state.presentations[state.presentationContext] = presentation;
        if (adapter.descriptor.presentationChanged)
        {
            adapter.descriptor.presentationChanged(presentation);
        }
    }
}

void drawPresentationControls(const AssetAdapter & adapter, AssetWindowState & state)
{
    if (!adapter.descriptor.capabilities.supports(AssetPresentationCapability::grid) ||
        !adapter.descriptor.capabilities.supports(AssetPresentationCapability::table))
    {
        return;
    }
    const auto drawPresentationOption =
        [&adapter, &state](IconId icon, const char * tooltip, AssetPresentation presentation, const char * id)
    {
        const std::string symbol = iconFontAvailable() ? iconGlyph(icon) : tooltip;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(symbol.c_str());
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", tooltip);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton(id, state.presentation == presentation))
        {
            setPresentation(adapter, state, presentation);
        }
    };

    drawPresentationOption(IconId::apps, strings::grid(), AssetPresentation::grid, "##AssetPresentationGrid");
    ImGui::SameLine();
    drawPresentationOption(IconId::formatListBulleted, strings::table(), AssetPresentation::table, "##AssetPresentationTable");
    if (adapter.descriptor.presentationTrailingControl)
    {
        ImGui::SameLine(0.0F, 0.0F);
        ImGui::TextUnformatted("     ");
        ImGui::SameLine(0.0F, 0.0F);
        adapter.descriptor.presentationTrailingControl();
    }
    if (adapter.descriptor.inlinePresentationControl)
    {
        ImGui::SameLine(0.0F, 0.0F);
        ImGui::TextUnformatted("     ");
        ImGui::SameLine(0.0F, 0.0F);
        adapter.descriptor.inlinePresentationControl();
    }
}

void drawItemDetails(const AssetDetails & details, bool wrap = true)
{
    if (!details.title.empty())
    {
        ImGui::TextUnformatted(details.title.c_str());
    }
    for (std::size_t index = 0; index < details.fields.size(); index++)
    {
        const auto & field = details.fields.at(index);
        if (wrap)
        {
            ImGui::TextWrapped("%s", field.c_str());
        }
        else
        {
            ImGui::TextUnformatted(field.c_str());
        }
        if (index < details.fieldTooltips.size() && !details.fieldTooltips.at(index).empty() && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", details.fieldTooltips.at(index).c_str());
        }
    }
}

[[nodiscard]] const std::optional<AssetDetails> &
cachedDetails(const AssetAdapter & adapter, AssetWindowState & state, const AssetItemId & id)
{
    const auto [iterator, inserted] = state.detailsCache.try_emplace(id.value);
    if (inserted)
    {
        iterator->second = adapter.loadDetails ? adapter.loadDetails(id) : std::nullopt;
    }
    return iterator->second;
}

void drawItemTooltip(const AssetAdapter & adapter, AssetWindowState & state, const AssetItem & item)
{
    if (!ImGui::IsItemHovered())
    {
        return;
    }

    if (item.state == AssetItemState::available && adapter.loadDetails)
    {
        const auto & details = cachedDetails(adapter, state, item.id);
        if (details)
        {
            ImGui::BeginTooltip();
            drawItemDetails(*details, false);
            ImGui::EndTooltip();
            return;
        }
    }

    if (item.state != AssetItemState::available && !item.stateMessage.empty())
    {
        ImGui::SetTooltip("%s", item.stateMessage.c_str());
    }
}

struct SelectionFieldValues
{
    std::string label;
    std::vector<std::string> values;
};

void appendLabeledValue(std::vector<SelectionFieldValues> & fields, const std::string & text)
{
    const std::size_t separator = text.find(':');
    if (separator == std::string::npos)
    {
        return;
    }
    const std::string label = text.substr(0U, separator);
    const auto field =
        std::ranges::find_if(fields, [&label](const SelectionFieldValues & value) { return value.label == label; });
    if (field == fields.end())
    {
        fields.push_back({.label = label, .values = {text.substr(separator + 1U)}});
    }
    else
    {
        field->values.push_back(text.substr(separator + 1U));
    }
}

void drawSelectionSummary(const AssetAdapter & adapter, AssetWindowState & state)
{
    const std::size_t selectedCount = state.selection.selected.size();
    if (selectedCount == 0U)
    {
        return;
    }

    const std::size_t displayedCount = std::min(selectedCount, maximumSelectionPreviews);
    std::vector<SelectionFieldValues> values;
    values.push_back({.label = "ID", .values = {}});
    for (std::size_t index = 0; index < displayedCount; index++)
    {
        const auto & selected = state.selection.selected.at(index);
        const std::string formattedId =
            adapter.descriptor.formatSelectionId ? adapter.descriptor.formatSelectionId(selected) : selected.value;
        values.front().values.push_back(formattedId);
        if (!adapter.loadDetails)
        {
            continue;
        }
        const auto & details = cachedDetails(adapter, state, selected);
        if (details)
        {
            for (const auto & field : details->fields)
            {
                appendLabeledValue(values, field);
            }
        }
    }

    for (const auto & field : values)
    {
        std::string line = field.label + ": ";
        for (std::size_t index = 0; index < field.values.size(); index++)
        {
            if (index > 0U)
            {
                line += ", ";
            }
            line += field.values.at(index);
        }
        if (selectedCount > displayedCount)
        {
            line += " and " + std::to_string(selectedCount - displayedCount) + " more";
        }
        ImGui::TextWrapped("%s", line.c_str());
    }
}

void drawDetails(const AssetAdapter & adapter, AssetWindowState & state)
{
    ImGui::BeginChild("AssetDetails", ImVec2(-detailsPadding, -detailsPadding), ImGuiChildFlags_Borders);
    const std::size_t selectedCount = state.selection.selected.size();
    const std::size_t previewCount = std::min(selectedCount, maximumSelectionPreviews);
    const float previewSpacing = ImGui::GetStyle().ItemSpacing.x;

    if (previewCount > 0U && adapter.descriptor.renderSelectionPreview && adapter.descriptor.selectionPreviewSize > 0.0F)
    {
        const float fullSize = adapter.descriptor.selectionPreviewSize;
        const float previewSize = previewCount == 1U ? fullSize : fullSize / (previewCount <= 4U ? 2.0F : 3.0F);
        const std::size_t previewColumns = previewCount > 4U ? 3U : 2U;
        ImGui::BeginGroup();
        const ImVec2 previewOrigin = ImGui::GetCursorScreenPos();
        for (std::size_t index = 0; index < previewCount; index++)
        {
            float cellSize = previewSize;
            if (previewCount > 1U)
            {
                const std::size_t column = index % previewColumns;
                const std::size_t row = index / previewColumns;
                const float cellMinX =
                    std::round(previewOrigin.x + (fullSize * static_cast<float>(column) / static_cast<float>(previewColumns)));
                const float cellMinY =
                    std::round(previewOrigin.y + (fullSize * static_cast<float>(row) / static_cast<float>(previewColumns)));
                const float cellMaxX = std::round(
                    previewOrigin.x + (fullSize * static_cast<float>(column + 1U) / static_cast<float>(previewColumns)));
                const float cellMaxY = std::round(
                    previewOrigin.y + (fullSize * static_cast<float>(row + 1U) / static_cast<float>(previewColumns)));
                ImGui::SetCursorScreenPos(ImVec2(cellMinX, cellMinY));
                cellSize = std::min(cellMaxX - cellMinX, cellMaxY - cellMinY);
            }
            ImGui::PushID(static_cast<int>(index));
            adapter.descriptor.renderSelectionPreview(state.selection.selected.at(index), cellSize);
            ImGui::PopID();
        }
        ImGui::EndGroup();
        ImGui::SameLine(0.0F, previewSpacing);
    }

    ImGui::BeginChild("SelectionDetailsText", ImVec2(0.0F, -1.0F), ImGuiChildFlags_None);
    if (state.selection.selected.size() > 1U)
    {
        drawSelectionSummary(adapter, state);
    }
    if (state.selection.selected.empty() || !adapter.loadDetails)
    {
        ImGui::TextUnformatted(strings::noItemSelected());
    }
    else if (state.selection.selected.size() == 1U)
    {
        const auto & details = cachedDetails(adapter, state, state.selection.selected.front());
        if (details)
        {
            drawItemDetails(*details);
        }
        else
        {
            ImGui::TextUnformatted(strings::itemDetailsUnavailable());
        }
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

void appendDragSelection(AssetWindowState & state,
                         const AssetItem & item,
                         std::size_t index,
                         const ImVec2 & itemMin,
                         const ImVec2 & itemMax)
{
    if (!state.selectionDragActive)
    {
        return;
    }

    const ImVec2 selectionMin(std::min(state.selectionDragStartX, ImGui::GetIO().MousePos.x),
                              std::min(state.selectionDragStartY, ImGui::GetIO().MousePos.y));
    const ImVec2 selectionMax(std::max(state.selectionDragStartX, ImGui::GetIO().MousePos.x),
                              std::max(state.selectionDragStartY, ImGui::GetIO().MousePos.y));
    if (itemMax.x < selectionMin.x || itemMin.x > selectionMax.x || itemMax.y < selectionMin.y || itemMin.y > selectionMax.y)
    {
        return;
    }

    if (!contains(state.selection, item.id))
    {
        state.selection.selected.push_back(item.id);
    }
    if (!state.selection.anchor)
    {
        state.selection.anchor = item.id;
        state.selection.anchorIndex = index;
    }
}

void updateSelectionDragState(const AssetAdapter & adapter, AssetWindowState & state)
{
    const ImGuiIO & io = ImGui::GetIO();
    if (!io.MouseDown[ImGuiMouseButton_Left])
    {
        state.selectionDragActive = false;
        state.selectionDragStartedOnItem = false;
    }
    if (!state.selectionDragActive && adapter.descriptor.capabilities.selection == AssetSelectionMode::multiple &&
        !state.selectionDragStartedOnItem && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
    {
        state.selectionDragActive = true;
        state.selectionDragStartX = io.MouseClickedPos[ImGuiMouseButton_Left].x;
        state.selectionDragStartY = io.MouseClickedPos[ImGuiMouseButton_Left].y;
        state.selectionDragBase = io.KeyCtrl ? state.selection : AssetSelection{};
    }
    if (state.selectionDragActive)
    {
        state.selection = state.selectionDragBase;
    }
}

void drawSelectionMarquee(const AssetWindowState & state)
{
    if (!state.selectionDragActive)
    {
        return;
    }
    const ImVec2 selectionMin(std::min(state.selectionDragStartX, ImGui::GetIO().MousePos.x),
                              std::min(state.selectionDragStartY, ImGui::GetIO().MousePos.y));
    const ImVec2 selectionMax(std::max(state.selectionDragStartX, ImGui::GetIO().MousePos.x),
                              std::max(state.selectionDragStartY, ImGui::GetIO().MousePos.y));
    ImGui::GetWindowDrawList()->AddRectFilled(selectionMin, selectionMax, theme::packedColor(theme::selectionOverlay));
    ImGui::GetWindowDrawList()->AddRect(selectionMin, selectionMax, theme::packedColor(theme::selection));
}

void updateSelectionOnClick(const AssetAdapter & adapter, AssetWindowState & state, const AssetItem & item, std::size_t index)
{
    state.selectionDragStartedOnItem = true;
    if (adapter.descriptor.itemSourcePicked && adapter.descriptor.itemSourcePicked(item.id))
    {
        return;
    }
    if (adapter.descriptor.itemPicked)
    {
        adapter.descriptor.itemPicked(item.id);
    }
    const ImGuiIO & io = ImGui::GetIO();
    const bool multiple = adapter.descriptor.capabilities.selection == AssetSelectionMode::multiple;
    if (!multiple)
    {
        selectSingleAt(state.selection, item.id, index);
        return;
    }

    if (io.KeyShift && state.selection.anchorIndex && adapter.loadRange)
    {
        const std::size_t anchorIndex = *state.selection.anchorIndex;
        const std::size_t first = std::min(anchorIndex, index);
        const std::size_t count = std::max(anchorIndex, index) - first + 1U;
        const auto range = adapter.loadRange({.first = first, .count = count, .presentation = state.presentation}, state.query);
        const auto anchor = state.selection.anchor;
        if (!io.KeyCtrl)
        {
            clear(state.selection);
        }
        state.selection.anchor = anchor;
        state.selection.anchorIndex = anchorIndex;
        for (const auto & rangeItem : range)
        {
            if (!contains(state.selection, rangeItem.id) && rangeItem.state == AssetItemState::available)
            {
                state.selection.selected.push_back(rangeItem.id);
            }
        }
        return;
    }

    if (io.KeyCtrl)
    {
        toggleAt(state.selection, item.id, index);
    }
    else
    {
        selectSingleAt(state.selection, item.id, index);
    }
}

void drawSelectionOutline(const ImVec2 & itemMin, const ImVec2 & itemMax)
{
    ImGui::GetWindowDrawList()->AddRect(
        itemMin, itemMax, theme::packedColor(theme::selection), 0.0F, ImDrawFlags_None, selectedBorderThickness);
}

struct AssetItemInteraction
{
    ImVec2 minimum;
    ImVec2 maximum;
    bool rendered = false;
    bool doubleClicked = false;
    bool rightClicked = false;
};

AssetItemInteraction drawItemControl(const AssetAdapter & adapter,
                                     AssetWindowState & state,
                                     const AssetItem & item,
                                     std::size_t index,
                                     bool handleSelection)
{
    AssetItemInteraction interaction;
    ImGui::BeginDisabled(item.state != AssetItemState::available);
    if (adapter.renderItem)
    {
        interaction.rendered = true;
        adapter.renderItem(item, contains(state.selection, item.id));
        interaction.minimum = ImGui::GetItemRectMin();
        interaction.maximum = ImGui::GetItemRectMax();
        if (handleSelection && ImGui::IsItemClicked())
        {
            updateSelectionOnClick(adapter, state, item, index);
        }
        interaction.doubleClicked =
            handleSelection && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered();
        interaction.rightClicked = handleSelection && ImGui::IsItemClicked(ImGuiMouseButton_Right);
    }
    else if (ImGui::Selectable(
                 item.label.c_str(), contains(state.selection, item.id), ImGuiSelectableFlags_None, ImVec2(0.0F, 0.0F)) &&
             handleSelection)
    {
        updateSelectionOnClick(adapter, state, item, index);
    }
    else
    {
        interaction.rightClicked = handleSelection && ImGui::IsItemClicked(ImGuiMouseButton_Right);
    }
    ImGui::EndDisabled();
    return interaction;
}

void drawItemDragSource(const AssetAdapter & adapter, const AssetWindowState & state)
{
    if (!adapter.makeDragPayload || !ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
    {
        return;
    }
    const auto payload = adapter.makeDragPayload(state.selection);
    if (payload && payload->values.size() == 1U)
    {
        ImGui::SetDragDropPayload(payload->type.c_str(), payload->values.data(), sizeof(AssetDragPayloadValue));
        ImGui::TextUnformatted(payload->label.empty() ? "Asset value" : payload->label.c_str());
    }
    ImGui::EndDragDropSource();
}

void drawItemDecorations(const AssetAdapter & adapter,
                         AssetWindowState & state,
                         const AssetItem & item,
                         std::size_t index,
                         const AssetItemInteraction & interaction)
{
    if (!interaction.rendered)
    {
        return;
    }
    appendDragSelection(state, item, index, interaction.minimum, interaction.maximum);
    if (adapter.descriptor.itemStaged && adapter.descriptor.itemStaged(item.id))
    {
        const ImVec2 marker(interaction.maximum.x - stagedMarkerPadding, interaction.minimum.y + stagedMarkerPadding);
        ImGui::GetWindowDrawList()->AddCircleFilled(marker, stagedMarkerRadius, theme::packedColor(theme::mauve));
    }
    if (contains(state.selection, item.id))
    {
        drawSelectionOutline(interaction.minimum, interaction.maximum);
    }
    if (state.highlightedItem && *state.highlightedItem == item.id && ImGui::GetTime() < state.highlightUntil)
    {
        const float pulse = 0.5F + (0.5F * std::sin(static_cast<float>(ImGui::GetTime() * 10.0)));
        const auto highlightColor = theme::withAlpha(theme::yellow, assetHighlightMinimumAlpha + (pulse * 0.65F));
        ImGui::GetWindowDrawList()->AddRect(interaction.minimum,
                                            interaction.maximum,
                                            theme::packedColor(highlightColor),
                                            0.0F,
                                            ImDrawFlags_None,
                                            assetHighlightBorderThickness);
    }
}

void drawItemDropTarget(const AssetAdapter & adapter, const AssetItem & item)
{
    if (!adapter.descriptor.itemValueDropped || adapter.descriptor.acceptedDragPayloadType.empty() ||
        !ImGui::BeginDragDropTarget())
    {
        return;
    }
    if (const ImGuiPayload * payload = ImGui::AcceptDragDropPayload(adapter.descriptor.acceptedDragPayloadType.c_str());
        payload != nullptr && payload->DataSize == sizeof(AssetDragPayloadValue))
    {
        const auto & value = *static_cast<const AssetDragPayloadValue *>(payload->Data);
        adapter.descriptor.itemValueDropped(item.id, value);
    }
    ImGui::EndDragDropTarget();
}

void drawItemContextMenu(const AssetAdapter & adapter, const AssetWindowState & state, const AssetItem & item)
{
    if ((adapter.descriptor.itemContextMenu || adapter.descriptor.itemRevert) &&
        ImGui::BeginPopupContextItem("AssetItemContext", ImGuiMouseButton_Right))
    {
        if (adapter.descriptor.itemContextMenu)
        {
            adapter.descriptor.itemContextMenu(item.id);
        }
        if (adapter.descriptor.itemRevert && adapter.descriptor.itemStaged)
        {
            const bool canRevert = hasStagedSelection(state.selection, adapter.descriptor.itemStaged);
            if (ImGui::MenuItem("Revert", nullptr, false, canRevert))
            {
                adapter.descriptor.itemRevert(state.selection);
            }
        }
        ImGui::EndPopup();
    }
}

void drawItem(const AssetAdapter & adapter,
              AssetWindowState & state,
              const AssetItem & item,
              std::size_t index,
              bool handleSelection = true)
{
    ImGui::PushID(item.id.value.c_str());
    const AssetItemInteraction interaction = drawItemControl(adapter, state, item, index, handleSelection);
    if (interaction.doubleClicked && adapter.descriptor.itemDoubleClicked)
    {
        adapter.descriptor.itemDoubleClicked(item.id);
    }
    if (interaction.rightClicked && !contains(state.selection, item.id))
    {
        selectSingleAt(state.selection, item.id, index);
    }
    drawItemDragSource(adapter, state);
    drawItemDecorations(adapter, state, item, index, interaction);
    drawItemDropTarget(adapter, item);
    drawItemContextMenu(adapter, state, item);
    drawItemTooltip(adapter, state, item);
    ImGui::PopID();
}

void drawGrid(const AssetAdapter & adapter, AssetWindowState & state)
{
    updateSelectionDragState(adapter, state);

    const float spacing = adapter.descriptor.gridItemSpacing.value_or(ImGui::GetStyle().ItemSpacing.x);
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float cellWidth = static_cast<float>(adapter.descriptor.fixedCellWidth);
    const float cellHeight = static_cast<float>(adapter.descriptor.fixedCellHeight);
    const AssetGridLayout layout = gridLayout(adapter.descriptor.itemCount, availableWidth, cellWidth, spacing);
    if (layout.rows == 0U || layout.columns == 0U)
    {
        ImGui::TextUnformatted(strings::noItems());
        return;
    }

    const float rowSpacing = adapter.descriptor.gridItemSpacing.value_or(ImGui::GetStyle().ItemSpacing.y);
    const float rowHeight = cellHeight + rowSpacing;
    const float viewportHeight = ImGui::GetContentRegionAvail().y;
    const float contentHeight = static_cast<float>(layout.rows) * rowHeight;
    if (state.requestedItemIndex)
    {
        const std::size_t targetRow = *state.requestedItemIndex / layout.columns;
        ImGui::SetScrollY(static_cast<float>(targetRow) * rowHeight);
        state.requestedItemIndex.reset();
    }
    const float scrollY = ImGui::GetScrollY();
    const std::size_t firstRow = std::min(layout.rows, static_cast<std::size_t>(std::floor(scrollY / rowHeight)));
    const std::size_t visibleRows = static_cast<std::size_t>(std::ceil(viewportHeight / rowHeight)) + 1U;
    const std::size_t lastRow = std::min(layout.rows, firstRow + visibleRows);
    const float startX = ImGui::GetCursorPosX();
    const float startY = ImGui::GetCursorPosY();
    ImGui::Dummy(ImVec2(0.0F, contentHeight));
    ImGui::SetCursorPosY(startY + (static_cast<float>(firstRow) * rowHeight));

    const std::size_t first = firstRow * layout.columns;
    const std::size_t count = std::min((lastRow - firstRow) * layout.columns, adapter.descriptor.itemCount - first);
    const AssetRangeRequest request{.first = first, .count = count, .presentation = AssetPresentation::grid};
    const std::vector<AssetItem> items = adapter.loadRange ? adapter.loadRange(request, state.query) : std::vector<AssetItem>{};
    for (std::size_t row = firstRow; row < lastRow; row++)
    {
        for (std::size_t column = 0; column < layout.columns; column++)
        {
            const std::size_t itemIndex = (row * layout.columns) + column;
            if (itemIndex < adapter.descriptor.itemCount)
            {
                ImGui::SetCursorPos(ImVec2(startX + (static_cast<float>(column) * (cellWidth + spacing)),
                                           startY + (static_cast<float>(row) * rowHeight)));
                const std::size_t loadedIndex = itemIndex - first;
                if (loadedIndex < items.size())
                {
                    drawItem(adapter, state, items.at(loadedIndex), itemIndex);
                }
                else
                {
                    ImGui::TextUnformatted(strings::loading());
                }
            }
        }
    }

    if (state.selectionDragActive)
    {
        drawSelectionMarquee(state);
    }
}

void drawTableRow(const AssetAdapter & adapter, AssetWindowState & state, const AssetItem & item, std::size_t index)
{
    ImGui::PushID(item.id.value.c_str());
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const bool selected = contains(state.selection, item.id);
    const ImVec2 rowCursor = ImGui::GetCursorPos();
    const ImVec2 previewMin = ImGui::GetCursorScreenPos();
    const ImVec2 previewMax(previewMin.x + static_cast<float>(adapter.descriptor.fixedCellWidth),
                            previewMin.y + static_cast<float>(adapter.descriptor.fixedCellHeight));
    const bool previewHovered = ImGui::IsMouseHoveringRect(previewMin, previewMax);
    const float rowHeight =
        std::max(static_cast<float>(adapter.descriptor.fixedCellHeight), ImGui::GetTextLineHeightWithSpacing() * 3.0F);
    if (ImGui::Selectable("##AssetTableRow",
                          selected,
                          ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
                          ImVec2(0.0F, rowHeight)) &&
        !previewHovered)
    {
        updateSelectionOnClick(adapter, state, item, index);
    }
    const ImVec2 rowMin = ImGui::GetItemRectMin();
    const ImVec2 rowMax = ImGui::GetItemRectMax();
    ImGui::SetItemAllowOverlap();
    appendDragSelection(state, item, index, rowMin, rowMax);
    ImGui::SetCursorPos(rowCursor);
    drawItem(adapter, state, item, index);

    ImGui::TableNextColumn();
    if (item.state != AssetItemState::available)
    {
        ImGui::TextUnformatted(item.stateMessage.empty() ? strings::loading() : item.stateMessage.c_str());
    }
    else if (!adapter.loadDetails)
    {
        ImGui::TextUnformatted(strings::itemDetailsUnavailable());
    }
    else
    {
        const auto & details = cachedDetails(adapter, state, item.id);
        if (details)
        {
            drawItemDetails(*details);
        }
        else
        {
            ImGui::TextUnformatted(strings::itemDetailsUnavailable());
        }
    }
    ImGui::PopID();
}

void drawTable(const AssetAdapter & adapter, AssetWindowState & state)
{
    const ImGuiIO & io = ImGui::GetIO();
    updateSelectionDragState(adapter, state);

    const float rowHeight =
        std::max(static_cast<float>(adapter.descriptor.fixedCellHeight), ImGui::GetTextLineHeightWithSpacing() * 3.0F);
    const float tableHeight = ImGui::GetContentRegionAvail().y;
    if (adapter.descriptor.itemCount == 0U)
    {
        ImGui::TextUnformatted(strings::noItems());
        return;
    }

    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg + ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("AssetTable", 2, flags, ImVec2(0.0F, tableHeight)))
    {
        return;
    }
    ImGui::TableSetupColumn(
        "##AssetPreviewColumn", ImGuiTableColumnFlags_WidthFixed, static_cast<float>(adapter.descriptor.fixedCellWidth));
    ImGui::TableSetupColumn("##AssetDetailsColumn", ImGuiTableColumnFlags_WidthStretch);

    if (!state.selectionDragActive && adapter.descriptor.capabilities.selection == AssetSelectionMode::multiple &&
        ImGui::TableGetHoveredColumn() >= 0 && ImGui::TableGetHoveredColumn() < 2 &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        state.selectionDragActive = true;
        state.selectionDragStartX = io.MouseClickedPos[ImGuiMouseButton_Left].x;
        state.selectionDragStartY = io.MouseClickedPos[ImGuiMouseButton_Left].y;
        state.selectionDragBase = io.KeyCtrl ? state.selection : AssetSelection{};
    }

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(adapter.descriptor.itemCount), rowHeight);
    if (state.requestedItemIndex)
    {
        clipper.IncludeItemByIndex(static_cast<int>(*state.requestedItemIndex));
    }
    while (clipper.Step())
    {
        const std::size_t first = static_cast<std::size_t>(clipper.DisplayStart);
        const std::size_t count = static_cast<std::size_t>(clipper.DisplayEnd - clipper.DisplayStart);
        const AssetRangeRequest request{.first = first, .count = count, .presentation = AssetPresentation::table};
        const std::vector<AssetItem> items =
            adapter.loadRange ? adapter.loadRange(request, state.query) : std::vector<AssetItem>{};
        for (std::size_t index = 0; index < count; index++)
        {
            if (index < items.size())
            {
                drawTableRow(adapter, state, items.at(index), first + index);
                if (state.requestedItemIndex && *state.requestedItemIndex == first + index)
                {
                    ImGui::SetScrollHereY(assetScrollCenter);
                    state.requestedItemIndex.reset();
                }
            }
            else
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(strings::loading());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(strings::loading());
            }
        }
    }
    ImGui::EndTable();

    if (state.selectionDragActive)
    {
        drawSelectionMarquee(state);
    }
}

void resetQueryCacheIfNeeded(const AssetAdapter & adapter, AssetWindowState & state)
{
    const std::string queryContext = presentationContextKey(adapter.descriptor) + "|" + adapter.descriptor.cacheContext;
    if (state.queryCacheContext == queryContext)
    {
        return;
    }

    const auto requestedItem = state.requestedItem;
    resetContext(state);
    state.requestedItem = requestedItem;
    state.queryCacheContext = queryContext;
}

void updateSearchItems(const AssetAdapter & adapter, AssetWindowState & state)
{
    if (adapter.loadSearchItems && !state.searchItemsValid &&
        (!state.query.filter.empty() || adapter.descriptor.queryWhenEmpty))
    {
        state.searchItems = adapter.loadSearchItems();
        state.searchItemsValid = true;
    }
}

void applyFilteredQuery(const AssetAdapter & adapter, AssetWindowState & state, AssetAdapter & queryAdapter)
{
    if ((!state.query.filter.empty() || adapter.descriptor.queryWhenEmpty) && adapter.loadSearchItems)
    {
        if (!state.queryCacheValid || state.queryCacheFilter != state.query.filter)
        {
            state.queryCacheFilter = state.query.filter;
            state.filteredItems.clear();
            for (const auto & searchItem : state.searchItems)
            {
                if (filterMatches(state.query, searchItem.values))
                {
                    state.filteredItems.push_back(searchItem.item);
                }
            }
            state.queryCacheValid = true;
        }
        queryAdapter.descriptor.itemCount = state.filteredItems.size();
        queryAdapter.loadRange = [&state](const AssetRangeRequest & request, const AssetQuery &)
        {
            const std::size_t first = std::min(request.first, state.filteredItems.size());
            const std::size_t last = std::min(first + request.count, state.filteredItems.size());
            return std::vector<AssetItem>(state.filteredItems.begin() + static_cast<std::ptrdiff_t>(first),
                                          state.filteredItems.begin() + static_cast<std::ptrdiff_t>(last));
        };
    }
}

void drawAssetContent(const AssetAdapter & adapter, const AssetAdapter & queryAdapter, AssetWindowState & state)
{
    const float remainingHeight = std::max(0.0F, ImGui::GetContentRegionAvail().y);
    const float reservedDetailsHeight = std::min(detailsHeight, remainingHeight);
    const float contentHeight = std::max(0.0F, remainingHeight - reservedDetailsHeight);
    ImGui::BeginChild("AssetContent", ImVec2(-detailsPadding, contentHeight), ImGuiChildFlags_Borders);
    if (state.scrollPositionRestorePending)
    {
        ImGui::SetScrollY(state.scrollPosition);
        state.scrollPositionRestorePending = false;
    }
    if (state.presentation == AssetPresentation::grid &&
        adapter.descriptor.capabilities.supports(AssetPresentationCapability::grid) && adapter.descriptor.fixedCellWidth > 0U)
    {
        drawGrid(queryAdapter, state);
    }
    else if (state.presentation == AssetPresentation::table &&
             adapter.descriptor.capabilities.supports(AssetPresentationCapability::table))
    {
        drawTable(queryAdapter, state);
    }
    else
    {
        ImGui::TextUnformatted(strings::assetViewUnavailable());
    }
    ImGui::EndChild();
    state.scrollPosition = ImGui::GetScrollY();
    drawDetails(adapter, state);
}

void selectAllAssetItems(const AssetAdapter & adapter, AssetWindowState & state)
{
    if (!adapter.loadRange || adapter.descriptor.itemCount == 0U)
    {
        return;
    }
    const auto items = adapter.loadRange(
        {.first = 0U, .count = adapter.descriptor.itemCount, .presentation = state.presentation}, state.query);
    state.selection.selected.clear();
    for (const auto & item : items)
    {
        if (item.state == AssetItemState::available)
        {
            state.selection.selected.push_back(item.id);
        }
    }
    if (state.selection.selected.empty())
    {
        state.selection.anchor.reset();
        state.selection.anchorIndex.reset();
    }
    else
    {
        state.selection.anchor = state.selection.selected.front();
        state.selection.anchorIndex = 0U;
    }
}

void clearAssetSelection(AssetWindowState & state)
{
    state.selection = {};
}

void invertAssetSelection(const AssetAdapter & adapter, AssetWindowState & state)
{
    if (!adapter.loadRange || adapter.descriptor.itemCount == 0U)
    {
        return;
    }
    const auto items = adapter.loadRange(
        {.first = 0U, .count = adapter.descriptor.itemCount, .presentation = state.presentation}, state.query);
    const AssetSelection previous = state.selection;
    state.selection = {};
    for (const auto & item : items)
    {
        if (item.state == AssetItemState::available && !contains(previous, item.id))
        {
            state.selection.selected.push_back(item.id);
        }
    }
    if (!state.selection.selected.empty())
    {
        state.selection.anchor = state.selection.selected.front();
        state.selection.anchorIndex = 0U;
    }
}

} // namespace

AssetGridLayout gridLayout(std::size_t itemCount, float availableWidth, float cellWidth, float spacing) noexcept
{
    if (itemCount == 0U)
    {
        return {};
    }
    const float effectiveCellWidth = std::max(1.0F, cellWidth) + std::max(0.0F, spacing);
    const auto columns = static_cast<std::size_t>(
        std::max(1.0F, std::floor((std::max(0.0F, availableWidth) + std::max(0.0F, spacing)) / effectiveCellWidth)));
    return {.columns = columns, .rows = (itemCount + columns - 1U) / columns};
}

namespace
{
void processRequestedItem(const AssetAdapter & adapter, AssetWindowState & state, const AssetAdapter & queryAdapter)
{
    if (!state.requestedItem)
    {
        return;
    }
    const AssetItemId requestedItem = *state.requestedItem;
    bool itemVisible = false;
    std::optional<std::size_t> requestedIndex;
    if (!state.query.filter.empty() || adapter.descriptor.queryWhenEmpty)
    {
        const auto match = std::ranges::find(state.filteredItems, requestedItem, &AssetItem::id);
        itemVisible = match != state.filteredItems.end();
        if (itemVisible)
        {
            requestedIndex = static_cast<std::size_t>(std::distance(state.filteredItems.begin(), match));
        }
    }
    else if (const auto index = itemIndex(requestedItem);
             index && *index < queryAdapter.descriptor.itemCount && queryAdapter.loadRange)
    {
        const auto items =
            queryAdapter.loadRange({.first = *index, .count = 1U, .presentation = state.presentation}, state.query);
        itemVisible = !items.empty() && items.front().state == AssetItemState::available && items.front().id == requestedItem;
        requestedIndex = *index;
    }
    if (itemVisible)
    {
        state.highlightedItem = requestedItem;
        state.highlightUntil = ImGui::GetTime() + assetHighlightDuration;
        state.requestedItemIndex = requestedIndex;
    }
    else if (state.focusResult)
    {
        state.focusResult(false);
    }
    state.requestedItem.reset();
}

void expireAssetHighlight(AssetWindowState & state)
{
    if (state.highlightedItem && ImGui::GetTime() >= state.highlightUntil)
    {
        state.highlightedItem.reset();
    }
}

void installSelectionCommands(const AssetAdapter & queryAdapter, AssetWindowState & state)
{
    state.selectAll = [queryAdapter, &state] { selectAllAssetItems(queryAdapter, state); };
    state.selectNone = [&state] { clearAssetSelection(state); };
    state.invertSelection = [queryAdapter, &state] { invertAssetSelection(queryAdapter, state); };
}

void dispatchAssetSelectionShortcuts(const AssetWindowState & state)
{
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || ImGui::GetIO().WantTextInput ||
        !ImGui::GetIO().KeyCtrl)
    {
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_A, false))
    {
        if (ImGui::GetIO().KeyShift)
        {
            state.selectNone();
        }
        else
        {
            state.selectAll();
        }
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_I, false))
    {
        state.invertSelection();
    }
}

} // namespace

void drawAssetBrowser(const AssetAdapter & adapter,
                      AssetWindowState & state,
                      bool & visible,
                      const std::function<void()> & becameVisible)
{
    const std::string title = windowName(adapter.descriptor);
    if (!ImGui::Begin(title.c_str(), &visible))
    {
        ImGui::End();
        return;
    }

    if (becameVisible)
    {
        becameVisible();
    }
    const bool windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (adapter.descriptor.hoveredContext && (windowHovered || windowFocused))
    {
        adapter.descriptor.hoveredContext(adapter.descriptor.title);
    }

    syncPresentation(adapter, state);
    AssetAdapter queryAdapter = adapter;
    resetQueryCacheIfNeeded(adapter, state);
    updateSearchItems(adapter, state);
    applyFilteredQuery(adapter, state, queryAdapter);
    processRequestedItem(adapter, state, queryAdapter);
    expireAssetHighlight(state);
    installSelectionCommands(queryAdapter, state);
    dispatchAssetSelectionShortcuts(state);
    if (!adapter.descriptor.sourceTooltip.empty())
    {
        const ImVec2 windowMin = ImGui::GetWindowPos();
        const ImVec2 windowMax(windowMin.x + ImGui::GetWindowWidth(), windowMin.y + ImGui::GetFrameHeight());
        if (ImGui::IsMouseHoveringRect(windowMin, windowMax))
        {
            ImGui::SetTooltip("%s", adapter.descriptor.sourceTooltip.c_str());
        }
    }

    drawQueryControls(adapter, state);
    drawPresentationControls(adapter, state);

    drawAssetContent(adapter, queryAdapter, state);
    ImGui::End();
}

} // namespace uo_mule::gui::assets
