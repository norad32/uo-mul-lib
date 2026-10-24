#pragma once

#include "uomul/mul_common/mul_common.h"
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>

namespace uomul
{

inline constexpr std::uint32_t tiledataMapTileCount = 0x4000;
inline constexpr std::uint32_t tiledataStaticTileCount = 0x10000;
inline constexpr std::uint32_t tiledataTilesPerBlock = 32;
inline constexpr std::uint32_t tiledataMapBlockCount = tiledataMapTileCount / tiledataTilesPerBlock;
inline constexpr std::uint32_t tiledataStaticBlockCount = tiledataStaticTileCount / tiledataTilesPerBlock;
inline constexpr std::size_t tiledataNameByteCount = 20;

enum class TiledataLayout : std::uint8_t
{
    legacy32,
    extended64,
};

enum class TiledataFlag : std::uint64_t
{
    none = 0,
    background = 1ULL << 0U,
    weapon = 1ULL << 1U,
    transparent = 1ULL << 2U,
    translucent = 1ULL << 3U,
    wall = 1ULL << 4U,
    damaging = 1ULL << 5U,
    impassable = 1ULL << 6U,
    wet = 1ULL << 7U,
    unknown1 = 1ULL << 8U,
    surface = 1ULL << 9U,
    bridge = 1ULL << 10U,
    generic = 1ULL << 11U,
    window = 1ULL << 12U,
    noShoot = 1ULL << 13U,
    articleA = 1ULL << 14U,
    articleAn = 1ULL << 15U,
    articleThe = 1ULL << 16U,
    foliage = 1ULL << 17U,
    partialHue = 1ULL << 18U,
    noHouse = 1ULL << 19U,
    map = 1ULL << 20U,
    container = 1ULL << 21U,
    wearable = 1ULL << 22U,
    lightSource = 1ULL << 23U,
    animation = 1ULL << 24U,
    hoverOver = 1ULL << 25U,
    noDiagonal = 1ULL << 26U,
    armor = 1ULL << 27U,
    roof = 1ULL << 28U,
    door = 1ULL << 29U,
    stairBack = 1ULL << 30U,
    stairRight = 1ULL << 31U,
    alphaBlend = 1ULL << 32U,
    useNewArt = 1ULL << 33U,
    artUsed = 1ULL << 34U,
    unused8 = 1ULL << 35U,
    noShadow = 1ULL << 36U,
    pixelBleed = 1ULL << 37U,
    playAnimOnce = 1ULL << 38U,
    multiMovable = 1ULL << 40U,
    unused10 = 1ULL << 41U,
    unused11 = 1ULL << 42U,
    unused12 = 1ULL << 43U,
    unused13 = 1ULL << 44U,
    unused14 = 1ULL << 45U,
    unused15 = 1ULL << 46U,
    unused16 = 1ULL << 47U,
    unused17 = 1ULL << 48U,
    unused18 = 1ULL << 49U,
    unused19 = 1ULL << 50U,
    unused20 = 1ULL << 51U,
    unused21 = 1ULL << 52U,
    unused22 = 1ULL << 53U,
    unused23 = 1ULL << 54U,
    unused24 = 1ULL << 55U,
    unused25 = 1ULL << 56U,
    unused26 = 1ULL << 57U,
    unused27 = 1ULL << 58U,
    unused28 = 1ULL << 59U,
    unused29 = 1ULL << 60U,
    unused30 = 1ULL << 61U,
    unused31 = 1ULL << 62U,
    unused32 = 1ULL << 63U,
};

[[nodiscard]] constexpr TiledataFlag operator|(TiledataFlag left, TiledataFlag right) noexcept
{
    return static_cast<TiledataFlag>(static_cast<std::uint64_t>(left) | static_cast<std::uint64_t>(right));
}

[[nodiscard]] constexpr TiledataFlag operator&(TiledataFlag left, TiledataFlag right) noexcept
{
    return static_cast<TiledataFlag>(static_cast<std::uint64_t>(left) & static_cast<std::uint64_t>(right));
}

constexpr TiledataFlag & operator|=(TiledataFlag & left, TiledataFlag right) noexcept
{
    left = left | right;
    return left;
}

[[nodiscard]] constexpr bool hasTiledataFlag(TiledataFlag flags, TiledataFlag flag) noexcept
{
    return (flags & flag) == flag;
}

struct MapTileRecord
{
    TiledataFlag flags = TiledataFlag::none;
    std::uint16_t textureId = 0;
    std::string name;

