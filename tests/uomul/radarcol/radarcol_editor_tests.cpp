#include "../map_test_helpers.h"
#include "uomul/radarcol/radarcol.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <signal.h>
#include <sys/resource.h>
#include <utility>

namespace
{

using uomul::test::TestDirectory;

void writeColors(const std::filesystem::path & path, std::uint32_t count)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const auto value = static_cast<std::uint16_t>((index * 17U) & 0x7FFFU);
        output.put(static_cast<char>(value & 0xFF));
        output.put(static_cast<char>(value >> 8));
    }
}

} // namespace

TEST_CASE("RadarColEditor_create_Should_StageSourceTable_If_SourceIsValid")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "edited.mul";
    writeColors(source, uomul::radarColorCount);

    auto editor = uomul::RadarColEditor::create(source, output);

    REQUIRE(editor);
    REQUIRE_FALSE(std::filesystem::exists(output));
    REQUIRE(editor->commit());
    const auto reader = uomul::RadarColReader::open(output);
    REQUIRE(reader);
    REQUIRE(reader->colorCount() == uomul::radarColorCount);
}

TEST_CASE("RadarColEditor_editColors_Should_ReplaceMapStaticAndRawEntries_If_SourceIsFull")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "edited.mul";
    writeColors(source, uomul::radarColorCount);

    auto editor = uomul::RadarColEditor::create(source, output);
    REQUIRE(editor);
    REQUIRE(editor->editMapColor(2, 0x1234));
    REQUIRE(editor->editStaticColor(3, 0x4567));
    REQUIRE(editor->editRawColor(10, 0x789A));
    REQUIRE(editor->commit());

    const auto reader = uomul::RadarColReader::open(output);
    REQUIRE(reader);
    REQUIRE(reader->colorCount() == uomul::radarColorCount);
    REQUIRE(reader->readMapColor(2) == 0x1234);
    REQUIRE(reader->readStaticColor(3) == 0x4567);
    REQUIRE(reader->readRawColor(10) == 0x789A);
}

TEST_CASE("RadarColEditor_commit_Should_ExtendPrefixAndZeroFillGap_If_HigherIndexIsEdited")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "edited.mul";
    writeColors(source, 8);

    auto editor = uomul::RadarColEditor::create(source, output);
    REQUIRE(editor);
    REQUIRE(editor->editMapColor(2, 0x1234));
    REQUIRE(editor->editStaticColor(3, 0x4567));
    REQUIRE(editor->editRawColor(7, 0x789A));
    REQUIRE(editor->editRawColor(uomul::radarStaticColorOffset + 2, 0xABCD));
    REQUIRE(editor->commit());

    const auto reader = uomul::RadarColReader::open(output);
    REQUIRE(reader);
    REQUIRE(reader->colorCount() == uomul::radarStaticColorOffset + 4);
    REQUIRE(reader->readMapColor(2) == 0x1234);
    REQUIRE(reader->readStaticColor(2) == 0xABCD);
    REQUIRE(reader->readStaticColor(3) == 0x4567);
    REQUIRE(reader->readRawColor(7) == 0x789A);
    REQUIRE(reader->readRawColor(8) == 0);
}

TEST_CASE("RadarColEditor_createEmpty_Should_InitializeFullZeroTable_If_OutputIsValid")
{
    const TestDirectory directory;
    const auto output = directory.path() / "empty.mul";

    auto editor = uomul::RadarColEditor::createEmpty(output);

    REQUIRE(editor);
    REQUIRE(editor->editRawColor(uomul::radarColorCount - 1, 0x4321));
    REQUIRE(editor->commit());
    const auto reader = uomul::RadarColReader::open(output);
    REQUIRE(reader);
    REQUIRE(reader->colorCount() == uomul::radarColorCount);
    REQUIRE(reader->readRawColor(0) == 0);
    REQUIRE(reader->readRawColor(uomul::radarColorCount - 1) == 0x4321);
}

TEST_CASE("RadarColEditor_createEmpty_Should_RejectFileSizeLimit_If_StagingFileCannotBeResized")
{
    const TestDirectory directory;
    const auto output = directory.path() / "output.mul";
    struct rlimit limit{};
    REQUIRE(::getrlimit(RLIMIT_FSIZE, &limit) == 0);
    const auto originalLimit = limit;
    const auto originalHandler = ::signal(SIGXFSZ, SIG_IGN);
    limit.rlim_cur = 0;
    REQUIRE(::setrlimit(RLIMIT_FSIZE, &limit) == 0);

    const auto editor = uomul::RadarColEditor::createEmpty(output);

    limit = originalLimit;
    REQUIRE(::setrlimit(RLIMIT_FSIZE, &limit) == 0);
    REQUIRE(::signal(SIGXFSZ, originalHandler) != SIG_ERR);
    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
    REQUIRE_FALSE(std::filesystem::exists(output));
}

