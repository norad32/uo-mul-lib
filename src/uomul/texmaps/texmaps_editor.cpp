#include "texmaps_common.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/texmaps/texmaps.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace uomul
{

namespace
{

using TextureIndexEntry = detail::TexMapsIndexEntry;
using detail::encodeTexMapsEntry;
using detail::encodeTexMapsTexture;
using detail::loadTexMapsIndex;
using detail::readTexMapsTexture;
constexpr int temporaryPathAttempts = 16;
constexpr std::uint64_t fnvOffsetBasis = 1469598103934665603ULL;
constexpr std::uint64_t fnvPrime = 1099511628211ULL;
constexpr std::uint32_t byteMask = std::numeric_limits<std::uint8_t>::max();
constexpr unsigned byteShift = std::numeric_limits<std::uint8_t>::digits;

std::expected<void, Error> validateOutput(const std::filesystem::path & path, bool overwrite)
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

    error.clear();

    if (std::filesystem::exists(path, error) && !std::filesystem::is_regular_file(path, error))
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::io, path, 0, error ? error.message() : "output must be a regular file"));
    }

    if (error)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, path, 0, error.message()));
    }

    return {};
}

std::expected<bool, Error> pathsAlias(const std::filesystem::path & first, const std::filesystem::path & second)
{
    std::error_code error;
    const auto firstAbsolute = std::filesystem::absolute(first, error).lexically_normal();

    if (error)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, first, 0, error.message()));
    }

    const auto secondAbsolute = std::filesystem::absolute(second, error).lexically_normal();

    if (error)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, second, 0, error.message()));
    }

    if (firstAbsolute == secondAbsolute)
    {
        return true;
    }

    error.clear();
    const bool firstExists = std::filesystem::exists(first, error);

    if (error)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, first, 0, error.message()));
    }

    error.clear();
    const bool secondExists = std::filesystem::exists(second, error);

    if (error)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, second, 0, error.message()));
    }

    if (!firstExists || !secondExists)
    {
        return false;
    }

    const bool equivalent = std::filesystem::equivalent(first, second, error);
    if (error)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, second, 0, error.message()));
    }

    return equivalent;
}

std::expected<void, Error> validatePairPaths(const std::filesystem::path & outputIndex,
                                             const std::filesystem::path & outputData,
                                             bool overwrite,
                                             const std::filesystem::path * sourceIndex = nullptr,
                                             const std::filesystem::path * sourceData = nullptr)
{
    if (auto valid = validateOutput(outputIndex, overwrite); !valid)
    {
        return valid;
    }

    if (auto valid = validateOutput(outputData, overwrite); !valid)
    {
        return valid;
    }

    auto alias = pathsAlias(outputIndex, outputData);
    if (!alias)
    {
        return std::unexpected(alias.error());
    }

    if (*alias)
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::io, outputData, 0, "index and data outputs must be different files"));
    }

    if (sourceIndex != nullptr && sourceData != nullptr)
    {
        for (const auto * source : {sourceIndex, sourceData})
        {
            for (const auto * output : {&outputIndex, &outputData})
            {
                alias = pathsAlias(*source, *output);
                if (!alias)
                {
                    return std::unexpected(alias.error());
                }

                if (*alias)
                {
                    return std::unexpected(detail::MulCommon::makeError(
                        ErrorCode::io, *output, 0, "texture outputs must not alias either source file"));
                }
            }
        }
    }

    return {};
}

std::expected<void, Error> copyFile(int source, int destination, std::uint64_t size, const std::filesystem::path & path)
{
    constexpr std::size_t bufferSize = 65536;
    std::array<std::byte, bufferSize> buffer{};

    for (std::uint64_t offset = 0; offset < size;)
    {
        const auto amount = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), size - offset));
        if (!detail::MulCommon::readExact(source, buffer.data(), amount, offset) ||
            !detail::MulCommon::writeExact(destination, buffer.data(), amount, offset))
        {
            return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, path, offset, "could not copy texture file"));
        }

        offset += amount;
    }

    return {};
}

std::expected<void, Error> syncDescriptor(int descriptor, const std::filesystem::path & path, ErrorCode code)
{
    if (::fsync(descriptor) != 0)
    {
        return std::unexpected(detail::MulCommon::makeError(code, path, 0, std::strerror(errno)));
    }

    return {};
}

