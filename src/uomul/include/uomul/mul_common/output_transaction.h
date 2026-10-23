#pragma once

#include "uomul/mul_common/mul_common.h"
#include <cstddef>
#include <expected>
#include <filesystem>
#include <vector>

namespace uomul::detail
{

class OutputTransaction
{
public:
    using OutputId = std::size_t;

    OutputTransaction() = default;
    OutputTransaction(const OutputTransaction &) = delete;
    OutputTransaction & operator=(const OutputTransaction &) = delete;
    OutputTransaction(OutputTransaction &&) = delete;
    OutputTransaction & operator=(OutputTransaction &&) = delete;
    ~OutputTransaction();

    [[nodiscard]] std::expected<OutputId, Error> addOutput(const std::filesystem::path & destination, bool overwrite);
    [[nodiscard]] int descriptor(OutputId output) const;
    [[nodiscard]] const std::filesystem::path & stagedPath(OutputId output) const;
    [[nodiscard]] const std::filesystem::path & destination(OutputId output) const;
    [[nodiscard]] std::expected<void, Error> sync(OutputId output, ErrorCode code) const;
    [[nodiscard]] std::expected<void, Error> close(OutputId output, ErrorCode code);
    void replaceDescriptor(OutputId output, ScopedDescriptor descriptor) noexcept;
    [[nodiscard]] std::expected<void, Error> publish();
    void abort() noexcept;

private:
    struct StagedOutput
    {
        std::filesystem::path destination;
        std::filesystem::path staging;
        ScopedDescriptor descriptor;
        bool overwrite = false;
    };

    [[nodiscard]] const StagedOutput & stagedOutput(OutputId output) const;
    [[nodiscard]] StagedOutput & stagedOutput(OutputId output);
    [[nodiscard]] std::expected<void, Error> prepareBackups(std::vector<std::filesystem::path> & backups) const;
    [[nodiscard]] std::expected<void, Error> publishStagedOutputs(std::vector<bool> & published) const;
    [[nodiscard]] bool rollbackPublished(const std::vector<bool> & published,
                                         std::vector<std::filesystem::path> & backups) const noexcept;
    [[nodiscard]] std::expected<void, Error> removeStagedLinks() const;

    std::vector<StagedOutput> outputs_;
    bool active_ = true;
};

} // namespace uomul::detail