TEST_CASE("RadarColEditor_editRawColor_Should_FailAndDeactivate_If_FileSizeLimitBlocksWrite")
{
    const TestDirectory directory;
    const auto output = directory.path() / "output.mul";
    auto editor = uomul::RadarColEditor::createEmpty(output);
    REQUIRE(editor);
    struct rlimit limit{};
    REQUIRE(::getrlimit(RLIMIT_FSIZE, &limit) == 0);
    const auto originalLimit = limit;
    const auto originalHandler = ::signal(SIGXFSZ, SIG_IGN);
    limit.rlim_cur = 0;
    REQUIRE(::setrlimit(RLIMIT_FSIZE, &limit) == 0);

    const auto edit = editor->editRawColor(0, 1);

    limit = originalLimit;
    REQUIRE(::setrlimit(RLIMIT_FSIZE, &limit) == 0);
    REQUIRE(::signal(SIGXFSZ, originalHandler) != SIG_ERR);
    const auto commit = editor->commit();
    REQUIRE_FALSE(edit);
    REQUIRE(edit.error().code == uomul::ErrorCode::io);
    REQUIRE_FALSE(commit);
    REQUIRE(commit.error().code == uomul::ErrorCode::commitFailed);
}

TEST_CASE("RadarColEditor_commit_Should_FailAndCleanStagingFile_If_OutputAppearsDuringEdit")
{
    const TestDirectory directory;
    const auto output = directory.path() / "output.mul";
    auto editor = uomul::RadarColEditor::createEmpty(output);
    REQUIRE(editor);
    {
        std::ofstream existing(output, std::ios::binary);
        existing.put('x');
    }

    const auto commit = editor->commit();

    REQUIRE_FALSE(commit);
    REQUIRE(commit.error().code == uomul::ErrorCode::commitFailed);
    REQUIRE(std::filesystem::file_size(output) == 1);
    std::uint32_t stagingFiles = 0;
    for (const auto & entry : std::filesystem::directory_iterator(directory.path()))
    {
        if (entry.path().filename().string().starts_with("output.mul.uomul-tmp-"))
        {
            ++stagingFiles;
        }
    }
    REQUIRE(stagingFiles == 0);
}

TEST_CASE("RadarColEditor_abort_Should_DiscardStagedChanges_If_EditorIsAborted")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeColors(source, 4);
    {
        auto editor = uomul::RadarColEditor::create(source, output);
        REQUIRE(editor);
        REQUIRE(editor->editRawColor(0, 0x1234));
        editor->abort();
        editor->abort();
        REQUIRE_FALSE(editor->editRawColor(1, 0x4321));
        REQUIRE_FALSE(editor->commit());
    }

    REQUIRE_FALSE(std::filesystem::exists(output));
}

TEST_CASE("RadarColEditor_editRawColor_Should_ReturnInactiveError_If_EditorWasMovedFrom")
{
    const TestDirectory directory;
    const auto output = directory.path() / "empty.mul";
    auto opened = uomul::RadarColEditor::createEmpty(output);
    REQUIRE(opened);
    uomul::RadarColEditor editor(std::move(*opened));

    const auto result = opened->editRawColor(0, 1);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::commitFailed);
}

TEST_CASE("RadarColEditor_create_Should_RejectOutputWithEmptyFilename_If_PathNamesDirectory")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    writeColors(source, 2);

    const auto editor = uomul::RadarColEditor::create(source, directory.path().string() + "/");

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("RadarColEditor_create_Should_RejectInvalidPathsAndMalformedSource_If_SourceCannotBeStaged")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    const auto malformed = directory.path() / "malformed.mul";
    writeColors(source, 2);
    {
        std::ofstream outputFile(malformed, std::ios::binary);
        outputFile.put('x');
    }

    REQUIRE_FALSE(uomul::RadarColEditor::create(source, source, {.overwriteOutput = true}));
    REQUIRE_FALSE(uomul::RadarColEditor::create(directory.path() / "missing.mul", output));
    REQUIRE_FALSE(uomul::RadarColEditor::create(malformed, output));
    REQUIRE_FALSE(uomul::RadarColEditor::create(source, directory.path() / "missing" / "output.mul"));
    REQUIRE_FALSE(uomul::RadarColEditor::createEmpty(directory.path() / "missing" / "output.mul"));
    auto directorySource = uomul::RadarColEditor::create(directory.path(), output);
    REQUIRE_FALSE(directorySource);
    REQUIRE(directorySource.error().code == uomul::ErrorCode::io);
}

