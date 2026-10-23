#pragma once

#include "gui_model/profile/data_profile.h"
#include "uomul/mul_common/mul_common.h"
#include "uomul/mul_common/output_transaction.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <vector>

namespace uo_mule::gui
{

enum class OutputSetFailureKind : std::uint8_t
{
    validation,
    collision,
    publication,
    partialPublication,
};

struct OutputSetError
{
    uomul::Error error;
    OutputSetFailureKind kind = OutputSetFailureKind::validation;
    bool dirtyStateRecoverable = true;
};

class OutputSetTransaction
{
public:
    using OutputId = uomul::detail::OutputTransaction::OutputId;

    OutputSetTransaction(std::span<const std::filesystem::path> sources, OutputCollisionPolicy collisionPolicy);
    OutputSetTransaction(const OutputSetTransaction &) = delete;
    OutputSetTransaction & operator=(const OutputSetTransaction &) = delete;
    OutputSetTransaction(OutputSetTransaction &&) = delete;
    OutputSetTransaction & operator=(OutputSetTransaction &&) = delete;
    ~OutputSetTransaction() = default;

    [[nodiscard]] std::expected<OutputId, OutputSetError> addOutput(const std::filesystem::path & destination);
    [[nodiscard]] int descriptor(OutputId output) const;
    [[nodiscard]] const std::filesystem::path & stagedPath(OutputId output) const;
    [[nodiscard]] const std::filesystem::path & destination(OutputId output) const;
    [[nodiscard]] std::expected<void, OutputSetError> sync(OutputId output);
    [[nodiscard]] std::expected<void, OutputSetError> close(OutputId output);
    void replaceDescriptor(OutputId output, uomul::detail::ScopedDescriptor descriptor) noexcept;
    [[nodiscard]] std::expected<void, OutputSetError> publish();
    void abort() noexcept;

private:
    [[nodiscard]] OutputSetError static error(OutputSetFailureKind kind,
                                              const std::filesystem::path & path,
                                              std::string message);
    [[nodiscard]] bool outputAliasesSource(const std::filesystem::path & destination,
                                           std::filesystem::path & aliasedSource) const;
    [[nodiscard]] bool outputAliasesOutput(const std::filesystem::path & destination) const;

    std::vector<std::filesystem::path> sources_;
    OutputCollisionPolicy collisionPolicy_ = OutputCollisionPolicy::confirm;
    uomul::detail::OutputTransaction transaction_;
    std::vector<std::filesystem::path> destinations_;
};

} // namespace uo_mule::gui
