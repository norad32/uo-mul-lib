#include "gui_model/layout/panel_registry.h"
#include <vector>

namespace uo_mule::gui
{

PanelRegistry::PanelRegistry() :
    panels_({
        {
            .id = PanelId::toolPalette,
            .stableId = "panel.tool-palette",
            .title = "Tool Palette",
            .defaultVisible = true,
            .visible = true,
        },
        {
            .id = PanelId::tileInspector,
            .stableId = "panel.tile-inspector",
            .title = "Tile Inspector",
            .defaultVisible = true,
            .visible = true,
        },
        {
            .id = PanelId::blockInspector,
            .stableId = "panel.block-inspector",
            .title = "Block Inspector",
            .defaultVisible = true,
            .visible = true,
        },
        {.id = PanelId::radarcol, .stableId = "panel.radarcol", .title = "Radarcol", .defaultVisible = true, .visible = true},
        {.id = PanelId::texmaps, .stableId = "panel.texmaps", .title = "Texmaps", .defaultVisible = true, .visible = true},
        {.id = PanelId::tiledata, .stableId = "panel.tiledata", .title = "Tiledata", .defaultVisible = true, .visible = true},
        {.id = PanelId::iconGallery,
         .stableId = "panel.icon-gallery",
         .title = "Icon Gallery",
         .defaultVisible = false,
         .visible = false},
    })
{
}

PanelDescriptor * PanelRegistry::find(PanelId id) noexcept
{
    for (auto & panel : panels_)
    {
        if (panel.id == id)
        {
            return &panel;
        }
    }
    return nullptr;
}

const PanelDescriptor * PanelRegistry::find(PanelId id) const noexcept
{
    for (const auto & panel : panels_)
    {
        if (panel.id == id)
        {
            return &panel;
        }
    }
    return nullptr;
}

std::vector<PanelDescriptor> & PanelRegistry::panels() noexcept
{
    return panels_;
}

const std::vector<PanelDescriptor> & PanelRegistry::panels() const noexcept
{
    return panels_;
}

void PanelRegistry::resetToDefaults() noexcept
{
    for (auto & panel : panels_)
    {
        panel.visible = panel.defaultVisible;
    }
}

} // namespace uo_mule::gui
