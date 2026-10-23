#pragma once

#include "uomul/mul_common/mul_common.h"
#include "uomul/tiledata/tiledata.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>

namespace uomul::detail
{

inline constexpr std::uint32_t tiledataBlockHeaderByteCount = 4;
inline constexpr std::uint32_t tiledataLegacyLandRecordByteCount = 26;
inline constexpr std::uint32_t tiledataExtendedLandRecordByteCount = 30;
inline constexpr std::uint32_t tiledataLegacyItemRecordByteCount = 37;
inline constexpr std::uint32_t tiledataExtendedItemRecordByteCount = 41;

[[nodiscard]] constexpr std::uint32_t tiledataMapRecordByteCount(TiledataLayout layout)
{
    return layout == TiledataLayout::legacy32 ? tiledataLegacyLandRecordByteCount : tiledataExtendedLandRecordByteCount;
}

[[nodiscard]] constexpr std::uint32_t tiledataStaticRecordByteCount(TiledataLayout layout)
{
    return layout == TiledataLayout::legacy32 ? tiledataLegacyItemRecordByteCount : tiledataExtendedItemRecordByteCount;
}

[[nodiscard]] constexpr std::uint64_t tiledataMapBlockByteCount(TiledataLayout layout)
{
    return tiledataBlockHeaderByteCount +
           (static_cast<std::uint64_t>(tiledataTilesPerBlock) * tiledataMapRecordByteCount(layout));
}

[[nodiscard]] constexpr std::uint64_t tiledataStaticBlockByteCount(TiledataLayout layout)
{
    return tiledataBlockHeaderByteCount +
           (static_cast<std::uint64_t>(tiledataTilesPerBlock) * tiledataStaticRecordByteCount(layout));
}

[[nodiscard]] constexpr std::uint64_t tiledataMapSectionByteCount(TiledataLayout layout)
{
    return static_cast<std::uint64_t>(tiledataMapBlockCount) * tiledataMapBlockByteCount(layout);
}

[[nodiscard]] constexpr std::uint64_t tiledataFileByteCount(TiledataLayout layout)
{
    return tiledataMapSectionByteCount(layout) +
           (static_cast<std::uint64_t>(tiledataStaticBlockCount) * tiledataStaticBlockByteCount(layout));
}

[[nodiscard]] std::expected<void, Error>
validateTiledataFile(int descriptor, const std::filesystem::path & path, TiledataLayout layout, std::uint64_t & fileSize);

[[nodiscard]] std::uint64_t tiledataMapRecordOffset(std::uint16_t id, TiledataLayout layout);
[[nodiscard]] std::uint64_t tiledataStaticRecordOffset(std::uint32_t id, TiledataLayout layout);

[[nodiscard]] std::expected<MapTileRecord, Error> decodeMapTile(std::span<const std::byte> bytes,
                                                                TiledataLayout layout,
                                                                const std::filesystem::path & path,
                                                                std::uint64_t offset);
[[nodiscard]] std::expected<StaticTileRecord, Error> decodeStaticTile(std::span<const std::byte> bytes,
                                                                      TiledataLayout layout,
                                                                      const std::filesystem::path & path,
                                                                      std::uint64_t offset);
[[nodiscard]] std::expected<std::array<std::byte, tiledataExtendedLandRecordByteCount>, Error>
encodeMapTile(const MapTileRecord & record, TiledataLayout layout, const std::filesystem::path & path);
[[nodiscard]] std::expected<std::array<std::byte, tiledataExtendedItemRecordByteCount>, Error>
encodeStaticTile(const StaticTileRecord & record, TiledataLayout layout, const std::filesystem::path & path);

} // namespace uomul::detail
