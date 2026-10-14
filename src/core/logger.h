#pragma once

#include "core/app_info.h"
#include "core/gui_log_sink.h"
#include "core/log_level.h"
#include <filesystem>
#include <memory>
#include <spdlog/logger.h>
#include <string>
#include <string_view>

namespace im_cpp::core
{

class Logger
{
public:
    explicit Logger(std::string_view name = applicationName());
    ~Logger();

    Logger(const Logger &) = delete;
    Logger & operator=(const Logger &) = delete;
    Logger(Logger &&) = delete;
    Logger & operator=(Logger &&) = delete;

    void setup(LogLevel level = defaultLogLevel(), const std::filesystem::path & logDirectory = {});
    void shutdown() noexcept;
    void setLevel(LogLevel level) noexcept;
    [[nodiscard]] std::shared_ptr<GuiLogSink> attachGuiSink();
    [[nodiscard]] const std::filesystem::path & logFilePath() const noexcept;
    [[nodiscard]] std::shared_ptr<spdlog::logger> get() const noexcept;

private:
    const std::string name_;
    std::filesystem::path logFilePath_;
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<GuiLogSink> guiSink_;
};

} // namespace im_cpp::core
