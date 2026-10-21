#include "app/cli.h"
#include "app/cli_arguments.h"
#include "core/gui_log_sink.h"
#include "core/logger.h"
#include "gui/application/application.h"
#include <cstddef>
#include <iostream>
#include <memory>
#include <span>
#include <spdlog/logger.h>
#include <utility>

int main(int argc, char * argv[])
{
    const auto arguments = uo_mule::app::makeCliArguments(std::span(argv, static_cast<std::size_t>(argc)));
    uo_mule::core::Logger logger;
    return uo_mule::app::runCli(
        arguments,
        std::cout,
        std::cerr,
        logger,
        {},
        [](std::shared_ptr<spdlog::logger> activeLogger, std::shared_ptr<uo_mule::core::GuiLogSink> logSink)
        {
            uo_mule::gui::Application application(std::move(activeLogger), std::move(logSink));
            return application.run();
        });
}
