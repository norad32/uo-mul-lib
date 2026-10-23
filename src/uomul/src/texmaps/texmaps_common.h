#pragma once

#include "uomul/mul_common/mul_common.h"
#include "uomul/texmaps/texmaps.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <vector>

namespace uomul::detail
{

using TexMapsIndexEntryBytes = std::array<std::byte, textureIndexEntryByteCount>;

struct TexMapsIndexEntry
{
    std::uint32_t lookup = 0;
    std::uint32_t length = 0;
    std::uint32_t extra = 0;
};

[[nodiscard]] TexMapsIndexEntry decodeTexMapsEntry(const TexMapsIndexEntryBytes & bytes);
[[nodiscard]] TexMapsIndexEntryBytes encodeTexMapsEntry(TexMapsIndexEntry entry);
[[nodiscard]] std::uint32_t texMapsEntryDimension(TexMapsIndexEntry entry);
[[nodiscard]] std::uint32_t expectedTexMapsLength(TexMapsIndexEntry entry);
[[nodiscard]] std::expected<std::vector<TexMapsIndexEntry>, Error>
loadTexMapsIndex(int descriptor, const std::filesystem::path & path, std::uint64_t dataSize);
[[nodiscard]] std::expected<TextureImage, Error>
readTexMapsTexture(int descriptor, const std::filesystem::path & path, TexMapsIndexEntry entry);
[[nodiscard]] std::vector<std::byte> encodeTexMapsTexture(const TextureImage & image);

} // namespace uomul::detail
