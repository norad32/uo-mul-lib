#pragma once

#include "gui/profile/data_profile.h"
#include "gui/documents/map_workspace.h"
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uo_mule::gui
{

enum class ProviderAssetState : std::uint8_t
{
    unavailable,
    empty,
    available,
    invalid,
    unsupported,
};

enum class ProviderSourceKind : std::uint8_t
{
    none,
    embeddedDefault,
    externalConfigured,
    embeddedDefaultWithWarning,
};

[[nodiscard]] std::string_view providerSourceName(ProviderSourceKind source) noexcept;

struct ProviderStatus
{
    ProviderAssetState state = ProviderAssetState::unavailable;
    ProviderSourceKind source = ProviderSourceKind::none;
    std::string message;
    std::uint64_t revision = 0;
};

struct ProviderTexture
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<MapColor> pixels;
};

struct ProviderTileData
{
    std::uint16_t id = 0;
    std::string description;
};

class RadarColorProvider
{
public:
    virtual ~RadarColorProvider() = default;
    [[nodiscard]] virtual ProviderStatus status() const = 0;
    [[nodiscard]] virtual ProviderStatus status(std::uint16_t id) const = 0;
    [[nodiscard]] virtual std::optional<MapColor> mapColor(std::uint16_t id) const = 0;
};

class TextureProvider
{
public:
    virtual ~TextureProvider() = default;
    [[nodiscard]] virtual ProviderStatus status() const = 0;
    [[nodiscard]] virtual ProviderStatus status(std::uint32_t id) const = 0;
    [[nodiscard]] virtual std::optional<ProviderTexture> texture(std::uint32_t id) const = 0;
};

class TileDataProvider
{
public:
    virtual ~TileDataProvider() = default;
    [[nodiscard]] virtual ProviderStatus status() const = 0;
    [[nodiscard]] virtual ProviderStatus status(std::uint16_t id) const = 0;
    [[nodiscard]] virtual std::optional<ProviderTileData> tile(std::uint16_t id) const = 0;
};

class DataProviders
{
public:
    DataProviders();
    ~DataProviders();
    DataProviders(DataProviders &&) noexcept;
    DataProviders & operator=(DataProviders &&) noexcept;
    DataProviders(const DataProviders &) = delete;
    DataProviders & operator=(const DataProviders &) = delete;

    void sync(const DataProfile & profile);
    void invalidate() noexcept;
    [[nodiscard]] const RadarColorProvider & radarColors() const noexcept;
    [[nodiscard]] const TextureProvider & textures() const noexcept;
    [[nodiscard]] const TileDataProvider & tileData() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace uo_mule::gui
