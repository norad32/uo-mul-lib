#pragma once

#include "gui/panel_registry.h"

namespace uo_mule::gui
{

class ViewRegistry
{
public:
    explicit ViewRegistry(PanelRegistry & panels) noexcept : panels_(&panels) {}

    [[nodiscard]] PanelDescriptor * find(PanelId id) noexcept { return panels_->find(id); }
    [[nodiscard]] const PanelDescriptor * find(PanelId id) const noexcept { return panels_->find(id); }
    [[nodiscard]] std::vector<PanelDescriptor> & views() noexcept { return panels_->panels(); }
    [[nodiscard]] const std::vector<PanelDescriptor> & views() const noexcept { return panels_->panels(); }
    void resetToDefaults() noexcept { panels_->resetToDefaults(); }

private:
    PanelRegistry * panels_;
};

} // namespace uo_mule::gui
