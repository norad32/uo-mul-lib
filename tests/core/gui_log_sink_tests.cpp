#include "core/gui_log_sink.h"
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <spdlog/logger.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/spdlog.h>

TEST_CASE("GuiLogSink_Should_KeepNewestEntriesWhenCapacityIsReached")
{
    const auto sink = std::make_shared<im_cpp::core::GuiLogSink>(2);
    sink->set_formatter(std::make_unique<spdlog::pattern_formatter>("%Y-%m-%d %H:%M:%S.%e [%l] [%n] [%#] %v"));
    spdlog::logger logger("gui-sink-test", sink);
    SPDLOG_LOGGER_INFO(&logger, "first");
    SPDLOG_LOGGER_WARN(&logger, "second");
    SPDLOG_LOGGER_ERROR(&logger, "third");

    const auto entries = sink->drain();
    REQUIRE(entries.size() == 2);
    CAPTURE(entries.at(0).formattedMessage);
    REQUIRE(entries.at(0).formattedMessage.ends_with(" [warning] [gui-sink-test] [15] second\n"));
    REQUIRE(entries.at(1).formattedMessage.ends_with(" [error] [gui-sink-test] [16] third\n"));
    REQUIRE(sink->drain().empty());
}

TEST_CASE("GuiLogSink_Should_ClearBufferedEntries")
{
    const auto sink = std::make_shared<im_cpp::core::GuiLogSink>();
    sink->set_formatter(std::make_unique<spdlog::pattern_formatter>("%Y-%m-%d %H:%M:%S.%e [%l] [%n] [%#] %v"));
    spdlog::logger logger("gui-sink-clear-test", sink);

    SPDLOG_LOGGER_INFO(&logger, "entry");
    sink->clear();

    REQUIRE(sink->drain().empty());
}
