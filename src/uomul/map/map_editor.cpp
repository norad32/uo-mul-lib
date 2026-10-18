#include "map_common.h"
#include "uomul/map.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/mul_common/output_transaction.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <iterator>
#include <memory>
#include <span>
#include <sys/types.h>
#ifdef __linux__
#include <linux/fs.h>
#include <sys/ioctl.h>
#endif
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

namespace uomul
{

using detail::MapCommon;

namespace
{

constexpr std::size_t bufferSize = 1'048'576U;

[[nodiscard]] inline bool outputAliasesSource(const std::filesystem::path & source,
                                              const std::filesystem::path & output,
                                              bool outputExists,
                                              std::error_code & pathError)
{
    return outputExists && std::filesystem::equivalent(source, output, pathError);
}

[[nodiscard]] inline bool
copyExactBytes(int sourceDescriptor, int destinationDescriptor, std::byte * buffer, std::size_t byteCount, std::uint64_t offset)
{
    return detail::MulCommon::readExact(sourceDescriptor, buffer, byteCount, offset) &&
           detail::MulCommon::writeExact(destinationDescriptor, buffer, byteCount, offset);
}

std::expected<void, Error>
validateEditorPaths(const std::filesystem::path & source, const std::filesystem::path & output, bool overwrite)
{
    std::error_code pathError;
    const auto sourceAbsolute = std::filesystem::absolute(source, pathError).lexically_normal();
    if (pathError)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, source, 0, pathError.message()));
    }
    const auto outputAbsolute = std::filesystem::absolute(output, pathError).lexically_normal();
    if (pathError)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, output, 0, pathError.message()));
    }
    if (sourceAbsolute == outputAbsolute)
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::io, output, 0, "in-place editing is not supported; choose a separate output path"));
    }

    pathError.clear();
    const bool outputExists = std::filesystem::exists(output, pathError);
    if (pathError)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, output, 0, pathError.message()));
    }
    if (outputAliasesSource(source, output, outputExists, pathError))
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::io, output, 0, "output path resolves to the source map"));
    }
    if (pathError)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, output, 0, pathError.message()));
    }
    if (auto check = detail::MulCommon::checkOutputPath(output, overwrite); !check)
    {
        return check;
    }

    const auto parent = output.parent_path();
    std::error_code directoryError;
    const bool parentExists = parent.empty() || std::filesystem::is_directory(parent, directoryError);
    if (directoryError || !parentExists)
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::io, parent, 0, directoryError ? directoryError.message() : "output directory does not exist"));
    }
    return {};
}

std::expected<void, Error> stageSourceMap(int sourceDescriptor,
                                          int stagedDescriptor,
                                          std::uint64_t totalBytes,
                                          const std::filesystem::path & source,
                                          const std::filesystem::path & staging)
{
    if (!detail::MulCommon::isRegularFileDescriptor(sourceDescriptor))
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, source, 0, "map input must be a regular file"));
    }
#ifdef __linux__
    if (::ioctl(stagedDescriptor, FICLONE, sourceDescriptor) == 0)
    {
        return {};
    }
    if (::ftruncate(stagedDescriptor, 0) != 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, staging, 0, "could not reset staging file"));
    }
    std::uint64_t copiedBytes = 0;
    bool copyRangeSupported = true;
    while (copiedBytes < totalBytes)
    {
        const auto amount = static_cast<std::size_t>(std::min<std::uint64_t>(bufferSize, totalBytes - copiedBytes));
        auto inputOffset = static_cast<off_t>(copiedBytes);
        auto outputOffset = static_cast<off_t>(copiedBytes);
        const auto copied = ::copy_file_range(sourceDescriptor, &inputOffset, stagedDescriptor, &outputOffset, amount, 0);
        if (copied < 0 && errno == EINTR)
        {
            continue;
        }
        if (copied <= 0)
        {
            copyRangeSupported = false;
            break;
        }
        copiedBytes += static_cast<std::uint64_t>(copied);
    }
    if (copyRangeSupported)
    {
        return {};
    }
