#pragma once

#include "core/gui_log_sink.h"
#include "gui/persistence/preferences.h"
#include <vector>

namespace uo_mule::gui
{

[[nodiscard]] float drawLogPanel(core::GuiLogSink & logSink,
                                 Preferences & preferences,
                                 std::vector<core::GuiLogEntry> & entries,
                                 bool & followScroll);

} // namespace uo_mule::gui