std::expected<std::filesystem::path, Error> linkBackup(const std::filesystem::path & source)
{
    for (int attempt = 0; attempt < temporaryPathAttempts; attempt++)
    {
        auto backup = detail::MulCommon::temporaryPath(source);
        if (::link(source.c_str(), backup.c_str()) == 0)
        {
            return backup;
        }

        if (errno != EEXIST)
        {
            return std::unexpected(detail::MulCommon::makeError(ErrorCode::commitFailed, source, 0, std::strerror(errno)));
        }
    }

    return std::unexpected(
        detail::MulCommon::makeError(ErrorCode::commitFailed, source, 0, "unable to allocate pair backup path"));
}

[[nodiscard]] std::uint64_t hashTexture(const TextureImage & texture)
{
    std::uint64_t hash = fnvOffsetBasis;
    const auto mix = [&hash](std::uint16_t value)
    {
        const auto word = static_cast<std::uint32_t>(value);
        hash ^= word & byteMask;
        hash *= fnvPrime;
        hash ^= word >> byteShift;
        hash *= fnvPrime;
    };

    mix(texture.dimension);

    for (const auto pixel : texture.pixels)
    {
        mix(pixel);
    }

    return hash;
}

std::expected<void, Error>
writeAt(int descriptor, const std::byte * data, std::size_t size, std::uint64_t offset, const std::filesystem::path & path)
{
    if (!detail::MulCommon::writeExact(descriptor, data, size, offset))
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, path, offset, "could not write texture data"));
    }

    return {};
}

std::expected<void, Error>
restoreBackup(const std::filesystem::path & backup, const std::filesystem::path & destination, bool destinationWasPublished)
{
    if (backup.empty())
    {
        if (destinationWasPublished && ::unlink(destination.c_str()) != 0)
        {
            return std::unexpected(detail::MulCommon::makeError(ErrorCode::commitFailed, destination, 0, std::strerror(errno)));
        }

        return {};
    }

    if (::rename(backup.c_str(), destination.c_str()) != 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::commitFailed, destination, 0, std::strerror(errno)));
    }

    return {};
}

struct DeduplicatedState
{
    TextureDeduplicationReport report;
    std::vector<TextureIndexEntry> entries;
};

std::expected<DeduplicatedState, Error> buildDeduplicatedPayload(int sourceDescriptor,
                                                                 int outputDescriptor,
                                                                 const std::filesystem::path & sourcePath,
                                                                 const std::filesystem::path & outputPath,
                                                                 const std::vector<TextureIndexEntry> & sourceEntries,
                                                                 std::uint64_t inputSize)
{
    DeduplicatedState state{
        .report = {.entriesScanned = static_cast<std::uint32_t>(sourceEntries.size()), .inputDataBytes = inputSize},
        .entries = sourceEntries,
    };

    struct Candidate
    {
        TextureImage image;
        TextureIndexEntry entry;
    };

    std::unordered_map<std::uint64_t, std::vector<std::size_t>> byHash;
    std::vector<Candidate> unique;
    std::uint64_t outputOffset = 0;

    for (std::size_t id = 0; id < sourceEntries.size(); id++)
    {
        const auto entry = sourceEntries.at(id);
        if (entry.length == 0)
        {
            state.entries.at(id) = {};
            continue;
        }

        auto image = readTexMapsTexture(sourceDescriptor, sourcePath, entry);
        if (!image)
        {
            return std::unexpected(image.error());
        }

        const auto hash = hashTexture(*image);
        auto & candidates = byHash[hash];
        const auto existing = std::ranges::find_if(
            candidates, [&unique, &image](std::size_t candidate) { return unique.at(candidate).image == *image; });

        if (existing != candidates.end())
        {
            state.entries.at(id) = unique.at(*existing).entry;
            continue;
        }

        auto encoded = encodeTexMapsTexture(*image);
        if (outputOffset > std::numeric_limits<std::uint32_t>::max() ||
            encoded.size() > std::numeric_limits<std::uint32_t>::max() - outputOffset)
        {
            return std::unexpected(detail::MulCommon::makeError(
                ErrorCode::invalidFormat, outputPath, outputOffset, "deduplicated texture data exceeds index offset range"));
        }

        if (auto written = writeAt(outputDescriptor, encoded.data(), encoded.size(), outputOffset, outputPath); !written)
        {
            return std::unexpected(written.error());
        }

        const auto length = static_cast<std::uint32_t>(encoded.size());
        const auto uniqueId = unique.size();
        const TextureIndexEntry canonical{
            .lookup = static_cast<std::uint32_t>(outputOffset),
            .length = length,
            .extra = length == textureLargeByteCount ? 1U : 0U,
        };
        unique.push_back(Candidate{.image = std::move(*image), .entry = canonical});
        candidates.push_back(uniqueId);
        state.entries.at(id) = canonical;
        outputOffset += encoded.size();
    }

    state.report.uniquePayloads = static_cast<std::uint32_t>(unique.size());
    const auto emptyCount = static_cast<std::uint32_t>(
        std::ranges::count_if(sourceEntries, [](TextureIndexEntry entry) { return entry.length == 0; }));
    state.report.duplicateEntries = state.report.entriesScanned - emptyCount - state.report.uniquePayloads;
    state.report.outputDataBytes = outputOffset;

    return state;
}

