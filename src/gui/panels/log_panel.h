#pragma once

#include "gui_model/persistence/preferences.h"
#include "logger/gui_log_sink.h"
#include <optional>
#include <vector>

namespace uo_mule::gui
{

struct LogPanelRequest
{
    std::optional<bool> visible;
    std::optional<bool> autoScroll;
    std::optional<bool> collapsed;
    std::optional<float> height;
    bool clearEntries = false;
};

[[nodiscard]] float drawLogPanel(logger::GuiLogSink & logSink,
                                 const Preferences & preferences,
                                 std::vector<logger::GuiLogEntry> & entries,
                                 bool & followScroll,
                                 LogPanelRequest & request);

} // namespace uo_mule::gui
