#pragma once

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

[[nodiscard]] std::string readBytes(const std::filesystem::path & path);

} // namespace uomul::test
