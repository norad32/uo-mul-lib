#include "gui/panels/icon_gallery_panel.h"
#include "gui/presentation/icons/iconography.h"
#include <imgui.h>
#include <string>

namespace uo_mule::gui
{
namespace
{

[[nodiscard]] std::string displayGlyph(IconId id, IconVariant variant, bool iconFontAvailable)
{
    if (iconFontAvailable)
    {
        return iconGlyph(id, variant);
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
        const std::string filledGlyph = displayGlyph(mapping.id, IconVariant::filled, iconFontAvailable);
        const std::string outlineGlyph = displayGlyph(mapping.id, IconVariant::outline, iconFontAvailable);
        const std::string idSuffix = "##icon-gallery-" + std::to_string(index++);
        ImGui::TextUnformatted(iconName(mapping.id).data());
        ImGui::SameLine(180.0F);
        ImGui::TextUnformatted("filled");
        ImGui::SameLine(230.0F);
        ImGui::Button((filledGlyph + idSuffix + "-filled-normal").c_str());
        ImGui::SameLine();
        ImGui::Selectable((filledGlyph + idSuffix + "-filled-active").c_str(), true, ImGuiSelectableFlags_None, ImVec2(0.0F, 0.0F));
        ImGui::SameLine();
        ImGui::BeginDisabled();
        ImGui::Button((filledGlyph + idSuffix + "-filled-disabled").c_str());
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextUnformatted("outline");
        ImGui::SameLine();
        ImGui::Button((outlineGlyph + idSuffix + "-outline-normal").c_str());
        ImGui::SameLine();
        ImGui::Selectable((outlineGlyph + idSuffix + "-outline-active").c_str(), true, ImGuiSelectableFlags_None, ImVec2(0.0F, 0.0F));
        ImGui::SameLine();
        ImGui::BeginDisabled();
        ImGui::Button((outlineGlyph + idSuffix + "-outline-disabled").c_str());
        ImGui::EndDisabled();
    }

    ImGui::End();
}

} // namespace uo_mule::gui
