#include "gui/frontend/document_surface.h"
#include "gui/documents/document_identity.h"
#include "gui/metadata/document_contracts.h"
#include <string>

namespace uo_mule::gui
{

std::string documentWindowName(const DocumentSummary & summary)
{
    std::string name(documentKindName(summary.identity.kind));
    name += ": ";
    name += summary.displayName.empty() ? "Untitled" : summary.displayName;
    if (summary.dirty)
    {
        name += " *";
    }
    if (summary.access == DocumentAccess::readOnly)
    {
        name += " [Read-only]";
    }
    name += "###document.";
    name += std::to_string(summary.identity.id);
    return name;
}

} // namespace uo_mule::gui
