#include "app/cli.h"
#include "app/cli_arguments.h"
#include "logger/logger.h"
#if HAS_GUI
#include "gui/application/application.h"
#include "logger/gui_log_sink.h"
#endif
#include <cstddef>
#include <iostream>
#include <span>
#if HAS_GUI
#include <memory>
#include <spdlog/logger.h>
#include <utility>
#endif

int main(int argc, char * argv[])
{
    const auto arguments = uo_mule::app::makeCliArguments(std::span(argv, static_cast<std::size_t>(argc)));

    uo_mule::logger::Logger logger;
#if HAS_GUI
    return uo_mule::app::runCli(
        arguments,
        std::cout,
        std::cerr,
        logger,
        {},
        [](std::shared_ptr<spdlog::logger> activeLogger, std::shared_ptr<uo_mule::logger::GuiLogSink> logSink)
        {
            uo_mule::gui::Application application(std::move(activeLogger), std::move(logSink));
            return application.run();
        });
#else
    return uo_mule::app::runCli(arguments, std::cout, std::cerr, logger);
#endif
}
