#include "gui_model/profile/data_profile.h"
#include "gui_model/source/source_validation.h"
#include "uomul/radarcol/radarcol.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <ios>
#include <vector>

namespace
{

class TemporaryDirectory
{
public:
    TemporaryDirectory() : path_(std::filesystem::temp_directory_path() / "uo-mule-source-validation-tests")
    {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }
    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory & operator=(const TemporaryDirectory &) = delete;
    TemporaryDirectory(TemporaryDirectory &&) = delete;
    TemporaryDirectory & operator=(TemporaryDirectory &&) = delete;
    ~TemporaryDirectory() { std::filesystem::remove_all(path_); }
    [[nodiscard]] const std::filesystem::path & path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

void createRadarcol(const std::filesystem::path & path)
{
    std::ofstream output(path, std::ios::binary);
    output.seekp(static_cast<std::streamoff>(uomul::radarColorCount * sizeof(uomul::RadarColor)) - 1);
    output.put('\0');
}

const uo_mule::gui::SourceValidationResult * findResult(const std::vector<uo_mule::gui::SourceValidationResult> & results,
                                                        uo_mule::gui::DataFileRole role)
{
    for (const auto & result : results)
    {
        if (result.role == role)
        {
            return &result;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("sourceValidation_Should_ReportMissingRole")
{
    const uo_mule::gui::DataProfile profile;
    const auto results = uo_mule::gui::validateDataProfileSources(profile);
    const auto * radarcol = findResult(results, uo_mule::gui::DataFileRole::radarcol);
    REQUIRE(radarcol != nullptr);
    REQUIRE(radarcol->status == uo_mule::gui::SourceValidationStatus::missing);
}

TEST_CASE("sourceValidation_Should_ReportValidRadarcolWithCanonicalPathAndRecordCount")
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "radarcol.mul";
    createRadarcol(path);
    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = directory.path();
    profile.files[uo_mule::gui::DataFileRole::radarcol] = {.path = path.filename()};

    const auto results = uo_mule::gui::validateDataProfileSources(profile);
    const auto * radarcol = findResult(results, uo_mule::gui::DataFileRole::radarcol);
    REQUIRE(radarcol != nullptr);
    if (radarcol != nullptr)
    {
        REQUIRE(radarcol->status == uo_mule::gui::SourceValidationStatus::valid);
        REQUIRE(radarcol->canonicalPath == std::filesystem::weakly_canonical(path));
        REQUIRE(radarcol->recordCount == uomul::radarColorCount);
    }
}

TEST_CASE("sourceValidation_Should_ReportIncompleteTexturePairByMissingRole")
{
    const TemporaryDirectory directory;
    std::ofstream(directory.path() / "texidx.mul").close();
    uo_mule::gui::DataProfile profile;
    profile.sourceRoot = directory.path();
    profile.files[uo_mule::gui::DataFileRole::texidx] = {.path = "texidx.mul"};

    const auto results = uo_mule::gui::validateDataProfileSources(profile);
    const auto * data = findResult(results, uo_mule::gui::DataFileRole::texmaps);
    REQUIRE(data != nullptr);
    if (data != nullptr)
    {
        REQUIRE(data->status == uo_mule::gui::SourceValidationStatus::incompletePair);
        REQUIRE(data->diagnostic.find("texmaps") != std::string::npos);
    }
}
