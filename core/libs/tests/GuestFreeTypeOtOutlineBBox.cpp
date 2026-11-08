#include "prx/libSceFreeTypeOt/include/Outline.hpp"
#include "prx/libSceFreeType/include/Api.hpp"

#include <cstdlib>

static void Require(bool condition) {
    if (!condition) std::abort();
}

int main() {
    FreeType::GuestVector points[] = {{0, 0}, {50, 100}, {100, 0}};
    unsigned char tags[] = {1, 0, 1};
    std::uint16_t contours[] = {2};
    FreeType::GuestOutline outline{1, 3, points, tags, contours, 0};
    FreeType::GuestBBox box{1, 2, 3, 4};

    Require(FT_Outline_Get_BBox_nid_postfix(&outline, &box) == 0);
    Require(box.xMin == 0 && box.yMin == 0 && box.xMax == 100 && box.yMax == 50);

    FreeType::GuestOutline empty{};
    box = {1, 2, 3, 4};
    Require(FT_Outline_Get_BBox_nid_postfix(&empty, &box) == 0);
    Require(box.xMin == 0 && box.yMin == 0 && box.xMax == 0 && box.yMax == 0);
}
