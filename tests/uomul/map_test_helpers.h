#pragma once

#include "uomul/map.h"
#include <cstdint>
#include <filesystem>
#include <string>

namespace uomul::test
{

class TestDirectory
{
public:
    TestDirectory();
    ~TestDirectory();

    TestDirectory(const TestDirectory &) = delete;
    TestDirectory & operator=(const TestDirectory &) = delete;
    TestDirectory(TestDirectory &&) = delete;
    TestDirectory & operator=(TestDirectory &&) = delete;

    [[nodiscard]] const std::filesystem::path & path() const;

private:
    std::filesystem::path path_;
};

inline constexpr uomul::Dimensions dimensions{.widthTiles = 16, .heightTiles = 16};
inline constexpr std::uint32_t defaultFixtureBlockCount = 4;

void writeFixture(const std::filesystem::path & path, std::uint32_t blocks = defaultFixtureBlockCount);
[[nodiscard]] std::string readBytes(const std::filesystem::path & path);

} // namespace uomul::test
