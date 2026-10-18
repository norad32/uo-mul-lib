#include "radarcol_common.h"
#include "uomul/mul_common/mul_common.h"
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <limits>
#include <sys/stat.h>
#include <system_error>

namespace uomul::detail
{

std::expected<std::uint32_t, Error>
RadarColCommon::inspectFile(int descriptor, const std::filesystem::path & path, std::uint64_t & fileSize)
{
    struct stat status{};
    if (::fstat(descriptor, &status) != 0)
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::io, path, 0, std::strerror(errno)));
    }

    if (!S_ISREG(status.st_mode))
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::io, path, 0, "radar color input must be a regular file"));
    }

    if (status.st_size < 0)
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::io, path, 0, "could not determine radar color file size"));
    }

    fileSize = static_cast<std::uint64_t>(status.st_size);

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

    std::error_code pathError;
    const auto sourceAbsolute = std::filesystem::absolute(source, pathError).lexically_normal();

    if (pathError)
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::io, source, 0, pathError.message()));
    }

    const auto outputAbsolute = std::filesystem::absolute(output, pathError).lexically_normal();

    if (pathError)
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::io, output, 0, pathError.message()));
    }

    if (sourceAbsolute == outputAbsolute)
    {
        return std::unexpected(
            MulCommon::makeError(ErrorCode::io, output, 0, "in-place editing is not supported; choose a separate output path"));
    }

    pathError.clear();
    const bool outputExists = std::filesystem::exists(output, pathError);

    if (pathError)
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::io, output, 0, pathError.message()));
    }

    if (outputExists && std::filesystem::equivalent(source, output, pathError))
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::io, output, 0, "output path resolves to the source file"));
    }

    if (pathError)
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::io, output, 0, pathError.message()));
    }

    if (auto check = MulCommon::checkOutputPath(output, overwrite); !check)
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
