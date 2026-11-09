#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <ft2build.h>
#include FT_OUTLINE_H

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>

using namespace FreeType;

extern "C" {

int APS5_VABI FT_Outline_Get_Orientation_nid_postfix(GuestOutline* outline) {
    if (!outline || outline->n_points == 0) return FT_ORIENTATION_TRUETYPE;
    const auto initial = outline->points[0];
    GuestBBox bounds{initial.x, initial.y, initial.x, initial.y};
    for (unsigned index = 1; index < outline->n_points; ++index) {
        const auto point = outline->points[index];
        bounds.xMin = std::min(bounds.xMin, point.x);
        bounds.yMin = std::min(bounds.yMin, point.y);
        bounds.xMax = std::max(bounds.xMax, point.x);
        bounds.yMax = std::max(bounds.yMax, point.y);
    }
    if (bounds.xMin == bounds.xMax || bounds.yMin == bounds.yMax ||
        bounds.xMin < -0x1000000 || bounds.yMin < -0x1000000 ||
        bounds.xMax > 0x1000000 || bounds.yMax > 0x1000000) return FT_ORIENTATION_NONE;
    const auto xMagnitude = static_cast<std::uint32_t>((bounds.xMin < 0 ? -bounds.xMin : bounds.xMin) |
                                                       (bounds.xMax < 0 ? -bounds.xMax : bounds.xMax));
    const int xShift = std::max(0, static_cast<int>(std::bit_width(xMagnitude)) - 15);
    const int yShift = std::max(0, static_cast<int>(std::bit_width(static_cast<std::uint32_t>(bounds.yMax - bounds.yMin))) - 15);
    std::int64_t area = 0;
    unsigned first = 0;
    for (unsigned contour = 0; contour < outline->n_contours; ++contour) {
        const unsigned last = outline->contours[contour];
        auto previousX = outline->points[last].x >> xShift;
        auto previousY = outline->points[last].y >> yShift;
        for (unsigned index = first; index <= last; ++index) {
            const auto x = outline->points[index].x >> xShift;
            const auto y = outline->points[index].y >> yShift;
            area += (y - previousY) * (x + previousX);
            previousX = x;
            previousY = y;
        }
        first = last + 1;
    }
    return area > 0 ? FT_ORIENTATION_POSTSCRIPT : area < 0 ? FT_ORIENTATION_TRUETYPE : FT_ORIENTATION_NONE;
}

int APS5_VABI FT_Outline_Copy_nid_postfix(const GuestOutline* source, GuestOutline* target) {
    if (!source || !target) return FT_Err_Invalid_Outline;
    if (source->n_points != target->n_points || source->n_contours != target->n_contours) return FT_Err_Invalid_Argument;
    if (source == target) return FT_Err_Ok;
    if (source->n_points) {
        std::memmove(target->points, source->points, source->n_points * sizeof(GuestVector));
        std::memmove(target->tags, source->tags, source->n_points * sizeof(unsigned char));
    }
    if (source->n_contours) std::memmove(target->contours, source->contours, source->n_contours * sizeof(std::uint16_t));
    target->flags = (source->flags & ~FT_OUTLINE_OWNER) | (target->flags & FT_OUTLINE_OWNER);
    return FT_Err_Ok;
}

}