#endif
    if (::ftruncate(stagedDescriptor, 0) != 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, staging, 0, "could not reset staging file"));
    }

    std::array<std::byte, bufferSize> buffer{};
    std::uint64_t offset = 0;
    while (offset < totalBytes)
    {
        const auto amount = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), totalBytes - offset));
        if (!copyExactBytes(sourceDescriptor, stagedDescriptor, buffer.data(), amount, offset))
        {
            return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, staging, offset, "failed to stage source map"));
        }
        offset += amount;
    }
    return {};
}

std::expected<void, Error> ensureEditorActive(bool committed, int descriptor, bool failed, const std::filesystem::path & output)
{
    if (committed || descriptor < 0 || failed)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::commitFailed, output, 0, "editor is no longer active"));
    }
    return {};
}

template <typename Region, typename Callback>
std::expected<void, Error> validateEditRequest(bool committed,
                                               int descriptor,
                                               bool failed,
                                               const std::filesystem::path & output,
                                               const std::filesystem::path & source,
                                               Region region,
                                               const Callback & callback,
                                               Dimensions dimensions)
{
    if (auto active = ensureEditorActive(committed, descriptor, failed, output); !active)
    {
        return active;
    }
    if (!callback)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, source, 0, "edit callback is empty"));
    }
    return MapCommon::validateRegion(region, dimensions, source);
}

} // namespace

struct MapEditor::Impl
{
    std::filesystem::path source;
    Dimensions dimensions{};
    detail::OutputTransaction transaction;
    detail::OutputTransaction::OutputId outputId = 0;
    bool committed = false;
    bool failed = false;

    [[nodiscard]] int descriptor() const { return transaction.descriptor(outputId); }

    [[nodiscard]] const std::filesystem::path & output() const { return transaction.destination(outputId); }

    [[nodiscard]] const std::filesystem::path & staging() const { return transaction.stagedPath(outputId); }
};

MapEditor::MapEditor(std::unique_ptr<Impl> impl) : impl_(std::move(impl))
{
}

MapEditor::MapEditor(MapEditor &&) noexcept = default;
MapEditor & MapEditor::operator=(MapEditor && other) noexcept
{
    if (this != &other)
    {
        abort();
        impl_ = std::move(other.impl_);
    }
    return *this;
}
MapEditor::~MapEditor()
{
    abort();
}

std::expected<MapEditor, Error> MapEditor::create(const std::filesystem::path & sourceMap,
                                                  const std::filesystem::path & outputMap,
                                                  Dimensions dimensions,
                                                  EditOptions options)
{
    auto source = MapReader::open(sourceMap, dimensions);
    if (!source)
    {
        return std::unexpected(source.error());
    }

    if (auto validPaths = validateEditorPaths(sourceMap, outputMap, options.overwriteOutput); !validPaths)
    {
        return std::unexpected(validPaths.error());
    }

    auto impl = std::make_unique<Impl>();
    impl->source = sourceMap;
    impl->dimensions = dimensions;
    auto output = impl->transaction.addOutput(outputMap, options.overwriteOutput);
    if (!output)
    {
        return std::unexpected(output.error());
    }
    impl->outputId = *output;
    const detail::ScopedDescriptor sourceDescriptor(::open(sourceMap.c_str(), O_RDONLY | O_CLOEXEC));
    if (sourceDescriptor.get() < 0)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, sourceMap, 0, std::strerror(errno)));
    }
    const auto totalBytes = source->completeBlocks() * MapCommon::mapBlockByteSize;
    if (auto staged = stageSourceMap(sourceDescriptor.get(), impl->descriptor(), totalBytes, sourceMap, impl->staging());
        !staged)
    {
        return std::unexpected(staged.error());
    }

    return MapEditor(std::move(impl));
}

