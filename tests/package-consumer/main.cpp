#include <cstdint>
#include <uomul/facet_sizes.h>
#include <uomul/map.h>

namespace
{

constexpr std::uint32_t testDimension = 8;

}

int main()
{
    const uomul::Dimensions dimensions{.widthTiles = testDimension, .heightTiles = testDimension};
    return dimensions.widthTiles == testDimension && !uomul::knownFacetSizes().empty() ? 0 : 1;
}
