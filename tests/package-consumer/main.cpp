#include <cstdint>
#include <uomul/map.h>

namespace
{

constexpr std::uint32_t testDimension = 8;

}

int main()
{
    const uomul::Dimensions dimensions{.widthTiles = testDimension, .heightTiles = testDimension};
    return dimensions.widthTiles == testDimension ? 0 : 1;
}