std::expected<void, Error>
MapEditor::createEmpty(const std::filesystem::path & outputMap, Dimensions dimensions, MapTile initialTile, EditOptions options)
{
    if (outputMap.empty() || outputMap.filename().empty())
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, outputMap, 0, "output path must name a file"));
    }
    if (auto count = MapCommon::blockCount(dimensions, outputMap); !count)
    {
        return std::unexpected(count.error());
    }
    if (auto check = detail::MulCommon::checkOutputPath(outputMap, options.overwriteOutput); !check)
    {
        return std::unexpected(check.error());
    }

    const auto parent = outputMap.parent_path();
    std::error_code directoryError;
    const bool parentExists = parent.empty() || std::filesystem::is_directory(parent, directoryError);
    if (directoryError || !parentExists)
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::io, parent, 0, directoryError ? directoryError.message() : "output directory does not exist"));
    }

    std::error_code statusError;
    const bool outputExists = std::filesystem::exists(outputMap, statusError);
    if (statusError)
    {
        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io, outputMap, 0, statusError.message()));
    }
    if (outputExists && !std::filesystem::is_regular_file(outputMap, statusError))
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::io, outputMap, 0, statusError ? statusError.message() : "output must be a regular file"));
    }

    detail::OutputTransaction transaction;
    auto stagedOutput = transaction.addOutput(outputMap, options.overwriteOutput);
    if (!stagedOutput)
    {
        return std::unexpected(stagedOutput.error());
    }
    const auto outputId = *stagedOutput;
    const int stagedDescriptor = transaction.descriptor(outputId);
    const auto & staging = transaction.stagedPath(outputId);

    MapBlock block{};
    block.fill(initialTile);
    MapBlockBytes raw{};
    MapCommon::encodeBlock(raw, block);
    for (std::uint32_t x = 0; x < dimensions.widthTiles / mapBlockDimension; x++)
    {
        for (std::uint32_t y = 0; y < dimensions.heightTiles / mapBlockDimension; y++)
        {
            const auto offset = MapCommon::blockOffset({.x = x, .y = y}, dimensions);
            if (!detail::MulCommon::writeExact(stagedDescriptor, raw.data(), raw.size(), offset))
            {
                return std::unexpected(
                    detail::MulCommon::makeError(ErrorCode::io, staging, offset, "failed to write initialized map block"));
            }
        }
    }

    if (auto synced = transaction.sync(outputId, ErrorCode::io); !synced)
    {
        return synced;
    }
    if (auto closed = transaction.close(outputId, ErrorCode::io); !closed)
    {
        return closed;
    }

    return transaction.publish();
}

std::expected<void, Error> MapEditor::editBlock(BlockCoord block, const MapBlock & data)
{
    if (auto active = ensureEditorActive(impl_->committed, impl_->descriptor(), impl_->failed, impl_->output()); !active)
    {
        return active;
    }
    if (auto valid = MapCommon::validateBlock(block, impl_->dimensions, impl_->source); !valid)
    {
        return valid;
    }

    MapBlockBytes raw{};
    const auto offset = MapCommon::blockOffset(block, impl_->dimensions);
    if (!detail::MulCommon::readExact(impl_->descriptor(), raw.data(), raw.size(), offset))
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::io, impl_->staging(), offset, "could not read staged block"));
    }
    MapCommon::encodeBlock(raw, data);
    if (!detail::MulCommon::writeExact(impl_->descriptor(), raw.data(), raw.size(), offset))
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::io, impl_->staging(), offset, "could not stage land block"));
    }

    return {};
}

std::expected<void, Error> MapEditor::editTile(TileCoord tile, MapTile data)
{
    if (auto active = ensureEditorActive(impl_->committed, impl_->descriptor(), impl_->failed, impl_->output()); !active)
    {
        return active;
    }
    if (auto valid = MapCommon::validateTile(tile, impl_->dimensions, impl_->source); !valid)
    {
        return valid;
    }

    const BlockCoord coordinate{.x = tile.x / mapBlockDimension, .y = tile.y / mapBlockDimension};
    const auto tileOffset = MapCommon::blockOffset(coordinate, impl_->dimensions) + MapCommon::mapHeaderByteSize +
                            (detail::tileIndexInBlock(tile) * MapCommon::mapTileByteSize);
    const auto raw = detail::encodeTile(data);
    if (!detail::MulCommon::descriptorContainsRange(impl_->descriptor(), tileOffset, raw.size()))
    {
        return std::unexpected(detail::MulCommon::makeError(
            ErrorCode::truncatedInput, impl_->staging(), tileOffset, "could not read complete land block"));
    }
    if (!detail::MulCommon::writeExact(impl_->descriptor(), raw.data(), raw.size(), tileOffset))
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::io, impl_->staging(), tileOffset, "could not stage land tile"));
    }
    return {};
}