    bool operator==(const MapTileRecord & other) const = default;
};

struct StaticTileRecord
{
    TiledataFlag flags = TiledataFlag::none;
    std::uint8_t weight = 0;
    std::uint8_t quality = 0;
    std::int16_t miscData = 0;
    std::uint8_t unknown2 = 0;
    std::uint8_t quantity = 0;
    std::int16_t animation = 0;
    std::uint8_t unknown3 = 0;
    std::uint8_t hue = 0;
    std::uint8_t stackingOffset = 0;
    std::uint8_t value = 0;
    std::uint8_t height = 0;
    std::string name;

    bool operator==(const StaticTileRecord & other) const = default;
};

struct TiledataEditOptions
{
    bool overwriteOutput = false;
};

class TiledataReader
{
public:
    TiledataReader(TiledataReader && other) noexcept;
    TiledataReader & operator=(TiledataReader && other) noexcept;
    TiledataReader(const TiledataReader &) = delete;
    TiledataReader & operator=(const TiledataReader &) = delete;
    ~TiledataReader();

    /** @brief Opens and validates a tiledata.mul file for an explicit binary layout.
     *  @param path Tiledata file to open.
     *  @param layout Legacy 32-bit-flags or extended 64-bit-flags record layout.
     *  @retval value Reader for 16,384 map and 65,536 static records.
     *  @retval error Inaccessible, size-mismatched, or malformed file details.
     */
    [[nodiscard]] static std::expected<TiledataReader, Error> open(const std::filesystem::path & path, TiledataLayout layout);

    /** @brief Returns the layout selected when the file was opened. */
    [[nodiscard]] TiledataLayout layout() const noexcept;

    /** @brief Returns the fixed number of map records in the file. */
    [[nodiscard]] std::uint32_t mapTileCount() const noexcept;

    /** @brief Returns the fixed number of static records in the file. */
    [[nodiscard]] std::uint32_t staticTileCount() const noexcept;

    /** @brief Reads one land record by ID.
     *  @param id Map tile ID in the range [0, 16,384).
     *  @retval value Decoded map metadata.
     *  @retval error ID out of range or file read failure details.
     */
    [[nodiscard]] std::expected<MapTileRecord, Error> readMapTile(std::uint16_t id) const;

    /** @brief Reads one item record by ID.
     *  @param id Static tile ID in the range [0, 65,536).
     *  @retval value Decoded static metadata.
     *  @retval error ID out of range or file read failure details.
     */
    [[nodiscard]] std::expected<StaticTileRecord, Error> readStaticTile(std::uint32_t id) const;

private:
    struct Impl;
    explicit TiledataReader(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

class TiledataEditor
{
public:
    TiledataEditor(TiledataEditor && other) noexcept;
    TiledataEditor & operator=(TiledataEditor && other) noexcept;
    TiledataEditor(const TiledataEditor &) = delete;
    TiledataEditor & operator=(const TiledataEditor &) = delete;
    ~TiledataEditor();

    /** @brief Creates an editor by staging a source file to an output path.
     *  The selected layout must exactly match the source file.
     *  @param source Source tiledata.mul file.
     *  @param output Destination published by commit.
     *  @param layout Binary layout of the source and output.
     *  @param options Controls replacement of an existing output.
     *  @retval value Active staged editor.
     *  @retval error Invalid source, paths, layout, or staging I/O details.
     */
    [[nodiscard]] static std::expected<TiledataEditor, Error> create(const std::filesystem::path & source,
                                                                     const std::filesystem::path & output,
                                                                     TiledataLayout layout,
                                                                     TiledataEditOptions options = {});

    /** @brief Creates an empty, fully sized tiledata.mul file for a layout.
     *  All headers, records, and names are initialized to zero.
     *  @param output Destination published by commit.
     *  @param layout Binary layout to create.
     *  @param options Controls replacement of an existing output.
     *  @retval value Active staged editor.
     *  @retval error Invalid output path or staging I/O details.
     */
    [[nodiscard]] static std::expected<TiledataEditor, Error>
    createEmpty(const std::filesystem::path & output, TiledataLayout layout, TiledataEditOptions options = {});

    /** @brief Stages one land record replacement. */
    [[nodiscard]] std::expected<void, Error> editMapTile(std::uint16_t id, const MapTileRecord & record);

    /** @brief Stages one item record replacement. */
    [[nodiscard]] std::expected<void, Error> editStaticTile(std::uint32_t id, const StaticTileRecord & record);

    /** @brief Publishes all staged changes to the output path. */
    [[nodiscard]] std::expected<void, Error> commit();

    /** @brief Discards staged changes without publishing them. */
    void abort() noexcept;

private:
    struct Impl;
    explicit TiledataEditor(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace uomul
