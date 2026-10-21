#include "gui/assets/asset_window.h"
#include "gui/assets/asset_window_contracts.h"
#include "gui/strings/ui_strings.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <imgui.h>
#include <string>
#include <vector>

namespace uo_mule::gui::assets
{
namespace
{

constexpr std::size_t filterBufferSize = 256;
constexpr float headerHeight = 128.0F;
constexpr float detailsHeight = 140.0F;

[[nodiscard]] std::string windowName(const AssetAdapterDescriptor & descriptor)
{
    return descriptor.title + "###" + descriptor.viewId;
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
        if (ImGui::InputText(strings::filter(), filterBuffer.data(), filterBuffer.size()))
        {
            state.query.filter.assign(filterBuffer.data());
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
    return adapter.descriptor.capabilities.supports(presentation == AssetPresentation::grid
                                                        ? AssetPresentationCapability::grid
                                                        : AssetPresentationCapability::table);
}

void syncPresentation(const AssetAdapter & adapter, AssetWindowState & state)
{
    const std::string key = presentationContextKey(adapter.descriptor);
    if (state.presentationContext != key)
    {
        state.presentationContext = key;
        const auto stored = state.presentations.find(key);
        state.presentation = stored == state.presentations.end() ? defaultPresentation(adapter.descriptor.capabilities) : stored->second;
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
    }
}

void drawPresentationControls(const AssetAdapter & adapter, AssetWindowState & state)
{
    if (!adapter.descriptor.capabilities.supports(AssetPresentationCapability::grid) ||
        !adapter.descriptor.capabilities.supports(AssetPresentationCapability::table))
    {
        return;
    }
    ImGui::TextUnformatted(strings::grid());
    ImGui::SameLine();
    if (ImGui::RadioButton("##AssetGridPresentation", state.presentation == AssetPresentation::grid))
    {
        setPresentation(adapter, state, AssetPresentation::grid);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(strings::table());
    ImGui::SameLine();
    if (ImGui::RadioButton("##AssetTablePresentation", state.presentation == AssetPresentation::table))
    {
        setPresentation(adapter, state, AssetPresentation::table);
    }
}

void drawItemDetails(const AssetDetails & details)
{
    ImGui::TextUnformatted(details.title.c_str());
    for (const auto & field : details.fields)
    {
        ImGui::TextWrapped("%s", field.c_str());
    }
}

void drawDetails(const AssetAdapter & adapter, const AssetWindowState & state, float height)
{
    ImGui::BeginChild("AssetDetails", ImVec2(0.0F, height), ImGuiChildFlags_Borders);
    if (state.detailsCollapsed)
    {
        ImGui::TextUnformatted(strings::detailsCollapsed());
        ImGui::EndChild();
        return;
    }

    if (state.selection.selected.empty() || !adapter.loadDetails)
    {
        ImGui::TextUnformatted(strings::noItemSelected());
        ImGui::EndChild();
        return;
    }

    const auto details = adapter.loadDetails(state.selection.selected.front());
    if (!details)
    {
        ImGui::TextUnformatted(strings::itemDetailsUnavailable());
        ImGui::EndChild();
        return;
    }

    drawItemDetails(*details);
    ImGui::EndChild();
}

void drawItem(const AssetAdapter & adapter, AssetWindowState & state, const AssetItem & item)
{
    ImGui::PushID(item.id.value.c_str());
    const bool selected = contains(state.selection, item.id);
    ImGui::BeginDisabled(item.state != AssetItemState::available);
    if (adapter.renderItem)
    {
        adapter.renderItem(item, selected);
        if (ImGui::IsItemClicked())
        {
            selectSingle(state.selection, item.id);
        }
    }
    else if (ImGui::Selectable(item.label.c_str(), selected, ImGuiSelectableFlags_None, ImVec2(0.0F, 0.0F)))
    {
        selectSingle(state.selection, item.id);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered() && !item.label.empty())
    {
        ImGui::SetTooltip("%s", item.label.c_str());
    }
    if (item.state != AssetItemState::available && !item.stateMessage.empty() && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", item.stateMessage.c_str());
    }
    ImGui::PopID();
}

void drawGrid(const AssetAdapter & adapter, AssetWindowState & state)
{
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
                    drawItem(adapter, state, items.at(loadedIndex));
                }
                else
                {
                    ImGui::TextUnformatted(strings::loading());
                }
            }
        }
    }
}