std::expected<void, Error>
writeIndexEntries(int descriptor, const std::filesystem::path & path, const std::vector<TextureIndexEntry> & entries)
{
    for (std::size_t id = 0; id < entries.size(); id++)
    {
        const auto encoded = encodeTexMapsEntry(entries.at(id));
        const auto offset = static_cast<std::uint64_t>(id) * textureIndexEntryByteCount;

        if (auto written = writeAt(descriptor, encoded.data(), encoded.size(), offset, path); !written)
        {
            return std::unexpected(written.error());
        }
    }

    return {};
}

std::expected<void, Error> replaceStagedPair(const std::filesystem::path & currentIndex,
                                             const std::filesystem::path & currentData,
                                             const std::filesystem::path & replacementIndex,
                                             const std::filesystem::path & replacementData)
{
    auto indexBackup = linkBackup(currentIndex);
    if (!indexBackup)
    {
        return std::unexpected(indexBackup.error());
    }

    auto dataBackup = linkBackup(currentData);
    if (!dataBackup)
    {
        static_cast<void>(::unlink(indexBackup->c_str()));
        return std::unexpected(dataBackup.error());
    }

    const int indexReplaceResult = ::rename(replacementIndex.c_str(), currentIndex.c_str());
    if (indexReplaceResult != 0)
    {
        const std::string message = std::strerror(errno);
        static_cast<void>(::unlink(indexBackup->c_str()));
        static_cast<void>(::unlink(dataBackup->c_str()));
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, currentIndex, 0, message));
    }

    const int dataReplaceResult = ::rename(replacementData.c_str(), currentData.c_str());
    if (dataReplaceResult != 0)
    {
        const std::string message = std::strerror(errno);
        const auto restoredIndex = restoreBackup(*indexBackup, currentIndex, true);
        const auto restoredData = restoreBackup(*dataBackup, currentData, true);
        const bool restored = restoredIndex && restoredData;
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::io, currentData, 0, message + (restored ? "; rollback succeeded" : "; rollback failed")));
    }

    static_cast<void>(::unlink(indexBackup->c_str()));
    static_cast<void>(::unlink(dataBackup->c_str()));

    return {};
}

std::expected<void, Error>
publishOutputFile(const std::filesystem::path & staged, const std::filesystem::path & destination, bool overwrite)
{
    const int result = overwrite ? ::rename(staged.c_str(), destination.c_str()) : ::link(staged.c_str(), destination.c_str());
    if (result != 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::commitFailed, destination, 0, std::strerror(errno)));
    }

    return {};
}

struct OutputBackups
{
    std::filesystem::path index;
    std::filesystem::path data;
};

void removeBackup(const std::filesystem::path & path) noexcept
{
    if (!path.empty())
    {
        static_cast<void>(::unlink(path.c_str()));
    }
}

std::expected<OutputBackups, Error>
makeOutputBackups(const std::filesystem::path & outputIndex, const std::filesystem::path & outputData, bool overwrite)
{
    OutputBackups backups;
    if (!overwrite)
    {
        return backups;
    }

    std::error_code error;
    const bool indexExists = std::filesystem::exists(outputIndex, error);

    if (error)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::commitFailed, outputIndex, 0, error.message()));
    }

    if (indexExists)
    {
        auto backup = linkBackup(outputIndex);
        if (!backup)
        {
            return std::unexpected(backup.error());
        }
        backups.index = std::move(*backup);
    }

    error.clear();
    const bool dataExists = std::filesystem::exists(outputData, error);

    if (error)
    {
        removeBackup(backups.index);
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::commitFailed, outputData, 0, error.message()));
    }

    if (dataExists)
    {
        auto backup = linkBackup(outputData);
        if (!backup)
        {
            removeBackup(backups.index);
            return std::unexpected(backup.error());
        }

        backups.data = std::move(*backup);
    }

    return backups;
}

