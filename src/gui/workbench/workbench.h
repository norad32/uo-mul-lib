#pragma once

#include "logger/gui_log_sink.h"

struct GLFWwindow;

namespace uo_mule::gui
{

struct WorkbenchState;

void drawWorkbench(GLFWwindow * window, logger::GuiLogSink & logSink, WorkbenchState & state);

} // namespace uo_mule::gui
