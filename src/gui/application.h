#pragma once

#include "core/gui_log_sink.h"
#include "gui/data_profile.h"
#include "gui/preferences.h"
#include <memory>
#include <spdlog/logger.h>

namespace uo_mule::gui
{

class Application
{
public:
    Application(std::shared_ptr<spdlog::logger> logger, std::shared_ptr<core::GuiLogSink> logSink);
    [[nodiscard]] int run();

private:
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<core::GuiLogSink> logSink_;
    Preferences preferences_;
    DataProfile activeProfile_;
    bool shouldClose_ = false;
};

} // namespace uo_mule::gui
