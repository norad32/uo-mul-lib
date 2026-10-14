#include <uomul/map.h>

int main()
{
    const uomul::Dimensions dimensions{8, 8};
    return dimensions.widthTiles == 8 ? 0 : 1;
}
