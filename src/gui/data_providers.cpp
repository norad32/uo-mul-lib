#include "gui/data_providers.h"
#include "gui/data_profile.h"
#include "gui/map_workspace.h"
#include "uomul/radarcol/radarcol.h"
#include "uomul/texmaps/texmaps.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace uo_mule::gui
{
namespace
{

constexpr std::uint32_t textureChannelMask = 0x1FU;
constexpr std::uint32_t textureRedShift = 10U;
constexpr std::uint32_t textureGreenShift = 5U;
constexpr std::uint32_t textureMaxByteValue = 255U;
constexpr std::uint32_t textureMaxFiveBitValue = 31U;
constexpr std::uint32_t textureRoundingValue = 15U;

MapColor radarColorToMapColor(uomul::RadarColor color) noexcept
{
    constexpr std::uint32_t channelMask = 0x1FU;
    constexpr std::uint32_t channelMaximum = 255U;
    constexpr std::uint32_t redShift = 10U;
    constexpr std::uint32_t greenShift = 5U;
    const auto expand = [](std::uint32_t channel) {
        return static_cast<std::uint8_t>(((channel * channelMaximum) + (channelMask / 2U)) / channelMask);
    };
    const std::uint32_t packed = static_cast<std::uint32_t>(color);
    return {.red = expand((packed >> redShift) & channelMask),
            .green = expand((packed >> greenShift) & channelMask),
            .blue = expand(packed & channelMask),};
}

ProviderStatus unavailableStatus(std::uint64_t revision, std::string message)
{
    return {.state = ProviderAssetState::unavailable, .message = std::move(message), .revision = revision};
}

class ProfileRadarColorProvider final : public RadarColorProvider
{
public:
    void sync(const DataProfile & profile)
    {
        const auto path = resolvedDataFilePath(profile, DataFileRole::radarcol);
        if (path == path_)
        {
            return;
        }
        path_ = path;
        reader_.reset();
        ++revision_;
        if (path_.empty())
        {
            status_ = unavailableStatus(revision_, "Radar colors are not configured");
            return;
        }
        auto result = uomul::RadarColReader::open(path_);
        if (!result)
        {
            status_ = {.state = ProviderAssetState::invalid, .message = result.error().message, .revision = revision_};
            return;
        }
        reader_ = std::move(*result);
        status_ = {.state = ProviderAssetState::available, .message = {}, .revision = revision_};
    }

    void invalidate() noexcept
    {
        path_.clear();
        reader_.reset();
        ++revision_;
        status_ = unavailableStatus(revision_, "Radar colors are not loaded");
    }

    [[nodiscard]] ProviderStatus status() const override { return status_; }
    [[nodiscard]] ProviderStatus status(std::uint16_t id) const override
    {
        if (!reader_)
        {
            return status_;
        }
        if (!reader_->readMapColor(id))
        {
            return {.state = ProviderAssetState::empty, .message = "No radar color entry", .revision = revision_};
        }
        return status_;
    }
    [[nodiscard]] std::optional<MapColor> mapColor(std::uint16_t id) const override
    {
        if (!reader_)
        {
            return std::nullopt;
        }
        const auto color = reader_->readMapColor(id);
        return color ? std::optional<MapColor>(radarColorToMapColor(*color)) : std::nullopt;
    }

private:
    std::filesystem::path path_;
    std::optional<uomul::RadarColReader> reader_;
    ProviderStatus status_;
    std::uint64_t revision_ = 0;
};

class ProfileTextureProvider final : public TextureProvider
{
public:
    void sync(const DataProfile & profile)
    {
        const auto index = resolvedDataFilePath(profile, DataFileRole::texidx);
        const auto data = resolvedDataFilePath(profile, DataFileRole::texmaps);
        if (index == indexPath_ && data == dataPath_)
        {
            return;
        }
        indexPath_ = index;
        dataPath_ = data;
        reader_.reset();
        ++revision_;
        if (indexPath_.empty() || dataPath_.empty())
        {
            status_ = unavailableStatus(revision_, "Textures are not configured");
            return;
        }
        auto result = uomul::TexMapsReader::open(indexPath_, dataPath_);
        if (!result)
        {
            status_ = {.state = ProviderAssetState::invalid, .message = result.error().message, .revision = revision_};
            return;
        }
        reader_ = std::move(*result);
        status_ = {.state = ProviderAssetState::available, .message = {}, .revision = revision_};
    }

    void invalidate() noexcept
    {
        indexPath_.clear();
        dataPath_.clear();
        reader_.reset();
        ++revision_;
        status_ = unavailableStatus(revision_, "Textures are not loaded");
    }

    [[nodiscard]] ProviderStatus status() const override { return status_; }
    [[nodiscard]] ProviderStatus status(std::uint32_t id) const override
    {
        if (!reader_)
        {
            return status_;
        }
        const auto hasTexture = reader_->hasTexture(id);
        if (!hasTexture)
        {
            return {.state = ProviderAssetState::invalid, .message = hasTexture.error().message, .revision = revision_};
        }
        if (!*hasTexture)
        {
            return {.state = ProviderAssetState::empty, .message = "No texture entry", .revision = revision_};
        }
        return status_;
    }
    [[nodiscard]] std::optional<ProviderTexture> texture(std::uint32_t id) const override
    {
        if (!reader_)
        {
            return std::nullopt;
        }
        const auto result = reader_->readTexture(id);
        if (!result)
        {
            return std::nullopt;
        }
        ProviderTexture texture{.width = result->dimension, .height = result->dimension, .pixels = {}};
        texture.pixels.reserve(result->pixels.size());
        for (const std::uint16_t pixel : result->pixels)
        {
            const std::uint32_t value = pixel;
            const auto channel = [](std::uint32_t component) {
                return static_cast<std::uint8_t>(((component * textureMaxByteValue) + textureRoundingValue) /
                                                 textureMaxFiveBitValue);
            };
            texture.pixels.push_back({.red = channel((value >> textureRedShift) & textureChannelMask),
                                     .green = channel((value >> textureGreenShift) & textureChannelMask),
                                     .blue = channel(value & textureChannelMask),});
        }
        return texture;
    }

private:
    std::filesystem::path indexPath_;
    std::filesystem::path dataPath_;
    std::optional<uomul::TexMapsReader> reader_;
    ProviderStatus status_;
    std::uint64_t revision_ = 0;
};

class ProfileTileDataProvider final : public TileDataProvider
{
public:
    void sync(const DataProfile & profile)
    {
        const auto path = resolvedDataFilePath(profile, DataFileRole::tiledata);
        if (path == path_)
        {
            return;
        }
        path_ = path;
        ++revision_;
        status_ = path_.empty()
                      ? unavailableStatus(revision_, "Tile data is not configured")
                      : ProviderStatus{.state = ProviderAssetState::unsupported,
                                       .message = "Tile data reading is not supported yet",
                                        .revision = revision_,};
    }

    void invalidate() noexcept
    {
        path_.clear();
        ++revision_;
        status_ = unavailableStatus(revision_, "Tile data is not loaded");
    }

    [[nodiscard]] ProviderStatus status() const override { return status_; }
    [[nodiscard]] ProviderStatus status(std::uint16_t id) const override
    {
        static_cast<void>(id);
        return status_;
    }
    [[nodiscard]] std::optional<ProviderTileData> tile(std::uint16_t id) const override
    {
        static_cast<void>(id);
        return std::nullopt;
    }

private:
    std::filesystem::path path_;
    ProviderStatus status_;
    std::uint64_t revision_ = 0;
};

} // namespace

class DataProviders::Impl
{
public:
    ProfileRadarColorProvider radar;
    ProfileTextureProvider textures;
    ProfileTileDataProvider tileData;
};

DataProviders::DataProviders() : impl_(std::make_unique<Impl>()) {}
DataProviders::~DataProviders() = default;
DataProviders::DataProviders(DataProviders &&) noexcept = default;
DataProviders & DataProviders::operator=(DataProviders &&) noexcept = default;

void DataProviders::sync(const DataProfile & profile)
{
    impl_->radar.sync(profile);
    impl_->textures.sync(profile);
    impl_->tileData.sync(profile);
}

void DataProviders::invalidate() noexcept
{
    impl_->radar.invalidate();
    impl_->textures.invalidate();
    impl_->tileData.invalidate();
}

const RadarColorProvider & DataProviders::radarColors() const noexcept { return impl_->radar; }
const TextureProvider & DataProviders::textures() const noexcept { return impl_->textures; }
const TileDataProvider & DataProviders::tileData() const noexcept { return impl_->tileData; }

} // namespace uo_mule::gui
