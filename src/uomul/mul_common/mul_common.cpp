#include "mul_common.h"
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <iterator>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace uomul::detail
{

namespace
{

constexpr int maximumAttempts = 16;
std::atomic<std::uint64_t> temporarySequence{0};

template <typename Byte, typename Transfer>
bool transferExact(int descriptor, Byte * bytes, std::size_t size, std::uint64_t offset, Transfer transfer)
{
    std::size_t transferred = 0;
    while (transferred < size)
    {
        const auto result = transfer(descriptor,
                                     std::next(bytes, static_cast<std::ptrdiff_t>(transferred)),
                                     size - transferred,
                                     static_cast<off_t>(offset + transferred));
        if (result < 0 && errno == EINTR)
        {
            continue;
        }

        if (result <= 0)
        {
            return false;
        }

        transferred += static_cast<std::size_t>(result);
    }
    return true;
}

} // namespace

ScopedDescriptor::ScopedDescriptor(int descriptor) noexcept : descriptor_(descriptor)
{
}

ScopedDescriptor::ScopedDescriptor(ScopedDescriptor && other) noexcept : descriptor_(other.release())
{
}

ScopedDescriptor & ScopedDescriptor::operator=(ScopedDescriptor && other) noexcept
{
    if (this != &other)
    {
        static_cast<void>(close());
        descriptor_ = other.release();
    }

    return *this;
}

ScopedDescriptor::~ScopedDescriptor()
{
    static_cast<void>(close());
}

int ScopedDescriptor::get() const noexcept
{
    return descriptor_;
}

int ScopedDescriptor::release() noexcept
{
    const int descriptor = descriptor_;
    descriptor_ = -1;
    return descriptor;
}

int ScopedDescriptor::close() noexcept
{
    if (descriptor_ < 0)
    {
        return 0;
    }

    const int descriptor = release();
    return ::close(descriptor);
}

TemporaryPath::TemporaryPath(std::filesystem::path path) : path_(std::move(path))
{
}

TemporaryPath::~TemporaryPath()
{
    if (active_ && !path_.empty())
    {
        static_cast<void>(::unlink(path_.c_str()));
    }
}

void TemporaryPath::release() noexcept
{
    active_ = false;
}

Error MulCommon::makeError(ErrorCode code, const std::filesystem::path & path, std::uint64_t offset, std::string message)
{
    return Error{.code = code, .path = path, .offset = offset, .message = std::move(message)};
}

bool MulCommon::isRegularFileDescriptor(int descriptor) noexcept
{
    struct stat status{};
    return ::fstat(descriptor, &status) == 0 && S_ISREG(status.st_mode);
}

bool MulCommon::descriptorContainsRange(int descriptor, std::uint64_t offset, std::uint64_t byteCount) noexcept
{
    struct stat status{};

    if (::fstat(descriptor, &status) != 0 || status.st_size < 0)
    {
        return false;
    }

    const auto fileSize = static_cast<std::uint64_t>(status.st_size);
    return offset <= fileSize && byteCount <= fileSize - offset;
}

std::expected<std::uint64_t, Error>
MulCommon::regularFileSize(int descriptor, const std::filesystem::path & path, const char * formatName)
{
    struct stat status{};
    if (::fstat(descriptor, &status) != 0)
    {
        return std::unexpected(makeError(ErrorCode::io, path, 0, std::strerror(errno)));
    }

    if (!S_ISREG(status.st_mode))
    {
        return std::unexpected(makeError(ErrorCode::io, path, 0, std::string(formatName) + " input must be a regular file"));
    }

    if (status.st_size < 0)
    {
        return std::unexpected(
            makeError(ErrorCode::io, path, 0, "could not determine " + std::string(formatName) + " file size"));
    }

    return static_cast<std::uint64_t>(status.st_size);
}

bool MulCommon::readExact(int descriptor, void * data, std::size_t size, std::uint64_t offset)
{
    return transferExact(descriptor, static_cast<std::byte *>(data), size, offset, ::pread);
}

bool MulCommon::writeExact(int descriptor, const void * data, std::size_t size, std::uint64_t offset)
{
    return transferExact(descriptor, static_cast<const std::byte *>(data), size, offset, ::pwrite);
}

std::filesystem::path MulCommon::temporaryPath(const std::filesystem::path & path)
{
    const auto sequence = temporarySequence.fetch_add(1, std::memory_order_relaxed);
    return path.parent_path() /
           (path.filename().string() + ".uomul-tmp-" + std::to_string(::getpid()) + "-" + std::to_string(sequence));
}

std::expected<bool, Error>
MulCommon::pathsExactlyMatch(const std::filesystem::path & first, const std::filesystem::path & second)
{
    std::error_code error;
    const auto firstAbsolute = std::filesystem::absolute(first, error).lexically_normal();
    if (error)
    {
        return std::unexpected(makeError(ErrorCode::io, first, 0, error.message()));
    }

    const auto secondAbsolute = std::filesystem::absolute(second, error).lexically_normal();
    if (error)
    {
        return std::unexpected(makeError(ErrorCode::io, second, 0, error.message()));
    }

    return firstAbsolute == secondAbsolute;
}

std::expected<bool, Error> MulCommon::pathsAlias(const std::filesystem::path & first,
                                                 const std::filesystem::path & second)
{
    auto exact = pathsExactlyMatch(first, second);
    if (!exact)
    {
        return std::unexpected(exact.error());
    }
    if (*exact)
    {
        return true;
    }

    std::error_code error;
    const bool firstExists = std::filesystem::exists(first, error);
    if (error)
    {
        return std::unexpected(makeError(ErrorCode::io, first, 0, error.message()));
    }

    error.clear();
    const bool secondExists = std::filesystem::exists(second, error);
    if (error)
    {
        return std::unexpected(makeError(ErrorCode::io, second, 0, error.message()));
    }

    if (!firstExists || !secondExists)
    {
        return false;
    }

    const bool equivalent = std::filesystem::equivalent(first, second, error);
    if (error)
    {
        return std::unexpected(makeError(ErrorCode::io, second, 0, error.message()));
    }

    return equivalent;
}

std::expected<int, Error> MulCommon::createTemporary(const std::filesystem::path & path, std::filesystem::path & temporary)
{
    for (int attempt = 0; attempt < maximumAttempts; attempt++)
    {
        temporary = temporaryPath(path);
        const int descriptor = ::open(temporary.c_str(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);

        if (descriptor >= 0)
        {
            return descriptor;
        }

        if (errno != EEXIST)
        {
            return std::unexpected(makeError(ErrorCode::io, temporary, 0, std::strerror(errno)));
        }
    }

    return std::unexpected(makeError(ErrorCode::io, path, 0, "unable to allocate unique temporary file"));
}

std::expected<void, Error>
MulCommon::publishFile(const std::filesystem::path & temporary, const std::filesystem::path & destination, bool overwrite)
{
    std::error_code filesystemError;
    const bool destinationExists = std::filesystem::exists(destination, filesystemError);
    if (filesystemError)
    {
        return std::unexpected(makeError(ErrorCode::commitFailed, destination, 0, filesystemError.message()));
    }

    if (destinationExists && !overwrite)
    {
        return std::unexpected(makeError(ErrorCode::commitFailed, destination, 0, "output already exists"));
    }

    if (!destinationExists)
    {
        if (::link(temporary.c_str(), destination.c_str()) == 0)
        {
            static_cast<void>(::unlink(temporary.c_str()));
            return {};
        }

        return std::unexpected(makeError(ErrorCode::commitFailed, destination, 0, std::strerror(errno)));
    }

    if (::rename(temporary.c_str(), destination.c_str()) != 0)
    {
        return std::unexpected(makeError(ErrorCode::commitFailed, destination, 0, std::strerror(errno)));
    }

    return {};
}

std::expected<void, Error> MulCommon::checkOutputPath(const std::filesystem::path & path, bool overwrite)
{
    std::error_code filesystemError;
    const bool exists = std::filesystem::exists(path, filesystemError);

    if (filesystemError)
    {
        return std::unexpected(makeError(ErrorCode::io, path, 0, filesystemError.message()));
    }

    if (!overwrite && exists)
    {
        return std::unexpected(makeError(ErrorCode::io, path, 0, "output already exists"));
    }

    return {};
}

} // namespace uomul::detail
