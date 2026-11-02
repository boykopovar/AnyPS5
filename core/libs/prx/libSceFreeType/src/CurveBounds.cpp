// Derived from FreeType src/base/ftbbox.c (FreeType 2.13.3, commit 42608f77f20749dd6ddc9e0536788eaad70ea4b5).
// Copyright (C) 1996-2024 by David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <bit>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

using FreeType::GuestBBox;
using FreeType::GuestOutlineFuncs;
using FreeType::GuestOutline;
using FreeType::GuestVector;

namespace {

constexpr std::int64_t Minimum = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t Maximum = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t Unset = 0x7FFFFFFF;

[[noreturn]] void Undefined(const char* reason) {
    throw std::invalid_argument(std::string("FT_Outline_Get_BBox: ") + reason);
}

std::int64_t Add(std::int64_t a, std::int64_t b) {
    if ((b > 0 && a > Maximum - b) || (b < 0 && a < Minimum - b)) Undefined("signed overflow in the extremum arithmetic");
    return a + b;
}

std::int64_t Subtract(std::int64_t a, std::int64_t b) {
    if ((b < 0 && a > Maximum + b) || (b > 0 && a < Minimum + b)) Undefined("signed overflow in the extremum arithmetic");
    return a - b;
}

std::int64_t Scale(std::int64_t value, std::int64_t factor) {
    if (value > Maximum / factor || value < Minimum / factor) Undefined("signed overflow in the extremum arithmetic");
    return value * factor;
}

std::int64_t Absolute(std::int64_t value) {
    if (value == Minimum) Undefined("signed overflow in the extremum arithmetic");
    return value < 0 ? -value : value;
}

std::int64_t MulDiv(std::int64_t a, std::int64_t b, std::int64_t c) {
    int sign = 1;
    const auto magnitude = [&sign](std::int64_t value) {
        if (value >= 0) return static_cast<std::uint64_t>(value);
        sign = -sign;
        return std::uint64_t{0} - static_cast<std::uint64_t>(value);
    };
    const std::uint64_t ua = magnitude(a);
    const std::uint64_t ub = magnitude(b);
    const std::uint64_t uc = magnitude(c);
    const std::uint64_t quotient = uc > 0 ? (ua * ub + (uc >> 1)) / uc : std::uint64_t{0x7FFFFFFF};
    return static_cast<std::int64_t>(sign < 0 ? std::uint64_t{0} - quotient : quotient);
}

void Include(const GuestVector& point, GuestBBox& box) {
    if (point.x < box.xMin) box.xMin = point.x;
    if (point.x > box.xMax) box.xMax = point.x;
    if (point.y < box.yMin) box.yMin = point.y;
    if (point.y > box.yMax) box.yMax = point.y;
}

void ConicCheck(std::int64_t y1, std::int64_t y2, std::int64_t y3, std::int64_t& minimum, std::int64_t& maximum) {
    y1 = Subtract(y1, y2);
    y3 = Subtract(y3, y2);
    y2 = Add(y2, MulDiv(y1, y3, Add(y1, y3)));
    if (y2 < minimum) minimum = y2;
    if (y2 > maximum) maximum = y2;
}

std::int64_t CubicPeak(std::int64_t q1, std::int64_t q2, std::int64_t q3, std::int64_t q4) {
    const auto bits = static_cast<std::uint32_t>(Absolute(q1) | Absolute(q2) | Absolute(q3) | Absolute(q4));
    if (bits == 0) Undefined("cubic control distances whose low 32 bits are zero");
    int shift = 27 - (31 - std::countl_zero(bits));
    if (shift > 0) {
        if (shift > 2) shift = 2;
        const std::int64_t factor = std::int64_t{1} << shift;
        q1 = Scale(q1, factor);
        q2 = Scale(q2, factor);
        q3 = Scale(q3, factor);
        q4 = Scale(q4, factor);
    } else {
        q1 >>= -shift;
        q2 >>= -shift;
        q3 >>= -shift;
        q4 >>= -shift;
    }
    std::int64_t peak = 0;
    while (q2 > 0 || q3 > 0) {
        if (Add(q1, q2) > Add(q3, q4)) {
            q4 = Add(q4, q3);
            q3 = Add(q3, q2);
            q2 = Add(q2, q1);
            q4 = Add(q4, q3);
            q3 = Add(q3, q2);
            q4 = Add(q4, q3) >> 3;
            q3 >>= 2;
            q2 >>= 1;
        } else {
            q1 = Add(q1, q2);
            q2 = Add(q2, q3);
            q3 = Add(q3, q4);
            q1 = Add(q1, q2);
            q2 = Add(q2, q3);
            q1 = Add(q1, q2) >> 3;
            q2 >>= 2;
            q3 >>= 1;
        }
        if (q1 == q2 && q1 >= q3) {
            peak = q1;
            break;
        }
        if (q3 == q4 && q2 <= q4) {
            peak = q4;
            break;
        }
    }
    if (shift > 0) return peak >> shift;
    if (shift == 0) return peak;
    if (peak < 0) Undefined("left shift of a negative cubic peak");
    return Scale(peak, std::int64_t{1} << -shift);
}

void CubicCheck(std::int64_t p1, std::int64_t p2, std::int64_t p3, std::int64_t p4, std::int64_t& minimum, std::int64_t& maximum) {
    if (p2 > maximum || p3 > maximum) {
        maximum = Add(maximum, CubicPeak(Subtract(p1, maximum), Subtract(p2, maximum), Subtract(p3, maximum), Subtract(p4, maximum)));
    }
    if (p2 < minimum || p3 < minimum) {
        minimum = Subtract(minimum, CubicPeak(Subtract(minimum, p1), Subtract(minimum, p2), Subtract(minimum, p3), Subtract(minimum, p4)));
    }
}

bool OutsideX(const GuestVector& point, const GuestBBox& box) {
    return point.x < box.xMin || point.x > box.xMax;
}

bool OutsideY(const GuestVector& point, const GuestBBox& box) {
    return point.y < box.yMin || point.y > box.yMax;
}

struct Walk {
    GuestVector last{};
    GuestBBox box{};
};

APS5_VABI int MoveTo(const GuestVector* to, void* user) {
    auto& walk = *static_cast<Walk*>(user);
    Include(*to, walk.box);
    walk.last = *to;
    return 0;
}

APS5_VABI int LineTo(const GuestVector* to, void* user) {
    static_cast<Walk*>(user)->last = *to;
    return 0;
}

APS5_VABI int ConicTo(const GuestVector* control, const GuestVector* to, void* user) {
    auto& walk = *static_cast<Walk*>(user);
    Include(*to, walk.box);
    if (OutsideX(*control, walk.box)) ConicCheck(walk.last.x, control->x, to->x, walk.box.xMin, walk.box.xMax);
    if (OutsideY(*control, walk.box)) ConicCheck(walk.last.y, control->y, to->y, walk.box.yMin, walk.box.yMax);
    walk.last = *to;
    return 0;
}

APS5_VABI int CubicTo(const GuestVector* control1, const GuestVector* control2, const GuestVector* to, void* user) {
    auto& walk = *static_cast<Walk*>(user);
    if (OutsideX(*control1, walk.box) || OutsideX(*control2, walk.box))
        CubicCheck(walk.last.x, control1->x, control2->x, to->x, walk.box.xMin, walk.box.xMax);
    if (OutsideY(*control1, walk.box) || OutsideY(*control2, walk.box))
        CubicCheck(walk.last.y, control1->y, control2->y, to->y, walk.box.yMin, walk.box.yMax);
    walk.last = *to;
    return 0;
}

}

