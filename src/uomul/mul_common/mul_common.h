#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>

namespace uomul
{

enum class ErrorCode : std::uint8_t
{
    io,
    invalidDimensions,
    invalidCoordinate,
    truncatedInput,
    invalidBlockCount,
    commitFailed,
};

struct Error
{
    ErrorCode code = ErrorCode::io;
    std::filesystem::path path;
    std::uint64_t offset = 0;
    std::string message;
};

namespace detail
{

class ScopedDescriptor
{
public:
    explicit ScopedDescriptor(int descriptor = -1) noexcept;
    ScopedDescriptor(const ScopedDescriptor &) = delete;
    ScopedDescriptor & operator=(const ScopedDescriptor &) = delete;
    ScopedDescriptor(ScopedDescriptor && other) noexcept;
    ScopedDescriptor & operator=(ScopedDescriptor && other) noexcept;
    ~ScopedDescriptor();

    [[nodiscard]] int get() const noexcept;
    [[nodiscard]] int release() noexcept;
    int close() noexcept;

private:
    int descriptor_;
};

class TemporaryPath
{
public:
    explicit TemporaryPath(std::filesystem::path path);
    TemporaryPath(const TemporaryPath &) = delete;
    TemporaryPath & operator=(const TemporaryPath &) = delete;
    TemporaryPath(TemporaryPath &&) = delete;
    TemporaryPath & operator=(TemporaryPath &&) = delete;
    ~TemporaryPath();

    void release() noexcept;

private:
    std::filesystem::path path_;
    bool active_ = true;
};

class MulCommon
{
public:
    [[nodiscard]] static Error
    makeError(ErrorCode code, const std::filesystem::path & path, std::uint64_t offset, std::string message);
    [[nodiscard]] static bool readExact(int descriptor, void * data, std::size_t size, std::uint64_t offset);
    [[nodiscard]] static bool writeExact(int descriptor, const void * data, std::size_t size, std::uint64_t offset);
    [[nodiscard]] static std::filesystem::path temporaryPath(const std::filesystem::path & path);
    [[nodiscard]] static std::expected<int, Error> createTemporary(const std::filesystem::path & path,
                                                                   std::filesystem::path & temporary);
    [[nodiscard]] static std::expected<void, Error>
    publishFile(const std::filesystem::path & temporary, const std::filesystem::path & destination, bool overwrite);
    [[nodiscard]] static std::expected<void, Error> checkOutputPath(const std::filesystem::path & path, bool overwrite);
};

} // namespace detail
} // namespace uomul
