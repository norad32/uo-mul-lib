#pragma once

#include <cstdint>
#include <string_view>

namespace uo_mule::gui
{

// The initial workbench document kinds. A Texmap Pair is one logical document.
enum class DocumentKind : std::uint8_t
{
    map,
    radarcol,
    texmapPair,
};

// Metadata documents are either independent user documents or profile sources.
enum class DocumentSourceKind : std::uint8_t
{
    standalone,
    configuredMetadata,
};

enum class DocumentAccess : std::uint8_t
{
    editable,
    readOnly,
};

enum class RadarcolSourcePolicy : std::uint8_t
{
    embeddedDefault,
    externalConfigured,
    embeddedDefaultWithWarning,
};

enum class UnsupportedMulPolicy : std::uint8_t
{
    rejectWithDiagnostic,
};

namespace document_contract
{

inline constexpr bool profileChangesRebindOpenDocuments = true;
inline constexpr bool mapEditsAreStaged = true;
inline constexpr bool mapRequiresGlobalEditMode = false;
inline constexpr bool configuredMetadataIsReadOnly = true;
inline constexpr bool standaloneRadarcolIsEditable = true;
inline constexpr bool standaloneTexmapPairIsEditable = false;
inline constexpr bool genericOpenMutatesProfile = false;

inline constexpr std::string_view genericOpenLabel = "File > Open...";
inline constexpr UnsupportedMulPolicy unsupportedMulPolicy = UnsupportedMulPolicy::rejectWithDiagnostic;

} // namespace document_contract

[[nodiscard]] constexpr std::string_view documentKindName(DocumentKind kind) noexcept
{
    switch (kind)
    {
    case DocumentKind::map:
        return "Map";
    case DocumentKind::radarcol:
        return "Radarcol";
    case DocumentKind::texmapPair:
        return "Texmap Pair";
    }
    return "Unknown";
}

} // namespace uo_mule::gui
