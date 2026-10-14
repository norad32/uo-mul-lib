#pragma once

#include "core/gui_log_sink.h"
#include "gui/preferences.h"
#include <vector>

namespace im_cpp::gui
{

[[nodiscard]] float drawLogPanel(core::GuiLogSink & logSink,
                                 Preferences & preferences,
                                 std::vector<core::GuiLogEntry> & entries,
                                 bool & followScroll);

} // namespace im_cpp::gui
