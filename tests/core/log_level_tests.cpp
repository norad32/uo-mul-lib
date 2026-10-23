#include "logger/log_level.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("defaultLogLevel_Should_MatchBuildConfiguration")
{
#ifdef DEBUG_BUILD
    REQUIRE(uo_mule::logger::defaultLogLevel() == uo_mule::logger::LogLevel::trace);
#else
    REQUIRE(uo_mule::logger::defaultLogLevel() == uo_mule::logger::LogLevel::info);
#endif
}

TEST_CASE("parseLogLevel_Should_ParseNamedLevels")
{
    REQUIRE(uo_mule::logger::parseLogLevel("TRACE") == uo_mule::logger::LogLevel::trace);
    REQUIRE(uo_mule::logger::parseLogLevel("DEBUG") == uo_mule::logger::LogLevel::debug);
    REQUIRE(uo_mule::logger::parseLogLevel("info") == uo_mule::logger::LogLevel::info);
    REQUIRE(uo_mule::logger::parseLogLevel(" Warning ") == uo_mule::logger::LogLevel::warning);
    REQUIRE(uo_mule::logger::parseLogLevel("ERROR") == uo_mule::logger::LogLevel::error);
}

TEST_CASE("parseLogLevel_Should_ParsePythonNumericLevels")
{
    REQUIRE(uo_mule::logger::parseLogLevel("0") == uo_mule::logger::LogLevel::trace);
    REQUIRE(uo_mule::logger::parseLogLevel("10") == uo_mule::logger::LogLevel::debug);
    REQUIRE(uo_mule::logger::parseLogLevel("20") == uo_mule::logger::LogLevel::info);
    REQUIRE(uo_mule::logger::parseLogLevel("30") == uo_mule::logger::LogLevel::warning);
    REQUIRE(uo_mule::logger::parseLogLevel("40") == uo_mule::logger::LogLevel::error);
    REQUIRE(uo_mule::logger::parseLogLevel("50") == uo_mule::logger::LogLevel::critical);
}

TEST_CASE("parseLogLevel_Should_ParseAliases")
{
    REQUIRE(uo_mule::logger::parseLogLevel("WARN") == uo_mule::logger::LogLevel::warning);
    REQUIRE(uo_mule::logger::parseLogLevel("FATAL") == uo_mule::logger::LogLevel::critical);
    REQUIRE(uo_mule::logger::parseLogLevel("CRITICAL") == uo_mule::logger::LogLevel::critical);
}

TEST_CASE("parseLogLevel_Should_RejectUnknownValues")
{
    REQUIRE_FALSE(uo_mule::logger::parseLogLevel("35").has_value());
    REQUIRE_FALSE(uo_mule::logger::parseLogLevel("notSet").has_value());
    REQUIRE_FALSE(uo_mule::logger::parseLogLevel("").has_value());
}
