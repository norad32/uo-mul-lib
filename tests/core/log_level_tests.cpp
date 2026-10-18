#include "core/log_level.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("defaultLogLevel_Should_MatchBuildConfiguration")
{
#ifdef DEBUG_BUILD
    REQUIRE(uo_mule::core::defaultLogLevel() == uo_mule::core::LogLevel::trace);
#else
    REQUIRE(uo_mule::core::defaultLogLevel() == uo_mule::core::LogLevel::info);
#endif
}

TEST_CASE("parseLogLevel_Should_ParseNamedLevels")
{
    REQUIRE(uo_mule::core::parseLogLevel("TRACE") == uo_mule::core::LogLevel::trace);
    REQUIRE(uo_mule::core::parseLogLevel("DEBUG") == uo_mule::core::LogLevel::debug);
    REQUIRE(uo_mule::core::parseLogLevel("info") == uo_mule::core::LogLevel::info);
    REQUIRE(uo_mule::core::parseLogLevel(" Warning ") == uo_mule::core::LogLevel::warning);
    REQUIRE(uo_mule::core::parseLogLevel("ERROR") == uo_mule::core::LogLevel::error);
}

TEST_CASE("parseLogLevel_Should_ParsePythonNumericLevels")
{
    REQUIRE(uo_mule::core::parseLogLevel("0") == uo_mule::core::LogLevel::trace);
    REQUIRE(uo_mule::core::parseLogLevel("10") == uo_mule::core::LogLevel::debug);
    REQUIRE(uo_mule::core::parseLogLevel("20") == uo_mule::core::LogLevel::info);
    REQUIRE(uo_mule::core::parseLogLevel("30") == uo_mule::core::LogLevel::warning);
    REQUIRE(uo_mule::core::parseLogLevel("40") == uo_mule::core::LogLevel::error);
    REQUIRE(uo_mule::core::parseLogLevel("50") == uo_mule::core::LogLevel::critical);
}

TEST_CASE("parseLogLevel_Should_ParseAliases")
{
    REQUIRE(uo_mule::core::parseLogLevel("WARN") == uo_mule::core::LogLevel::warning);
    REQUIRE(uo_mule::core::parseLogLevel("FATAL") == uo_mule::core::LogLevel::critical);
    REQUIRE(uo_mule::core::parseLogLevel("CRITICAL") == uo_mule::core::LogLevel::critical);
}

TEST_CASE("parseLogLevel_Should_RejectUnknownValues")
{
    REQUIRE_FALSE(uo_mule::core::parseLogLevel("35").has_value());
    REQUIRE_FALSE(uo_mule::core::parseLogLevel("notSet").has_value());
    REQUIRE_FALSE(uo_mule::core::parseLogLevel("").has_value());
}
