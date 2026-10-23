#include "gui_model/output/output_set_transaction.h"
#include "gui_model/profile/data_profile.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <vector>

namespace
{

class TestDirectory
{
public:
    TestDirectory() : path_(std::filesystem::temp_directory_path() / "uo-mule-output-set-tests")
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_, error);
    }

    TestDirectory(const TestDirectory &) = delete;
    TestDirectory & operator=(const TestDirectory &) = delete;
    TestDirectory(TestDirectory &&) = delete;
    TestDirectory & operator=(TestDirectory &&) = delete;

    ~TestDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path & path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

void writeStaged(const uo_mule::gui::OutputSetTransaction & transaction,
                 uo_mule::gui::OutputSetTransaction::OutputId output,
                 const std::string & contents)
{
    std::ofstream file(transaction.stagedPath(output), std::ios::binary);
    file << contents;
}

} // namespace

TEST_CASE("outputSetTransaction_Should_RejectOutputAliasingSource")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    std::ofstream(source) << "source";
    const std::vector<std::filesystem::path> sources{source};
    uo_mule::gui::OutputSetTransaction transaction(sources, uo_mule::gui::OutputCollisionPolicy::overwrite);

    const auto result = transaction.addOutput(source);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().kind == uo_mule::gui::OutputSetFailureKind::validation);
    REQUIRE(result.error().dirtyStateRecoverable);
}

TEST_CASE("outputSetTransaction_Should_ApplyCollisionPolicyBeforeStaging")
{
    const TestDirectory directory;
    const auto existing = directory.path() / "existing.mul";
    std::ofstream(existing) << "existing";
    const std::vector<std::filesystem::path> sources;
    uo_mule::gui::OutputSetTransaction transaction(sources, uo_mule::gui::OutputCollisionPolicy::reject);

    const auto result = transaction.addOutput(existing);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().kind == uo_mule::gui::OutputSetFailureKind::collision);
}

TEST_CASE("outputSetTransaction_Should_PublishStagedOutput")
{
    const TestDirectory directory;
    const auto output = directory.path() / "output.mul";
    const std::vector<std::filesystem::path> sources;
    uo_mule::gui::OutputSetTransaction transaction(sources, uo_mule::gui::OutputCollisionPolicy::reject);
    const auto outputId = transaction.addOutput(output);
    REQUIRE(outputId);

    writeStaged(transaction, *outputId, "published");
    REQUIRE(transaction.sync(*outputId));
    REQUIRE(transaction.close(*outputId));
    REQUIRE(transaction.publish());
    REQUIRE(std::filesystem::exists(output));
    REQUIRE(std::ifstream(output).good());
}

TEST_CASE("outputSetTransaction_Should_RollBackEarlierOutputsAfterPublicationRace")
{
    const TestDirectory directory;
    const auto first = directory.path() / "first.mul";
    const auto second = directory.path() / "second.mul";
    const std::vector<std::filesystem::path> sources;
    uo_mule::gui::OutputSetTransaction transaction(sources, uo_mule::gui::OutputCollisionPolicy::reject);
    const auto firstId = transaction.addOutput(first);
    const auto secondId = transaction.addOutput(second);
    REQUIRE(firstId);
    REQUIRE(secondId);

    writeStaged(transaction, *firstId, "first");
    writeStaged(transaction, *secondId, "second");
    REQUIRE(transaction.close(*firstId));
    REQUIRE(transaction.close(*secondId));
    std::ofstream(second) << "race";

    const auto result = transaction.publish();

    REQUIRE_FALSE(result);
    REQUIRE(result.error().kind == uo_mule::gui::OutputSetFailureKind::partialPublication);
    REQUIRE(result.error().dirtyStateRecoverable);
    REQUIRE_FALSE(std::filesystem::exists(first));
    REQUIRE(std::ifstream(second).good());
}