std::expected<void, Error> rollbackOutputPair(const OutputBackups & backups,
                                              const std::filesystem::path & outputIndex,
                                              const std::filesystem::path & outputData,
                                              const Error & cause)
{
    const auto rollbackIndex = restoreBackup(backups.index, outputIndex, true);
    const auto rollbackData = restoreBackup(backups.data, outputData, true);
    const bool restored = rollbackIndex && rollbackData;
    const std::string message =
        cause.message + (restored ? "; rollback succeeded" : "; rollback failed; output pair may be inconsistent");
    return std::unexpected(detail::MulCommon::makeError(ErrorCode::commitFailed, outputData, 0, message));
}

std::expected<void, Error> publishOutputPair(const std::filesystem::path & stagedIndex,
                                             const std::filesystem::path & stagedData,
                                             const std::filesystem::path & outputIndex,
                                             const std::filesystem::path & outputData,
                                             bool overwrite)
{
    auto backups = makeOutputBackups(outputIndex, outputData, overwrite);
    if (!backups)
    {
        return std::unexpected(backups.error());
    }

    auto publishedIndex = publishOutputFile(stagedIndex, outputIndex, overwrite);
    if (!publishedIndex)
    {
        removeBackup(backups->index);
        removeBackup(backups->data);
        return std::unexpected(publishedIndex.error());
    }

    auto publishedData = publishOutputFile(stagedData, outputData, overwrite);
    if (!publishedData)
    {
        return rollbackOutputPair(*backups, outputIndex, outputData, publishedData.error());
    }

    if (!overwrite)
    {
        const int indexCleanup = ::unlink(stagedIndex.c_str());
        const int dataCleanup = ::unlink(stagedData.c_str());
        if (indexCleanup != 0 || dataCleanup != 0)
        {
            return std::unexpected(detail::MulCommon::makeError(
                ErrorCode::commitFailed, stagedIndex, 0, "outputs published but temporary links could not be removed"));
        }

        return {};
    }

    removeBackup(backups->index);
    removeBackup(backups->data);

    return {};
}

std::expected<void, Error> rollbackTextureAppend(int indexDescriptor,
                                                 int dataDescriptor,
                                                 std::vector<TextureIndexEntry> & entries,
                                                 std::size_t originalEntryCount,
                                                 std::uint64_t originalIndexSize,
                                                 std::uint64_t originalDataSize,
                                                 std::uint32_t id,
                                                 const std::filesystem::path & dataPath)
{
    const bool dataRestored = ::ftruncate(dataDescriptor, static_cast<off_t>(originalDataSize)) == 0;
    bool indexRestored = true;

    if (id >= originalEntryCount)
    {
        indexRestored = ::ftruncate(indexDescriptor, static_cast<off_t>(originalIndexSize)) == 0;
        entries.resize(originalEntryCount);
    }

    if (!dataRestored || !indexRestored)
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::io, dataPath, originalDataSize, "texture append failed and staged rollback was incomplete"));
    }

    return {};
}

