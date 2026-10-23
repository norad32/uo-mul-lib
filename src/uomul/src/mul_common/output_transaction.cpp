#include "uomul/mul_common/output_transaction.h"
#include "uomul/mul_common/mul_common.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <expected>
#include <filesystem>
#include <string>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

namespace uomul::detail
{

namespace
{

std::expected<std::filesystem::path, Error> makeBackup(const std::filesystem::path & source)
{
    constexpr int maximumAttempts = 16;
    for (int attempt = 0; attempt < maximumAttempts; attempt++)
    {
        auto backup = MulCommon::temporaryPath(source);
        if (::link(source.c_str(), backup.c_str()) == 0)
        {
            return backup;
        }

        if (errno != EEXIST)
        {
            return std::unexpected(MulCommon::makeError(ErrorCode::commitFailed, source, 0, std::strerror(errno)));
        }
    }

    return std::unexpected(MulCommon::makeError(ErrorCode::commitFailed, source, 0, "unable to allocate backup path"));
}

void removePath(const std::filesystem::path & path) noexcept
{
    if (!path.empty())
    {
        static_cast<void>(::unlink(path.c_str()));
    }
}

} // namespace

OutputTransaction::~OutputTransaction()
{
    abort();
}

std::expected<OutputTransaction::OutputId, Error> OutputTransaction::addOutput(const std::filesystem::path & destinationPath,
                                                                               bool overwrite)
{
    if (!active_)
    {
        return std::unexpected(
            MulCommon::makeError(ErrorCode::io, destinationPath, 0, "output transaction is no longer active"));
    }

    std::filesystem::path stagingPath;
    auto created = MulCommon::createTemporary(destinationPath, stagingPath);
    if (!created)
    {
        return std::unexpected(created.error());
    }

    TemporaryPath stagingCleanup(stagingPath);
    ScopedDescriptor stagedDescriptor(*created);
    outputs_.push_back(StagedOutput{
        .destination = destinationPath,
        .staging = stagingPath,
        .descriptor = std::move(stagedDescriptor),
        .overwrite = overwrite,
    });
    stagingCleanup.release();
    return outputs_.size() - 1;
}

int OutputTransaction::descriptor(OutputId output) const
{
    return stagedOutput(output).descriptor.get();
}

const std::filesystem::path & OutputTransaction::stagedPath(OutputId output) const
{
    return stagedOutput(output).staging;
}

const std::filesystem::path & OutputTransaction::destination(OutputId output) const
{
    return stagedOutput(output).destination;
}

std::expected<void, Error> OutputTransaction::sync(OutputId output, ErrorCode code) const
{
    const StagedOutput & staged = stagedOutput(output);
    if (::fsync(staged.descriptor.get()) != 0)
    {
        return std::unexpected(MulCommon::makeError(code, staged.staging, 0, std::strerror(errno)));
    }

    return {};
}

std::expected<void, Error> OutputTransaction::close(OutputId output, ErrorCode code)
{
    StagedOutput & staged = stagedOutput(output);
    if (staged.descriptor.close() != 0)
    {
        return std::unexpected(MulCommon::makeError(code, staged.staging, 0, std::strerror(errno)));
    }

    return {};
}

void OutputTransaction::replaceDescriptor(OutputId output, ScopedDescriptor descriptorValue) noexcept
{
    stagedOutput(output).descriptor = std::move(descriptorValue);
}

std::expected<void, Error> OutputTransaction::publish()
{
    if (!active_)
    {
        return std::unexpected(MulCommon::makeError(ErrorCode::commitFailed, {}, 0, "output transaction is no longer active"));
    }

    for (const StagedOutput & output : outputs_)
    {
        if (output.descriptor.get() >= 0)
        {
            return std::unexpected(
                MulCommon::makeError(ErrorCode::commitFailed, output.staging, 0, "staged output descriptor is still open"));
        }
    }

    if (outputs_.size() == 1)
    {
        auto published =
            MulCommon::publishFile(outputs_.front().staging, outputs_.front().destination, outputs_.front().overwrite);
        if (!published)
        {
            abort();
            return published;
        }
        active_ = false;
        return {};
    }

    std::vector<std::filesystem::path> backups(outputs_.size());
    if (auto prepared = prepareBackups(backups); !prepared)
    {
        const Error & error = prepared.error();
        abort();
        return std::unexpected(error);
    }

    std::vector<bool> published(outputs_.size(), false);
    auto result = publishStagedOutputs(published);
    if (!result)
    {
        const Error & error = result.error();
        const bool restored = rollbackPublished(published, backups);
        const Error rollbackError = MulCommon::makeError(
            ErrorCode::commitFailed,
            error.path,
            error.offset,
            error.message + (restored ? "; rollback succeeded" : "; rollback failed; outputs may be inconsistent"));
        abort();
        return std::unexpected(rollbackError);
    }

    for (const std::filesystem::path & backup : backups)
    {
        removePath(backup);
    }

    auto cleanup = removeStagedLinks();
    active_ = false;
    return cleanup;
}

std::expected<void, Error> OutputTransaction::prepareBackups(std::vector<std::filesystem::path> & backups) const
{
    for (std::size_t index = 0; index < outputs_.size(); index++)
    {
        const StagedOutput & output = outputs_.at(index);
        if (!output.overwrite)
        {
            continue;
        }

        std::error_code filesystemError;
        const bool destinationExists = std::filesystem::exists(output.destination, filesystemError);
        if (filesystemError)
        {
            for (const std::filesystem::path & backup : backups)
            {
                removePath(backup);
            }
            return std::unexpected(
                MulCommon::makeError(ErrorCode::commitFailed, output.destination, 0, filesystemError.message()));
        }

        if (!destinationExists)
        {
            continue;
        }

        auto backup = makeBackup(output.destination);
        if (!backup)
        {
            for (const std::filesystem::path & existingBackup : backups)
            {
                removePath(existingBackup);
            }
            return std::unexpected(backup.error());
        }
        backups.at(index) = std::move(*backup);
    }

    return {};
}

std::expected<void, Error> OutputTransaction::publishStagedOutputs(std::vector<bool> & published) const
{
    for (std::size_t index = 0; index < outputs_.size(); index++)
    {
        const StagedOutput & output = outputs_.at(index);
        const int result = output.overwrite ? ::rename(output.staging.c_str(), output.destination.c_str())
                                            : ::link(output.staging.c_str(), output.destination.c_str());
        if (result != 0)
        {
            return std::unexpected(MulCommon::makeError(ErrorCode::commitFailed, output.destination, 0, std::strerror(errno)));
        }
        published.at(index) = true;
    }

    return {};
}

bool OutputTransaction::rollbackPublished(const std::vector<bool> & published,
                                          std::vector<std::filesystem::path> & backups) const noexcept
{
    bool rollbackSucceeded = true;
    for (std::size_t index = outputs_.size(); index > 0; --index)
    {
        const std::size_t publishedIndex = index - 1;
        if (!published.at(publishedIndex))
        {
            removePath(backups.at(publishedIndex));
            continue;
        }

        const StagedOutput & output = outputs_.at(publishedIndex);
        int result = 0;
        if (backups.at(publishedIndex).empty())
        {
            result = ::unlink(output.destination.c_str());
        }
        else
        {
            result = ::rename(backups.at(publishedIndex).c_str(), output.destination.c_str());
        }
        if (result != 0)
        {
            rollbackSucceeded = false;
        }
        else
        {
            backups.at(publishedIndex).clear();
        }
    }

    return rollbackSucceeded;
}

std::expected<void, Error> OutputTransaction::removeStagedLinks() const
{
    bool cleanupSucceeded = true;
    std::filesystem::path failedPath;
    for (const StagedOutput & output : outputs_)
    {
        if (!output.overwrite && ::unlink(output.staging.c_str()) != 0)
        {
            if (cleanupSucceeded)
            {
                failedPath = output.staging;
            }
            cleanupSucceeded = false;
        }
    }

    if (!cleanupSucceeded)
    {
        return std::unexpected(MulCommon::makeError(
            ErrorCode::commitFailed, failedPath, 0, "outputs published but temporary links could not be removed"));
    }

    return {};
}

void OutputTransaction::abort() noexcept
{
    if (!active_)
    {
        return;
    }

    for (StagedOutput & output : outputs_)
    {
        static_cast<void>(output.descriptor.close());
        removePath(output.staging);
    }
    active_ = false;
}

const OutputTransaction::StagedOutput & OutputTransaction::stagedOutput(OutputId output) const
{
    return outputs_.at(output);
}

OutputTransaction::StagedOutput & OutputTransaction::stagedOutput(OutputId output)
{
    return outputs_.at(output);
}

} // namespace uomul::detail
