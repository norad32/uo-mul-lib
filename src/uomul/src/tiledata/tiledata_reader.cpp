#include "tiledata_common.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/tiledata/tiledata.h"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <utility>

namespace uomul
{

struct TiledataReader::Impl
{
    detail::ScopedDescriptor descriptor;
    std::filesystem::path path;
    TiledataLayout layout = TiledataLayout::legacy32;
};

TiledataReader::TiledataReader(std::unique_ptr<Impl> impl) : impl_(std::move(impl))
{
}

TiledataReader::TiledataReader(TiledataReader &&) noexcept = default;
TiledataReader & TiledataReader::operator=(TiledataReader &&) noexcept = default;
TiledataReader::~TiledataReader() = default;

std::expected<TiledataReader, Error> TiledataReader::open(const std::filesystem::path & path, TiledataLayout layout)
{
    detail::ScopedDescriptor descriptor(::open(path.c_str(), O_RDONLY | O_CLOEXEC));
    if (descriptor.get() < 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, path, 0, std::strerror(errno)));
    }

    std::uint64_t fileSize = 0;
    if (auto valid = detail::validateTiledataFile(descriptor.get(), path, layout, fileSize); !valid)
    {
        return std::unexpected(valid.error());
    }

    auto impl = std::make_unique<Impl>();
    impl->descriptor = std::move(descriptor);
    impl->path = path;
    impl->layout = layout;
    return TiledataReader(std::move(impl));
}

TiledataLayout TiledataReader::layout() const noexcept
{
    return impl_ ? impl_->layout : TiledataLayout::legacy32;
}

std::uint32_t TiledataReader::mapTileCount() const noexcept
{
    return impl_ ? tiledataMapTileCount : 0;
}

std::uint32_t TiledataReader::staticTileCount() const noexcept
{
    return impl_ ? tiledataStaticTileCount : 0;
}

std::expected<MapTileRecord, Error> TiledataReader::readMapTile(std::uint16_t id) const
{
    if (!impl_)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, {}, 0, "reader is no longer active"));
    }

    if (id >= tiledataMapTileCount)
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::invalidCoordinate, impl_->path, id, "land tile ID is out of range"));
    }

    const auto offset = detail::tiledataMapRecordOffset(id, impl_->layout);
    std::array<std::byte, detail::tiledataExtendedLandRecordByteCount> bytes{};
    const auto recordSize = detail::tiledataMapRecordByteCount(impl_->layout);
    if (!detail::MulCommon::readExact(impl_->descriptor.get(), bytes.data(), recordSize, offset))
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::truncatedInput, impl_->path, offset, "land record is missing or truncated"));
    }
    return detail::decodeMapTile(std::span(bytes).first(recordSize), impl_->layout, impl_->path, offset);
}

std::expected<StaticTileRecord, Error> TiledataReader::readStaticTile(std::uint32_t id) const
{
    if (!impl_)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, {}, 0, "reader is no longer active"));
    }
    if (id >= tiledataStaticTileCount)
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::invalidCoordinate, impl_->path, id, "item tile ID is out of range"));
    }

    const auto offset = detail::tiledataStaticRecordOffset(id, impl_->layout);
    std::array<std::byte, detail::tiledataExtendedItemRecordByteCount> bytes{};
    const auto recordSize = detail::tiledataStaticRecordByteCount(impl_->layout);
    if (!detail::MulCommon::readExact(impl_->descriptor.get(), bytes.data(), recordSize, offset))
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::truncatedInput, impl_->path, offset, "item record is missing or truncated"));
    }
    return detail::decodeStaticTile(std::span(bytes).first(recordSize), impl_->layout, impl_->path, offset);
}

} // namespace uomul