std::expected<void, Error> stageTextureUpdate(int indexDescriptor,
                                              int dataDescriptor,
                                              const std::filesystem::path & indexPath,
                                              const std::filesystem::path & dataPath,
                                              std::vector<TextureIndexEntry> & entries,
                                              std::uint32_t id,
                                              const TextureImage & texture)
{
    const bool small = texture.dimension == textureSmallDimension;
    const bool large = texture.dimension == textureLargeDimension;
    const auto expectedPixels = small ? textureSmallPixelCount : textureLargePixelCount;

    if ((!small && !large) || texture.pixels.size() != expectedPixels)
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::invalidDimensions, indexPath, id, "texture must be 64x64 or 128x128 with a complete pixel array"));
    }

    const auto encoded = encodeTexMapsTexture(texture);
    auto dataSize = detail::MulCommon::regularFileSize(dataDescriptor, dataPath, "texture data");

    if (!dataSize)
    {
        return std::unexpected(dataSize.error());
    }

    const auto maximumOffset = static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max());

    if (*dataSize > maximumOffset || encoded.size() > maximumOffset - *dataSize)
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::invalidFormat, dataPath, *dataSize, "texture data exceeds the supported index offset range"));
    }

    const auto originalEntryCount = entries.size();
    const auto originalIndexSize = static_cast<std::uint64_t>(originalEntryCount) * textureIndexEntryByteCount;
    const bool extendingIndex = id >= originalEntryCount;

    if (extendingIndex)
    {
        const auto newIndexBytes = (static_cast<std::uint64_t>(id) + 1) * textureIndexEntryByteCount;
        if (newIndexBytes > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()))
        {
            return std::unexpected(detail::MulCommon::makeError(
                ErrorCode::invalidFormat, indexPath, originalIndexSize, "texture index exceeds the supported file size"));
        }

        if (::ftruncate(indexDescriptor, static_cast<off_t>(newIndexBytes)) != 0)
        {
            return std::unexpected(
                detail::MulCommon::makeError(ErrorCode::io, indexPath, originalIndexSize, std::strerror(errno)));
        }

        entries.resize(static_cast<std::size_t>(id) + 1);
    }
    const auto oldEntry = extendingIndex ? TextureIndexEntry{} : entries.at(id);
    const TextureIndexEntry newEntry{
        .lookup = static_cast<std::uint32_t>(*dataSize),
        .length = static_cast<std::uint32_t>(encoded.size()),
        .extra = large ? 1U : 0U,
    };

    if (!detail::MulCommon::writeExact(dataDescriptor, encoded.data(), encoded.size(), *dataSize))
    {
        const auto rolledBack = rollbackTextureAppend(
            indexDescriptor, dataDescriptor, entries, originalEntryCount, originalIndexSize, *dataSize, id, dataPath);
        if (!rolledBack)
        {
            return std::unexpected(rolledBack.error());
        }

        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::io, dataPath, *dataSize, "could not append texture payload"));
    }

    const auto encodedEntry = encodeTexMapsEntry(newEntry);
    const auto entryOffset = static_cast<std::uint64_t>(id) * textureIndexEntryByteCount;

    if (!detail::MulCommon::writeExact(indexDescriptor, encodedEntry.data(), encodedEntry.size(), entryOffset))
    {
        const auto previous = encodeTexMapsEntry(oldEntry);
        const bool indexRestored =
            detail::MulCommon::writeExact(indexDescriptor, previous.data(), previous.size(), entryOffset);
        const auto rolledBack = rollbackTextureAppend(
            indexDescriptor, dataDescriptor, entries, originalEntryCount, originalIndexSize, *dataSize, id, dataPath);

        if (!indexRestored || !rolledBack)
        {
            return std::unexpected(detail::MulCommon::makeError(
                ErrorCode::io, indexPath, entryOffset, "texture update failed and staged rollback was incomplete"));
        }

        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::io, indexPath, entryOffset, "could not update texture index entry"));
    }

    entries.at(id) = newEntry;
    return {};
}

} // namespace

struct TexMapsEditor::Impl
{
    std::filesystem::path outputIndex;
    std::filesystem::path outputData;
    std::filesystem::path stagedIndex;
    std::filesystem::path stagedData;
    detail::ScopedDescriptor indexDescriptor;
    detail::ScopedDescriptor dataDescriptor;
    std::vector<TextureIndexEntry> entries;
    bool overwrite = false;
    bool active = true;
};

TexMapsEditor::TexMapsEditor(std::unique_ptr<Impl> impl) : impl_(std::move(impl))
{
}

TexMapsEditor::TexMapsEditor(TexMapsEditor &&) noexcept = default;
TexMapsEditor & TexMapsEditor::operator=(TexMapsEditor && other) noexcept
{
    if (this != &other)
    {
        abort();
        impl_ = std::move(other.impl_);
    }

    return *this;
}
TexMapsEditor::~TexMapsEditor()
{
    abort();
}

