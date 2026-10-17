#include "map_common.h"
#include "uomul/map.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace uomul::detail
{

namespace
{

constexpr int maximumAttempts = 16;

[[nodiscard]] inline bool hasInvalidMapDimensions(Dimensions dimensions)
{
    return dimensions.widthTiles == 0 || dimensions.heightTiles == 0 || dimensions.widthTiles % mapBlockDimension != 0 ||
           dimensions.heightTiles % mapBlockDimension != 0;
}

[[nodiscard]] inline bool exceedsSupportedMapFileSize(std::uint64_t blockCount)
{
    return blockCount > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()) / MapCommon::mapBlockByteSize;
}

template <typename Region>
[[nodiscard]] inline bool regionExceedsDimensions(Region region, Dimensions dimensions, std::uint32_t unit)
{
    return region.min.x > region.maxExclusive.x || region.min.y > region.maxExclusive.y ||
           region.maxExclusive.x > dimensions.widthTiles / unit || region.maxExclusive.y > dimensions.heightTiles / unit;
}

template <typename Region>
std::expected<void, Error> validateRegionBounds(Region region,
                                                Dimensions dimensions,
                                                std::uint32_t unit,
                                                const std::filesystem::path & path,
                                                const char * message)
{
    if (regionExceedsDimensions(region, dimensions, unit))
    {
        return std::unexpected(MapCommon::makeError(ErrorCode::invalidCoordinate, path, 0, message));
    }
    return {};
}

std::atomic<std::uint64_t> temporarySequence{0};

template <typename Byte, typename Transfer>
bool transferExact(int descriptor, Byte * bytes, std::size_t size, std::uint64_t offset, Transfer transfer)
{
    std::size_t transferred = 0;
    while (transferred < size)
    {
        const auto result = transfer(descriptor,
                                     std::next(bytes, static_cast<std::ptrdiff_t>(transferred)),
                                     size - transferred,
                                     static_cast<off_t>(offset + transferred));
        if (result < 0 && errno == EINTR)
        {
            continue;
        }
        if (result <= 0)
        {
            return false;
        }
        transferred += static_cast<std::size_t>(result);
    }
    return true;
}

} // namespace

ScopedDescriptor::ScopedDescriptor(int descriptor) noexcept : descriptor_(descriptor)
{
}

ScopedDescriptor::ScopedDescriptor(ScopedDescriptor && other) noexcept : descriptor_(other.release())
{
}

ScopedDescriptor & ScopedDescriptor::operator=(ScopedDescriptor && other) noexcept
{
    if (this != &other)
    {
        static_cast<void>(close());
        descriptor_ = other.release();
    }
    return *this;
}

ScopedDescriptor::~ScopedDescriptor()
{
    static_cast<void>(close());
}

int ScopedDescriptor::get() const noexcept
{
    return descriptor_;
}

int ScopedDescriptor::release() noexcept
{
    const int descriptor = descriptor_;
    descriptor_ = -1;
    return descriptor;
}

int ScopedDescriptor::close() noexcept
{
    if (descriptor_ < 0)
    {
        return 0;
    }
    const int descriptor = release();
    return ::close(descriptor);
}

TemporaryPath::TemporaryPath(std::filesystem::path path) : path_(std::move(path))
{
}

TemporaryPath::~TemporaryPath()
{
    if (active_ && !path_.empty())
    {
        static_cast<void>(::unlink(path_.c_str()));
    }
}

void TemporaryPath::release() noexcept
{
    active_ = false;
}

Error MapCommon::makeError(ErrorCode code, const std::filesystem::path & path, std::uint64_t offset, std::string message)
{
    return Error{.code = code, .path = path, .offset = offset, .message = std::move(message)};
}

std::expected<std::uint64_t, Error> MapCommon::blockCount(Dimensions dimensions, const std::filesystem::path & path)
{
    if (hasInvalidMapDimensions(dimensions))
    {
        return std::unexpected(makeError(ErrorCode::invalidDimensions, path, 0, "dimensions must be positive multiples of 8"));
    }

    const auto width = static_cast<std::uint64_t>(dimensions.widthTiles / mapBlockDimension);
    const auto height = static_cast<std::uint64_t>(dimensions.heightTiles / mapBlockDimension);
    const auto blocks = width * height;
    if (exceedsSupportedMapFileSize(blocks))
    {
        return std::unexpected(
            makeError(ErrorCode::invalidDimensions, path, 0, "map file size exceeds supported offset range"));
    }

    return blocks;
}

std::expected<void, Error> MapCommon::validateBlock(BlockCoord block, Dimensions dimensions, const std::filesystem::path & path)
{
    if (block.x >= dimensions.widthTiles / mapBlockDimension || block.y >= dimensions.heightTiles / mapBlockDimension)
    {
        return std::unexpected(makeError(ErrorCode::invalidCoordinate, path, 0, "block coordinate is outside map dimensions"));
    }

    return {};
}

std::expected<void, Error> MapCommon::validateTile(TileCoord tile, Dimensions dimensions, const std::filesystem::path & path)
{
    if (tile.x >= dimensions.widthTiles || tile.y >= dimensions.heightTiles)
    {
        return std::unexpected(makeError(ErrorCode::invalidCoordinate, path, 0, "tile coordinate is outside map dimensions"));
    }

    return {};
}

