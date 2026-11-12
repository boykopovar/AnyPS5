#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_OUTLINE_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace FreeType;

namespace {

FT_Pos HostCoordinate(std::int64_t value) {
    if (value < std::numeric_limits<FT_Pos>::min() || value > std::numeric_limits<FT_Pos>::max()) {
        throw std::runtime_error("FT_Outline_Get_Bitmap: an outline coordinate outside the host FT_Pos range is not implemented");
    }
    return static_cast<FT_Pos>(value);
}

}

extern "C" {

int APS5_VABI FT_Outline_Get_Bitmap_nid_postfix(GuestLibrary* library, GuestOutline* outline, const GuestBitmap* bitmap) {
    FT_Library host = library ? library->host : nullptr;
    if (!bitmap) return FT_Outline_Get_Bitmap(host, nullptr, nullptr);
    const FT_Bitmap target{bitmap->rows, bitmap->width, bitmap->pitch, bitmap->buffer, bitmap->num_grays, bitmap->pixel_mode, bitmap->palette_mode, bitmap->palette};
    if (!outline) return FT_Outline_Get_Bitmap(host, nullptr, &target);
    std::vector<FT_Vector> points;
    points.reserve(outline->n_points);
    for (std::size_t index = 0; index < outline->n_points; ++index) {
        points.push_back({HostCoordinate(outline->points[index].x), HostCoordinate(outline->points[index].y)});
    }
    FT_Outline native{outline->n_contours, outline->n_points, points.data(), outline->tags, outline->contours, outline->flags};
    const std::size_t pitch = static_cast<std::size_t>(bitmap->pitch < 0 ? -static_cast<std::int64_t>(bitmap->pitch) : bitmap->pitch);
    GuestArena::HostWrite write(bitmap->buffer, bitmap->buffer ? pitch * bitmap->rows : 0);
    return FT_Outline_Get_Bitmap(host, &native, &target);
}

}