extern "C" int APS5_VABI FT_Outline_Get_BBox_nid_postfix(GuestOutline* outline, GuestBBox* boundingBox) {
    if (!boundingBox) return FT_Err_Invalid_Argument;
    if (!outline) return FT_Err_Invalid_Outline;
    if (outline->n_points == 0 || outline->n_contours == 0) {
        *boundingBox = {};
        return 0;
    }
    if (!outline->points || !outline->tags)
        throw std::invalid_argument("FT_Outline_Get_BBox: null point or tag array in a nonempty outline");
    GuestBBox control{Unset, Unset, -Unset, -Unset};
    GuestBBox on = control;
    for (std::uint32_t index = 0; index < outline->n_points; ++index) {
        Include(outline->points[index], control);
        if ((outline->tags[index] & 3) == FT_CURVE_TAG_ON) Include(outline->points[index], on);
    }
    if (control.xMin < on.xMin || control.xMax > on.xMax || control.yMin < on.yMin || control.yMax > on.yMax) {
        if (!outline->contours) Undefined("null contour array in a nonempty outline");
        Walk walk{{}, on};
        const GuestOutlineFuncs functions{MoveTo, LineTo, ConicTo, CubicTo, 0, 0};
        const int error = FT_Outline_Decompose_nid_postfix(outline, &functions, &walk);
        if (error != 0) return error;
        *boundingBox = walk.box;
    } else {
        *boundingBox = on;
    }
    return 0;
}
