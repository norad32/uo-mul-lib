#include "gui/dialogs/map_facet_dialog.h"
#include "core/facet_sizes.h"
#include "gui/new_map_dialog.h"
#include "gui/theme.h"
#include "uomul/map.h"
#include <cstddef>
#include <cstdint>
#include <imgui.h>

namespace uo_mule::gui
{

namespace
{

constexpr int customFacetIndex = 0;
constexpr std::uint32_t minimumDimension = uomul::mapBlockDimension;

[[nodiscard]] const core::FacetSize * facetAt(std::size_t index) noexcept
{
    std::size_t current = 0;
    for (const auto & facet : core::knownFacetSizes())
    {
        if (current == index)
        {
            return &facet;
        }
        current++;
    }
    return nullptr;
}

[[nodiscard]] const char * facetLabel(std::size_t index) noexcept
{
    if (index == 0U)
    {
        return "Custom dimensions";
    }
    return facetAt(index - 1U)->mapLabel;
}

[[nodiscard]] bool drawFacetCombo(int & preset, std::uint32_t & width, std::uint32_t & height)
{
    const std::size_t facetCount = core::knownFacetSizes().size() + 1U;
    const std::size_t requestedIndex = preset < 0 ? 0U : static_cast<std::size_t>(preset);
    const std::size_t selectedIndex = requestedIndex < facetCount ? requestedIndex : 0U;
    if (ImGui::BeginCombo("Facet", facetLabel(selectedIndex)))
    {
        for (std::size_t index = 0; index < facetCount; index++)
        {
            const std::size_t activeIndex = preset < 0 ? 0U : static_cast<std::size_t>(preset);
            const bool selected = activeIndex == index;
            if (ImGui::Selectable(facetLabel(index), selected))
            {
                preset = static_cast<int>(index);
                if (index != 0U)
                {
                    const auto * facet = facetAt(index - 1U);
                    width = facet->widthTiles;
                    height = facet->heightTiles;
                }
            }

            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (preset != customFacetIndex)
    {
        return true;
    }
    ImGui::InputScalar("Width", ImGuiDataType_U32, &width);
    ImGui::InputScalar("Height", ImGuiDataType_U32, &height);
    return width >= minimumDimension && height >= minimumDimension && width % minimumDimension == 0U &&
           height % minimumDimension == 0U;
}

} // namespace

MapFacetDialogResult drawMapFacetDialog(NewMapDialogState & state)
{
    ImGui::Text("Map path: %s", state.openPath.data());
    if (state.openCandidates.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::amber), "No exact known size match, custom dimensions is selected.");
    }

    const bool validDimensions = drawFacetCombo(state.openFacet, state.openWidth, state.openHeight);
    if (!validDimensions)
    {
        ImGui::TextColored(theme::imguiColor(theme::amber), "Dimensions must be positive multiples of 8 tiles.");
    }

    if ((ImGui::Button("Select") || ImGui::IsKeyPressed(ImGuiKey_Enter)) && validDimensions)
    {
        return MapFacetDialogResult::accept;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        return MapFacetDialogResult::cancel;
    }
    return MapFacetDialogResult::none;
}

} // namespace uo_mule::gui
