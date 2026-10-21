#include "gui/views/common.h"
#include "gui/documents/map_workspace.h"
#include "gui/frontend/theme.h"
#include "gui/strings/ui_strings.h"
#include "uomul/map.h"
#include "uomul/mul_common/mul_common.h"
#include <imgui.h>
#include <string>

namespace uo_mule::gui
{

namespace
{

constexpr float textOutlineOffset = 1.0F;

}

std::string formatError(const uomul::Error & error)
{
    return strings::formatError(error);
}

std::string tileLabel(MapDisplayMode mode, uomul::MapTile tile)
{
    if (mode == MapDisplayMode::tileIds)
    {
        return std::to_string(tile.id);
    }

    if (mode == MapDisplayMode::z)
    {
        return std::to_string(static_cast<int>(tile.z));
    }

    return std::to_string(tile.id) + "\n" + std::to_string(static_cast<int>(tile.z));
}

void drawOutlinedText(ImDrawList & drawList, ImVec2 position, const std::string & text)
{
    const ImU32 outlineColor = theme::packedColor(theme::stoneSurface);
    const ImU32 textColor = theme::packedColor(theme::parchmentText);
    drawList.AddText(ImVec2(position.x - textOutlineOffset, position.y), outlineColor, text.c_str());
    drawList.AddText(ImVec2(position.x + textOutlineOffset, position.y), outlineColor, text.c_str());
    drawList.AddText(ImVec2(position.x, position.y - textOutlineOffset), outlineColor, text.c_str());
    drawList.AddText(ImVec2(position.x, position.y + textOutlineOffset), outlineColor, text.c_str());
    drawList.AddText(position, textColor, text.c_str());
}

} // namespace uo_mule::gui
