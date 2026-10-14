#include "core/log_level.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("defaultLogLevel_Should_MatchBuildConfiguration")
{
#ifdef IM_CPP_DEBUG_BUILD
    REQUIRE(im_cpp::core::defaultLogLevel() == im_cpp::core::LogLevel::trace);
#else
    REQUIRE(im_cpp::core::defaultLogLevel() == im_cpp::core::LogLevel::info);
#endif
}

TEST_CASE("parseLogLevel_Should_ParseNamedLevels")
{
    REQUIRE(im_cpp::core::parseLogLevel("TRACE") == im_cpp::core::LogLevel::trace);
    REQUIRE(im_cpp::core::parseLogLevel("DEBUG") == im_cpp::core::LogLevel::debug);
    REQUIRE(im_cpp::core::parseLogLevel("info") == im_cpp::core::LogLevel::info);
    REQUIRE(im_cpp::core::parseLogLevel(" Warning ") == im_cpp::core::LogLevel::warning);
    REQUIRE(im_cpp::core::parseLogLevel("ERROR") == im_cpp::core::LogLevel::error);
}

TEST_CASE("parseLogLevel_Should_ParsePythonNumericLevels")
{
    REQUIRE(im_cpp::core::parseLogLevel("0") == im_cpp::core::LogLevel::trace);
    REQUIRE(im_cpp::core::parseLogLevel("10") == im_cpp::core::LogLevel::debug);
    REQUIRE(im_cpp::core::parseLogLevel("20") == im_cpp::core::LogLevel::info);
    REQUIRE(im_cpp::core::parseLogLevel("30") == im_cpp::core::LogLevel::warning);
    REQUIRE(im_cpp::core::parseLogLevel("40") == im_cpp::core::LogLevel::error);
    REQUIRE(im_cpp::core::parseLogLevel("50") == im_cpp::core::LogLevel::critical);
}

TEST_CASE("parseLogLevel_Should_ParseAliases")
{
    REQUIRE(im_cpp::core::parseLogLevel("WARN") == im_cpp::core::LogLevel::warning);
    REQUIRE(im_cpp::core::parseLogLevel("FATAL") == im_cpp::core::LogLevel::critical);
    REQUIRE(im_cpp::core::parseLogLevel("CRITICAL") == im_cpp::core::LogLevel::critical);
}

TEST_CASE("parseLogLevel_Should_RejectUnknownValues")
{
    REQUIRE_FALSE(im_cpp::core::parseLogLevel("35").has_value());
    REQUIRE_FALSE(im_cpp::core::parseLogLevel("notSet").has_value());
    REQUIRE_FALSE(im_cpp::core::parseLogLevel("").has_value());
}
