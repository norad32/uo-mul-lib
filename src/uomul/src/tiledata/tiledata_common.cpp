#include "tiledata_common.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/tiledata/tiledata.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iterator>
#include <span>
#include <string>
#include <string_view>

namespace uomul::detail
{

namespace
{

constexpr unsigned bitsPerByte = 8U;
constexpr std::size_t itemWeightOffset = 0;
constexpr std::size_t itemQualityOffset = 1;
constexpr std::size_t itemMiscDataOffset = 2;
constexpr std::size_t itemUnknown2Offset = 4;
constexpr std::size_t itemQuantityOffset = 5;
constexpr std::size_t itemAnimationOffset = 6;
constexpr std::size_t itemUnknown3Offset = 8;
constexpr std::size_t itemHueOffset = 9;
constexpr std::size_t itemStackingOffset = 10;
constexpr std::size_t itemValueOffset = 11;
constexpr std::size_t itemHeightOffset = 12;
constexpr std::size_t itemNameOffset = 13;

[[nodiscard]] std::uint64_t readUnsigned(std::span<const std::byte> bytes)
{
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < bytes.size(); index++)
    {
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes.subspan(index).front()))
                 << (index * bitsPerByte);
    }
    return value;
}

[[nodiscard]] std::int16_t readSigned16(std::span<const std::byte> bytes)
{
    return static_cast<std::int16_t>(readUnsigned(bytes));
}

void writeUnsigned(std::span<std::byte> bytes, std::uint64_t value)
{
    for (std::size_t index = 0; index < bytes.size(); index++)
    {
        bytes.subspan(index).front() = static_cast<std::byte>(value >> (index * bitsPerByte));
    }
}

void writeSigned16(std::span<std::byte> bytes, std::int16_t value)
{
    writeUnsigned(bytes, static_cast<std::uint16_t>(value));
}

[[nodiscard]] std::string readName(std::span<const std::byte> bytes)
{
    const auto end = std::ranges::find(bytes, std::byte{});
    const auto length = static_cast<std::size_t>(std::distance(bytes.begin(), end));
    std::string name;
    name.reserve(length);
    for (const auto byte : bytes.first(length))
    {
        name.push_back(static_cast<char>(std::to_integer<unsigned char>(byte)));
    }
    return name;
}

template <std::size_t Size>
[[nodiscard]] std::expected<std::array<std::byte, Size>, Error> encodeName(std::string_view name,
                                                                           const std::filesystem::path & path)
{
    if (name.size() > tiledataNameByteCount)
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::invalidFormat, path, 0, "tiledata name exceeds 20 bytes"));
    }

    std::array<std::byte, Size> bytes{};
    for (std::size_t index = 0; index < name.size(); index++)
    {
        bytes.at(index) = static_cast<std::byte>(static_cast<unsigned char>(name.at(index)));
    }
    return bytes;
}

} // namespace

std::expected<void, Error>
validateTiledataFile(int descriptor, const std::filesystem::path & path, TiledataLayout layout, std::uint64_t & fileSize)
{
    auto inspectedSize = MulCommon::regularFileSize(descriptor, path, "tiledata");
    if (!inspectedSize)
    {
        return std::unexpected(inspectedSize.error());
    }
    fileSize = *inspectedSize;
    if (fileSize != tiledataFileByteCount(layout))
    {
        return std::unexpected(MulCommon::makeError(
            ErrorCode::truncatedInput, path, fileSize, "tiledata size does not match the selected layout"));
    }
    return {};
}

std::uint64_t tiledataMapRecordOffset(std::uint16_t id, TiledataLayout layout)
{
    const auto block = static_cast<std::uint64_t>(id) / tiledataTilesPerBlock;
    const auto record = static_cast<std::uint64_t>(id) % tiledataTilesPerBlock;
    return (block * tiledataMapBlockByteCount(layout)) + tiledataBlockHeaderByteCount +
           (record * tiledataMapRecordByteCount(layout));
}

std::uint64_t tiledataStaticRecordOffset(std::uint32_t id, TiledataLayout layout)
{
    const auto block = static_cast<std::uint64_t>(id) / tiledataTilesPerBlock;
    const auto record = static_cast<std::uint64_t>(id) % tiledataTilesPerBlock;
    return tiledataMapSectionByteCount(layout) + (block * tiledataStaticBlockByteCount(layout)) + tiledataBlockHeaderByteCount +
           (record * tiledataStaticRecordByteCount(layout));
}

std::expected<MapTileRecord, Error>
decodeMapTile(std::span<const std::byte> bytes, TiledataLayout layout, const std::filesystem::path & path, std::uint64_t offset)
{
    const auto expectedSize = tiledataMapRecordByteCount(layout);
    if (bytes.size() != expectedSize)
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::invalidFormat, path, offset, "invalid land record size"));
    }
    const auto flagsSize = layout == TiledataLayout::legacy32 ? 4U : 8U;
    return MapTileRecord{
        .flags = static_cast<TiledataFlag>(readUnsigned(bytes.first(flagsSize))),
        .textureId = static_cast<std::uint16_t>(readUnsigned(bytes.subspan(flagsSize, 2))),
        .name = readName(bytes.subspan(flagsSize + 2, tiledataNameByteCount)),
    };
}

