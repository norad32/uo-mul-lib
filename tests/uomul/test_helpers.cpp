#include "test_helpers.h"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>

namespace uomul::test
{

TestDirectory::TestDirectory()
{
    static std::uint64_t sequence = 0;
    path_ = std::filesystem::temp_directory_path() / ("uomul-test-" + std::to_string(sequence++));
    std::filesystem::remove_all(path_);
    std::filesystem::create_directories(path_);
}

TestDirectory::~TestDirectory()
{
    std::filesystem::remove_all(path_);
}

const std::filesystem::path & TestDirectory::path() const
{
    return path_;
}

std::string readBytes(const std::filesystem::path & path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

} // namespace uomul::test