namespace
{

constexpr std::uint64_t journalHeaderBytes = sizeof(std::uint64_t);
constexpr std::size_t journalEntryBytes = static_cast<std::size_t>(journalHeaderBytes + (2 * MapCommon::mapBlockByteSize));
constexpr std::size_t journalEntriesPerWrite = 4096;
constexpr std::size_t journalEntriesPerRead = 1024;
constexpr std::size_t journalBlockBytes = static_cast<std::size_t>(MapCommon::mapBlockByteSize);

std::uint64_t journalOffset(std::span<const std::byte, journalEntryBytes> entry)
{
    std::array<std::byte, sizeof(std::uint64_t)> encoded{};
    std::ranges::copy_n(entry.begin(), encoded.size(), encoded.begin());
    return detail::decodeLittleEndian<std::uint64_t>(encoded);
}

[[nodiscard]] std::uint64_t journalEntryOffset(std::uint64_t entryIndex)
{
    return entryIndex * journalEntryBytes;
}

[[nodiscard]] inline bool journalEntryFollowsRun(std::span<const std::byte, journalEntryBytes> entry,
                                                 std::uint64_t firstBlockOffset,
                                                 std::size_t entryDistance)
{
    return journalOffset(entry) == firstBlockOffset + (entryDistance * MapCommon::mapBlockByteSize);
}

std::expected<void, Error> flushJournal(int journal,
                                        const std::filesystem::path & journalPath,
                                        const std::vector<std::byte> & buffer,
                                        std::size_t bufferedEntries,
                                        std::uint64_t firstEntry)
{
    const auto offset = journalEntryOffset(firstEntry);
    const auto byteCount = bufferedEntries * journalEntryBytes;
    if (!detail::MulCommon::writeExact(journal, buffer.data(), byteCount, offset))
    {
        return std::unexpected(
            detail::MulCommon::makeError(ErrorCode::io, journalPath, offset, "could not write edit journal"));
    }
    return {};
}

[[nodiscard]] std::size_t journalRunEnd(const std::vector<std::byte> & entries, std::size_t runStart, std::size_t entryCount)
{
    const auto firstEntryOffset = runStart * journalEntryBytes;
    const auto firstEntry = std::span<const std::byte, journalEntryBytes>(
        std::next(entries.begin(), static_cast<std::ptrdiff_t>(firstEntryOffset)), journalEntryBytes);
    const auto blockOffset = journalOffset(firstEntry);
    std::size_t runEnd = runStart + 1;
    while (runEnd < entryCount)
    {
        const auto entryOffset = runEnd * journalEntryBytes;
        const auto entry = std::span<const std::byte, journalEntryBytes>(
            std::next(entries.begin(), static_cast<std::ptrdiff_t>(entryOffset)), journalEntryBytes);

        if (!journalEntryFollowsRun(entry, blockOffset, runEnd - runStart))
        {
            break;
        }

        runEnd++;
    }

    return runEnd;
}

void copyJournalRun(const std::vector<std::byte> & entries,
                    std::vector<std::byte> & blocks,
                    std::size_t runStart,
                    std::size_t runEnd,
                    std::size_t dataOffset)
{
    for (std::size_t index = runStart; index < runEnd; index++)
    {
        const auto entryOffset = index * journalEntryBytes;
        const auto sourceOffset = entryOffset + dataOffset;
        const auto destinationOffset = (index - runStart) * journalBlockBytes;
        std::ranges::copy_n(std::next(entries.begin(), static_cast<std::ptrdiff_t>(sourceOffset)),
                            journalBlockBytes,
                            std::next(blocks.begin(), static_cast<std::ptrdiff_t>(destinationOffset)));
    }
}

std::expected<std::uint64_t, Error> writeEditJournal(int stagedDescriptor,
                                                     const std::filesystem::path & staging,
                                                     int journal,
                                                     const std::filesystem::path & journalPath,
                                                     Dimensions dimensions,
                                                     BlockRegion region,
                                                     const MapBlockEdit & callback)
{
    std::vector<std::byte> buffer(journalEntriesPerWrite * journalEntryBytes);
    std::size_t bufferedEntries = 0;
    std::uint64_t entryCount = 0;
    for (std::uint32_t x = region.min.x; x < region.maxExclusive.x; x++)
    {
        for (std::uint32_t y = region.min.y; y < region.maxExclusive.y; y++)
        {
            const BlockCoord coordinate{.x = x, .y = y};
            const auto offset = MapCommon::blockOffset(coordinate, dimensions);
            MapBlockBytes before{};

            if (!detail::MulCommon::readExact(stagedDescriptor, before.data(), before.size(), offset))
            {
                return std::unexpected(
                    detail::MulCommon::makeError(ErrorCode::io, staging, offset, "could not read staged block"));
            }

            auto changed = callback(coordinate, MapCommon::decodeBlock(before));
            if (!changed)
            {
                return std::unexpected(changed.error());
            }

            MapBlockBytes after = before;
            MapCommon::encodeBlock(after, *changed);
            std::array<std::byte, journalEntryBytes> entry{};
            const auto encodedOffset = detail::encodeLittleEndian(offset);
            std::ranges::copy(encodedOffset, entry.begin());
            const auto beforeOffset = static_cast<std::size_t>(journalHeaderBytes);
            const auto afterOffset = beforeOffset + static_cast<std::size_t>(MapCommon::mapBlockByteSize);
            std::ranges::copy(before, std::next(entry.begin(), static_cast<std::ptrdiff_t>(beforeOffset)));
            std::ranges::copy(after, std::next(entry.begin(), static_cast<std::ptrdiff_t>(afterOffset)));
            const auto bufferOffset = bufferedEntries * static_cast<std::size_t>(journalEntryBytes);
            std::ranges::copy(entry, std::next(buffer.begin(), static_cast<std::ptrdiff_t>(bufferOffset)));
            bufferedEntries++;
            entryCount++;

            if (bufferedEntries == journalEntriesPerWrite)
            {
                if (auto flushed = flushJournal(journal, journalPath, buffer, bufferedEntries, entryCount - bufferedEntries);
                    !flushed)
                {
                    return std::unexpected(flushed.error());
                }

                bufferedEntries = 0;
            }
        }
    }

    if (bufferedEntries != 0)
    {
        if (auto flushed = flushJournal(journal, journalPath, buffer, bufferedEntries, entryCount - bufferedEntries); !flushed)
        {
            return std::unexpected(flushed.error());
        }
    }

    return entryCount;
}

enum class JournalTransferFailurePolicy : std::uint8_t
{
    stop,
    continueRecovery,
};

bool transferJournalBlocks(int stagedDescriptor,
                           int journal,
                           std::uint64_t entryCount,
                           std::uint64_t & attempted,
                           std::size_t dataOffset,
                           JournalTransferFailurePolicy failurePolicy)
{
    std::vector<std::byte> entries(journalEntriesPerRead * journalEntryBytes);
    std::vector<std::byte> blocks(journalEntriesPerRead * journalBlockBytes);

    attempted = 0;
    bool succeeded = true;
    while (attempted < entryCount)
    {
        const auto chunkStart = attempted;
        const auto entryCountInChunk =
            static_cast<std::size_t>(std::min<std::uint64_t>(journalEntriesPerRead, entryCount - chunkStart));
        const auto entriesBytes = entryCountInChunk * static_cast<std::size_t>(journalEntryBytes);
        if (!detail::MulCommon::readExact(journal, entries.data(), entriesBytes, chunkStart * journalEntryBytes))
        {
            if (failurePolicy == JournalTransferFailurePolicy::stop)
            {
                return false;
            }

            succeeded = false;
            attempted += entryCountInChunk;
            continue;
        }

        std::size_t runStart = 0;
        while (runStart < entryCountInChunk)
        {
            const auto entry = std::span<const std::byte, journalEntryBytes>(
                std::next(entries.begin(), static_cast<std::ptrdiff_t>(runStart * journalEntryBytes)), journalEntryBytes);
            const auto blockOffset = journalOffset(entry);
            const auto runEnd = journalRunEnd(entries, runStart, entryCountInChunk);
            copyJournalRun(entries, blocks, runStart, runEnd, dataOffset);
            attempted = chunkStart + runEnd;
            const auto runBytes = (runEnd - runStart) * journalBlockBytes;

            if (!detail::MulCommon::writeExact(stagedDescriptor, blocks.data(), runBytes, blockOffset))
            {
                if (failurePolicy == JournalTransferFailurePolicy::stop)
                {
                    return false;
                }

                succeeded = false;
            }

            runStart = runEnd;
        }
    }

    return succeeded;
}

bool replayEditJournal(int stagedDescriptor, int journal, std::uint64_t entryCount, std::uint64_t & attempted)
{
    return transferJournalBlocks(stagedDescriptor,
                                 journal,
                                 entryCount,
                                 attempted,
                                 static_cast<std::size_t>(journalHeaderBytes + MapCommon::mapBlockByteSize),
                                 JournalTransferFailurePolicy::stop);
}

bool rollbackEditJournal(int stagedDescriptor, int journal, std::uint64_t entryCount)
{
    std::uint64_t attempted = 0;
    return transferJournalBlocks(stagedDescriptor,
                                 journal,
                                 entryCount,
                                 attempted,
                                 static_cast<std::size_t>(journalHeaderBytes),
                                 JournalTransferFailurePolicy::continueRecovery);
}

std::expected<void, Error> applyJournaledEdit(int stagedDescriptor,
                                              const std::filesystem::path & staging,
                                              const std::filesystem::path & output,
                                              Dimensions dimensions,
                                              BlockRegion region,
                                              const MapBlockEdit & callback,
                                              bool & failed)
{
    std::filesystem::path journalPath;

    auto journalDescriptor = detail::MulCommon::createTemporary(output, journalPath);
    if (!journalDescriptor)
    {
        return std::unexpected(journalDescriptor.error());
    }

    const detail::TemporaryPath journalCleanup(journalPath);
    const detail::ScopedDescriptor journal(*journalDescriptor);

    auto entryCount = writeEditJournal(stagedDescriptor, staging, journal.get(), journalPath, dimensions, region, callback);
    if (!entryCount)
    {
        return std::unexpected(entryCount.error());
    }

    std::uint64_t attempted = 0;
    if (!replayEditJournal(stagedDescriptor, journal.get(), *entryCount, attempted))
    {
        const bool rollbackSucceeded = rollbackEditJournal(stagedDescriptor, journal.get(), attempted);
        if (!rollbackSucceeded)
        {
            failed = true;
        }

        return std::unexpected(detail::MulCommon::makeError(ErrorCode::io,
                                                            staging,
                                                            0,
                                                            rollbackSucceeded
                                                                ? "edit journal failed; staged changes were rolled back"
                                                                : "edit failed and rollback was incomplete"));
    }

    return {};
}

} // namespace

