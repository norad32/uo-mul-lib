#include "app/cli.h"
#include "app/map_cli.h"
#include "core/app_info.h"
#include "core/gui_log_sink.h"
#include "core/log_level.h"
#include "core/logger.h"
#include <CLI/CLI.hpp>
#include <algorithm>
#include <exception>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <vector>

namespace uo_mule::app
{
namespace
{

[[nodiscard]] std::vector<std::string> makeArguments(std::span<const std::string_view> arguments)
{
    std::vector<std::string> cliArguments;
    cliArguments.reserve(arguments.size());
    for (const auto argument : arguments)
    {
        cliArguments.emplace_back(argument);
    }

    return cliArguments;
}

void configureApplication(CLI::App & application,
                          std::string & logLevel,
                          CLI::App *& checkCommand,
                          CLI::App *& guiCommand,
                          MapCommandOptions & mapOptions,
                          CLI::App *& mapCommand)
{
    application.set_help_flag("-h,--help");
    application.set_version_flag("-V,--version",
                                 std::string(core::applicationName()) + " " + std::string(core::applicationVersion()));
    application.add_option("--log-level,-l", logLevel, "Set the logging level");

    checkCommand = application.add_subcommand("check", "Run a logging check");
    guiCommand = application.add_subcommand("gui", "Launch the graphical interface");
    mapCommand = configureMapCommand(application, mapOptions);
    checkCommand->add_option("--log-level,-l", logLevel, "Set the logging level");
    guiCommand->add_option("--log-level,-l", logLevel, "Set the logging level");
}

[[nodiscard]] int
reportParseError(const CLI::App & application, const CLI::ParseError & exception, std::ostream & output, std::ostream & error)
{
    const bool successfulExit = exception.get_exit_code() == 0;
    const int result = application.exit(exception, successfulExit ? output : error);
    return successfulExit ? result : 2;
}

} // namespace

int runCli(std::span<const std::string_view> arguments,
           std::ostream & output,
           std::ostream & error,
           core::Logger & logger,
           const std::filesystem::path & logDirectory,
           const std::function<int(std::shared_ptr<spdlog::logger>, std::shared_ptr<core::GuiLogSink>)> & runGui)
{
    std::string logLevelValue{core::toString(core::defaultLogLevel())};
    CLI::App application{"", std::string(core::applicationName())};
    CLI::App * checkCommand = nullptr;
    CLI::App * guiCommand = nullptr;
    CLI::App * mapCommand = nullptr;
    MapCommandOptions mapOptions;
    configureApplication(application, logLevelValue, checkCommand, guiCommand, mapOptions, mapCommand);

    try
    {
        auto cliArguments = makeArguments(arguments);
        std::ranges::reverse(cliArguments);
        application.parse(cliArguments);
    }
    catch (const CLI::ParseError & exception)
    {
        return reportParseError(application, exception, output, error);
    }

    const bool runCheck = checkCommand->parsed();
    const bool executeMapCommand = mapCommand->parsed();
    const bool runGuiCommand = guiCommand->parsed() || (!runCheck && !executeMapCommand && arguments.empty());
    if (static_cast<unsigned int>(runCheck) + static_cast<unsigned int>(runGuiCommand) +
            static_cast<unsigned int>(executeMapCommand) !=
        1U)
    {
        error << "Exactly one of the commands 'check', 'gui', or 'map' is required\n";
        return 2;
    }

    const std::optional<core::LogLevel> parsedLevel = core::parseLogLevel(logLevelValue);
    if (!parsedLevel)
    {
        error << "Invalid log level: " << logLevelValue << '\n';
        return 2;
    }

    try
    {
        logger.setup(*parsedLevel, logDirectory);
    }
    catch (const std::exception & exception)
    {
        error << "Logger setup failed: " << exception.what() << '\n';
        return 1;
    }
    const auto activeLogger = logger.get();
    SPDLOG_LOGGER_INFO(activeLogger, "Application started");
    if (runGuiCommand)
    {
        try
        {
            const auto logSink = logger.attachGuiSink();
            if (!runGui)
            {
                SPDLOG_LOGGER_ERROR(activeLogger, "GUI support is unavailable in this build");
                error << "GUI support is unavailable in this build\n";
                logger.shutdown();
                return 1;
            }
            const int result = runGui(logger.get(), logSink);
            SPDLOG_LOGGER_INFO(activeLogger, "Application closed");
            logger.shutdown();
            return result;
        }
        catch (const std::exception & exception)
        {
            SPDLOG_LOGGER_ERROR(activeLogger, "Application failed: {}", exception.what());
            error << "Application failed: " << exception.what() << '\n';
            logger.shutdown();
            return 1;
        }
    }

    if (executeMapCommand)
    {
        const int result = uo_mule::app::runMapCommand(mapOptions, output, error);
        SPDLOG_LOGGER_INFO(activeLogger, "Application closed");
        logger.shutdown();
        return result;
    }

    SPDLOG_LOGGER_INFO(activeLogger, "I'm C++ OK");
    SPDLOG_LOGGER_INFO(activeLogger, "Application closed");
    logger.shutdown();
    output << "I'm C++ OK\n";
    return 0;
}

} // namespace uo_mule::app
