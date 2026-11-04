#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdint>
#include <limits>
#include <stdexcept>

using namespace FreeType;

namespace {

std::int64_t Scale(std::int64_t value, std::int32_t shift, std::int64_t delta) {
    if (shift < 0 || shift >= 63) {
        throw std::runtime_error("FT_Outline_Decompose: a shift outside 0-62 is not implemented");
    }
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    const std::int64_t factor = std::int64_t{1} << shift;
    if (value < minimum / factor || value > maximum / factor) {
        throw std::runtime_error("FT_Outline_Decompose: overflowing coordinate scaling is not implemented");
    }
    const std::int64_t scaled = value * factor;
    if ((delta > 0 && scaled < minimum + delta) || (delta < 0 && scaled > maximum + delta)) {
        throw std::runtime_error("FT_Outline_Decompose: overflowing coordinate translation is not implemented");
    }
    return scaled - delta;
}

GuestVector Scale(GuestVector point, std::int32_t shift, std::int64_t delta) {
    return {Scale(point.x, shift, delta), Scale(point.y, shift, delta)};
}

std::int64_t Midpoint(std::int64_t first, std::int64_t second) {
    if ((second > 0 && first > std::numeric_limits<std::int64_t>::max() - second) ||
        (second < 0 && first < std::numeric_limits<std::int64_t>::min() - second)) {
        throw std::runtime_error("FT_Outline_Decompose: overflowing midpoint arithmetic is not implemented");
    }
    return (first + second) / 2;
}

GuestVector Midpoint(GuestVector first, GuestVector second) {
    return {Midpoint(first.x, second.x), Midpoint(first.y, second.y)};
}

}

extern "C" int APS5_VABI FT_Outline_Decompose_nid_postfix(GuestOutline* outline, const GuestOutlineFuncs* functions, void* user) {
    if (!outline) return FT_Err_Invalid_Outline;
    if (!functions) return FT_Err_Invalid_Argument;
    const std::int32_t shift = functions->shift;
    const std::int64_t delta = functions->delta;
    std::int32_t previousEnd = -1;
    for (std::uint32_t contour = 0; contour < outline->n_contours; ++contour) {
        const std::int32_t first = previousEnd + 1;
        const std::int32_t last = outline->contours[contour];
        if (last < first) return FT_Err_Invalid_Outline;
        if (last >= outline->n_points) throw std::invalid_argument("FT_Outline_Decompose: contour endpoint at or beyond n_points");
        previousEnd = last;
        auto* points = outline->points;
        auto* tags = outline->tags;
        std::int32_t limit = last;
        std::int32_t point = first;
        GuestVector start = Scale(points[first], shift, delta);
        const GuestVector end = Scale(points[last], shift, delta);
        const int firstTag = tags[first] & 3;
        if (firstTag == FT_CURVE_TAG_CUBIC) return FT_Err_Invalid_Outline;
        if (firstTag == FT_CURVE_TAG_CONIC) {
            if ((tags[last] & 3) == FT_CURVE_TAG_ON) {
                start = end;
                --limit;
            } else {
                start = Midpoint(start, end);
            }
            --point;
        }
        int error = functions->move_to(&start, user);
        if (error != 0) return error;
        bool closed = false;
        while (point < limit) {
            ++point;
            const int tag = tags[point] & 3;
            if (tag == FT_CURVE_TAG_ON) {
                const GuestVector to = Scale(points[point], shift, delta);
                error = functions->line_to(&to, user);
                if (error != 0) return error;
            } else if (tag == FT_CURVE_TAG_CONIC) {
                GuestVector control = Scale(points[point], shift, delta);
                while (true) {
                    if (point == limit) {
                        error = functions->conic_to(&control, &start, user);
                        closed = true;
                        break;
                    }
                    ++point;
                    const int nextTag = tags[point] & 3;
                    const GuestVector next = Scale(points[point], shift, delta);
                    if (nextTag == FT_CURVE_TAG_ON) {
                        error = functions->conic_to(&control, &next, user);
                        break;
                    }
                    if (nextTag != FT_CURVE_TAG_CONIC) return FT_Err_Invalid_Outline;
                    const GuestVector middle = Midpoint(control, next);
                    error = functions->conic_to(&control, &middle, user);
                    if (error != 0) return error;
                    control = next;
                }
                if (error != 0) return error;
            } else {
                if (point + 1 > limit || (tags[point + 1] & 3) != FT_CURVE_TAG_CUBIC) return FT_Err_Invalid_Outline;
                const GuestVector control1 = Scale(points[point], shift, delta);
                const GuestVector control2 = Scale(points[point + 1], shift, delta);
                point += 2;
                if (point <= limit) {
                    const GuestVector to = Scale(points[point], shift, delta);
                    error = functions->cubic_to(&control1, &control2, &to, user);
                } else {
                    error = functions->cubic_to(&control1, &control2, &start, user);
                    closed = true;
                }
                if (error != 0) return error;
            }
        }
        if (!closed) {
            error = functions->line_to(&start, user);
            if (error != 0) return error;
        }
    }
    return 0;
}
