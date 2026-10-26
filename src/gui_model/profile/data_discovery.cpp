#include "gui_model/profile/data_discovery.h"
#include "gui_model/profile/data_profile.h"
#include "uomul/radarcol/radarcol.h"
#include "uomul/texmaps/texmaps.h"
#include "uomul/tiledata/tiledata.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace uo_mule::gui
{

namespace
{

constexpr int exactFilenameScore = 100;
constexpr int caseInsensitiveFilenameScore = 90;
constexpr int knownLayoutScore = 20;
constexpr int directoryDepthScore = 5;
constexpr int validCandidateScore = 10;

[[nodiscard]] std::string lowerCase(std::string value)
{
    std::ranges::transform(
        value, value.begin(), [](const unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

[[nodiscard]] bool isKnownLayoutDirectory(std::string_view name) noexcept
{
    const auto lowerName = lowerCase(std::string(name));
    return lowerName == "data" || lowerName == "client" || lowerName == "mul" || lowerName == "art";
}

[[nodiscard]] bool isSupportedRole(DataFileRole role) noexcept
{
    static_cast<void>(role);
    return true;
}

[[nodiscard]] std::filesystem::path normalizedRoot(const std::filesystem::path & root, std::vector<Diagnostic> & diagnostics)
{
    std::error_code pathError;
    const auto absolute = std::filesystem::absolute(root, pathError);

    if (pathError)
    {
        diagnostics.push_back(Diagnostic{
            .severity = Diagnostic::Severity::error,
            .role = std::nullopt,
            .path = root,
            .message = pathError.message(),
            .fixSuggestions = {"Choose an accessible directory."},
        });
        return root.lexically_normal();
    }

    return absolute.lexically_normal();
}

void addDiagnostic(DataDiscoveryResult & result,
                   Diagnostic::Severity severity,
                   const std::filesystem::path & path,
                   std::string message,
                   std::optional<DataFileRole> role = std::nullopt)
{
    result.diagnostics.push_back(Diagnostic{
        .severity = severity,
        .role = role,
        .path = path,
        .message = std::move(message),
        .fixSuggestions = {},
    });
}

[[nodiscard]] std::vector<std::filesystem::path> installationDirectories()
{
    return {
        "Data",
        "Client",
        "MUL",
        "art",
        std::filesystem::path{"Client"} / "Data",
        std::filesystem::path{"Client"} / "MUL",
        std::filesystem::path{"Data"} / "MUL",
        std::filesystem::path{"Data"} / "Client",
        std::filesystem::path{"Client"} / "art",
    };
}

[[nodiscard]] std::vector<std::filesystem::path> folderChildDirectories(const std::filesystem::path & root,
                                                                        DataDiscoveryResult & result)
{
    std::vector<std::filesystem::path> relativeDirectories;
    std::error_code iteratorError;
    for (const auto & entry : std::filesystem::directory_iterator(root, iteratorError))
    {
        if (entry.is_directory(iteratorError) && isKnownLayoutDirectory(entry.path().filename().string()))
        {
            relativeDirectories.push_back(entry.path().filename());
        }
    }

    if (iteratorError)
    {
        addDiagnostic(result, Diagnostic::Severity::error, root, iteratorError.message());
    }

    return relativeDirectories;
}

void appendExistingDirectories(const std::filesystem::path & root,
                               const std::vector<std::filesystem::path> & relativeDirectories,
                               std::vector<std::filesystem::path> & directories,
                               DataDiscoveryResult & result)
{
    for (const auto & relativeDirectory : relativeDirectories)
    {
        const auto directory = root / relativeDirectory;
        std::error_code directoryError;
        if (std::filesystem::is_directory(directory, directoryError))
        {
            directories.push_back(directory);
        }
        else if (directoryError && directoryError != std::errc::no_such_file_or_directory)
        {
            addDiagnostic(result, Diagnostic::Severity::warning, directory, directoryError.message());
        }
    }
}

void normalizeDirectories(std::vector<std::filesystem::path> & directories)
{
    std::ranges::sort(directories);
    directories.erase(std::ranges::unique(directories).begin(), directories.end());
}

[[nodiscard]] std::vector<std::filesystem::path>
scanDirectories(const std::filesystem::path & root, DiscoveryMode mode, DataDiscoveryResult & result)
{
    std::vector<std::filesystem::path> directories;
    directories.push_back(root);

    if (mode == DiscoveryMode::deep)
    {
        addDiagnostic(result,
                      Diagnostic::Severity::warning,
                      root,
                      "deep discovery is not available until cancellable scanning is implemented");
        return directories;
    }

    const auto relativeDirectories =
        mode == DiscoveryMode::folder ? folderChildDirectories(root, result) : installationDirectories();
    appendExistingDirectories(root, relativeDirectories, directories, result);
    normalizeDirectories(directories);
    return directories;
}

struct CandidateNameMatch
{
    std::string expected;
    std::string actual;
    bool exact = false;
};

[[nodiscard]] bool isRegularFile(const std::filesystem::directory_entry & entry)
{
    std::error_code fileError;
    return entry.is_regular_file(fileError) && !fileError;
}

[[nodiscard]] std::optional<CandidateNameMatch> matchCandidateName(DataFileRole role,
                                                                   const std::filesystem::directory_entry & entry)
{
    CandidateNameMatch match{
        .expected = std::string(dataFileRoleName(role)) + ".mul",
        .actual = entry.path().filename().string(),
        .exact = false,
    };

    if (lowerCase(match.actual) != lowerCase(match.expected))
    {
        return std::nullopt;
    }

    match.exact = match.actual == match.expected;
    return match;
}

[[nodiscard]] std::size_t candidateDirectoryDepth(const std::filesystem::path & root, const std::filesystem::path & directory)
{
    std::error_code relativeError;
    const auto relativeDirectory = std::filesystem::relative(directory, root, relativeError);
    if (relativeError)
    {
        return directory == root ? 0U : 1U;
    }

    return static_cast<std::size_t>(std::distance(relativeDirectory.begin(), relativeDirectory.end()));
}

[[nodiscard]] int candidateScore(const CandidateNameMatch & nameMatch, bool knownLayout, std::size_t directoryDepth)
{
    return (nameMatch.exact ? exactFilenameScore : caseInsensitiveFilenameScore) + (knownLayout ? knownLayoutScore : 0) -
           static_cast<int>(directoryDepth * directoryDepthScore);
}

[[nodiscard]] DiscoveryCandidate makeUnvalidatedCandidate(const std::filesystem::path & root,
                                                          const std::filesystem::path & directory,
                                                          const std::filesystem::directory_entry & entry,
                                                          const CandidateNameMatch & nameMatch)
{
    const auto directoryDepth = candidateDirectoryDepth(root, directory);
    const bool knownLayout = directory != root;
    return DiscoveryCandidate{
        .path = entry.path(),
        .score = candidateScore(nameMatch, knownLayout, directoryDepth),
        .directoryDepth = directoryDepth,
        .exactFilename = nameMatch.exact,
        .knownLayout = knownLayout,
        .valid = true,
        .validationMessage = {},
    };
}

void validateCandidate(DataFileRole role, bool validate, DiscoveryCandidate & candidate)
{
    if (validate && role == DataFileRole::radarcol)
    {
        const auto reader = uomul::RadarColReader::open(candidate.path);
        if (!reader)
        {
            candidate.valid = false;
            candidate.validationMessage = reader.error().message;
        }
    }

    if (validate && role == DataFileRole::tiledata)
    {
        const auto extended = uomul::TiledataReader::open(candidate.path, uomul::TiledataLayout::extended64);
        const auto legacy = uomul::TiledataReader::open(candidate.path, uomul::TiledataLayout::legacy32);
        if (!extended && !legacy)
        {
            candidate.valid = false;
            candidate.validationMessage = "Tiledata size does not match a supported layout";
        }
    }

    if (candidate.valid)
    {
        candidate.score += validCandidateScore;
    }
}

[[nodiscard]] std::optional<DiscoveryCandidate> makeCandidate(DataFileRole role,
                                                              const std::filesystem::path & root,
                                                              const std::filesystem::path & directory,
                                                              const std::filesystem::directory_entry & entry,
                                                              bool validate)
{
    if (!isRegularFile(entry))
    {
        return std::nullopt;
    }

    const auto nameMatch = matchCandidateName(role, entry);
    if (!nameMatch)
    {
        return std::nullopt;
    }

    auto candidate = makeUnvalidatedCandidate(root, directory, entry, *nameMatch);
    validateCandidate(role, validate, candidate);
    return candidate;
}

[[nodiscard]] std::vector<DiscoveryCandidate>
collectCandidatesFromDirectories(const std::filesystem::path & root,
                                 const std::vector<std::filesystem::path> & directories,
                                 bool validate,
                                 DataFileRole role,
                                 DataDiscoveryResult & result)
{
    std::vector<DiscoveryCandidate> candidates;
    for (const auto & directory : directories)
    {
        std::error_code iteratorError;
        for (const auto & entry : std::filesystem::directory_iterator(directory, iteratorError))
        {
            if (const auto candidate = makeCandidate(role, root, directory, entry, validate))
            {
                candidates.push_back(*candidate);
            }
        }

        if (iteratorError)
        {
            addDiagnostic(result, Diagnostic::Severity::warning, directory, iteratorError.message(), role);
        }
    }

    std::ranges::sort(candidates,
                      [](const DiscoveryCandidate & left, const DiscoveryCandidate & right) { return left.path < right.path; });
    return candidates;
}

struct CandidateSelection
{
    std::optional<std::size_t> index;
    bool ambiguous = false;
};

[[nodiscard]] CandidateSelection selectBestCandidate(const std::vector<DiscoveryCandidate> & candidates)
{
    if (candidates.empty())
    {
        return {};
    }

    const auto best = std::ranges::max_element(
        candidates, [](const DiscoveryCandidate & left, const DiscoveryCandidate & right) { return left.score < right.score; });
    const auto bestScore = best->score;
    const auto equivalentBest = std::ranges::count_if(
        candidates, [bestScore](const DiscoveryCandidate & candidate) { return candidate.score == bestScore; });
    if (equivalentBest != 1)
    {
        return {.index = std::nullopt, .ambiguous = true};
    }

    return {
        .index = static_cast<std::size_t>(std::distance(candidates.begin(), best)),
        .ambiguous = false,
    };
}

void reportAmbiguousCandidates(const std::filesystem::path & root, DataFileRole role, DataDiscoveryResult & result)
{
    addDiagnostic(result,
                  Diagnostic::Severity::warning,
                  root,
                  "multiple candidates remain equivalent; explicit selection is required",
                  role);
}

void applyCandidateStatus(DataFileRole role,
                          const DiscoveryCandidate & candidate,
                          DiscoveredDataFile & discovered,
                          DataDiscoveryResult & result)
{
    if (!candidate.valid)
    {
        discovered.status = DataFileStatus::invalid;
        addDiagnostic(result, Diagnostic::Severity::warning, candidate.path, candidate.validationMessage, role);
    }
    else if (isSupportedRole(role))
    {
        discovered.status = DataFileStatus::found;
    }
    else
    {
        discovered.status = DataFileStatus::unsupported;
    }
}

void resolveCandidateSelection(const std::filesystem::path & root,
                               DataFileRole role,
                               DiscoveredDataFile & discovered,
                               DataDiscoveryResult & result)
{
    const auto selection = selectBestCandidate(discovered.candidates);
    if (selection.ambiguous)
    {
        discovered.status = DataFileStatus::ambiguous;
        reportAmbiguousCandidates(root, role, result);
        return;
    }

    if (!selection.index)
    {
        discovered.status = DataFileStatus::missing;
        return;
    }

    discovered.selectedCandidate = selection.index;
    applyCandidateStatus(role, discovered.candidates.at(*selection.index), discovered, result);
}

void collectRoleCandidates(const std::filesystem::path & root,
                           const std::vector<std::filesystem::path> & directories,
                           bool validate,
                           DataFileRole role,
                           DataDiscoveryResult & result)
{
    DiscoveredDataFile discovered{
        .role = role,
        .status = DataFileStatus::missing,
        .candidates = collectCandidatesFromDirectories(root, directories, validate, role, result),
        .selectedCandidate = std::nullopt,
    };
    resolveCandidateSelection(root, role, discovered, result);
    result.files.push_back(std::move(discovered));
}

void collectCandidates(const std::filesystem::path & root,
                       const std::vector<std::filesystem::path> & directories,
                       bool validate,
                       DataDiscoveryResult & result)
{
    for (const auto role : knownDataFileRoles())
    {
        collectRoleCandidates(root, directories, validate, role, result);
    }
}

void validateTexmapsSet(DataDiscoveryResult & result)
{
    auto * index = result.find(DataFileRole::texidx);
    auto * data = result.find(DataFileRole::texmaps);
    if (index == nullptr || data == nullptr || index->candidates.empty() || data->candidates.empty())
    {
        return;
    }

    for (auto & indexCandidate : index->candidates)
    {
        for (auto & dataCandidate : data->candidates)
        {
            const auto reader = uomul::TexMapsReader::open(indexCandidate.path, dataCandidate.path);
            if (reader)
            {
                indexCandidate.valid = true;
                dataCandidate.valid = true;
            }
        }
    }
}

[[nodiscard]] std::filesystem::path profilePathForCandidate(const std::filesystem::path & root,
                                                            const DiscoveryCandidate & candidate)
{
    std::error_code relativeError;
    const auto relative = std::filesystem::relative(candidate.path, root, relativeError);
    return relativeError ? candidate.path : relative;
}

[[nodiscard]] bool isSupportedDiscoveryMode(DiscoveryMode mode) noexcept
{
    return mode == DiscoveryMode::folder || mode == DiscoveryMode::installation || mode == DiscoveryMode::deep;
}

[[nodiscard]] bool validateDiscoveryRoot(const std::filesystem::path & root, DataDiscoveryResult & result)
{
    std::error_code rootError;
    if (std::filesystem::is_directory(root, rootError))
    {
        return true;
    }

    addDiagnostic(
        result, Diagnostic::Severity::error, root, rootError ? rootError.message() : "discovery root is not a directory");
    return false;
}

} // namespace

DiscoveredDataFile * DataDiscoveryResult::find(DataFileRole role) noexcept
{
    for (auto & file : files)
    {
        if (file.role == role)
        {
            return &file;
        }
    }
    return nullptr;
}

const DiscoveredDataFile * DataDiscoveryResult::find(DataFileRole role) const noexcept
{
    for (const auto & file : files)
    {
        if (file.role == role)
        {
            return &file;
        }
    }
    return nullptr;
}

DataDiscoveryResult discoverDataFiles(const std::filesystem::path & root, DiscoveryMode mode, DataDiscoveryOptions options)
{
    DataDiscoveryResult result;
    result.sourceRoot = normalizedRoot(root, result.diagnostics);
    result.mode = mode;
    if (!isSupportedDiscoveryMode(mode))
    {
        addDiagnostic(result, Diagnostic::Severity::error, result.sourceRoot, "unsupported discovery mode");
        return result;
    }

    if (!validateDiscoveryRoot(result.sourceRoot, result))
    {
        return result;
    }

    const auto directories = scanDirectories(result.sourceRoot, mode, result);
    collectCandidates(result.sourceRoot, directories, options.validateCandidates, result);
    validateTexmapsSet(result);
    return result;
}

void applyDiscovery(DataProfile & profile, const DataDiscoveryResult & result)
{
    profile.sourceRoot = result.sourceRoot;
    profile.discoveryMode = result.mode;
    for (const auto & discovered : result.files)
    {
        const auto existing = profile.files.find(discovered.role);
        if (existing != profile.files.end() && existing->second.userOverride)
        {
            continue;
        }

        if (discovered.selectedCandidate && discovered.status != DataFileStatus::ambiguous)
        {
            const auto & candidate = discovered.candidates.at(*discovered.selectedCandidate);
            profile.files[discovered.role] = DataFileEntry{
                .path = profilePathForCandidate(result.sourceRoot, candidate),
                .userOverride = false,
            };
        }
        else
        {
            profile.files.erase(discovered.role);
        }
    }
}

} // namespace uo_mule::gui
