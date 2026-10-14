#include "app/cli.h"
#include "app/cli_arguments.h"
#include "core/gui_log_sink.h"
#include "core/logger.h"
#include "gui/application.h"
#include <cstddef>
#include <iostream>
#include <span>
#include <memory>
#include <spdlog/logger.h>
#include <utility>

int main(int argc, char * argv[])
{
    const auto arguments = im_cpp::app::makeCliArguments(std::span(argv, static_cast<std::size_t>(argc)));
    im_cpp::core::Logger logger;
    return im_cpp::app::runCli(
        arguments,
        std::cout,
        std::cerr,
        logger,
        {},
        [](std::shared_ptr<spdlog::logger> activeLogger, std::shared_ptr<im_cpp::core::GuiLogSink> logSink)
        {
            im_cpp::gui::Application application(std::move(activeLogger), std::move(logSink));
            return application.run();
        });
}
