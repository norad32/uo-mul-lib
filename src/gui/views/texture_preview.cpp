#include "gui/views/texture_preview.h"
#include "gui/documents/map_workspace.h"
#include "gui/frontend/theme.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <imgui.h>

namespace uo_mule::gui
{
namespace
{

constexpr float checkerSize = 4.0F;

void drawCheckerboard(ImDrawList & drawList, ImVec2 minimum, ImVec2 maximum, float size)
{
    const ImU32 dark = theme::packedColor(theme::checkerboardDark);
    const ImU32 light = theme::packedColor(theme::checkerboardLight);
    for (int y = 0; static_cast<float>(y) * checkerSize < size; y++)
    {
        for (int x = 0; static_cast<float>(x) * checkerSize < size; x++)
        {
            const ImVec2 cellMinimum(minimum.x + (static_cast<float>(x) * checkerSize),
                                     minimum.y + (static_cast<float>(y) * checkerSize));
            const ImVec2 cellMaximum(std::min(cellMinimum.x + checkerSize, maximum.x),
                                     std::min(cellMinimum.y + checkerSize, maximum.y));
            drawList.AddRectFilled(cellMinimum, cellMaximum, (x % 2) != (y % 2) ? dark : light);
        }
    }
}

void drawPixels(ImDrawList & drawList, TexturePreviewData texture, ImVec2 minimum, ImVec2 maximum, float size)
{
    if (texture.pixels.empty() || texture.width == 0U || texture.height == 0U)
    {
        return;
    }

    const float scaleX = size / static_cast<float>(texture.width);
    const float scaleY = size / static_cast<float>(texture.height);
    for (std::uint32_t y = 0; y < texture.height; y++)
    {
        for (std::uint32_t x = 0; x < texture.width; x++)
        {
            const std::size_t index = (static_cast<std::size_t>(y) * texture.width) + x;
            if (index >= texture.pixels.size())
            {
                continue;
            }

            const MapColor color = texture.pixels.subspan(index, 1U).front();
            const ImVec2 pixelMinimum(minimum.x + std::round(static_cast<float>(x) * scaleX),
                                      minimum.y + std::round(static_cast<float>(y) * scaleY));
            const ImVec2 pixelMaximum(
                x + 1U == texture.width ? maximum.x : minimum.x + std::round(static_cast<float>(x + 1U) * scaleX),
                y + 1U == texture.height ? maximum.y : minimum.y + std::round(static_cast<float>(y + 1U) * scaleY));
            drawList.AddRectFilled(pixelMinimum, pixelMaximum, IM_COL32(color.red, color.green, color.blue, 255));
        }
    }
}

} // namespace

void drawTexturePreview(TexturePreviewData texture, float size, float borderThickness)
{
    const ImVec2 minimum = ImGui::GetCursorScreenPos();
    const ImVec2 maximum(minimum.x + size, minimum.y + size);
    ImDrawList * drawList = ImGui::GetWindowDrawList();
    drawCheckerboard(*drawList, minimum, maximum, size);
    drawPixels(*drawList, texture, minimum, maximum, size);
    drawList->AddRect(minimum, maximum, theme::packedColor(theme::border), 0.0F, ImDrawFlags_None, borderThickness);
    ImGui::Dummy(ImVec2(size, size));
}

} // namespace uo_mule::gui