TEST_CASE("RadarColEditor_create_Should_RejectEmptySourcePath_If_SourceDoesNotNameFile")
{
    const TestDirectory directory;
    const auto output = directory.path() / "output.mul";

    const auto editor = uomul::RadarColEditor::create({}, output);

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("RadarColEditor_create_Should_RejectEmptyOutputPath_If_OutputDoesNotNameFile")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    writeColors(source, 2);

    const auto editor = uomul::RadarColEditor::create(source, {});

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("RadarColEditor_createEmpty_Should_RejectEmptyOutputPath_If_OutputDoesNotNameFile")
{
    const auto editor = uomul::RadarColEditor::createEmpty({});

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("RadarColEditor_create_Should_RejectEquivalentPaths_If_OutputIsHardLinkToSource")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "alias.mul";
    writeColors(source, 2);
    std::filesystem::create_hard_link(source, output);

    const auto editor = uomul::RadarColEditor::create(source, output, {.overwriteOutput = true});

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("RadarColEditor_createEmpty_Should_RejectDirectoryOutput_If_OverwriteIsEnabled")
{
    const TestDirectory directory;

    const auto editor = uomul::RadarColEditor::createEmpty(directory.path(), {.overwriteOutput = true});

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("RadarColEditor_create_Should_RejectExistingOutput_If_OverwriteIsDisabled")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeColors(source, 2);
    {
        std::ofstream existing(output, std::ios::binary);
        existing.put('x');
    }

    const auto editor = uomul::RadarColEditor::create(source, output);

    REQUIRE_FALSE(editor);
    REQUIRE(editor.error().code == uomul::ErrorCode::io);
}

TEST_CASE("RadarColEditor_commit_Should_ReplaceExistingOutput_If_OverwriteIsEnabled")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeColors(source, 4);
    {
        std::ofstream existing(output, std::ios::binary);
        existing.put('x');
    }

    auto editor = uomul::RadarColEditor::create(source, output, {.overwriteOutput = true});
    REQUIRE(editor);
    REQUIRE(editor->editRawColor(0, 0x1234));
    REQUIRE(editor->commit());
    REQUIRE_FALSE(editor->commit());
    REQUIRE_FALSE(editor->editRawColor(1, 0x4321));
    const auto reader = uomul::RadarColReader::open(output);
    REQUIRE(reader);
    REQUIRE(reader->readRawColor(0) == 0x1234);
}

TEST_CASE("RadarColEditor_editMapColor_Should_RejectInvalidId_If_IdExceedsMapColorCount")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeColors(source, 2);
    auto editor = uomul::RadarColEditor::create(source, output);
    REQUIRE(editor);

    const auto result = editor->editMapColor(uomul::radarMapColorCount, 1);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("RadarColEditor_editStaticColor_Should_RejectInvalidId_If_IdExceedsMapColorCount")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeColors(source, 2);
    auto editor = uomul::RadarColEditor::create(source, output);
    REQUIRE(editor);

    const auto result = editor->editStaticColor(uomul::radarMapColorCount, 1);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("RadarColEditor_editRawColor_Should_RejectInvalidIndex_If_IndexExceedsTableLimit")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeColors(source, 2);
    auto editor = uomul::RadarColEditor::create(source, output);
    REQUIRE(editor);

    const auto result = editor->editRawColor(uomul::radarColorCount, 1);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == uomul::ErrorCode::invalidCoordinate);
}

TEST_CASE("RadarColEditor_editRawColor_Should_ExtendPrefix_If_IndexIsAvailable")
{
    const TestDirectory directory;
    const auto source = directory.path() / "source.mul";
    const auto output = directory.path() / "output.mul";
    writeColors(source, 2);
    auto editor = uomul::RadarColEditor::create(source, output);
    REQUIRE(editor);

    REQUIRE(editor->editMapColor(1, 1));
    REQUIRE(editor->editStaticColor(0, 1));
    REQUIRE(editor->editRawColor(2, 1));
    editor->abort();
}

TEST_CASE("RadarColEditor_moveAssignment_Should_ReplaceEditorState_If_AnotherEditorIsMovedIntoIt")
{
    const TestDirectory directory;
    const auto firstOutput = directory.path() / "first.mul";
    const auto secondOutput = directory.path() / "second.mul";
    auto first = uomul::RadarColEditor::createEmpty(firstOutput);
    auto second = uomul::RadarColEditor::createEmpty(secondOutput);
    REQUIRE(first);
    REQUIRE(second);

    *first = std::move(*second);

    REQUIRE(first->editRawColor(0, 0x1111));
    REQUIRE(first->commit());
    REQUIRE_FALSE(std::filesystem::exists(firstOutput));
    REQUIRE(std::filesystem::exists(secondOutput));
}

TEST_CASE("RadarColEditor_abort_Should_SafelyHandleMovedFromEditor_If_EditorWasMoved")
{
    const TestDirectory directory;
    auto opened = uomul::RadarColEditor::createEmpty(directory.path() / "output.mul");
    REQUIRE(opened);
    uomul::RadarColEditor editor(std::move(*opened));

    opened->abort();
    editor.abort();

    REQUIRE_FALSE(std::filesystem::exists(directory.path() / "output.mul"));
}
