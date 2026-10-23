#include "gui_model/output/output_set_transaction.h"
#include "gui_model/profile/data_profile.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/mul_common/output_transaction.h"
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <system_error>
#include <utility>

namespace uo_mule::gui
{

OutputSetTransaction::OutputSetTransaction(std::span<const std::filesystem::path> sources,
                                           OutputCollisionPolicy collisionPolicy) :
    sources_(sources.begin(), sources.end()), collisionPolicy_(collisionPolicy)
{
}

std::expected<OutputSetTransaction::OutputId, OutputSetError>
OutputSetTransaction::addOutput(const std::filesystem::path & destinationPath)
{
    if (destinationPath.empty())
    {
        return std::unexpected(error(OutputSetFailureKind::validation, destinationPath, "output path cannot be empty"));
    }

    std::filesystem::path aliasedSource;
    if (outputAliasesSource(destinationPath, aliasedSource))
    {
        return std::unexpected(
            error(OutputSetFailureKind::validation, destinationPath, "output aliases source " + aliasedSource.string()));
    }
    if (outputAliasesOutput(destinationPath))
    {
        return std::unexpected(error(OutputSetFailureKind::validation, destinationPath, "output paths alias each other"));
    }

    std::error_code filesystemError;
    const bool exists = std::filesystem::exists(destinationPath, filesystemError);
    if (filesystemError)
    {
        return std::unexpected(error(OutputSetFailureKind::validation, destinationPath, filesystemError.message()));
    }
    if (exists && collisionPolicy_ != OutputCollisionPolicy::overwrite)
    {
        return std::unexpected(error(OutputSetFailureKind::collision,
                                     destinationPath,
                                     collisionPolicy_ == OutputCollisionPolicy::confirm
                                         ? "output collision requires confirmation"
                                         : "output already exists"));
    }

    auto output = transaction_.addOutput(destinationPath, collisionPolicy_ == OutputCollisionPolicy::overwrite);
    if (!output)
    {
        return std::unexpected(error(OutputSetFailureKind::validation, output.error().path, output.error().message));
    }
    destinations_.push_back(destinationPath);
    return *output;
}

int OutputSetTransaction::descriptor(OutputId output) const
{
    return transaction_.descriptor(output);
}

const std::filesystem::path & OutputSetTransaction::stagedPath(OutputId output) const
{
    return transaction_.stagedPath(output);
}

const std::filesystem::path & OutputSetTransaction::destination(OutputId output) const
{
    return transaction_.destination(output);
}

std::expected<void, OutputSetError> OutputSetTransaction::sync(OutputId output)
{
    auto result = transaction_.sync(output, uomul::ErrorCode::io);
    if (!result)
    {
        return std::unexpected(error(OutputSetFailureKind::publication, result.error().path, result.error().message));
    }
    return {};
}

std::expected<void, OutputSetError> OutputSetTransaction::close(OutputId output)
{
    auto result = transaction_.close(output, uomul::ErrorCode::io);
    if (!result)
    {
        return std::unexpected(error(OutputSetFailureKind::publication, result.error().path, result.error().message));
    }
    return {};
}

void OutputSetTransaction::replaceDescriptor(OutputId output, uomul::detail::ScopedDescriptor descriptorValue) noexcept
{
    transaction_.replaceDescriptor(output, std::move(descriptorValue));
}

std::expected<void, OutputSetError> OutputSetTransaction::publish()
{
    auto result = transaction_.publish();
    if (!result)
    {
        const bool partial = result.error().message.contains("rollback");
        return std::unexpected(error(partial ? OutputSetFailureKind::partialPublication : OutputSetFailureKind::publication,
                                     result.error().path,
                                     result.error().message));
    }
    return {};
}

void OutputSetTransaction::abort() noexcept
{
    transaction_.abort();
}

OutputSetError OutputSetTransaction::error(OutputSetFailureKind kind, const std::filesystem::path & path, std::string message)
{
    return {
        .error = uomul::detail::MulCommon::makeError(kind == OutputSetFailureKind::collision ? uomul::ErrorCode::io
                                                                                             : uomul::ErrorCode::commitFailed,
                                                     path,
                                                     0,
                                                     std::move(message)),
        .kind = kind,
        .dirtyStateRecoverable = true,
    };
}

bool OutputSetTransaction::outputAliasesSource(const std::filesystem::path & destinationPath,
                                               std::filesystem::path & aliasedSource) const
{
    for (const auto & source : sources_)
    {
        auto aliased = uomul::detail::MulCommon::pathsAlias(source, destinationPath);
        if (aliased && *aliased)
        {
            aliasedSource = source;
            return true;
        }
    }
    return false;
}

bool OutputSetTransaction::outputAliasesOutput(const std::filesystem::path & destinationPath) const
{
    for (const auto & existing : destinations_)
    {
        auto aliased = uomul::detail::MulCommon::pathsAlias(existing, destinationPath);
        if (aliased && *aliased)
        {
            return true;
        }
    }
    return false;
}

} // namespace uo_mule::gui
