#pragma once

#include "gui/documents/document_identity.h"
#include "gui/layout/panel_registry.h"
#include <string>
#include <string_view>

struct GLFWwindow;

namespace uo_mule::core
{
class GuiLogSink;
}

namespace uo_mule::gui
{

struct WorkbenchState;

void drawWorkbenchFrame(GLFWwindow * window, core::GuiLogSink & logSink, WorkbenchState & state);

} // namespace uo_mule::gui
