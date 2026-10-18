#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <string>
#include <type_traits>

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
    invalidFormat,
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

template <typename T, std::size_t Size = sizeof(T)>
    requires(std::is_unsigned_v<T> && Size <= sizeof(T))
[[nodiscard]] constexpr std::array<std::byte, Size> encodeLittleEndian(T value)
{
    std::array<std::byte, Size> bytes{};
    const auto wideValue = static_cast<std::uintmax_t>(value);
    for (std::size_t index = 0; index < Size; ++index)
    {
        const auto shift = index * std::numeric_limits<unsigned char>::digits;
        const auto byte = (wideValue >> shift) % (std::numeric_limits<unsigned char>::max() + 1U);
        bytes.at(index) = static_cast<std::byte>(byte);
    }
    return bytes;
}

template <typename T, std::size_t Size = sizeof(T), std::size_t ContainerSize>
    requires(std::is_unsigned_v<T> && Size <= ContainerSize)
[[nodiscard]] constexpr T decodeLittleEndian(const std::array<std::byte, ContainerSize> & bytes)
{
    static_assert(Size <= sizeof(T));
    T value = 0;
    for (std::size_t index = 0; index < Size; ++index)
    {
        const auto byte = static_cast<std::uintmax_t>(std::to_integer<unsigned char>(bytes.at(index)));
        const auto shifted = byte << (index * std::numeric_limits<unsigned char>::digits);
        value = static_cast<T>(static_cast<std::uintmax_t>(value) | shifted);
    }
    return value;
}

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
    [[nodiscard]] static std::expected<std::uint64_t, Error>
    regularFileSize(int descriptor, const std::filesystem::path & path, const char * formatName);
    [[nodiscard]] static bool isRegularFileDescriptor(int descriptor) noexcept;
    [[nodiscard]] static bool descriptorContainsRange(int descriptor, std::uint64_t offset, std::uint64_t byteCount) noexcept;
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
