#include "gui/layout/panel_registry.h"
#include <vector>

namespace uo_mule::gui
{

PanelRegistry::PanelRegistry() :
    panels_({
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