std::expected<void, Error> MapEditor::editBlocks(BlockRegion region, const MapBlockEdit & callback)
{
    if (auto valid = validateEditRequest(impl_->committed,
                                         impl_->descriptor(),
                                         impl_->failed,
                                         impl_->output(),
                                         impl_->source,
                                         region,
                                         callback,
                                         impl_->dimensions);
        !valid)
    {
        return valid;
    }

    return applyJournaledEdit(
        impl_->descriptor(), impl_->staging(), impl_->output(), impl_->dimensions, region, callback, impl_->failed);
}

std::expected<void, Error> MapEditor::editTiles(TileRegion region, const MapTileEdit & callback)
{
    if (auto valid = validateEditRequest(impl_->committed,
                                         impl_->descriptor(),
                                         impl_->failed,
                                         impl_->output(),
                                         impl_->source,
                                         region,
                                         callback,
                                         impl_->dimensions);
        !valid)
    {
        return valid;
    }

    const auto blocks = detail::blockRegionForTiles(region);
    return applyJournaledEdit(
        impl_->descriptor(),
        impl_->staging(),
        impl_->output(),
        impl_->dimensions,
        blocks,
        [&region, &callback](BlockCoord coordinate, const MapBlock & current) -> std::expected<MapBlock, Error>
        {
            MapBlock changed = current;
            auto visited = detail::visitTilesInBlock(
                region,
                coordinate,
                [&callback, &changed, &current](TileCoord tile, std::size_t index) -> std::expected<void, Error>
                {
                    auto result = callback(tile, current.at(index));
                    if (!result)
                    {
                        return std::unexpected(result.error());
                    }
                    changed.at(index) = *result;
                    return {};
                });

            if (!visited)
            {
                return std::unexpected(visited.error());
            }

            return changed;
        },
        impl_->failed);
}

