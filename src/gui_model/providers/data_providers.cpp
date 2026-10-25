#include "gui_model/providers/data_providers.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/map_document.h"
#include "gui_model/documents/radarcol_document.h"
#include "gui_model/documents/texmaps_document.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/providers/color_conversion.h"
#include "gui_model/providers/embedded_radarcol.h"
#include "gui_model/source/source_identity.h"
#include "gui_model/source/tiledata_source.h"
#include "uomul/radarcol/radarcol.h"
#include "uomul/texmaps/texmaps.h"
#include "uomul/tiledata/tiledata.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace uo_mule::gui
{

std::string_view providerSourceName(ProviderSourceKind source) noexcept
{
    switch (source)
    {
        case ProviderSourceKind::none :
            return "Radarcol unavailable";
        case ProviderSourceKind::embeddedDefault :
            return "Embedded Radarcol";
        case ProviderSourceKind::externalConfigured :
            return "Configured Radarcol";
        case ProviderSourceKind::embeddedDefaultWithWarning :
            return "Embedded Radarcol (fallback)";
    }
    return "Radarcol unavailable";
}

namespace
{

ProviderStatus unavailableStatus(std::uint64_t revision, std::string message)
{
    return {
        .state = ProviderAssetState::unavailable,
        .message = std::move(message),
        .sourceIdentity = emptyProviderSourceIdentity(),
        .revision = revision,
    };
}

[[nodiscard]] ProviderTexture makeProviderTexture(const uomul::TextureImage & image)
{
    ProviderTexture texture{.width = image.dimension, .height = image.dimension, .pixels = {}};
    texture.pixels.reserve(image.pixels.size());
    for (const std::uint16_t pixel : image.pixels)
    {
        texture.pixels.push_back(texturePixelToMapColor(pixel));
    }
    return texture;
}

class ProfileRadarColorProvider final : public RadarColorProvider
{
public:
    ProfileRadarColorProvider() : embedded_(makeEmbeddedRadarColorProvider()) {}

    void sync(const DataProfile & profile) { sync(profile, false); }

    void refresh(const DataProfile & profile) { sync(profile, true); }

    void sync(const DataProfile & profile, bool force)
    {
        const auto path = resolvedDataFilePath(profile, DataFileRole::radarcol);
        if (synchronized_ && !force && path == path_)
        {
            return;
        }
        synchronized_ = true;
        path_ = path;
        reader_.reset();
        revision_++;
        if (path_.empty())
        {
            status_ = embeddedStatus(ProviderSourceKind::embeddedDefault, "Embedded Radarcol");
            return;
        }
        auto result = uomul::RadarColReader::open(path_);
        if (!result)
        {
            status_ = embeddedStatus(ProviderSourceKind::embeddedDefaultWithWarning,
                                     "Configured Radarcol unavailable: " + result.error().message);
            return;
        }
        reader_ = std::move(*result);
        status_ = {
            .state = ProviderAssetState::available,
            .source = ProviderSourceKind::externalConfigured,
            .message = {},
            .sourceIdentity = makeDocumentSourceIdentity({path_}),
            .revision = revision_,
        };
    }

    void invalidate() noexcept
    {
        path_.clear();
        reader_.reset();
        synchronized_ = true;
        revision_++;
        status_ = embeddedStatus(ProviderSourceKind::embeddedDefault, "Embedded Radarcol");
    }

    void syncDocument(const RadarcolDocument * document)
    {
        if (document == document_ && document != nullptr && documentRevision_ == document->displayRevision())
        {
            return;
        }
        if (document == nullptr || !document->reader())
        {
            if (document_ == nullptr)
            {
                return;
            }
            document_ = nullptr;
            documentRevision_ = 0;
            revision_++;
            status_.revision = revision_;
            return;
        }

        document_ = document;
        documentRevision_ = document->displayRevision();
        revision_++;
        status_ = {
            .state = ProviderAssetState::available,
            .source = ProviderSourceKind::externalConfigured,
            .message = {},
            .sourceIdentity = makeDocumentSourceIdentity({document->sourcePath()}),
            .revision = revision_,
        };
    }

    [[nodiscard]] ProviderStatus status() const override { return status_; }
    [[nodiscard]] ProviderStatus status(std::uint16_t id) const override
    {
        if (document_ != nullptr)
        {
            return document_->rawColor(id).has_value() ? status_
                                                       : ProviderStatus{
                                                             .state = ProviderAssetState::empty,
                                                             .source = status_.source,
                                                             .message = "No radar color entry",
                                                             .sourceIdentity = status_.sourceIdentity,
                                                             .revision = revision_,
                                                         };
        }
        if (!reader_)
        {
            ProviderStatus result = embedded_->status(id);
            result.source = status_.source;
            result.message = status_.message;
            result.sourceIdentity = status_.sourceIdentity;
            result.revision = revision_;
            return result;
        }
        if (!reader_->readMapColor(id))
        {
            return {
                .state = ProviderAssetState::empty,
                .source = status_.source,
                .message = "No radar color entry",
                .sourceIdentity = status_.sourceIdentity,
                .revision = revision_,
            };
        }
        return status_;
    }
    [[nodiscard]] std::optional<MapColor> mapColor(std::uint16_t id) const override
    {
        if (document_ != nullptr)
        {
            const auto color = document_->rawColor(id);
            return color ? std::optional<MapColor>(radarColorToMapColor(*color)) : std::nullopt;
        }
        if (!reader_)
        {
            return embedded_->mapColor(id);
        }
        const auto color = reader_->readMapColor(id);
        return color ? std::optional<MapColor>(radarColorToMapColor(*color)) : std::nullopt;
    }
    [[nodiscard]] std::uint32_t colorCount() const override
    {
        if (document_ != nullptr && document_->reader())
        {
            return document_->reader()->colorCount();
        }
        if (reader_)
        {
            return reader_->colorCount();
        }
        return uomul::radarColorCount;
    }
    [[nodiscard]] std::optional<uomul::RadarColor> rawColor(std::uint32_t index) const override
    {
        if (document_ != nullptr)
        {
            return document_->rawColor(index);
        }
        if (!reader_)
        {
            return embeddedRadarcolColor(index);
        }
        const auto result = reader_->readRawColor(index);
        return result ? std::optional<uomul::RadarColor>(*result) : std::nullopt;
    }

private:
    [[nodiscard]] ProviderStatus embeddedStatus(ProviderSourceKind source, std::string message) const
    {
        return {
            .state = ProviderAssetState::available,
            .source = source,
            .message = std::move(message),
            .sourceIdentity = emptyProviderSourceIdentity(),
            .revision = revision_,
        };
    }

    std::filesystem::path path_;
    std::optional<uomul::RadarColReader> reader_;
    const RadarcolDocument * document_ = nullptr;
    std::uint64_t documentRevision_ = 0;
    std::unique_ptr<RadarColorProvider> embedded_;
    ProviderStatus status_;
    std::uint64_t revision_ = 0;
    bool synchronized_ = false;
};

class ProfileTextureProvider final : public TextureProvider
{
public:
    void sync(const DataProfile & profile) { sync(profile, false); }

    void refresh(const DataProfile & profile) { sync(profile, true); }

    void sync(const DataProfile & profile, bool force)
    {
        const auto index = resolvedDataFilePath(profile, DataFileRole::texidx);
        const auto data = resolvedDataFilePath(profile, DataFileRole::texmaps);
        if (synchronized_ && !force && index == indexPath_ && data == dataPath_)
        {
            return;
        }
        synchronized_ = true;
        indexPath_ = index;
        dataPath_ = data;
        reader_.reset();
        revision_++;
        if (indexPath_.empty() || dataPath_.empty())
        {
            status_ = unavailableStatus(revision_, "Textures are not configured");
            status_.sourceIdentity = makeRoleAwareDocumentSourceIdentity(
                {{.role = SourceRole::index, .path = indexPath_}, {.role = SourceRole::data, .path = dataPath_}});
            return;
        }
        auto result = uomul::TexMapsReader::open(indexPath_, dataPath_);
        if (!result)
        {
            status_ = {
                .state = ProviderAssetState::invalid,
                .source = ProviderSourceKind::externalConfigured,
                .message = result.error().message,
                .sourceIdentity = makeRoleAwareDocumentSourceIdentity(
                    {{.role = SourceRole::index, .path = indexPath_}, {.role = SourceRole::data, .path = dataPath_}}),
                .revision = revision_,
            };
            return;
        }
        reader_ = std::move(*result);
        status_ = {
            .state = ProviderAssetState::available,
            .source = ProviderSourceKind::externalConfigured,
            .message = {},
            .sourceIdentity = makeRoleAwareDocumentSourceIdentity(
                {{.role = SourceRole::index, .path = indexPath_}, {.role = SourceRole::data, .path = dataPath_}}),
            .revision = revision_,
        };
    }

    void invalidate() noexcept
    {
        indexPath_.clear();
        dataPath_.clear();
        reader_.reset();
        revision_++;
        status_ = unavailableStatus(revision_, "Textures are not loaded");
        status_.sourceIdentity = makeRoleAwareDocumentSourceIdentity(
            {{.role = SourceRole::index, .path = indexPath_}, {.role = SourceRole::data, .path = dataPath_}});
    }

    void syncDocument(const TexmapsDocument * document)
    {
        if (document == document_ && document != nullptr && documentRevision_ == document->displayRevision())
        {
            return;
        }
        if (document == nullptr || !document->reader())
        {
            if (document_ == nullptr)
            {
                return;
            }
            document_ = nullptr;
            documentRevision_ = 0;
            revision_++;
            status_.revision = revision_;
            return;
        }

        document_ = document;
        documentRevision_ = document->displayRevision();
        revision_++;
        status_ = {
            .state = ProviderAssetState::available,
            .source = ProviderSourceKind::externalConfigured,
            .message = {},
            .sourceIdentity = document->sourceIdentity(),
            .revision = revision_,
        };
    }

    [[nodiscard]] ProviderStatus status() const override { return status_; }
    [[nodiscard]] ProviderStatus status(std::uint32_t id) const override
    {
        if (document_ != nullptr)
        {
            const auto hasTexture = document_->hasTexture(id);
            if (!hasTexture)
            {
                return {
                    .state = ProviderAssetState::invalid,
                    .source = status_.source,
                    .message = hasTexture.error().message,
                    .sourceIdentity = status_.sourceIdentity,
                    .revision = revision_,
                };
            }
            return *hasTexture ? status_
                               : ProviderStatus{
                                     .state = ProviderAssetState::empty,
                                     .source = status_.source,
                                     .message = "No texture entry",
                                     .sourceIdentity = status_.sourceIdentity,
                                     .revision = revision_,
                                 };
        }
        if (!reader_)
        {
            return status_;
        }
        const auto hasTexture = reader_->hasTexture(id);
        if (!hasTexture)
        {
            return {
                .state = ProviderAssetState::invalid,
                .source = status_.source,
                .message = hasTexture.error().message,
                .sourceIdentity = status_.sourceIdentity,
                .revision = revision_,
            };
        }
        if (!*hasTexture)
        {
            return {
                .state = ProviderAssetState::empty,
                .source = status_.source,
                .message = "No texture entry",
                .sourceIdentity = status_.sourceIdentity,
                .revision = revision_,
            };
        }
        return status_;
    }
    [[nodiscard]] std::optional<ProviderTexture> texture(std::uint32_t id) const override
    {
        if (document_ != nullptr)
        {
            const auto result = document_->effectiveTexture(id);
            if (!result || !result->has_value())
            {
                return std::nullopt;
            }
            return makeProviderTexture(result->value());
        }
        if (!reader_)
        {
            return std::nullopt;
        }
        const auto result = reader_->readTexture(id);
        if (!result)
        {
            return std::nullopt;
        }
        return makeProviderTexture(*result);
    }
    [[nodiscard]] std::uint32_t textureCount() const override
    {
        if (document_ != nullptr && document_->reader())
        {
            return document_->reader()->textureCount();
        }
        return reader_ ? reader_->textureCount() : 0U;
    }
    [[nodiscard]] std::optional<bool> hasTexture(std::uint32_t id) const override
    {
        if (document_ != nullptr)
        {
            const auto result = document_->hasTexture(id);
            return result ? std::optional<bool>(*result) : std::nullopt;
        }
        if (!reader_)
        {
            return std::nullopt;
        }
        const auto result = reader_->hasTexture(id);
        return result ? std::optional<bool>(*result) : std::nullopt;
    }

private:
    std::filesystem::path indexPath_;
    std::filesystem::path dataPath_;
    std::optional<uomul::TexMapsReader> reader_;
    const TexmapsDocument * document_ = nullptr;
    std::uint64_t documentRevision_ = 0;
    ProviderStatus status_;
    std::uint64_t revision_ = 0;
    bool synchronized_ = false;
};

class ProfileTiledataProvider final : public TiledataProvider
{
public:
    void sync(const DataProfile & profile) { sync(profile, false); }

    void refresh(const DataProfile & profile) { sync(profile, true); }

    void sync(const DataProfile & profile, bool force)
    {
        const auto path = resolvedDataFilePath(profile, DataFileRole::tiledata);
        if (synchronized_ && !force && path == path_ && profile.tiledataLayout == layoutSelection_)
        {
            return;
        }
        path_ = path;
        layoutSelection_ = profile.tiledataLayout;
        reader_.reset();
        revision_++;
        synchronized_ = true;
        if (path_.empty())
        {
            status_ = unavailableStatus(revision_, "Tiledata is not configured");
            return;
        }

        const auto resolved = resolveTiledataLayout(path_, layoutSelection_);
        if (!resolved)
        {
            status_ = {
                .state = layoutSelection_ == TiledataLayoutSelection::autoDetect ? ProviderAssetState::unsupported
                                                                                 : ProviderAssetState::invalid,
                .source = ProviderSourceKind::externalConfigured,
                .message = "Tiledata file does not match the selected layout",
                .sourceIdentity = makeDocumentSourceIdentity({path_}),
                .revision = revision_,
            };
            return;
        }

        auto opened = uomul::TiledataReader::open(path_, *resolved);
        if (!opened)
        {
            status_ = {
                .state = ProviderAssetState::invalid,
                .source = ProviderSourceKind::externalConfigured,
                .message = opened.error().message,
                .sourceIdentity = makeDocumentSourceIdentity({path_}),
                .revision = revision_,
            };
            return;
        }
        layout_ = resolved;
        reader_ = std::move(*opened);
        status_ = {
            .state = ProviderAssetState::available,
            .source = ProviderSourceKind::externalConfigured,
            .message = {},
            .sourceIdentity = makeDocumentSourceIdentity({path_}),
            .revision = revision_,
        };
    }

    void invalidate() noexcept
    {
        path_.clear();
        reader_.reset();
        layout_.reset();
        revision_++;
        status_ = unavailableStatus(revision_, "Tile data is not loaded");
        status_.sourceIdentity = makeDocumentSourceIdentity({path_});
    }

    [[nodiscard]] ProviderStatus status() const override { return status_; }
    [[nodiscard]] ProviderStatus status(std::uint16_t id) const override
    {
        if (status_.state != ProviderAssetState::available)
        {
            return status_;
        }
        const auto record = mapTile(id);
        if (!record)
        {
            return {
                .state = ProviderAssetState::empty,
                .source = status_.source,
                .message = "No MapTileRecord for ID",
                .sourceIdentity = status_.sourceIdentity,
                .revision = revision_,
            };
        }
        return status_;
    }
    [[nodiscard]] ProviderStatus staticStatus(std::uint32_t id) const override
    {
        if (status_.state != ProviderAssetState::available)
        {
            return status_;
        }
        const auto record = staticTile(id);
        if (!record)
        {
            return {
                .state = ProviderAssetState::empty,
                .source = status_.source,
                .message = "No StaticTileRecord for ID",
                .sourceIdentity = status_.sourceIdentity,
                .revision = revision_,
            };
        }
        return status_;
    }
    [[nodiscard]] std::optional<uomul::TiledataLayout> layout() const override { return layout_; }
    [[nodiscard]] std::optional<ProviderMapTile> mapTile(std::uint16_t id) const override
    {
        if (!reader_)
        {
            return std::nullopt;
        }
        const auto record = reader_->readMapTile(id);
        return record ? std::optional<ProviderMapTile>(ProviderMapTile{.id = id, .record = *record}) : std::nullopt;
    }
    [[nodiscard]] std::optional<ProviderStaticTile> staticTile(std::uint32_t id) const override
    {
        if (!reader_)
        {
            return std::nullopt;
        }
        const auto record = reader_->readStaticTile(id);
        return record ? std::optional<ProviderStaticTile>(ProviderStaticTile{.id = id, .record = *record}) : std::nullopt;
    }

private:
    std::filesystem::path path_;
    TiledataLayoutSelection layoutSelection_ = TiledataLayoutSelection::autoDetect;
    std::optional<uomul::TiledataLayout> layout_;
    std::optional<uomul::TiledataReader> reader_;
    ProviderStatus status_;
    std::uint64_t revision_ = 0;
    bool synchronized_ = false;
};

} // namespace

class DataProviders::Impl
{
public:
    ProfileRadarColorProvider radar;
    ProfileTextureProvider textures;
    ProfileTiledataProvider tiledata;

    void syncDocuments(const std::vector<DocumentRecord> & documents)
    {
        const RadarcolDocument * radarDocument = nullptr;
        const TexmapsDocument * texmapsDocument = nullptr;
        for (const auto & document : documents)
        {
            if (document.summary.sourceKind != DocumentSourceKind::configuredMetadata)
            {
                continue;
            }
            if (document.radarcol)
            {
                radarDocument = &*document.radarcol;
            }
            if (document.texmaps)
            {
                texmapsDocument = &*document.texmaps;
            }
        }
        radar.syncDocument(radarDocument);
        textures.syncDocument(texmapsDocument);
    }
};

DataProviders::DataProviders() : impl_(std::make_unique<Impl>())
{
}
DataProviders::~DataProviders() = default;
DataProviders::DataProviders(DataProviders &&) noexcept = default;
DataProviders & DataProviders::operator=(DataProviders &&) noexcept = default;

void DataProviders::sync(const DataProfile & profile)
{
    impl_->radar.sync(profile);
    impl_->textures.sync(profile);
    impl_->tiledata.sync(profile);
}

void DataProviders::syncDocuments(const std::vector<DocumentRecord> & documents)
{
    impl_->syncDocuments(documents);
}

bool DataProviders::refresh(const DataProfile & profile)
{
    const auto previous = revisions();
    impl_->radar.refresh(profile);
    impl_->textures.refresh(profile);
    impl_->tiledata.refresh(profile);
    return revisions() != previous;
}

void DataProviders::invalidate() noexcept
{
    impl_->radar.invalidate();
    impl_->textures.invalidate();
    impl_->tiledata.invalidate();
}

ProviderRevisions DataProviders::revisions() const noexcept
{
    return {
        .radarColors = impl_->radar.status().revision,
        .textures = impl_->textures.status().revision,
        .tiledata = impl_->tiledata.status().revision,
    };
}

const RadarColorProvider & DataProviders::radarColors() const noexcept
{
    return impl_->radar;
}
const TextureProvider & DataProviders::textures() const noexcept
{
    return impl_->textures;
}
const TiledataProvider & DataProviders::tiledata() const noexcept
{
    return impl_->tiledata;
}

} // namespace uo_mule::gui
