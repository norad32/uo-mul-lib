#include "gui_model/source/tiledata_source.h"
#include "gui_model/profile/data_profile.h"
#include "uomul/tiledata/tiledata.h"
#include <filesystem>
#include <optional>

namespace uo_mule::gui
{

std::optional<uomul::TiledataLayout> resolveTiledataLayout(const std::filesystem::path & path,
                                                           TiledataLayoutSelection selection)
{
    if (path.empty())
    {
        return std::nullopt;
    }
    if (selection == TiledataLayoutSelection::legacy32)
    {
        if (const auto reader = uomul::TiledataReader::open(path, uomul::TiledataLayout::legacy32); reader)
        {
            return uomul::TiledataLayout::legacy32;
        }
        return std::nullopt;
    }
    if (selection == TiledataLayoutSelection::extended64)
    {
        if (const auto reader = uomul::TiledataReader::open(path, uomul::TiledataLayout::extended64); reader)
        {
            return uomul::TiledataLayout::extended64;
        }
        return std::nullopt;
    }
    if (const auto extended = uomul::TiledataReader::open(path, uomul::TiledataLayout::extended64); extended)
    {
        return uomul::TiledataLayout::extended64;
    }
    if (const auto legacy = uomul::TiledataReader::open(path, uomul::TiledataLayout::legacy32); legacy)
    {
        return uomul::TiledataLayout::legacy32;
    }
    return std::nullopt;
}

} // namespace uo_mule::gui
