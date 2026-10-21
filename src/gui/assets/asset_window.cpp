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
constexpr float detailsWidth = 240.0F;
constexpr float contentReservedWidth = 250.0F;

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
    ImGui::BeginChild("AssetDetails", ImVec2(detailsWidth, 0.0F), ImGuiChildFlags_Borders);
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
    if (item.state != AssetItemState::available && !item.stateMessage.empty() && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", item.stateMessage.c_str());
    }
    ImGui::PopID();
}

void drawGrid(const AssetAdapter & adapter, AssetWindowState & state)
{
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const AssetGridLayout layout = gridLayout(adapter.descriptor.itemCount,
                                              ImGui::GetContentRegionAvail().x,
                                              static_cast<float>(adapter.descriptor.fixedCellWidth),
                                              spacing);
    if (layout.rows == 0U)
    {
        ImGui::TextUnformatted(strings::noItems());
        return;
    }

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(layout.rows));
    while (clipper.Step())
    {
        const std::size_t first = static_cast<std::size_t>(clipper.DisplayStart) * layout.columns;
        const std::size_t count = std::min(layout.columns * static_cast<std::size_t>(clipper.DisplayEnd - clipper.DisplayStart),
                                           adapter.descriptor.itemCount - first);
        const AssetRangeRequest request{.first = first, .count = count, .presentation = AssetPresentation::grid};
        const std::vector<AssetItem> items = adapter.loadRange ? adapter.loadRange(request, state.query) : std::vector<AssetItem>{};
        for (std::size_t column = 0; column < layout.columns; column++)
        {
            ImGui::TableNextColumn();
            const std::size_t itemIndex = (static_cast<std::size_t>(clipper.DisplayStart) * layout.columns) + column;
            if (itemIndex < first + items.size())
            {
                drawItem(adapter, state, items.at(itemIndex - first));
            }
            else if (itemIndex < adapter.descriptor.itemCount)
            {
                ImGui::TextUnformatted(strings::loading());
            }
        }
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

    ImGui::BeginChild("AssetContent", ImVec2(-contentReservedWidth, 0.0F), ImGuiChildFlags_Borders);
    if (state.presentation == AssetPresentation::grid && adapter.descriptor.capabilities.supports(AssetPresentationCapability::grid) &&
        adapter.descriptor.fixedCellWidth > 0U)
    {
        if (ImGui::BeginTable("AssetGrid", static_cast<int>(gridLayout(adapter.descriptor.itemCount,
                                                                        ImGui::GetContentRegionAvail().x,
                                                                        static_cast<float>(adapter.descriptor.fixedCellWidth),
                                                                        ImGui::GetStyle().ItemSpacing.x).columns),
                              ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX))
        {
            drawGrid(adapter, state);
            ImGui::EndTable();
        }
    }
    else
    {
        ImGui::TextUnformatted(strings::assetViewUnavailable());
    }
    ImGui::EndChild();
    ImGui::SameLine();
    drawDetails(adapter, state);
    ImGui::End();
}

} // namespace uo_mule::gui::assets
