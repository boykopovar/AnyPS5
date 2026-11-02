#include "prx/libSceFreeType/include/Api.hpp"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <stdexcept>

namespace {

bool Clockwise(const FreeType::GuestOutline* outline) {
    if (!outline || outline->n_points == 0) return true;
    if (!outline->points) throw std::invalid_argument("FreeType outline border: null points");
    auto minimum = outline->points[0];
    auto maximum = minimum;
    for (unsigned i = 1; i < outline->n_points; ++i) {
        const auto point = outline->points[i];
        minimum.x = std::min(minimum.x, point.x);
        minimum.y = std::min(minimum.y, point.y);
        maximum.x = std::max(maximum.x, point.x);
        maximum.y = std::max(maximum.y, point.y);
    }
    if (minimum.x == maximum.x || minimum.y == maximum.y) return false;
    constexpr std::int64_t limit = 0x1000000;
    if (minimum.x < -limit || minimum.y < -limit || maximum.x > limit || maximum.y > limit) return false;
    if (outline->n_contours == 0) return false;
    if (!outline->contours) throw std::invalid_argument("FreeType outline border: null contours");
    unsigned first = 0;
    for (unsigned contour = 0; contour < outline->n_contours; ++contour) {
        const unsigned last = outline->contours[contour];
        if (last < first || last >= outline->n_points)
            throw std::invalid_argument("FreeType outline border: invalid contour endpoint");
        first = last + 1;
    }
    const auto magnitude = [](std::int64_t value) { return static_cast<std::uint32_t>(value < 0 ? -value : value); };
    const auto shift = [](std::uint32_t value) { const auto bits = std::bit_width(value); return bits > 15 ? bits - 15 : 0; };
    const auto xShift = shift(magnitude(maximum.x) | magnitude(minimum.x));
    const auto yShift = shift(static_cast<std::uint32_t>(maximum.y - minimum.y));
    std::int64_t area = 0;
    first = 0;
    for (unsigned contour = 0; contour < outline->n_contours; ++contour) {
        const unsigned last = outline->contours[contour];
        auto previous = outline->points[last];
        previous.x >>= xShift;
        previous.y >>= yShift;
        for (unsigned i = first; i <= last; ++i) {
            auto current = outline->points[i];
            current.x >>= xShift;
            current.y >>= yShift;
            area += (current.y - previous.y) * (current.x + previous.x);
            previous = current;
        }
        first = last + 1;
    }
    return area < 0;
}

}

extern "C" {

std::int32_t APS5_VABI FT_Outline_GetInsideBorder_nid_postfix(const FreeType::GuestOutline* outline) {
    return Clockwise(outline) ? 1 : 0;
}

std::int32_t APS5_VABI FT_Outline_GetOutsideBorder_nid_postfix(const FreeType::GuestOutline* outline) {
    return Clockwise(outline) ? 0 : 1;
}

}
