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

void drawDetails(const AssetAdapter & adapter, const AssetWindowState & state)
{
    ImGui::BeginChild("AssetDetails", ImVec2(0.0F, detailsHeight), ImGuiChildFlags_Borders);
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

    ImGui::TextUnformatted(details->title.c_str());
    for (const auto & field : details->fields)
    {
        ImGui::TextWrapped("%s", field.c_str());
    }
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
        ImGui::SetCursorPosX(startX);
        for (std::size_t column = 0; column < layout.columns; column++)
        {
            const std::size_t itemIndex = (row * layout.columns) + column;
            if (itemIndex < adapter.descriptor.itemCount)
            {
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
            ImGui::SetCursorPosX(startX + (static_cast<float>(column + 1U) * (cellWidth + spacing)));
        }
        ImGui::SetCursorPosY(startY + (static_cast<float>(row + 1U) * rowHeight));
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
    const auto columns = static_cast<std::size_t>(std::max(1.0F, std::floor(std::max(0.0F, availableWidth) / effectiveCellWidth)));
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

    ImGui::TextUnformatted(adapter.descriptor.title.c_str());
    drawActions(adapter.descriptor.actions);
    drawExtensions(adapter.descriptor.extensionSlots);
    drawQueryControls(adapter, state);
    ImGui::Separator();

    ImGui::BeginChild("AssetContent", ImVec2(0.0F, -detailsHeight), ImGuiChildFlags_Borders);
    if (state.presentation == AssetPresentation::grid && adapter.descriptor.capabilities.supports(AssetPresentationCapability::grid) &&
        adapter.descriptor.fixedCellWidth > 0U)
    {
        drawGrid(adapter, state);
    }
    else
    {
        ImGui::TextUnformatted(strings::assetViewUnavailable());
    }
    ImGui::EndChild();
    ImGui::Separator();
    drawDetails(adapter, state);
    ImGui::End();
}

} // namespace uo_mule::gui::assets