void drawTableRow(const AssetAdapter & adapter, AssetWindowState & state, const AssetItem & item)
{
    ImGui::PushID(item.id.value.c_str());
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const bool selected = contains(state.selection, item.id);
    ImGui::BeginDisabled(item.state != AssetItemState::available);
    if (ImGui::Selectable(item.label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
    {
        selectSingle(state.selection, item.id);
    }
    ImGui::EndDisabled();
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
        const auto details = adapter.loadDetails(item.id);
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
    const float rowHeight = ImGui::GetTextLineHeightWithSpacing() * 2.0F;
    const float tableHeight = ImGui::GetContentRegionAvail().y;
    if (adapter.descriptor.itemCount == 0U)
    {
        ImGui::TextUnformatted(strings::noItems());
        return;
    }

    const ImGuiTableFlags flags = ImGuiTableFlags_Borders + ImGuiTableFlags_RowBg + ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("AssetTable", 2, flags, ImVec2(0.0F, tableHeight)))
    {
        return;
    }
    ImGui::TableSetupColumn(strings::item(), ImGuiTableColumnFlags_WidthFixed, 100.0F);
    ImGui::TableSetupColumn(strings::details(), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(adapter.descriptor.itemCount), rowHeight);
    while (clipper.Step())
    {
        const std::size_t first = static_cast<std::size_t>(clipper.DisplayStart);
        const std::size_t count = static_cast<std::size_t>(clipper.DisplayEnd - clipper.DisplayStart);
        const AssetRangeRequest request{.first = first, .count = count, .presentation = AssetPresentation::table};
        const std::vector<AssetItem> items = adapter.loadRange ? adapter.loadRange(request, state.query) : std::vector<AssetItem>{};
        for (std::size_t index = 0; index < count; index++)
        {
            if (index < items.size())
            {
                drawTableRow(adapter, state, items.at(index));
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

void drawAssetWindow(const AssetAdapter & adapter, AssetWindowState & state, bool & visible)
{
    const std::string title = windowName(adapter.descriptor);
    if (!ImGui::Begin(title.c_str(), &visible))
    {
        ImGui::End();
        return;
    }

    syncPresentation(adapter, state);
    const float availableHeight = std::max(0.0F, ImGui::GetContentRegionAvail().y);
    const float actualHeaderHeight = std::min(headerHeight, availableHeight);
    ImGui::BeginChild("AssetHeader", ImVec2(0.0F, actualHeaderHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextUnformatted(adapter.descriptor.title.c_str());
    drawActions(adapter.descriptor.actions);
    drawExtensions(adapter.descriptor.extensionSlots);
    drawQueryControls(adapter, state);
    drawPresentationControls(adapter, state);
    ImGui::EndChild();

    const float separatorHeight = ImGui::GetStyle().ItemSpacing.y;
    const float remainingHeight = std::max(0.0F, ImGui::GetContentRegionAvail().y);
    const float actualDetailsHeight = std::min(detailsHeight, remainingHeight);
    const float contentHeight = std::max(0.0F, remainingHeight - actualDetailsHeight - separatorHeight);
    ImGui::BeginChild("AssetContent", ImVec2(0.0F, contentHeight), ImGuiChildFlags_Borders);
    if (state.presentation == AssetPresentation::grid && adapter.descriptor.capabilities.supports(AssetPresentationCapability::grid) &&
        adapter.descriptor.fixedCellWidth > 0U)
    {
        drawGrid(adapter, state);
    }
    else if (state.presentation == AssetPresentation::table && adapter.descriptor.capabilities.supports(AssetPresentationCapability::table))
    {
        drawTable(adapter, state);
    }
    else
    {
        ImGui::TextUnformatted(strings::assetViewUnavailable());
    }
    ImGui::EndChild();
    ImGui::Separator();
    drawDetails(adapter, state, actualDetailsHeight);
    ImGui::End();
}

} // namespace uo_mule::gui::assets
