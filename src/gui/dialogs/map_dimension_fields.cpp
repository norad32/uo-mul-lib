#include "gui/dialogs/map_dimension_fields.h"
#include "gui/presentation/dialog_presentation.h"
#include "gui/presentation/strings.h"
#include "uomul/facet_sizes.h"
#include "uomul/map/map.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <imgui.h>
#include <optional>
#include <string>

namespace uo_mule::gui
{
namespace
{

constexpr int customFacetIndex = 0;
constexpr std::uint32_t minimumDimension = uomul::mapBlockDimension;

void drawFieldLabel(const char * text, const std::optional<float> & labelWidth)
{
    if (!labelWidth)
    {
        dialogFieldLabel(text);
        return;
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(text);
    ImGui::SameLine(*labelWidth);
}

} // namespace

const uomul::FacetSize * mapFacetAt(std::size_t index) noexcept
{
    std::size_t current = 0;
    for (const auto & facet : uomul::knownFacetSizes())
    {
        if (current == index)
        {
            return &facet;
        }
        current++;
    }
    return nullptr;
}

const char * mapFacetLabel(std::size_t index, bool useMapLabel) noexcept
{
    if (index == 0U)
    {
        return strings::customDimensions();
    }

    const auto * facet = mapFacetAt(index - 1U);
    return useMapLabel ? facet->mapLabel : facet->label;
}

std::expected<uomul::Dimensions, std::string> resolveMapDimensions(int preset, std::uint32_t width, std::uint32_t height)
{
    if (preset == customFacetIndex)
    {
        if (width < minimumDimension || height < minimumDimension || width % minimumDimension != 0U ||
            height % minimumDimension != 0U)
        {
            return std::unexpected(strings::invalidDimensions());
        }
        return uomul::Dimensions{.widthTiles = width, .heightTiles = height};
    }

    if (preset > customFacetIndex)
    {
        const auto * facet = mapFacetAt(static_cast<std::size_t>(preset - 1));
        if (facet != nullptr)
        {
            return uomul::Dimensions{.widthTiles = facet->widthTiles, .heightTiles = facet->heightTiles};
        }
    }

    return std::unexpected(strings::unsupportedFacetSelection());
}

bool drawMapDimensionFields(int & preset,
                            std::uint32_t & width,
                            std::uint32_t & height,
                            bool useMapLabel,
                            MapDimensionFieldLayout layout)
{
    const std::size_t facetCount = uomul::knownFacetSizes().size() + 1U;
    const std::size_t requestedIndex = preset < 0 ? 0U : static_cast<std::size_t>(preset);
    const std::size_t selectedIndex = requestedIndex < facetCount ? requestedIndex : 0U;
    drawFieldLabel(strings::facet(), layout.labelWidth);
    dialogFieldWidth(layout.fieldWidth);
    if (ImGui::BeginCombo("##Facet", mapFacetLabel(selectedIndex, useMapLabel)))
    {
        for (std::size_t index = 0; index < facetCount; index++)
        {
            const std::size_t activeIndex = preset < 0 ? 0U : static_cast<std::size_t>(preset);
            const bool selected = activeIndex == index;
            if (ImGui::Selectable(mapFacetLabel(index, useMapLabel), selected))
            {
                preset = static_cast<int>(index);
                if (index != 0U)
                {
                    const auto dimensions = resolveMapDimensions(preset, width, height);
                    if (dimensions)
                    {
                        width = dimensions->widthTiles;
                        height = dimensions->heightTiles;
                    }
                }
            }

            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (preset == customFacetIndex)
    {
        drawFieldLabel(strings::width(), layout.labelWidth);
        dialogFieldWidth(layout.fieldWidth);
        ImGui::InputScalar("##Width", ImGuiDataType_U32, &width, nullptr, nullptr, "%u", ImGuiInputTextFlags_CharsDecimal);
        drawFieldLabel(strings::height(), layout.labelWidth);
        dialogFieldWidth(layout.fieldWidth);
        ImGui::InputScalar("##Height", ImGuiDataType_U32, &height, nullptr, nullptr, "%u", ImGuiInputTextFlags_CharsDecimal);
    }

    return resolveMapDimensions(preset, width, height).has_value();
}

} // namespace uo_mule::gui