std::expected<StaticTileRecord, Error> decodeStaticTile(std::span<const std::byte> bytes,
                                                        TiledataLayout layout,
                                                        const std::filesystem::path & path,
                                                        std::uint64_t offset)
{
    const auto expectedSize = tiledataStaticRecordByteCount(layout);
    if (bytes.size() != expectedSize)
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::invalidFormat, path, offset, "invalid item record size"));
    }
    const auto flagsSize = layout == TiledataLayout::legacy32 ? 4U : 8U;
    const auto fields = bytes.subspan(flagsSize);
    return StaticTileRecord{
        .flags = static_cast<TiledataFlag>(readUnsigned(bytes.first(flagsSize))),
        .weight = std::to_integer<std::uint8_t>(fields.subspan(itemWeightOffset).front()),
        .quality = std::to_integer<std::uint8_t>(fields.subspan(itemQualityOffset).front()),
        .miscData = readSigned16(fields.subspan(itemMiscDataOffset, 2)),
        .unknown2 = std::to_integer<std::uint8_t>(fields.subspan(itemUnknown2Offset).front()),
        .quantity = std::to_integer<std::uint8_t>(fields.subspan(itemQuantityOffset).front()),
        .animation = readSigned16(fields.subspan(itemAnimationOffset, 2)),
        .unknown3 = std::to_integer<std::uint8_t>(fields.subspan(itemUnknown3Offset).front()),
        .hue = std::to_integer<std::uint8_t>(fields.subspan(itemHueOffset).front()),
        .stackingOffset = std::to_integer<std::uint8_t>(fields.subspan(itemStackingOffset).front()),
        .value = std::to_integer<std::uint8_t>(fields.subspan(itemValueOffset).front()),
        .height = std::to_integer<std::uint8_t>(fields.subspan(itemHeightOffset).front()),
        .name = readName(fields.subspan(itemNameOffset, tiledataNameByteCount)),
    };
}

std::expected<std::array<std::byte, tiledataExtendedLandRecordByteCount>, Error>
encodeMapTile(const MapTileRecord & record, TiledataLayout layout, const std::filesystem::path & path)
{
    const auto name = encodeName<tiledataNameByteCount>(record.name, path);
    if (!name)
    {
        return std::unexpected(name.error());
    }
    std::array<std::byte, tiledataExtendedLandRecordByteCount> bytes{};
    const auto flagsSize = layout == TiledataLayout::legacy32 ? 4U : 8U;
    writeUnsigned(std::span(bytes).first(flagsSize), static_cast<std::uint64_t>(record.flags));
    writeUnsigned(std::span(bytes).subspan(flagsSize, 2), record.textureId);
    std::ranges::copy(*name, std::span(bytes).subspan(flagsSize + 2, tiledataNameByteCount).begin());
    return bytes;
}

std::expected<std::array<std::byte, tiledataExtendedItemRecordByteCount>, Error>
encodeStaticTile(const StaticTileRecord & record, TiledataLayout layout, const std::filesystem::path & path)
{
    const auto name = encodeName<tiledataNameByteCount>(record.name, path);
    if (!name)
    {
        return std::unexpected(name.error());
    }
    std::array<std::byte, tiledataExtendedItemRecordByteCount> bytes{};
    const auto flagsSize = layout == TiledataLayout::legacy32 ? 4U : 8U;
    const auto fields = std::span(bytes).subspan(flagsSize);
    writeUnsigned(std::span(bytes).first(flagsSize), static_cast<std::uint64_t>(record.flags));
    fields.subspan(itemWeightOffset).front() = static_cast<std::byte>(record.weight);
    fields.subspan(itemQualityOffset).front() = static_cast<std::byte>(record.quality);
    writeSigned16(fields.subspan(2, 2), record.miscData);
    fields.subspan(itemUnknown2Offset).front() = static_cast<std::byte>(record.unknown2);
    fields.subspan(itemQuantityOffset).front() = static_cast<std::byte>(record.quantity);
    writeSigned16(fields.subspan(itemAnimationOffset, 2), record.animation);
    fields.subspan(itemUnknown3Offset).front() = static_cast<std::byte>(record.unknown3);
    fields.subspan(itemHueOffset).front() = static_cast<std::byte>(record.hue);
    fields.subspan(itemStackingOffset).front() = static_cast<std::byte>(record.stackingOffset);
    fields.subspan(itemValueOffset).front() = static_cast<std::byte>(record.value);
    fields.subspan(itemHeightOffset).front() = static_cast<std::byte>(record.height);
    std::ranges::copy(*name, fields.subspan(itemNameOffset, tiledataNameByteCount).begin());
    return bytes;
}

} // namespace uomul::detail
