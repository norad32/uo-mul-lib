#pragma once

#include "uomul/tiledata/tiledata.h"
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
    statics,
    combined,
    tiledata,
    unavailable,
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
inline constexpr bool standaloneTexmapPairIsEditable = true;
inline constexpr bool standaloneTiledataIsEditable = false;
inline constexpr bool genericOpenMutatesProfile = false;

inline constexpr std::string_view genericOpenLabel = "File > Open...";
inline constexpr UnsupportedMulPolicy unsupportedMulPolicy = UnsupportedMulPolicy::rejectWithDiagnostic;

} // namespace document_contract

[[nodiscard]] constexpr std::string_view documentKindName(DocumentKind kind) noexcept
{
    switch (kind)
    {
        case DocumentKind::map :
            return "Map";
        case DocumentKind::radarcol :
            return "Radarcol";
        case DocumentKind::texmapPair :
            return "Texmap Pair";
        case DocumentKind::statics :
            return "Statics";
        case DocumentKind::combined :
            return "Combined";
        case DocumentKind::tiledata :
            return "Tiledata";
        case DocumentKind::unavailable :
            return "Unavailable";
    }
    return "Unknown";
}

[[nodiscard]] constexpr std::string_view documentKindStableName(DocumentKind kind) noexcept
{
    switch (kind)
    {
        case DocumentKind::map :
            return "map";
        case DocumentKind::radarcol :
            return "radarcol";
        case DocumentKind::texmapPair :
            return "texmap_pair";
        case DocumentKind::statics :
            return "statics";
        case DocumentKind::combined :
            return "combined";
        case DocumentKind::tiledata :
            return "tiledata";
        case DocumentKind::unavailable :
            return "unavailable";
    }
    return "unavailable";
}

} // namespace uo_mule::gui
