#pragma once

#include "gui_model/documents/document_identity.h"
#include "gui_model/layout/panel_registry.h"
#include "logger/gui_log_sink.h"
#include <string>
#include <string_view>

struct GLFWwindow;

namespace uo_mule::gui
{

struct WorkbenchState;

void drawWorkbenchFrame(GLFWwindow * window, logger::GuiLogSink & logSink, WorkbenchState & state);

} // namespace uo_mule::gui
