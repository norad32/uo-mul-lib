#include "gui_model/documents/document_identity.h"
#include "gui_model/documents/document_contracts.h"
#include "gui_model/profile/data_profile.h"
#include "gui_model/source/source_identity.h"
#include <optional>

namespace uo_mule::gui
{

std::optional<DocumentSourceIdentity> configuredDocumentSourceIdentity(const DataProfile & profile, DocumentKind kind)
{
    switch (kind)
    {
        case DocumentKind::radarcol :
        {
            const auto path = resolvedDataFilePath(profile, DataFileRole::radarcol);
            if (path.empty())
            {
                return std::nullopt;
            }
            return makeDocumentSourceIdentity({path});
        }
        case DocumentKind::texmapPair :
        {
            const auto index = resolvedDataFilePath(profile, DataFileRole::texidx);
            const auto data = resolvedDataFilePath(profile, DataFileRole::texmaps);
            if (index.empty() || data.empty())
            {
                return std::nullopt;
            }
            return makeRoleAwareDocumentSourceIdentity(
                {{.role = SourceRole::index, .path = index}, {.role = SourceRole::data, .path = data}});
        }
        case DocumentKind::tiledata :
        {
            const auto path = resolvedDataFilePath(profile, DataFileRole::tiledata);
            if (path.empty())
            {
                return std::nullopt;
            }
            return makeDocumentSourceIdentity({path});
        }
        case DocumentKind::map :
        case DocumentKind::statics :
        case DocumentKind::combined :
        case DocumentKind::unavailable :
            return std::nullopt;
    }
    return std::nullopt;
}

bool sameDocumentIdentity(const DocumentIdentity & first, const DocumentIdentity & second) noexcept
{
    return first.kind == second.kind && first.source == second.source;
}

} // namespace uo_mule::gui
