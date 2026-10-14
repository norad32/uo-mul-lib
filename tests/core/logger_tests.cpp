#include "core/log_level.h"
#include "core/logger.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>

TEST_CASE("Logger_Should_IgnoreRelativeXdgStateHome")
{
    const auto log_directory = std::filesystem::temp_directory_path() / "logger-relative-xdg-state";
    const auto working_directory = std::filesystem::current_path();
    const auto relative_directory = working_directory / "relative-state-home";
    std::filesystem::remove_all(log_directory);
    std::filesystem::remove_all(relative_directory);
    const char * previous_home_value = std::getenv("HOME");
    const char * previous_state_value = std::getenv("XDG_STATE_HOME");
    const bool had_previous_home = previous_home_value != nullptr;
    const bool had_previous_state = previous_state_value != nullptr;
    const std::string previous_home = had_previous_home ? previous_home_value : "";
    const std::string previous_state = had_previous_state ? previous_state_value : "";
    setenv("HOME", log_directory.c_str(), 1);
    setenv("XDG_STATE_HOME", "relative-state-home", 1);

    im_cpp::core::Logger logger("relative-state-test");
    logger.setup(im_cpp::core::defaultLogLevel());
    const auto expected_path = log_directory / ".local" / "state" / "im-cpp" / "logs" / "relative-state-test.log";
    const auto actual_path = logger.logFilePath();
    logger.shutdown();
    REQUIRE(actual_path == expected_path);

    if (!had_previous_home)
    {
        unsetenv("HOME");
    }
    else
    {
        setenv("HOME", previous_home.c_str(), 1);
    }
    if (!had_previous_state)
    {
        unsetenv("XDG_STATE_HOME");
    }
    else
    {
        setenv("XDG_STATE_HOME", previous_state.c_str(), 1);
    }
    std::filesystem::remove_all(log_directory);
    std::filesystem::remove_all(relative_directory);
}

TEST_CASE("Logger_Should_WriteLogToRotatingFile")
{
    const auto log_directory = std::filesystem::temp_directory_path() / "logger-tests";
    std::filesystem::remove_all(log_directory);

    im_cpp::core::Logger logger("test");
    logger.setup(im_cpp::core::defaultLogLevel(), log_directory);
    const auto guiSink = logger.attachGuiSink();
    SPDLOG_LOGGER_INFO(logger.get(), "test log message");
    logger.shutdown();

    std::ifstream log_file(log_directory / "test.log");
    const std::string contents((std::istreambuf_iterator<char>(log_file)), std::istreambuf_iterator<char>());
    const auto guiEntries = guiSink->drain();
    REQUIRE(contents.contains("test log message"));
    REQUIRE(guiEntries.size() == 1);
#ifdef IM_CPP_DEBUG_BUILD
    constexpr std::string_view expectedLogSuffix{" [info] [logger_tests.cpp] [64] test log message\n"};
#else
    constexpr std::string_view expectedLogSuffix{" [info] test log message\n"};
#endif
    REQUIRE(guiEntries.front().formattedMessage.ends_with(expectedLogSuffix));
    REQUIRE(contents == guiEntries.front().formattedMessage);

    std::filesystem::remove_all(log_directory);
}

TEST_CASE("Logger_Should_UseSourceFilenameForDebugAndTraceAndDisableThemInRelease")
{
    const auto log_directory = std::filesystem::temp_directory_path() / "logger-level-tests";
    std::filesystem::remove_all(log_directory);

    im_cpp::core::Logger logger("level-test");
    logger.setup(im_cpp::core::LogLevel::trace, log_directory);
    const auto guiSink = logger.attachGuiSink();
    logger.setLevel(im_cpp::core::LogLevel::trace);
    const auto activeLogger = logger.get();
    constexpr int debugSourceLine = 42;
    constexpr int traceSourceLine = 43;
    activeLogger->log(
        spdlog::source_loc{"src/tests/debug_source.cpp", debugSourceLine, ""}, spdlog::level::debug, "debug message");
    activeLogger->log(
        spdlog::source_loc{"src/tests/trace_source.cpp", traceSourceLine, ""}, spdlog::level::trace, "trace message");
    logger.shutdown();

    const auto entries = guiSink->drain();
#ifdef RELEASE
    REQUIRE(entries.empty());
#else
    REQUIRE(entries.size() == 2);
#ifdef IM_CPP_DEBUG_BUILD
    REQUIRE(entries.at(0).formattedMessage.ends_with(" [debug] [debug_source.cpp] [42] debug message\n"));
    REQUIRE(entries.at(1).formattedMessage.ends_with(" [trace] [trace_source.cpp] [43] trace message\n"));
#else
    REQUIRE(entries.at(0).formattedMessage.ends_with(" [debug] [debug_source.cpp] debug message\n"));
    REQUIRE(entries.at(1).formattedMessage.ends_with(" [trace] [trace_source.cpp] trace message\n"));
#endif
#endif

    std::filesystem::remove_all(log_directory);
}
