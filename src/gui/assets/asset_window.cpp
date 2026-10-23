#include "gui/assets/asset_window.h"
#include "gui/assets/asset_window_contracts.h"
#include "gui/frontend/theme.h"
#include "gui/strings/ui_strings.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <imgui.h>
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
constexpr std::size_t maximumSelectionPreviews = 9U;

[[nodiscard]] std::string windowName(const AssetAdapterDescriptor & descriptor)
{
    return descriptor.windowIdentity.empty() ? descriptor.title + "###" + descriptor.viewId : descriptor.windowIdentity;
}

void drawActions(const std::vector<AssetAction> & actions)
{
    for (const auto & action : actions)
    {
        ImGui::BeginDisabled(!action.enabled);
        if (ImGui::Button(action.label.c_str()) && action.invoke)
        {
            action.invoke();
        }
        ImGui::EndDisabled();
        if (!action.disabledReason.empty() && !action.enabled && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", action.disabledReason.c_str());
        }
        ImGui::SameLine();
    }
    if (!actions.empty())
    {
        ImGui::NewLine();
    }
}

void drawExtensions(const std::vector<AssetExtensionSlot> & slots)
{
    for (const auto & slot : slots)
    {
        if (slot.draw)
        {
            slot.draw();
        }
    }
}

void drawQueryControls(const AssetAdapter & adapter, AssetWindowState & state)
{
    if (adapter.descriptor.capabilities.supports(AssetQueryCapability::filter))
    {
        std::array<char, filterBufferSize> filterBuffer{};
        const std::size_t length = std::min(state.query.filter.size(), filterBuffer.size() - 1U);
        std::copy_n(state.query.filter.data(), length, filterBuffer.data());
        ImGui::TextUnformatted(strings::filter());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * halfWidth);
        if (ImGui::InputText("##AssetFilter", filterBuffer.data(), filterBuffer.size()))
        {
            state.query.filter.assign(filterBuffer.data());
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Words, \"phrases\", or /regex/. Spaces mean OR, commas mean AND.");
        }
    }
    if (adapter.descriptor.capabilities.supports(AssetQueryCapability::sort))
    {
        ImGui::SameLine();
        ImGui::Checkbox(strings::ascending(), &state.query.sortAscending);
    }
}

[[nodiscard]] std::string presentationContextKey(const AssetAdapterDescriptor & descriptor)
{
    std::string key = std::to_string(static_cast<int>(descriptor.source.kind));
    key += "|" + descriptor.source.contextId;
    for (const auto & path : descriptor.source.source.canonicalPaths)
    {
        key += "|" + path.generic_string();
    }
    return key;
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
        resetContext(state);
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
    if (ImGui::RadioButton(strings::grid(), state.presentation == AssetPresentation::grid))
    {
        setPresentation(adapter, state, AssetPresentation::grid);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(strings::table(), state.presentation == AssetPresentation::table))
    {
        setPresentation(adapter, state, AssetPresentation::table);
    }
    if (adapter.descriptor.inlinePresentationControl)
    {
        ImGui::SameLine();
        adapter.descriptor.inlinePresentationControl();
    }
}

void drawItemDetails(const AssetDetails & details, bool wrap = true)
{
    ImGui::TextUnformatted(details.title.c_str());
    for (const auto & field : details.fields)
    {
        if (wrap)
        {
            ImGui::TextWrapped("%s", field.c_str());
        }
        else
        {
            ImGui::TextUnformatted(field.c_str());
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
    if (state.detailsCollapsed)
    {
        ImGui::TextUnformatted(strings::detailsCollapsed());
        ImGui::EndChild();
        return;
    }

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
    if (state.selectionDragActive && !io.MouseDown[ImGuiMouseButton_Left])
    {
        state.selectionDragActive = false;
    }
    if (!state.selectionDragActive && adapter.descriptor.capabilities.selection == AssetSelectionMode::multiple &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
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

void drawItem(const AssetAdapter & adapter,
              AssetWindowState & state,
              const AssetItem & item,
              std::size_t index,
              bool handleSelection = true)
{
    ImGui::PushID(item.id.value.c_str());
    const bool selected = contains(state.selection, item.id);
    ImGui::BeginDisabled(item.state != AssetItemState::available);
    ImVec2 itemMin;
    ImVec2 itemMax;
    if (adapter.renderItem)
    {
        adapter.renderItem(item, selected);
        itemMin = ImGui::GetItemRectMin();
        itemMax = ImGui::GetItemRectMax();
        if (handleSelection && ImGui::IsItemClicked())
        {
            updateSelectionOnClick(adapter, state, item, index);
        }
    }
    else if (ImGui::Selectable(item.label.c_str(), selected, ImGuiSelectableFlags_None, ImVec2(0.0F, 0.0F)) && handleSelection)
    {
        updateSelectionOnClick(adapter, state, item, index);
    }
    ImGui::EndDisabled();
    if (adapter.renderItem)
    {
        appendDragSelection(state, item, index, itemMin, itemMax);
        if (contains(state.selection, item.id))
        {
            drawSelectionOutline(itemMin, itemMax);
        }
    }
    drawItemTooltip(adapter, state, item);
    ImGui::PopID();
}

void drawGrid(const AssetAdapter & adapter, AssetWindowState & state)
{
    updateSelectionDragState(adapter, state);

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float cellWidth = static_cast<float>(adapter.descriptor.fixedCellWidth);
    const float cellHeight = static_cast<float>(adapter.descriptor.fixedCellHeight);
    const AssetGridLayout layout = gridLayout(adapter.descriptor.itemCount, availableWidth, cellWidth, spacing);
    if (layout.rows == 0U || layout.columns == 0U)
    {
        ImGui::TextUnformatted(strings::noItems());
        return;
    }

    const float rowHeight = cellHeight + ImGui::GetStyle().ItemSpacing.y;
    const float viewportHeight = ImGui::GetContentRegionAvail().y;
    const float contentHeight = static_cast<float>(layout.rows) * rowHeight;
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

    resetContext(state);
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
    ImGui::SetScrollY(state.scrollPosition);
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

void drawAssetWindow(const AssetAdapter & adapter,
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

    syncPresentation(adapter, state);
    AssetAdapter queryAdapter = adapter;
    resetQueryCacheIfNeeded(adapter, state);
    updateSearchItems(adapter, state);
    applyFilteredQuery(adapter, state, queryAdapter);
    if (!adapter.descriptor.sourceTooltip.empty())
    {
        const ImVec2 windowMin = ImGui::GetWindowPos();
        const ImVec2 windowMax(windowMin.x + ImGui::GetWindowWidth(), windowMin.y + ImGui::GetFrameHeight());
        if (ImGui::IsMouseHoveringRect(windowMin, windowMax))
        {
            ImGui::SetTooltip("%s", adapter.descriptor.sourceTooltip.c_str());
        }
    }

    drawActions(adapter.descriptor.actions);
    drawExtensions(adapter.descriptor.extensionSlots);
    drawQueryControls(adapter, state);
    drawPresentationControls(adapter, state);

    drawAssetContent(adapter, queryAdapter, state);
    ImGui::End();
}

} // namespace uo_mule::gui::assets
