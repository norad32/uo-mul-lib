#include "texmaps_common.h"
#include "uomul/texmaps/texmaps.h"
#include <cerrno>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

namespace uomul
{

struct TexMapsReader::Impl
{
    std::filesystem::path indexPath;
    std::filesystem::path dataPath;
    detail::ScopedDescriptor indexDescriptor;
    detail::ScopedDescriptor dataDescriptor;
    std::vector<detail::TexMapsIndexEntry> entries;
};

TexMapsReader::TexMapsReader(std::unique_ptr<Impl> impl) : impl_(std::move(impl))
{
}

TexMapsReader::TexMapsReader(TexMapsReader &&) noexcept = default;
TexMapsReader & TexMapsReader::operator=(TexMapsReader &&) noexcept = default;
TexMapsReader::~TexMapsReader() = default;

std::expected<TexMapsReader, Error> TexMapsReader::open(const std::filesystem::path & indexFile,
                                                        const std::filesystem::path & dataFile)
{
    detail::ScopedDescriptor indexDescriptor(::open(indexFile.c_str(), O_RDONLY | O_CLOEXEC));
    if (indexDescriptor.get() < 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, indexFile, 0, std::strerror(errno)));
    }

    detail::ScopedDescriptor dataDescriptor(::open(dataFile.c_str(), O_RDONLY | O_CLOEXEC));
    if (dataDescriptor.get() < 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, dataFile, 0, std::strerror(errno)));
    }

    auto dataSize = detail::MulCommon::regularFileSize(dataDescriptor.get(), dataFile, "texture data");
    if (!dataSize)
    {
        return std::unexpected(dataSize.error());
    }

    auto entries = detail::loadTexMapsIndex(indexDescriptor.get(), indexFile, *dataSize);
    if (!entries)
    {
        return std::unexpected(entries.error());
    }

    auto impl = std::make_unique<Impl>();
    impl->indexPath = indexFile;
    impl->dataPath = dataFile;
    impl->indexDescriptor = std::move(indexDescriptor);
    impl->dataDescriptor = std::move(dataDescriptor);
    impl->entries = std::move(*entries);

    return TexMapsReader(std::move(impl));
}

std::uint32_t TexMapsReader::textureCount() const noexcept
{
    return impl_ ? static_cast<std::uint32_t>(impl_->entries.size()) : 0;
}

std::expected<bool, Error> TexMapsReader::hasTexture(std::uint32_t id) const
{
    const auto path = impl_ ? impl_->indexPath : std::filesystem::path{};

    if (!impl_ || id >= impl_->entries.size())
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::invalidCoordinate, path, id, "texture ID is outside the index"));
    }

    return impl_->entries.at(id).length != 0;
}

std::expected<TextureImage, Error> TexMapsReader::readTexture(std::uint32_t id) const
{
    const auto path = impl_ ? impl_->indexPath : std::filesystem::path{};
    if (!impl_ || id >= impl_->entries.size())
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::invalidCoordinate, path, id, "texture ID is outside the index"));
    }
    const auto entry = impl_->entries.at(id);

    if (entry.length == 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::invalidFormat,
                                                            path,
                                                            static_cast<std::uint64_t>(id) * textureIndexEntryByteCount,
                                                            "texture slot is empty"));
    }

    return detail::readTexMapsTexture(impl_->dataDescriptor.get(), impl_->dataPath, entry);
}

} // namespace uomul
