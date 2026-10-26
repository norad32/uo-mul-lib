#include "gui_model/layout/window_registry.h"
#include <vector>

namespace uo_mule::gui
{

WindowRegistry::WindowRegistry() :
    windows_({
        {.id = WindowId::radarcol, .stableId = "window.radarcol", .title = "Radarcol"},
        {.id = WindowId::texmaps, .stableId = "window.texmaps", .title = "Texmaps"},
        {.id = WindowId::tiledata, .stableId = "window.tiledata", .title = "Tiledata"},
    })
{
}

WindowDescriptor * WindowRegistry::find(WindowId id) noexcept
{
    for (auto & window : windows_)
    {
        if (window.id == id)
        {
            return &window;
        }
    }
    return nullptr;
}

const WindowDescriptor * WindowRegistry::find(WindowId id) const noexcept
{
    for (const auto & window : windows_)
    {
        if (window.id == id)
        {
            return &window;
        }
    }
    return nullptr;
}

std::vector<WindowDescriptor> & WindowRegistry::windows() noexcept
{
    return windows_;
}

const std::vector<WindowDescriptor> & WindowRegistry::windows() const noexcept
{
    return windows_;
}

void WindowRegistry::resetToDefaults() noexcept
{
    for (auto & window : windows_)
    {
        window.visible = window.defaultVisible;
    }
}

} // namespace uo_mule::gui
