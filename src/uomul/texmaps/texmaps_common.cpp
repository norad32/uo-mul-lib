#include "texmaps_common.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iterator>
#include <limits>
#include <vector>

namespace uomul::detail
{

namespace
{

constexpr std::size_t indexFieldByteCount = sizeof(std::uint32_t);
constexpr std::size_t lengthFieldOffset = indexFieldByteCount;
constexpr std::size_t extraFieldOffset = 2 * indexFieldByteCount;

} // namespace

TexMapsIndexEntry decodeTexMapsEntry(const TexMapsIndexEntryBytes & bytes)
{
    std::array<std::byte, sizeof(std::uint32_t)> field{};
    std::ranges::copy_n(bytes.begin(), field.size(), field.begin());
    const auto lookup = decodeLittleEndian<std::uint32_t>(field);
    std::ranges::copy_n(std::next(bytes.begin(), lengthFieldOffset), field.size(), field.begin());
    const auto length = decodeLittleEndian<std::uint32_t>(field);
    std::ranges::copy_n(std::next(bytes.begin(), extraFieldOffset), field.size(), field.begin());
    return {.lookup = lookup, .length = length, .extra = decodeLittleEndian<std::uint32_t>(field)};
}

TexMapsIndexEntryBytes encodeTexMapsEntry(TexMapsIndexEntry entry)
{
    TexMapsIndexEntryBytes bytes{};
    const auto lookup = encodeLittleEndian(entry.lookup);
    const auto length = encodeLittleEndian(entry.length);
    const auto extra = encodeLittleEndian(entry.extra);
    std::ranges::copy(lookup, bytes.begin());
    std::ranges::copy(length, std::next(bytes.begin(), lengthFieldOffset));
    std::ranges::copy(extra, std::next(bytes.begin(), extraFieldOffset));
    return bytes;
}

std::uint32_t texMapsEntryDimension(TexMapsIndexEntry entry)
{
    return entry.extra == 0 ? textureSmallDimension : textureLargeDimension;
}

std::uint32_t expectedTexMapsLength(TexMapsIndexEntry entry)
{
    return entry.extra == 0 ? static_cast<std::uint32_t>(textureSmallByteCount)
                            : static_cast<std::uint32_t>(textureLargeByteCount);
}

std::expected<std::vector<TexMapsIndexEntry>, Error>
loadTexMapsIndex(int descriptor, const std::filesystem::path & path, std::uint64_t dataSize)
{
    auto indexSize = MulCommon::regularFileSize(descriptor, path, "texture index");
    if (!indexSize)
    {
        return std::unexpected(indexSize.error());
    }

    if (*indexSize % textureIndexEntryByteCount != 0)
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::truncatedInput,
                                                    path,
                                                    *indexSize - (*indexSize % textureIndexEntryByteCount),
                                                    "texture index has a partial entry"));
    }

    const auto count = *indexSize / textureIndexEntryByteCount;
    if (count > textureCount)
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::invalidFormat,
                                                    path,
                                                    static_cast<std::uint64_t>(textureCount) * textureIndexEntryByteCount,
                                                    "texture index contains more than 0x4000 entries"));
    }

    std::vector<TexMapsIndexEntry> entries(static_cast<std::size_t>(count));
    for (std::size_t id = 0; id < entries.size(); id++)
    {
        TexMapsIndexEntryBytes bytes{};
        const auto offset = static_cast<std::uint64_t>(id) * textureIndexEntryByteCount;

        if (!MulCommon::readExact(descriptor, bytes.data(), bytes.size(), offset))
        {
            return std::unexpected(
                MulCommon::makeError(ErrorCode::truncatedInput, path, offset, "texture index entry is missing or truncated"));
        }

        const auto entry = decodeTexMapsEntry(bytes);
        if (entry.length == 0)
        {
            entries.at(id) = entry;
            continue;
        }

        if (entry.length != expectedTexMapsLength(entry))
        {
            return std::unexpected(MulCommon::makeError(
                ErrorCode::invalidFormat, path, offset + 4, "texture length does not match its dimension metadata"));
        }

        if (static_cast<std::uint64_t>(entry.lookup) + entry.length > dataSize)
        {
            return std::unexpected(MulCommon::makeError(
                ErrorCode::truncatedInput, path, offset, "texture payload range extends past the data file"));
        }

        entries.at(id) = entry;
    }

    return entries;
}

std::expected<TextureImage, Error>
readTexMapsTexture(int descriptor, const std::filesystem::path & path, TexMapsIndexEntry entry)
{
    TextureImage image;
    image.dimension = static_cast<TextureDimension>(texMapsEntryDimension(entry));
    std::vector<std::byte> bytes(entry.length);

    if (!MulCommon::readExact(descriptor, bytes.data(), bytes.size(), entry.lookup))
    {
        return std::unexpected(
            MulCommon::makeError(ErrorCode::truncatedInput, path, entry.lookup, "texture payload is missing or truncated"));
    }

    image.pixels.resize(bytes.size() / sizeof(std::uint16_t));

    for (std::size_t index = 0; index < image.pixels.size(); index++)
    {
        const std::array<std::byte, sizeof(std::uint16_t)> word{bytes.at(index * sizeof(std::uint16_t)),
                                                                bytes.at((index * sizeof(std::uint16_t)) + 1)};
        image.pixels.at(index) = decodeLittleEndian<std::uint16_t>(word);
    }

    return image;
}

std::vector<std::byte> encodeTexMapsTexture(const TextureImage & image)
{
    std::vector<std::byte> bytes(image.pixels.size() * sizeof(std::uint16_t));

    for (std::size_t index = 0; index < image.pixels.size(); index++)
    {
        const auto word = encodeLittleEndian(image.pixels.at(index));
        bytes.at(index * 2) = word.at(0);
        bytes.at((index * 2) + 1) = word.at(1);
    }

    return bytes;
}

} // namespace uomul::detail