std::expected<TexMapsEditor, Error> TexMapsEditor::create(const std::filesystem::path & sourceIndex,
                                                          const std::filesystem::path & sourceData,
                                                          const std::filesystem::path & outputIndex,
                                                          const std::filesystem::path & outputData,
                                                          TexMapsEditOptions options)
{
    const detail::ScopedDescriptor sourceIndexDescriptor(::open(sourceIndex.c_str(), O_RDONLY | O_CLOEXEC));

    if (sourceIndexDescriptor.get() < 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, sourceIndex, 0, std::strerror(errno)));
    }

    const detail::ScopedDescriptor sourceDataDescriptor(::open(sourceData.c_str(), O_RDONLY | O_CLOEXEC));
    if (sourceDataDescriptor.get() < 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, sourceData, 0, std::strerror(errno)));
    }

    auto sourceDataSize = detail::MulCommon::regularFileSize(sourceDataDescriptor.get(), sourceData, "texture data");
    if (!sourceDataSize)
    {
        return std::unexpected(sourceDataSize.error());
    }

    auto entries = loadTexMapsIndex(sourceIndexDescriptor.get(), sourceIndex, *sourceDataSize);
    if (!entries)
    {
        return std::unexpected(entries.error());
    }

    if (auto paths = validatePairPaths(outputIndex, outputData, options.overwriteOutput, &sourceIndex, &sourceData); !paths)
    {
        return std::unexpected(paths.error());
    }

    auto impl = std::make_unique<Impl>();
    impl->outputIndex = outputIndex;
    impl->outputData = outputData;
    impl->overwrite = options.overwriteOutput;

    auto indexTemporary = detail::MulCommon::createTemporary(outputIndex, impl->stagedIndex);
    if (!indexTemporary)
    {
        return std::unexpected(indexTemporary.error());
    }
    detail::TemporaryPath indexCleanup(impl->stagedIndex);
    detail::ScopedDescriptor stagedIndex(*indexTemporary);

    auto dataTemporary = detail::MulCommon::createTemporary(outputData, impl->stagedData);
    if (!dataTemporary)
    {
        return std::unexpected(dataTemporary.error());
    }
    detail::TemporaryPath dataCleanup(impl->stagedData);
    detail::ScopedDescriptor stagedData(*dataTemporary);

    auto indexSize = detail::MulCommon::regularFileSize(sourceIndexDescriptor.get(), sourceIndex, "texture index");
    if (!indexSize)
    {
        return std::unexpected(indexSize.error());
    }

    if (auto copied = copyFile(sourceIndexDescriptor.get(), stagedIndex.get(), *indexSize, impl->stagedIndex); !copied)
    {
        return std::unexpected(copied.error());
    }

    if (auto copied = copyFile(sourceDataDescriptor.get(), stagedData.get(), *sourceDataSize, impl->stagedData); !copied)
    {
        return std::unexpected(copied.error());
    }
    impl->indexDescriptor = std::move(stagedIndex);
    impl->dataDescriptor = std::move(stagedData);
    impl->entries = std::move(*entries);
    indexCleanup.release();
    dataCleanup.release();

    return TexMapsEditor(std::move(impl));
}

std::expected<TexMapsEditor, Error> TexMapsEditor::createEmpty(const std::filesystem::path & outputIndex,
                                                               const std::filesystem::path & outputData,
                                                               TexMapsEditOptions options)
{
    if (auto paths = validatePairPaths(outputIndex, outputData, options.overwriteOutput); !paths)
    {
        return std::unexpected(paths.error());
    }

    auto impl = std::make_unique<Impl>();
    impl->outputIndex = outputIndex;
    impl->outputData = outputData;
    impl->overwrite = options.overwriteOutput;

    auto indexTemporary = detail::MulCommon::createTemporary(outputIndex, impl->stagedIndex);
    if (!indexTemporary)
    {
        return std::unexpected(indexTemporary.error());
    }

    detail::TemporaryPath indexCleanup(impl->stagedIndex);
    detail::ScopedDescriptor stagedIndex(*indexTemporary);

    auto dataTemporary = detail::MulCommon::createTemporary(outputData, impl->stagedData);
    if (!dataTemporary)
    {
        return std::unexpected(dataTemporary.error());
    }

    detail::TemporaryPath dataCleanup(impl->stagedData);
    detail::ScopedDescriptor stagedData(*dataTemporary);

    const auto indexBytes = static_cast<std::uint64_t>(textureCount) * textureIndexEntryByteCount;
    if (indexBytes > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()))
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::invalidFormat, outputIndex, 0, "empty texture index exceeds the supported file size"));
    }

    if (::ftruncate(stagedIndex.get(), static_cast<off_t>(indexBytes)) != 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, impl->stagedIndex, 0, std::strerror(errno)));
    }

    impl->entries.resize(textureCount);
    impl->indexDescriptor = std::move(stagedIndex);
    impl->dataDescriptor = std::move(stagedData);
    indexCleanup.release();
    dataCleanup.release();

    return TexMapsEditor(std::move(impl));
}

