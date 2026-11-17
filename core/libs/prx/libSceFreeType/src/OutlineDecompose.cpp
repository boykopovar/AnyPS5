#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/General.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

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
    GuestOutlineFuncs callbacks{};
    const auto refresh = [&] { return GuestCopyIn_nid_no_patch(&callbacks, functions, sizeof(callbacks)); };
    if (!refresh()) return FT_Err_Invalid_Argument;
    const std::int32_t shift = callbacks.shift;
    const std::int64_t delta = callbacks.delta;
    GuestOutline header{};
    std::vector<GuestVector> points;
    std::vector<unsigned char> tags;
    std::int32_t previousEnd = -1;
    for (std::uint32_t contour = 0;; ++contour) {
        if (!GuestCopyIn_nid_no_patch(&header, outline, sizeof(header))) return FT_Err_Invalid_Outline;
        if (contour >= header.n_contours) break;
        std::uint16_t endpoint = 0;
        if (!header.contours || !GuestCopyIn_nid_no_patch(&endpoint, header.contours + contour, sizeof(endpoint))) return FT_Err_Invalid_Outline;
        const std::int32_t first = previousEnd + 1;
        const std::int32_t last = endpoint;
        if (last < first) return FT_Err_Invalid_Outline;
        if (last >= header.n_points) throw std::invalid_argument("FT_Outline_Decompose: contour endpoint at or beyond n_points");
        previousEnd = last;
        const auto count = static_cast<std::size_t>(last - first + 1);
        points.resize(count);
        tags.resize(count);
        if (!header.points || !header.tags || !GuestCopyIn_nid_no_patch(points.data(), header.points + first, count * sizeof(GuestVector)) ||
            !GuestCopyIn_nid_no_patch(tags.data(), header.tags + first, count)) {
            return FT_Err_Invalid_Outline;
        }
        const auto pointAt = [&](std::int32_t index) { return Scale(points[static_cast<std::size_t>(index - first)], shift, delta); };
        const auto tagAt = [&](std::int32_t index) { return tags[static_cast<std::size_t>(index - first)] & 3; };
        std::int32_t limit = last;
        std::int32_t point = first;
        GuestVector start = pointAt(first);
        const GuestVector end = pointAt(last);
        const int firstTag = tagAt(first);
        if (firstTag == FT_CURVE_TAG_CUBIC) return FT_Err_Invalid_Outline;
        if (firstTag == FT_CURVE_TAG_CONIC) {
            if (tagAt(last) == FT_CURVE_TAG_ON) {
                start = end;
                --limit;
            } else {
                start = Midpoint(start, end);
            }
            --point;
        }
        if (!refresh()) return FT_Err_Invalid_Argument;
        int error = callbacks.move_to(&start, user);
        if (error != 0) return error;
        bool closed = false;
        while (point < limit) {
            ++point;
            const int tag = tagAt(point);
            if (tag == FT_CURVE_TAG_ON) {
                const GuestVector to = pointAt(point);
                if (!refresh()) return FT_Err_Invalid_Argument;
                error = callbacks.line_to(&to, user);
                if (error != 0) return error;
            } else if (tag == FT_CURVE_TAG_CONIC) {
                GuestVector control = pointAt(point);
                while (true) {
                    if (point == limit) {
                        if (!refresh()) return FT_Err_Invalid_Argument;
                        error = callbacks.conic_to(&control, &start, user);
                        closed = true;
                        break;
                    }
                    ++point;
                    const int nextTag = tagAt(point);
                    const GuestVector next = pointAt(point);
                    if (nextTag == FT_CURVE_TAG_ON) {
                        if (!refresh()) return FT_Err_Invalid_Argument;
                        error = callbacks.conic_to(&control, &next, user);
                        break;
                    }
                    if (nextTag != FT_CURVE_TAG_CONIC) return FT_Err_Invalid_Outline;
                    const GuestVector middle = Midpoint(control, next);
                    if (!refresh()) return FT_Err_Invalid_Argument;
                    error = callbacks.conic_to(&control, &middle, user);
                    if (error != 0) return error;
                    control = next;
                }
                if (error != 0) return error;
            } else {
                if (point + 1 > limit || tagAt(point + 1) != FT_CURVE_TAG_CUBIC) return FT_Err_Invalid_Outline;
                const GuestVector control1 = pointAt(point);
                const GuestVector control2 = pointAt(point + 1);
                point += 2;
                if (!refresh()) return FT_Err_Invalid_Argument;
                if (point <= limit) {
                    const GuestVector to = pointAt(point);
                    error = callbacks.cubic_to(&control1, &control2, &to, user);
                } else {
                    error = callbacks.cubic_to(&control1, &control2, &start, user);
                    closed = true;
                }
                if (error != 0) return error;
            }
        }
        if (!closed) {
            if (!refresh()) return FT_Err_Invalid_Argument;
            error = callbacks.line_to(&start, user);
            if (error != 0) return error;
        }
    }
    return 0;
}
