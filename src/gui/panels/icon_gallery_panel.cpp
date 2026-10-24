#include "gui/panels/icon_gallery_panel.h"
#include "gui/presentation/icons/iconography.h"
#include <imgui.h>
#include <string>

namespace uo_mule::gui
{
namespace
{

[[nodiscard]] std::string displayGlyph(IconId id, bool iconFontAvailable)
{
    if (iconFontAvailable)
    {
        return iconGlyph(id);
    }
    return "[" + std::string(iconName(id)) + "]";
}

} // namespace

void drawIconGalleryPanel(bool & visible, bool iconFontAvailable)
{
    if (!ImGui::Begin("Icon Gallery###view.icon-gallery", &visible))
    {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("Semantic icon vocabulary");
    ImGui::Text("Font: %s | Display scale: %.2gx, %.2gx",
                iconFontAvailable ? "available" : "text fallback",
                ImGui::GetIO().DisplayFramebufferScale.x,
                ImGui::GetIO().DisplayFramebufferScale.y);
    ImGui::Separator();

    std::size_t index = 0;
    for (const IconMapping & mapping : iconMappings())
    {
        const std::string glyph = displayGlyph(mapping.id, iconFontAvailable);
        const std::string idSuffix = "##icon-gallery-" + std::to_string(index++);
        ImGui::TextUnformatted(iconName(mapping.id).data());
        ImGui::SameLine(180.0F);
        ImGui::TextUnformatted(glyph.c_str());
        ImGui::SameLine(230.0F);
        ImGui::Button((glyph + idSuffix + "-normal").c_str());
        ImGui::SameLine();
        ImGui::Selectable((glyph + idSuffix + "-active").c_str(), true, ImGuiSelectableFlags_None, ImVec2(0.0F, 0.0F));
        ImGui::SameLine();
        ImGui::BeginDisabled();
        ImGui::Button((glyph + idSuffix + "-disabled").c_str());
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::Text("%s", iconName(mapping.id).data());
    }

    ImGui::End();
}

} // namespace uo_mule::gui