std::expected<void, Error> TexMapsEditor::editTexture(std::uint32_t id, const TextureImage & texture)
{
    if (!impl_ || !impl_->active)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::commitFailed, {}, 0, "editor is no longer active"));
    }

    if (id >= textureCount)
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::invalidCoordinate, impl_->stagedIndex, id, "texture ID is outside the supported range"));
    }

    auto updated = stageTextureUpdate(impl_->indexDescriptor.get(),
                                      impl_->dataDescriptor.get(),
                                      impl_->stagedIndex,
                                      impl_->stagedData,
                                      impl_->entries,
                                      id,
                                      texture);

    if (!updated && updated.error().message.contains("rollback was incomplete"))
    {
        abort();
    }

    return updated;
}

std::expected<void, Error> TexMapsEditor::clearTexture(std::uint32_t id)
{
    if (!impl_ || !impl_->active)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::commitFailed, {}, 0, "editor is no longer active"));
    }

    if (id >= textureCount)
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::invalidCoordinate, impl_->stagedIndex, id, "texture ID is outside the supported range"));
    }

    const auto oldCount = impl_->entries.size();
    const auto oldIndexBytes = static_cast<std::uint64_t>(oldCount) * textureIndexEntryByteCount;
    const auto oldSize = static_cast<off_t>(oldIndexBytes);

    if (id >= oldCount)
    {
        const auto newIndexBytes = (static_cast<std::uint64_t>(id) + 1) * textureIndexEntryByteCount;
        if (newIndexBytes > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()))
        {
            return std::unexpected(detail::MulCommon::makeError(
                ErrorCode::invalidFormat, impl_->stagedIndex, oldIndexBytes, "texture index exceeds the supported file size"));
        }

        if (::ftruncate(impl_->indexDescriptor.get(), static_cast<off_t>(newIndexBytes)) != 0)
        {
            return std::unexpected(detail::MulCommon::makeError(
                ErrorCode::io, impl_->stagedIndex, static_cast<std::uint64_t>(oldSize), std::strerror(errno)));
        }

        impl_->entries.resize(static_cast<std::size_t>(id) + 1);
    }
    const auto oldEntry = impl_->entries.at(id);
    const auto empty = encodeTexMapsEntry({});
    const auto offset = static_cast<std::uint64_t>(id) * textureIndexEntryByteCount;

    if (!detail::MulCommon::writeExact(impl_->indexDescriptor.get(), empty.data(), empty.size(), offset))
    {
        const auto previous = encodeTexMapsEntry(oldEntry);
        const bool restored =
            detail::MulCommon::writeExact(impl_->indexDescriptor.get(), previous.data(), previous.size(), offset);

        if (id >= oldCount)
        {
            static_cast<void>(::ftruncate(impl_->indexDescriptor.get(), oldSize));
            impl_->entries.resize(oldCount);
        }

        if (!restored)
        {
            impl_->active = false;
        }

        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::io,
            impl_->stagedIndex,
            offset,
            restored ? "could not clear texture index entry" : "texture clear failed and staged rollback was incomplete"));
    }

    impl_->entries.at(id) = {};
    return {};
}

