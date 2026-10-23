#pragma once

#include "gui_model/persistence/preferences.h"
#include "logger/gui_log_sink.h"
#include <vector>

namespace uo_mule::gui
{

[[nodiscard]] float drawLogPanel(logger::GuiLogSink & logSink,
                                 Preferences & preferences,
                                 std::vector<logger::GuiLogEntry> & entries,
                                 bool & followScroll);

} // namespace uo_mule::gui
