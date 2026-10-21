#pragma once

#include "gui/documents/document_identity.h"
#include "gui/views/panel_registry.h"
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

[[nodiscard]] std::string documentTabLabel(const DocumentSummary & summary);
void drawWorkbenchFrame(GLFWwindow * window, core::GuiLogSink & logSink, WorkbenchState & state);

} // namespace uo_mule::gui
