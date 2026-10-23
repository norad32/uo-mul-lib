#include "radarcol_common.h"
#include "uomul/mul_common/mul_common.h"
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <system_error>

namespace uomul::detail
{

std::expected<std::uint32_t, Error>
RadarColCommon::inspectFile(int descriptor, const std::filesystem::path & path, std::uint64_t & fileSize)
{
    auto inspectedSize = MulCommon::regularFileSize(descriptor, path, "radar color");
    if (!inspectedSize)
    {
        return std::unexpected(inspectedSize.error());
    }
    fileSize = *inspectedSize;

    if (fileSize % colorByteSize != 0)
    {
        return std::unexpected(MulCommon::makeError(
            ErrorCode::truncatedInput, path, fileSize - (fileSize % colorByteSize), "radar color table has a partial color"));
    }

    const std::uint64_t colorCount = fileSize / colorByteSize;
    if (colorCount > std::numeric_limits<std::uint32_t>::max())
    {
        return std::unexpected(MulCommon::makeError(
            ErrorCode::truncatedInput, path, fileSize, "radar color count exceeds the supported index range"));
    }

    return static_cast<std::uint32_t>(colorCount);
}

std::uint64_t RadarColCommon::colorOffset(std::uint32_t index) noexcept
{
    return static_cast<std::uint64_t>(index) * colorByteSize;
}

std::expected<void, Error>
RadarColCommon::validateEditorPaths(const std::filesystem::path & source, const std::filesystem::path & output, bool overwrite)
{
    if (source.empty() || output.empty() || output.filename().empty())
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::io, output, 0, "source and output paths must name files"));
    }

    auto exactPath = MulCommon::pathsExactlyMatch(source, output);
    if (!exactPath)
    {
        return std::unexpected(exactPath.error());
    }

    auto aliased = MulCommon::pathsAlias(source, output);
    if (!aliased)
    {
        return std::unexpected(aliased.error());
    }

    if (*aliased && !*exactPath)
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::io, output, 0, "output path resolves to the source file"));
    }

    if (auto check = MulCommon::checkOutputPath(output, overwrite || *exactPath); !check)
    {
        return check;
    }

    const auto parent = output.parent_path();
    std::error_code directoryError;

    if (!parent.empty() && !std::filesystem::is_directory(parent, directoryError))
    {
        return std::unexpected(MulCommon::makeError(
            ErrorCode::io, parent, 0, directoryError ? directoryError.message() : "output directory does not exist"));
    }

    return {};
}

std::expected<void, Error> RadarColCommon::validateOutputPath(const std::filesystem::path & output, bool overwrite)
{
    if (output.empty() || output.filename().empty())
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::io, output, 0, "output path must name a file"));
    }

    if (auto check = MulCommon::checkOutputPath(output, overwrite); !check)
    {
        return check;
    }

    const auto parent = output.parent_path();
    std::error_code directoryError;
    const bool parentExists = parent.empty() || std::filesystem::is_directory(parent, directoryError);

    if (directoryError || !parentExists)
    {
        return std::unexpected(MulCommon::makeError(
            ErrorCode::io, parent, 0, directoryError ? directoryError.message() : "output directory does not exist"));
    }

    std::error_code statusError;
    const bool outputExists = std::filesystem::exists(output, statusError);

    if (statusError)
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::io, output, 0, statusError.message()));
    }

    if (outputExists && !std::filesystem::is_regular_file(output, statusError))
    {
        return std::unexpected(MulCommon::makeError(
            ErrorCode::io, output, 0, statusError ? statusError.message() : "output must be a regular file"));
    }

    return {};
}

} // namespace uomul::detail
