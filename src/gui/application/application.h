#pragma once

#include "logger/gui_log_sink.h"
#include <memory>
#include <spdlog/logger.h>

namespace uo_mule::gui
{

class Application
{
public:
    Application(std::shared_ptr<spdlog::logger> logger, std::shared_ptr<logger::GuiLogSink> logSink);
    [[nodiscard]] int run();

private:
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<logger::GuiLogSink> logSink_;
};

} // namespace uo_mule::gui
