#pragma once

#include "uomul/map.h"

#include <expected>
#include <filesystem>
#include <limits>
#include <string>

namespace uomul::detail
{

class MapHelper
{
public:
    static constexpr std::uint64_t landBlockByteSize = landBlockByteCount;
    static constexpr std::uint64_t landTileByteSize = 3;
    static constexpr std::uint64_t landHeaderByteSize = 4;
    static constexpr std::uint32_t blockLocalWidth = landBlockDimension;
    static constexpr std::uint32_t bitsPerByte = std::numeric_limits<unsigned char>::digits;
    static constexpr std::uint32_t byteMask = std::numeric_limits<std::uint8_t>::max();

    [[nodiscard]] static Error
    makeError(ErrorCode code, const std::filesystem::path & path, std::uint64_t offset, std::string message);
    [[nodiscard]] static std::expected<std::uint64_t, Error> blockCount(Dimensions dimensions,
                                                                        const std::filesystem::path & path);
    [[nodiscard]] static std::expected<void, Error>
    validateBlock(BlockCoord block, Dimensions dimensions, const std::filesystem::path & path);
    [[nodiscard]] static std::expected<void, Error>
    validateTile(TileCoord tile, Dimensions dimensions, const std::filesystem::path & path);
    [[nodiscard]] static std::uint64_t blockId(BlockCoord block, Dimensions dimensions);
    [[nodiscard]] static std::uint64_t blockOffset(BlockCoord block, Dimensions dimensions);
    [[nodiscard]] static bool readExact(int descriptor, void * data, std::size_t size, std::uint64_t offset);
    [[nodiscard]] static bool writeExact(int descriptor, const void * data, std::size_t size, std::uint64_t offset);
    [[nodiscard]] static LandBlock decodeBlock(const LandBlockBytes & raw);
    static void encodeBlock(LandBlockBytes & raw, const LandBlock & block);
    [[nodiscard]] static std::filesystem::path temporaryPath(const std::filesystem::path & path);
    [[nodiscard]] static std::expected<int, Error> createTemporary(const std::filesystem::path & path,
                                                                   std::filesystem::path & temporary);
    [[nodiscard]] static std::expected<void, Error>
    publishFile(const std::filesystem::path & temporary, const std::filesystem::path & destination, bool overwrite);
    [[nodiscard]] static std::expected<void, Error>
    validateRegion(BlockRegion region, Dimensions dimensions, const std::filesystem::path & path);
    [[nodiscard]] static std::expected<void, Error>
    validateRegion(TileRegion region, Dimensions dimensions, const std::filesystem::path & path);
    [[nodiscard]] static std::expected<void, Error> checkOutputPath(const std::filesystem::path & path, bool overwrite);
};

} // namespace uomul::detail
