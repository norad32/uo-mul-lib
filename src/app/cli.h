#pragma once

#include <filesystem>
#include <functional>
#include <iosfwd>
#include <memory>
#include <span>
#include <spdlog/logger.h>
#include <string_view>

#include "core/gui_log_sink.h"
#include "core/logger.h"

namespace im_cpp::app
{

[[nodiscard]] int
runCli(std::span<const std::string_view> arguments,
       std::ostream & output,
       std::ostream & error,
       core::Logger & logger,
       const std::filesystem::path & logDirectory = {},
       const std::function<int(std::shared_ptr<spdlog::logger>, std::shared_ptr<core::GuiLogSink>)> & runGui = {});

}