std::uint64_t MapCommon::blockId(BlockCoord block, Dimensions dimensions)
{
    return (static_cast<std::uint64_t>(block.x) * (dimensions.heightTiles / mapBlockDimension)) + block.y;
}

std::uint64_t MapCommon::blockOffset(BlockCoord block, Dimensions dimensions)
{
    return blockId(block, dimensions) * mapBlockByteSize;
}

bool MapCommon::readExact(int descriptor, void * data, std::size_t size, std::uint64_t offset)
{
    return transferExact(descriptor, static_cast<std::byte *>(data), size, offset, ::pread);
}

bool MapCommon::readBlockBatch(int descriptor,
                               BlockCoord first,
                               std::size_t blockCount,
                               Dimensions dimensions,
                               std::span<std::byte> buffer)
{
    const auto blockByteSize = static_cast<std::size_t>(mapBlockByteSize);
    if (blockCount > buffer.size() / blockByteSize)
    {
        return false;
    }

    const auto byteCount = blockCount * blockByteSize;
    return readExact(descriptor, buffer.data(), byteCount, blockOffset(first, dimensions));
}

bool MapCommon::writeExact(int descriptor, const void * data, std::size_t size, std::uint64_t offset)
{
    return transferExact(descriptor, static_cast<const std::byte *>(data), size, offset, ::pwrite);
}

MapBlock MapCommon::decodeBlock(const MapBlockBytes & raw)
{
    MapBlock block{};
    for (std::size_t index = 0; index < block.size(); index++)
    {
        const auto offset = static_cast<std::size_t>(mapHeaderByteSize) + (index * static_cast<std::size_t>(mapTileByteSize));
        const auto tileBytes = std::span<const std::byte, mapTileByteSize>(
            std::next(raw.begin(), static_cast<std::ptrdiff_t>(offset)), static_cast<std::size_t>(mapTileByteSize));
        block.at(index) = decodeTile(tileBytes);
    }

    return block;
}

void MapCommon::encodeBlock(MapBlockBytes & raw, const MapBlock & block)
{
    for (std::size_t index = 0; index < block.size(); index++)
    {
        const auto offset = static_cast<std::size_t>(mapHeaderByteSize) + (index * static_cast<std::size_t>(mapTileByteSize));
        const auto encoded = encodeTile(block.at(index));
        std::ranges::copy(encoded, std::next(raw.begin(), static_cast<std::ptrdiff_t>(offset)));
    }
}

std::filesystem::path MapCommon::temporaryPath(const std::filesystem::path & path)
{
    const auto sequence = temporarySequence.fetch_add(1, std::memory_order_relaxed);
    return path.parent_path() /
           (path.filename().string() + ".uomul-tmp-" + std::to_string(::getpid()) + "-" + std::to_string(sequence));
}

std::expected<int, Error> MapCommon::createTemporary(const std::filesystem::path & path, std::filesystem::path & temporary)
{
    for (int attempt = 0; attempt < maximumAttempts; attempt++)
    {
        temporary = temporaryPath(path);
        const int descriptor = ::open(temporary.c_str(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
        if (descriptor >= 0)
        {
            return descriptor;
        }
        if (errno != EEXIST)
        {
            return std::unexpected(makeError(ErrorCode::io, temporary, 0, std::strerror(errno)));
        }
    }

    return std::unexpected(makeError(ErrorCode::io, path, 0, "unable to allocate unique temporary file"));
}

std::expected<void, Error>
MapCommon::publishFile(const std::filesystem::path & temporary, const std::filesystem::path & destination, bool overwrite)
{
    std::error_code filesystemError;
    const bool destinationExists = std::filesystem::exists(destination, filesystemError);
    if (filesystemError)
    {
        return std::unexpected(makeError(ErrorCode::commitFailed, destination, 0, filesystemError.message()));
    }
    if (destinationExists && !overwrite)
    {
        return std::unexpected(makeError(ErrorCode::commitFailed, destination, 0, "output already exists"));
    }
    if (!destinationExists)
    {
        if (::link(temporary.c_str(), destination.c_str()) == 0)
        {
            static_cast<void>(::unlink(temporary.c_str()));
            return {};
        }

        return std::unexpected(makeError(ErrorCode::commitFailed, destination, 0, std::strerror(errno)));
    }

    if (::rename(temporary.c_str(), destination.c_str()) != 0)
    {
        return std::unexpected(makeError(ErrorCode::commitFailed, destination, 0, std::strerror(errno)));
    }

    return {};
}

std::expected<void, Error>
MapCommon::validateRegion(BlockRegion region, Dimensions dimensions, const std::filesystem::path & path)
{
    return validateRegionBounds(
        region, dimensions, mapBlockDimension, path, "block region is invalid or exceeds map dimensions");
}

std::expected<void, Error>
MapCommon::validateRegion(TileRegion region, Dimensions dimensions, const std::filesystem::path & path)
{
    return validateRegionBounds(region, dimensions, 1, path, "tile region is invalid or exceeds map dimensions");
}

std::expected<void, Error> MapCommon::checkOutputPath(const std::filesystem::path & path, bool overwrite)
{
    std::error_code filesystemError;
    const bool exists = std::filesystem::exists(path, filesystemError);
    if (filesystemError)
    {
        return std::unexpected(makeError(ErrorCode::io, path, 0, filesystemError.message()));
    }
    if (!overwrite && exists)
    {
        return std::unexpected(makeError(ErrorCode::io, path, 0, "output already exists"));
    }

    return {};
}

} // namespace uomul::detail
