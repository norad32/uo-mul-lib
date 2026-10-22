#include "radarcol_common.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/mul_common/output_transaction.h"
#include "uomul/radarcol/radarcol.h"
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
#include <unistd.h>
#include <utility>

namespace uomul
{

namespace
{

std::expected<void, Error>
stageSource(int sourceDescriptor, int stagedDescriptor, std::uint64_t byteCount, const std::filesystem::path & staging)
{
    constexpr std::size_t bufferSize = 65536;
    std::array<std::byte, bufferSize> buffer{};
    for (std::uint64_t offset = 0; offset < byteCount;)
    {
        const auto amount = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), byteCount - offset));

        if (!detail::MulCommon::readExact(sourceDescriptor, buffer.data(), amount, offset) ||
            !detail::MulCommon::writeExact(stagedDescriptor, buffer.data(), amount, offset))
        {
            return std::unexpected(
                detail::MulCommon::makeError(ErrorCode::io, staging, offset, "failed to stage radar color table"));
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

struct RadarColEditor::Impl
{
    detail::OutputTransaction transaction;
    detail::OutputTransaction::OutputId outputId = 0;
    std::uint32_t colors = 0;
    bool committed = false;
    bool failed = false;

    [[nodiscard]] int descriptor() const { return transaction.descriptor(outputId); }

    [[nodiscard]] const std::filesystem::path & output() const { return transaction.destination(outputId); }

    [[nodiscard]] const std::filesystem::path & staging() const { return transaction.stagedPath(outputId); }
};

RadarColEditor::RadarColEditor(std::unique_ptr<Impl> impl) : impl_(std::move(impl))
{
}

RadarColEditor::RadarColEditor(RadarColEditor &&) noexcept = default;
RadarColEditor & RadarColEditor::operator=(RadarColEditor && other) noexcept
{
    if (this != &other)
    {
        abort();
        impl_ = std::move(other.impl_);
    }

    return *this;
}

RadarColEditor::~RadarColEditor()
{
    abort();
}

std::expected<RadarColEditor, Error> RadarColEditor::create(const std::filesystem::path & sourceFile,
                                                            const std::filesystem::path & outputFile,
                                                            RadarColEditOptions options)
{
    const detail::ScopedDescriptor sourceDescriptor(::open(sourceFile.c_str(), O_RDONLY | O_CLOEXEC));

    if (sourceDescriptor.get() < 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, sourceFile, 0, std::strerror(errno)));
    }

    std::uint64_t sourceSize = 0;
    auto colorCount = detail::RadarColCommon::inspectFile(sourceDescriptor.get(), sourceFile, sourceSize);

    if (!colorCount)
    {
        return std::unexpected(colorCount.error());
    }

    if (auto paths = detail::RadarColCommon::validateEditorPaths(sourceFile, outputFile, options.overwriteOutput); !paths)
    {
        return std::unexpected(paths.error());
    }

    auto exactPath = detail::MulCommon::pathsExactlyMatch(sourceFile, outputFile);
    if (!exactPath)
    {
        return std::unexpected(exactPath.error());
    }

    auto impl = std::make_unique<Impl>();
    auto output = impl->transaction.addOutput(outputFile, options.overwriteOutput || *exactPath);
    if (!output)
    {
        return std::unexpected(output.error());
    }
    impl->outputId = *output;

    if (auto copied = stageSource(sourceDescriptor.get(), impl->descriptor(), sourceSize, impl->staging()); !copied)
    {
        return std::unexpected(copied.error());
    }

    impl->colors = *colorCount;

    return RadarColEditor(std::move(impl));
}

std::expected<RadarColEditor, Error> RadarColEditor::createEmpty(const std::filesystem::path & outputFile,
                                                                 RadarColEditOptions options)
{
    if (auto valid = detail::RadarColCommon::validateOutputPath(outputFile, options.overwriteOutput); !valid)
    {
        return std::unexpected(valid.error());
    }

    auto impl = std::make_unique<Impl>();
    auto output = impl->transaction.addOutput(outputFile, options.overwriteOutput);
    if (!output)
    {
        return std::unexpected(output.error());
    }
    impl->outputId = *output;

    if (::ftruncate(impl->descriptor(), static_cast<off_t>(detail::RadarColCommon::tableByteSize)) != 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, impl->staging(), 0, std::strerror(errno)));
    }

    if (auto synced = impl->transaction.sync(impl->outputId, ErrorCode::io); !synced)
    {
        return std::unexpected(synced.error());
    }

    impl->colors = radarColorCount;

    return RadarColEditor(std::move(impl));
}

std::expected<void, Error> RadarColEditor::editMapColor(std::uint16_t mapId, RadarColor value)
{
    if (mapId >= radarMapColorCount)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::invalidCoordinate,
                                                            impl_ ? impl_->output() : std::filesystem::path{},
                                                            mapId,
                                                            "map color ID is outside the radar table"));
    }

    return editRawColor(mapId, value);
}

std::expected<void, Error> RadarColEditor::editStaticColor(std::uint16_t id, RadarColor value)
{
    return editRawColor(radarStaticColorOffset + id, value);
}

std::expected<void, Error> RadarColEditor::editRawColor(std::uint32_t index, RadarColor value)
{
    if (!impl_)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::commitFailed, {}, 0, "editor is no longer active"));
    }

    if (auto active = ensureActive(impl_->committed, impl_->failed, impl_->descriptor(), impl_->output()); !active)
    {
        return active;
    }

    const auto offset = detail::RadarColCommon::colorOffset(index);
    const auto bytes = detail::encodeLittleEndian(value);

    if (!detail::MulCommon::writeExact(impl_->descriptor(), bytes.data(), bytes.size(), offset))
    {
        impl_->failed = true;
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::io, impl_->staging(), offset, "could not stage radar color"));
    }

    impl_->colors = std::max(impl_->colors, index + 1);
    return {};
}

std::expected<void, Error> RadarColEditor::commit()
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

void RadarColEditor::abort() noexcept
{
    if (!impl_)
    {
        return;
    }

    impl_->transaction.abort();
    impl_->committed = true;
}

} // namespace uomul