std::expected<TextureDeduplicationReport, Error> TexMapsEditor::deduplicateTextures()
{
    if (!impl_ || !impl_->active)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::commitFailed, {}, 0, "editor is no longer active"));
    }

    auto inputSize = detail::MulCommon::regularFileSize(impl_->dataDescriptor.get(), impl_->stagedData, "texture data");
    if (!inputSize)
    {
        return std::unexpected(inputSize.error());
    }

    std::filesystem::path replacementIndexPath;
    auto replacementIndex = detail::MulCommon::createTemporary(impl_->stagedIndex, replacementIndexPath);
    if (!replacementIndex)
    {
        return std::unexpected(replacementIndex.error());
    }

    detail::TemporaryPath indexCleanup(replacementIndexPath);
    detail::ScopedDescriptor indexDescriptor(*replacementIndex);
    std::filesystem::path replacementDataPath;

    auto replacementData = detail::MulCommon::createTemporary(impl_->stagedData, replacementDataPath);
    if (!replacementData)
    {
        return std::unexpected(replacementData.error());
    }

    detail::TemporaryPath dataCleanup(replacementDataPath);
    detail::ScopedDescriptor dataDescriptor(*replacementData);
    auto state = buildDeduplicatedPayload(
        impl_->dataDescriptor.get(), dataDescriptor.get(), impl_->stagedData, replacementDataPath, impl_->entries, *inputSize);

    if (!state)
    {
        return std::unexpected(state.error());
    }

    if (auto written = writeIndexEntries(indexDescriptor.get(), replacementIndexPath, state->entries); !written)
    {
        return std::unexpected(written.error());
    }

    if (auto synced = syncDescriptor(indexDescriptor.get(), replacementIndexPath, ErrorCode::io); !synced)
    {
        return std::unexpected(synced.error());
    }

    if (auto synced = syncDescriptor(dataDescriptor.get(), replacementDataPath, ErrorCode::io); !synced)
    {
        return std::unexpected(synced.error());
    }

    const auto indexCloseResult = indexDescriptor.close();
    const auto dataCloseResult = dataDescriptor.close();

    if (indexCloseResult != 0 || dataCloseResult != 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, replacementIndexPath, 0, std::strerror(errno)));
    }

    if (auto replaced = replaceStagedPair(impl_->stagedIndex, impl_->stagedData, replacementIndexPath, replacementDataPath);
        !replaced)
    {
        return std::unexpected(replaced.error());
    }

    indexCleanup.release();
    dataCleanup.release();
    detail::ScopedDescriptor newIndex(::open(impl_->stagedIndex.c_str(), O_RDWR | O_CLOEXEC));

    if (newIndex.get() < 0)
    {
        impl_->active = false;
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, impl_->stagedIndex, 0, std::strerror(errno)));
    }

    detail::ScopedDescriptor newData(::open(impl_->stagedData.c_str(), O_RDWR | O_CLOEXEC));
    if (newData.get() < 0)
    {
        impl_->active = false;
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, impl_->stagedData, 0, std::strerror(errno)));
    }

    impl_->indexDescriptor = std::move(newIndex);
    impl_->dataDescriptor = std::move(newData);
    impl_->entries = std::move(state->entries);

    return state->report;
}

std::expected<void, Error> TexMapsEditor::commit()
{
    if (!impl_ || !impl_->active)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::commitFailed, {}, 0, "editor is no longer active"));
    }

    if (auto synced = syncDescriptor(impl_->indexDescriptor.get(), impl_->stagedIndex, ErrorCode::commitFailed); !synced)
    {
        return synced;
    }

    if (auto synced = syncDescriptor(impl_->dataDescriptor.get(), impl_->stagedData, ErrorCode::commitFailed); !synced)
    {
        return synced;
    }

    if (const auto closed = impl_->indexDescriptor.close(); closed != 0)
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::commitFailed, impl_->stagedIndex, 0, std::strerror(errno)));
    }

    if (const auto closed = impl_->dataDescriptor.close(); closed != 0)
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::commitFailed, impl_->stagedData, 0, std::strerror(errno)));
    }

    if (auto valid = validatePairPaths(impl_->outputIndex, impl_->outputData, impl_->overwrite); !valid)
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::commitFailed, valid.error().path, valid.error().offset, valid.error().message));
    }

    auto published =
        publishOutputPair(impl_->stagedIndex, impl_->stagedData, impl_->outputIndex, impl_->outputData, impl_->overwrite);

    if (!published)
    {
        impl_->active = false;
        return std::unexpected(published.error());
    }

    impl_->active = false;
    return {};
}

void TexMapsEditor::abort() noexcept
{
    if (!impl_ || !impl_->active)
    {
        return;
    }

    static_cast<void>(impl_->indexDescriptor.close());
    static_cast<void>(impl_->dataDescriptor.close());

    if (!impl_->stagedIndex.empty())
    {
        static_cast<void>(::unlink(impl_->stagedIndex.c_str()));
    }

    if (!impl_->stagedData.empty())
    {
        static_cast<void>(::unlink(impl_->stagedData.c_str()));
    }

    impl_->active = false;
}

} // namespace uomul
