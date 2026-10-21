#pragma once

#include "gui/views/panel_registry.h"
#include <span>
#include <string_view>

struct GLFWwindow;

namespace uo_mule::core
{
class GuiLogSink;
}

namespace uo_mule::gui
{

struct WorkbenchState;

struct DockWindowDescriptor
{
    PanelId panel;
    const char * windowName;
};

[[nodiscard]] std::span<const DockWindowDescriptor> defaultDockWindows() noexcept;
void drawWorkbenchFrame(GLFWwindow * window, core::GuiLogSink & logSink, WorkbenchState & state);

} // namespace uo_mule::gui
