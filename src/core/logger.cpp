#include "core/app_info.h"
#include "core/logger.h"
#include "core/gui_log_sink.h"
#include "core/log_level.h"
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <memory>
#include <spdlog/common.h>
#include <spdlog/details/fmt_helper.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/logger.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace im_cpp::core
{
namespace
{

constexpr std::size_t maxLogFileSize = std::size_t{1024} * std::size_t{1024};
constexpr std::size_t maxRotatedLogFiles = std::size_t{3};

class SourceFilenameFlagFormatter final : public spdlog::custom_flag_formatter
{
public:
    [[nodiscard]] std::unique_ptr<spdlog::custom_flag_formatter> clone() const override
    {
        return std::make_unique<SourceFilenameFlagFormatter>();
    }

    void format(const spdlog::details::log_msg & message, const std::tm & time, spdlog::memory_buf_t & destination) override
    {
        static_cast<void>(time);
        std::string_view value = message.source.filename;
        const std::size_t separator = value.find_last_of("/\\");
        if (separator != std::string_view::npos)
        {
            value.remove_prefix(separator + 1);
        }
        spdlog::details::fmt_helper::append_string_view(value, destination);
    }
};

[[nodiscard]] std::unique_ptr<spdlog::formatter> makeLogFormatter()
{
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<SourceFilenameFlagFormatter>('N');
#ifdef IM_CPP_DEBUG_BUILD
    formatter->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] [%N] [%#] %v");
#else
    formatter->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] %v");
#endif
    return formatter;
}

[[nodiscard]] spdlog::level::level_enum toSpdlogLevel(LogLevel level) noexcept
{
    switch (level)
    {
        case LogLevel::trace :
            return spdlog::level::trace;
        case LogLevel::debug :
            return spdlog::level::debug;
        case LogLevel::info :
            return spdlog::level::info;
        case LogLevel::warning :
            return spdlog::level::warn;
        case LogLevel::error :
            return spdlog::level::err;
        case LogLevel::critical :
            return spdlog::level::critical;
    }
    return spdlog::level::off;
}

[[nodiscard]] spdlog::level::level_enum configuredLevel(LogLevel level) noexcept
{
#ifdef RELEASE
    const auto requestedLevel = toSpdlogLevel(level);
    return requestedLevel < spdlog::level::info ? spdlog::level::info : requestedLevel;
#else
    return toSpdlogLevel(level);
#endif
}

[[nodiscard]] std::filesystem::path defaultLogDirectory()
{
    if (const char * stateDirectory = std::getenv("XDG_STATE_HOME");
        stateDirectory != nullptr && *stateDirectory != '\0' &&
        std::filesystem::path(stateDirectory).is_absolute())
    {
        return std::filesystem::path(stateDirectory) / std::string(applicationName()) / "logs";
    }
    if (const char * homeDirectory = std::getenv("HOME"); homeDirectory != nullptr && !std::string_view(homeDirectory).empty())
    {
        return std::filesystem::path(homeDirectory) / ".local" / "state" / std::string(applicationName()) / "logs";
    }
    throw std::runtime_error("Cannot determine log directory: XDG_STATE_HOME and HOME are unset");
}

} // namespace

Logger::Logger(std::string_view name) : name_(name)
{
}

void Logger::setup(LogLevel level, const std::filesystem::path & logDirectory)
{
    shutdown();
    const auto directory = logDirectory.empty() ? defaultLogDirectory() : logDirectory;
    std::filesystem::create_directories(directory);
    logFilePath_ = directory / (name_ + ".log");

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    sinks.push_back(
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFilePath_.string(), maxLogFileSize, maxRotatedLogFiles));
    if (guiSink_)
    {
        sinks.push_back(guiSink_);
    }

    logger_ = std::make_shared<spdlog::logger>(name_, sinks.begin(), sinks.end());
    logger_->set_level(configuredLevel(level));
    for (const auto & sink : logger_->sinks())
    {
        sink->set_formatter(makeLogFormatter());
    }
    logger_->flush_on(spdlog::level::err);
}

Logger::~Logger()
{
    shutdown();
}

void Logger::shutdown() noexcept
{
    if (logger_)
    {
        logger_->flush();
        logger_.reset();
    }
}

void Logger::setLevel(LogLevel level) noexcept
{
    if (logger_)
    {
        logger_->set_level(configuredLevel(level));
    }
}

std::shared_ptr<GuiLogSink> Logger::attachGuiSink()
{
    if (!logger_)
    {
        throw std::logic_error("Logger must be set up before attaching a GUI sink");
    }
    if (!guiSink_)
    {
        guiSink_ = std::make_shared<GuiLogSink>();
        guiSink_->set_formatter(makeLogFormatter());
        logger_->sinks().push_back(guiSink_);
    }
    return guiSink_;
}

const std::filesystem::path & Logger::logFilePath() const noexcept
{
    return logFilePath_;
}

std::shared_ptr<spdlog::logger> Logger::get() const noexcept
{
    return logger_;
}

} // namespace im_cpp::core
