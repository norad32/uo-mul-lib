#include "gui/dialogs/map_facet_dialog.h"
#include "core/facet_sizes.h"
#include "gui/dialogs/new_map_dialog.h"
#include "gui/frontend/theme.h"
#include "gui/dialogs/dialog_presentation.h"
#include "uomul/map.h"
#include <cstddef>
#include <cstdint>
#include <imgui.h>
#include <string>

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
    dialogSubheading((std::string("Map path: ") + state.openPath.data()).c_str());
    if (state.openCandidates.empty())
    {
        ImGui::TextColored(theme::imguiColor(theme::warning), "No exact known size match, custom dimensions is selected.");
    }

    const bool validDimensions = drawFacetCombo(state.openFacet, state.openWidth, state.openHeight);
    if (!validDimensions)
    {
        ImGui::TextColored(theme::imguiColor(theme::warning), "Dimensions must be positive multiples of 8 tiles.");
    }

    beginDialogFooter({"Select", "Cancel"});
    if ((dialogButton("Select", validDimensions) || ImGui::IsKeyPressed(ImGuiKey_Enter)) && validDimensions)
    {
        endDialogFooter();
        return MapFacetDialogResult::accept;
    }
    ImGui::SameLine();
    if (dialogButton("Cancel"))
    {
        endDialogFooter();
        return MapFacetDialogResult::cancel;
    }
    endDialogFooter();
    return MapFacetDialogResult::none;
}

} // namespace uo_mule::gui
