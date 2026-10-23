#include "tiledata_common.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/mul_common/output_transaction.h"
#include "uomul/tiledata/tiledata.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace uomul
{

namespace
{

std::expected<void, Error> validateOutputPath(const std::filesystem::path & path, bool overwrite)
{
    if (path.empty() || path.filename().empty())
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, path, 0, "output path must name a file"));
    }
    if (auto checked = detail::MulCommon::checkOutputPath(path, overwrite); !checked)
    {
        return checked;
    }

    std::error_code error;
    const auto parent = path.parent_path();
    if (!parent.empty() && !std::filesystem::is_directory(parent, error))
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::io, parent, 0, error ? error.message() : "output directory does not exist"));
    }
    return {};
}

std::expected<void, Error>
validateSourceOutput(const std::filesystem::path & source, const std::filesystem::path & output, bool overwrite)
{
    auto exact = detail::MulCommon::pathsExactlyMatch(source, output);
    if (!exact)
    {
        return std::unexpected(exact.error());
    }
    auto alias = detail::MulCommon::pathsAlias(source, output);
    if (!alias)
    {
        return std::unexpected(alias.error());
    }
    if (*alias && !*exact)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, output, 0, "output path resolves to the source"));
    }
    return validateOutputPath(output, overwrite || *exact);
}

std::expected<void, Error>
copySource(int sourceDescriptor, int destinationDescriptor, std::uint64_t size, const std::filesystem::path & staging)
{
    constexpr std::size_t bufferSize = 65536;
    std::array<std::byte, bufferSize> buffer{};
    for (std::uint64_t offset = 0; offset < size;)
    {
        const auto amount = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), size - offset));
        if (!detail::MulCommon::readExact(sourceDescriptor, buffer.data(), amount, offset) ||
            !detail::MulCommon::writeExact(destinationDescriptor, buffer.data(), amount, offset))
        {
            return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, staging, offset, "failed to stage tiledata"));
        }
        offset += amount;
    }
    return {};
}

std::expected<void, Error> ensureActive(bool committed, bool failed, int descriptor, const std::filesystem::path & output)
{
    if (committed || failed || descriptor < 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::commitFailed, output, 0, "editor is no longer active"));
    }
    return {};
}

} // namespace

struct TiledataEditor::Impl
{
    detail::OutputTransaction transaction;
    detail::OutputTransaction::OutputId outputId = 0;
    TiledataLayout layout = TiledataLayout::legacy32;
    bool committed = false;
    bool failed = false;

    [[nodiscard]] int descriptor() const { return transaction.descriptor(outputId); }
    [[nodiscard]] const std::filesystem::path & output() const { return transaction.destination(outputId); }
    [[nodiscard]] const std::filesystem::path & staging() const { return transaction.stagedPath(outputId); }
};

TiledataEditor::TiledataEditor(std::unique_ptr<Impl> impl) : impl_(std::move(impl))
{
}

TiledataEditor::TiledataEditor(TiledataEditor &&) noexcept = default;
TiledataEditor & TiledataEditor::operator=(TiledataEditor && other) noexcept
{
    if (this != &other)
    {
        abort();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

TiledataEditor::~TiledataEditor()
{
    abort();
}

std::expected<TiledataEditor, Error> TiledataEditor::create(const std::filesystem::path & source,
                                                            const std::filesystem::path & output,
                                                            TiledataLayout layout,
                                                            TiledataEditOptions options)
{
    const detail::ScopedDescriptor sourceDescriptor(::open(source.c_str(), O_RDONLY | O_CLOEXEC));
    if (sourceDescriptor.get() < 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, source, 0, std::strerror(errno)));
    }

    std::uint64_t sourceSize = 0;
    if (auto valid = detail::validateTiledataFile(sourceDescriptor.get(), source, layout, sourceSize); !valid)
    {
        return std::unexpected(valid.error());
    }
    if (auto valid = validateSourceOutput(source, output, options.overwriteOutput); !valid)
    {
        return std::unexpected(valid.error());
    }

    auto exact = detail::MulCommon::pathsExactlyMatch(source, output);
    if (!exact)
    {
        return std::unexpected(exact.error());
    }

    auto impl = std::make_unique<Impl>();
    auto outputId = impl->transaction.addOutput(output, options.overwriteOutput || *exact);
    if (!outputId)
    {
        return std::unexpected(outputId.error());
    }
    impl->outputId = *outputId;
    impl->layout = layout;
    if (auto copied = copySource(sourceDescriptor.get(), impl->descriptor(), sourceSize, impl->staging()); !copied)
    {
        return std::unexpected(copied.error());
    }
    return TiledataEditor(std::move(impl));
}

