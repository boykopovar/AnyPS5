#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <ft2build.h>
#include FT_OUTLINE_H

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>

using namespace FreeType;

extern "C" {

int APS5_VABI FT_Outline_Check_nid_postfix(GuestOutline* outline) {
    if (!outline) return FT_Outline_Check(nullptr);
    FT_Outline native{outline->n_contours, outline->n_points, nullptr, nullptr, outline->contours, outline->flags};
    return FT_Outline_Check(&native);
}

void APS5_VABI FT_Outline_Get_CBox_nid_postfix(const GuestOutline* outline, GuestBBox* box) {
    if (!outline || !box) return;
    GuestBBox bounds{};
    if (outline->n_points) {
        const GuestVector& first = outline->points[0];
        bounds = {first.x, first.y, first.x, first.y};
        for (std::size_t index = 1; index < outline->n_points; ++index) {
            const GuestVector& point = outline->points[index];
            bounds.xMin = std::min(bounds.xMin, point.x);
            bounds.yMin = std::min(bounds.yMin, point.y);
            bounds.xMax = std::max(bounds.xMax, point.x);
            bounds.yMax = std::max(bounds.yMax, point.y);
        }
    }
    *box = bounds;
}

void APS5_VABI FT_Outline_Translate_nid_postfix(const GuestOutline* outline, std::int64_t xOffset, std::int64_t yOffset) {
    if (!outline) return;
    for (std::size_t index = 0; index < outline->n_points; ++index) {
        GuestVector& point = outline->points[index];
        point.x = std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(point.x) + static_cast<std::uint64_t>(xOffset));
        point.y = std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(point.y) + static_cast<std::uint64_t>(yOffset));
    }
}

void APS5_VABI FT_Outline_Reverse_nid_postfix(GuestOutline* outline) {
    if (!outline) return;
    std::size_t first = 1;
    for (std::size_t contour = 0; contour < outline->n_contours; ++contour) {
        const std::size_t last = outline->contours[contour];
        for (std::size_t left = first, right = last; left < right; ++left, --right) {
            std::swap(outline->points[left], outline->points[right]);
        }
        for (std::size_t left = first, right = last; left < right; ++left, --right) {
            std::swap(outline->tags[left], outline->tags[right]);
        }
        first = last + 2;
    }
    outline->flags ^= FT_OUTLINE_REVERSE_FILL;
}

}
