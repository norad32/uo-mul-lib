#include "radarcol_common.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/radarcol/radarcol.h"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <unistd.h>
#include <utility>

namespace uomul
{

struct RadarColReader::Impl
{
    std::filesystem::path path;
    int descriptor = -1;
    std::uint32_t colors = 0;

    Impl() = default;
    Impl(const Impl &) = delete;
    Impl & operator=(const Impl &) = delete;
    Impl(Impl &&) = delete;
    Impl & operator=(Impl &&) = delete;

    ~Impl()
    {
        if (descriptor >= 0)
        {
            static_cast<void>(::close(descriptor));
        }
    }
};

RadarColReader::RadarColReader(std::unique_ptr<Impl> impl) : impl_(std::move(impl))
{
}

RadarColReader::RadarColReader(RadarColReader &&) noexcept = default;
RadarColReader & RadarColReader::operator=(RadarColReader &&) noexcept = default;
RadarColReader::~RadarColReader() = default;

std::expected<RadarColReader, Error> RadarColReader::open(const std::filesystem::path & path)
{
    detail::ScopedDescriptor descriptor(::open(path.c_str(), O_RDONLY | O_CLOEXEC));
    if (descriptor.get() < 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, path, 0, std::strerror(errno)));
    }

    std::uint64_t fileSize = 0;
    auto colors = detail::RadarColCommon::inspectFile(descriptor.get(), path, fileSize);
    if (!colors)
    {
        return std::unexpected(colors.error());
    }

    auto impl = std::make_unique<Impl>();
    impl->path = path;
    impl->descriptor = descriptor.release();
    impl->colors = *colors;
    return RadarColReader(std::move(impl));
}

std::uint32_t RadarColReader::colorCount() const noexcept
{
    return impl_ ? impl_->colors : 0;
}

std::expected<RadarColor, Error> RadarColReader::readMapColor(std::uint16_t id) const
{
    if (id >= radarMapColorCount)
    {
        const auto path = impl_ ? impl_->path : std::filesystem::path{};
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::invalidCoordinate, path, id, "map color ID is outside the radar table"));
    }
    return readRawColor(id);
}

std::expected<RadarColor, Error> RadarColReader::readStaticColor(std::uint16_t id) const
{
    return readRawColor(radarStaticColorOffset + id);
}

std::expected<RadarColor, Error> RadarColReader::readRawColor(std::uint32_t index) const
{
    if (!impl_)
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::io, {}, detail::RadarColCommon::colorOffset(index), "reader is no longer active"));
    }

    const auto offset = detail::RadarColCommon::colorOffset(index);

    if (index >= impl_->colors)
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::truncatedInput, impl_->path, offset, "radar color is unavailable"));
    }

    std::array<std::byte, sizeof(RadarColor)> bytes{};

    if (!detail::MulCommon::readExact(impl_->descriptor, bytes.data(), bytes.size(), offset))
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::truncatedInput, impl_->path, offset, "radar color is missing or truncated"));
    }

    return detail::decodeLittleEndian<RadarColor>(bytes);
}

} // namespace uomul