std::expected<TiledataEditor, Error>
TiledataEditor::createEmpty(const std::filesystem::path & output, TiledataLayout layout, TiledataEditOptions options)
{
    if (auto valid = validateOutputPath(output, options.overwriteOutput); !valid)
    {
        return std::unexpected(valid.error());
    }

    auto impl = std::make_unique<Impl>();
    auto outputId = impl->transaction.addOutput(output, options.overwriteOutput);
    if (!outputId)
    {
        return std::unexpected(outputId.error());
    }
    impl->outputId = *outputId;
    impl->layout = layout;
    if (::ftruncate(impl->descriptor(), static_cast<off_t>(detail::tiledataFileByteCount(layout))) != 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, impl->staging(), 0, std::strerror(errno)));
    }
    if (auto synced = impl->transaction.sync(impl->outputId, ErrorCode::io); !synced)
    {
        return std::unexpected(synced.error());
    }
    return TiledataEditor(std::move(impl));
}

std::expected<void, Error> TiledataEditor::editMapTile(std::uint16_t id, const MapTileRecord & record)
{
    if (!impl_)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::commitFailed, {}, 0, "editor is no longer active"));
    }
    if (id >= tiledataMapTileCount)
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::invalidCoordinate, impl_->output(), id, "land tile ID is out of range"));
    }
    if (auto active = ensureActive(impl_->committed, impl_->failed, impl_->descriptor(), impl_->output()); !active)
    {
        return active;
    }
    const auto encoded = detail::encodeMapTile(record, impl_->layout, impl_->output());
    if (!encoded)
    {
        return std::unexpected(encoded.error());
    }
    const auto offset = detail::tiledataMapRecordOffset(id, impl_->layout);
    const auto size = detail::tiledataMapRecordByteCount(impl_->layout);
    if (!detail::MulCommon::writeExact(impl_->descriptor(), encoded->data(), size, offset))
    {
        impl_->failed = true;
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::io, impl_->staging(), offset, "could not stage land tile"));
    }
    return {};
}

std::expected<void, Error> TiledataEditor::editStaticTile(std::uint32_t id, const StaticTileRecord & record)
{
    if (!impl_)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::commitFailed, {}, 0, "editor is no longer active"));
    }
    if (id >= tiledataStaticTileCount)
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::invalidCoordinate, impl_->output(), id, "item tile ID is out of range"));
    }
    if (auto active = ensureActive(impl_->committed, impl_->failed, impl_->descriptor(), impl_->output()); !active)
    {
        return active;
    }
    const auto encoded = detail::encodeStaticTile(record, impl_->layout, impl_->output());
    if (!encoded)
    {
        return std::unexpected(encoded.error());
    }
    const auto offset = detail::tiledataStaticRecordOffset(id, impl_->layout);
    const auto size = detail::tiledataStaticRecordByteCount(impl_->layout);
    if (!detail::MulCommon::writeExact(impl_->descriptor(), encoded->data(), size, offset))
    {
        impl_->failed = true;
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::io, impl_->staging(), offset, "could not stage item tile"));
    }
    return {};
}

std::expected<void, Error> TiledataEditor::commit()
{
    if (!impl_)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::commitFailed, {}, 0, "editor is no longer active"));
    }
    if (auto active = ensureActive(impl_->committed, impl_->failed, impl_->descriptor(), impl_->output()); !active)
    {
        return active;
    }
    if (auto synced = impl_->transaction.sync(impl_->outputId, ErrorCode::commitFailed); !synced)
    {
        const Error & error = synced.error();
        impl_->transaction.abort();
        impl_->committed = true;
        return std::unexpected(error);
    }
    if (auto closed = impl_->transaction.close(impl_->outputId, ErrorCode::commitFailed); !closed)
    {
        const Error & error = closed.error();
        impl_->transaction.abort();
        impl_->committed = true;
        return std::unexpected(error);
    }
    auto published = impl_->transaction.publish();
    impl_->committed = true;
    return published;
}

void TiledataEditor::abort() noexcept
{
    if (!impl_)
    {
        return;
    }
    impl_->transaction.abort();
    impl_->committed = true;
}

} // namespace uomul
