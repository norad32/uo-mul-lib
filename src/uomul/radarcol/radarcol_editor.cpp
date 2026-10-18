#include "radarcol_common.h"
#include "uomul/mul_common/mul_common.h"
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
    std::filesystem::path output;
    std::filesystem::path staging;
    int descriptor = -1;
    std::uint32_t colors = 0;
    bool overwrite = false;
    bool committed = false;
    bool failed = false;
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

    auto impl = std::make_unique<Impl>();
    impl->output = outputFile;
    impl->overwrite = options.overwriteOutput;
    auto temporaryDescriptor = detail::MulCommon::createTemporary(outputFile, impl->staging);

    if (!temporaryDescriptor)
    {
        return std::unexpected(temporaryDescriptor.error());
    }

    detail::TemporaryPath cleanup(impl->staging);
    detail::ScopedDescriptor stagingDescriptor(*temporaryDescriptor);

    if (auto copied = stageSource(sourceDescriptor.get(), stagingDescriptor.get(), sourceSize, impl->staging); !copied)
    {
        return std::unexpected(copied.error());
    }

    impl->descriptor = stagingDescriptor.release();
    impl->colors = *colorCount;
    cleanup.release();

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
    impl->output = outputFile;
    impl->overwrite = options.overwriteOutput;
    auto temporaryDescriptor = detail::MulCommon::createTemporary(outputFile, impl->staging);

    if (!temporaryDescriptor)
    {
        return std::unexpected(temporaryDescriptor.error());
    }

    detail::TemporaryPath cleanup(impl->staging);
    detail::ScopedDescriptor stagingDescriptor(*temporaryDescriptor);

    if (::ftruncate(stagingDescriptor.get(), static_cast<off_t>(detail::RadarColCommon::tableByteSize)) != 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, impl->staging, 0, std::strerror(errno)));
    }

    if (::fsync(stagingDescriptor.get()) != 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, impl->staging, 0, std::strerror(errno)));
    }

    impl->descriptor = stagingDescriptor.release();
    impl->colors = radarColorCount;
    cleanup.release();

    return RadarColEditor(std::move(impl));
}

std::expected<void, Error> RadarColEditor::editMapColor(std::uint16_t mapId, RadarColor value)
{
    if (mapId >= radarMapColorCount)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::invalidCoordinate,
                                                            impl_ ? impl_->output : std::filesystem::path{},
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

    if (auto active = ensureActive(impl_->committed, impl_->failed, impl_->descriptor, impl_->output); !active)
    {
        return active;
    }

    const auto offset = detail::RadarColCommon::colorOffset(index);
    constexpr std::uint32_t byteMask = 0xFF;
    constexpr std::uint32_t byteShift = 8;
    const auto packed = static_cast<std::uint32_t>(value);
    const std::array<std::byte, 2> bytes{
        static_cast<std::byte>(packed & byteMask),
        static_cast<std::byte>((packed >> byteShift) & byteMask),
    };

    if (!detail::MulCommon::writeExact(impl_->descriptor, bytes.data(), bytes.size(), offset))
    {
        impl_->failed = true;
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::io, impl_->staging, offset, "could not stage radar color"));
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

    if (auto active = ensureActive(impl_->committed, impl_->failed, impl_->descriptor, impl_->output); !active)
    {
        return active;
    }

    if (::fsync(impl_->descriptor) != 0)
    {
        const auto error = detail::MulCommon::makeError(ErrorCode::commitFailed, impl_->staging, 0, std::strerror(errno));
        static_cast<void>(::close(impl_->descriptor));
        impl_->descriptor = -1;
        static_cast<void>(::unlink(impl_->staging.c_str()));
        impl_->committed = true;
        return std::unexpected(error);
    }

    if (::close(impl_->descriptor) != 0)
    {
        const auto error = detail::MulCommon::makeError(ErrorCode::commitFailed, impl_->staging, 0, std::strerror(errno));
        impl_->descriptor = -1;
        static_cast<void>(::unlink(impl_->staging.c_str()));
        impl_->committed = true;
        return std::unexpected(error);
    }

    impl_->descriptor = -1;

    auto published = detail::MulCommon::publishFile(impl_->staging, impl_->output, impl_->overwrite);

    if (!published)
    {
        static_cast<void>(::unlink(impl_->staging.c_str()));
        impl_->committed = true;
        return published;
    }

    impl_->committed = true;
    return {};
}

void RadarColEditor::abort() noexcept
{
    if (!impl_)
    {
        return;
    }

    if (impl_->descriptor >= 0)
    {
        static_cast<void>(::close(impl_->descriptor));
        impl_->descriptor = -1;
    }

    if (!impl_->committed && !impl_->staging.empty())
    {
        static_cast<void>(::unlink(impl_->staging.c_str()));
    }

    impl_->committed = true;
}

} // namespace uomul
