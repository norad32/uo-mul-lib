#pragma once

#include "gui_model/documents/document_identity.h"
#include <string>

namespace uo_mule::gui
{

[[nodiscard]] std::string documentWindowName(const DocumentSummary & summary);

} // namespace uo_mule::gui
