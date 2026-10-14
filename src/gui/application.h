#pragma once

#include "core/gui_log_sink.h"
#include "gui/preferences.h"
#include <memory>
#include <spdlog/logger.h>

namespace im_cpp::gui
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
    bool shouldClose_ = false;
};

} // namespace im_cpp::gui
