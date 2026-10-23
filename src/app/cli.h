#pragma once

#include "logger/gui_log_sink.h"
#include "logger/logger.h"
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <memory>
#include <span>
#include <spdlog/logger.h>
#include <string_view>

namespace uo_mule::app
{

[[nodiscard]] int
runCli(std::span<const std::string_view> arguments,
       std::ostream & output,
       std::ostream & error,
       logger::Logger & logger,
       const std::filesystem::path & logDirectory = {},
       const std::function<int(std::shared_ptr<spdlog::logger>, std::shared_ptr<logger::GuiLogSink>)> & runGui = {});

}