std::expected<void, Error> MapEditor::fill(MapTile value)
{
    return fillBlocks(detail::fullBlockRegion(impl_->dimensions), value);
}

std::expected<void, Error> MapEditor::fillBlocks(BlockRegion region, MapTile value)
{
    return editBlocks(region,
                      [value](BlockCoord, const MapBlock &) -> std::expected<MapBlock, Error>
                      {
                          MapBlock result{};
                          result.fill(value);
                          return result;
                      });
}

std::expected<void, Error> MapEditor::fillTiles(TileRegion region, MapTile value)
{
    return editTiles(region, [value](TileCoord, const MapTile &) -> std::expected<MapTile, Error> { return value; });
}

std::expected<void, Error> MapEditor::replaceIds(std::uint16_t from, std::uint16_t to)
{
    return replaceIdsBlocks(detail::fullBlockRegion(impl_->dimensions), from, to);
}

std::expected<void, Error> MapEditor::replaceZ(std::int8_t from, std::int8_t to)
{
    return replaceZBlocks(detail::fullBlockRegion(impl_->dimensions), from, to);
}

std::expected<void, Error> MapEditor::replaceIdsTiles(TileRegion region, std::uint16_t from, std::uint16_t to)
{
    return editTiles(region,
                     [from, to](TileCoord, const MapTile & tile) -> std::expected<MapTile, Error>
                     {
                         auto result = tile;
                         if (result.id == from)
                         {
                             result.id = to;
                         }
                         return result;
                     });
}

