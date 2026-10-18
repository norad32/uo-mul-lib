#pragma once

#include "uomul/radarcol/radarcol.h"
#include <cstdint>
#include <expected>
#include <filesystem>

namespace uomul::detail
{

class RadarColCommon
{
public:
    static constexpr std::uint64_t colorByteSize = sizeof(RadarColor);
    static constexpr std::uint64_t tableByteSize = static_cast<std::uint64_t>(radarColorCount) * colorByteSize;

    [[nodiscard]] static std::expected<std::uint32_t, Error>
    inspectFile(int descriptor, const std::filesystem::path & path, std::uint64_t & fileSize);
    [[nodiscard]] static std::uint64_t colorOffset(std::uint32_t index) noexcept;
    [[nodiscard]] static std::expected<void, Error>
    validateEditorPaths(const std::filesystem::path & source, const std::filesystem::path & output, bool overwrite);
    [[nodiscard]] static std::expected<void, Error> validateOutputPath(const std::filesystem::path & output, bool overwrite);
};

} // namespace uomul::detail