std::expected<void, Error> MapEditor::replaceIdsBlocks(BlockRegion region, std::uint16_t from, std::uint16_t to)
{
    return editBlocks(region,
                      [from, to](BlockCoord, const MapBlock & current) -> std::expected<MapBlock, Error>
                      {
                          MapBlock result = current;
                          for (auto & tile : result)
                          {
                              if (tile.id == from)
                              {
                                  tile.id = to;
                              }
                          }
                          return result;
                      });
}

std::expected<void, Error> MapEditor::replaceZTiles(TileRegion region, std::int8_t from, std::int8_t to)
{
    return editTiles(region,
                     [from, to](TileCoord, const MapTile & tile) -> std::expected<MapTile, Error>
                     {
                         auto result = tile;
                         if (result.z == from)
                         {
                             result.z = to;
                         }
                         return result;
                     });
}

std::expected<void, Error> MapEditor::replaceZBlocks(BlockRegion region, std::int8_t from, std::int8_t to)
{
    return editBlocks(region,
                      [from, to](BlockCoord, const MapBlock & current) -> std::expected<MapBlock, Error>
                      {
                          MapBlock result = current;
                          for (auto & tile : result)
                          {
                              if (tile.z == from)
                              {
                                  tile.z = to;
                              }
                          }
                          return result;
                      });
}

std::expected<void, Error> MapEditor::commit()
{
    if (auto active = ensureEditorActive(impl_->committed, impl_->descriptor(), impl_->failed, impl_->output()); !active)
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

    auto result = impl_->transaction.publish();
    impl_->committed = true;
    return result;
}

void MapEditor::abort() noexcept
{
    if (!impl_)
    {
        return;
    }

    impl_->transaction.abort();
    impl_->committed = true;
}

} // namespace uomul
