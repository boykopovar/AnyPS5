// Derived from FreeType src/base/ftoutln.c and src/base/ftcalc.c (FreeType 2.4.11, commit c6a66b49e64e9a21c013f23c867d238cf1aa98cb).
// Copyright 1996-2008, 2010, 2012 by David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#include "prx/libSceFreeTypeOt/include/OutlineTransform.hpp"
#include "prx/libSceFreeTypeOt/include/Polar.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace FreeType;

namespace {

constexpr std::uint64_t Int32Max = 0x7FFFFFFF;
constexpr int InvalidArgument = 0x06;
constexpr int OrientationTrueType = 0;
constexpr int OrientationPostScript = 1;
constexpr int OrientationNone = 2;

std::uint64_t Magnitude(std::int64_t value) {
    return value < 0 ? 0 - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
}

std::int64_t MulFix(std::int64_t a, std::int64_t b, const char* function) {
    const std::uint64_t x = Magnitude(a);
    const std::uint64_t y = Magnitude(b);
    if (x > Int32Max || y > Int32Max) NotImplemented_nid_no_patch(function);
    const auto result = static_cast<std::int64_t>((x * y + 0x8000) >> 16);
    return (a < 0) != (b < 0) ? -result : result;
}

std::int64_t MulDiv(std::int64_t a, std::int64_t b, std::int64_t c, const char* function) {
    const std::uint64_t x = Magnitude(a);
    const std::uint64_t y = Magnitude(b);
    const std::uint64_t z = Magnitude(c);
    if (x > Int32Max || y > Int32Max || z > Int32Max || (z == 0 && (x == 0 || y == 0))) {
        NotImplemented_nid_no_patch(function);
    }
    const std::uint64_t quotient = z == 0 ? Int32Max : (x * y + (z >> 1)) / z;
    if (quotient > 0xFFFFFFFF) NotImplemented_nid_no_patch(function);
    const auto result = static_cast<std::int64_t>(quotient);
    return ((a < 0) != (b < 0)) != (c < 0) ? -result : result;
}

std::int64_t Narrow(__int128 value, const char* function) {
    if (value < std::numeric_limits<std::int64_t>::min() || value > std::numeric_limits<std::int64_t>::max()) {
        throw std::runtime_error(std::string(function) + ": signed 64-bit overflow, undefined in FreeType 2.4.11, is not implemented");
    }
    return static_cast<std::int64_t>(value);
}

std::int64_t Add(std::int64_t a, std::int64_t b, const char* function) {
    return Narrow(static_cast<__int128>(a) + b, function);
}

std::int64_t Sub(std::int64_t a, std::int64_t b, const char* function) {
    return Narrow(static_cast<__int128>(a) - b, function);
}

std::int64_t Mul(std::int64_t a, std::int64_t b, const char* function) {
    return Narrow(static_cast<__int128>(a) * b, function);
}

class Points {
public:
    Points(const GuestOutline& outline, const char* function) : function(function) {
        const auto count = static_cast<std::int16_t>(outline.n_points);
        if (count <= 0 || !outline.points) return;
        points.resize(static_cast<std::size_t>(count));
        readable = GuestCopyIn_nid_no_patch(points.data(), outline.points, points.size() * sizeof(GuestVector));
    }

    bool Readable() const {
        return readable;
    }

    GuestVector& operator[](int index) {
        if (index < 0 || static_cast<std::size_t>(index) >= points.size()) NotImplemented_nid_no_patch(function);
        return points[static_cast<std::size_t>(index)];
    }

    const std::vector<GuestVector>& All() const {
        return points;
    }

private:
    const char* function;
    std::vector<GuestVector> points;
    bool readable = true;
};

class Contours {
public:
    Contours(const GuestOutline& outline, const char* function) : function(function) {
        const auto count = static_cast<std::int16_t>(outline.n_contours);
        if (count <= 0 || !outline.contours) return;
        ends.resize(static_cast<std::size_t>(count));
        readable = GuestCopyIn_nid_no_patch(ends.data(), outline.contours, ends.size() * sizeof(std::uint16_t));
    }

    bool Readable() const {
        return readable;
    }

    int operator[](int index) const {
        if (index < 0 || static_cast<std::size_t>(index) >= ends.size()) NotImplemented_nid_no_patch(function);
        return static_cast<std::int16_t>(ends[static_cast<std::size_t>(index)]);
    }

private:
    const char* function;
    std::vector<std::uint16_t> ends;
    bool readable = true;
};

int Orientation(const GuestOutline& outline, Points& points, const Contours& contourEnds, const char* function) {
    if (static_cast<std::int16_t>(outline.n_points) <= 0) return OrientationTrueType;
    std::int64_t area = 0;
    int first = 0;
    const int contours = static_cast<std::int16_t>(outline.n_contours);
    for (int c = 0; c < contours; ++c) {
        const int last = contourEnds[c];
        GuestVector previous = points[last];
        for (int n = first; n <= last; ++n) {
            const GuestVector current = points[n];
            area = Add(area, Mul(Sub(current.y, previous.y, function), Add(current.x, previous.x, function), function), function);
            previous = current;
        }
        first = last + 1;
    }
    if (area > 0) return OrientationPostScript;
    if (area < 0) return OrientationTrueType;
    return OrientationNone;
}

}

extern "C" {

void APS5_VABI sceFreeTypeOtOutlineTransform_nid_no_patch(const GuestOutline* outline, const GuestMatrix* matrix) {
    if (!outline || !matrix) return;
    GuestOutline header{};
    if (!GuestCopyIn_nid_no_patch(&header, outline, sizeof(header))) {
        throw std::invalid_argument("FT_Outline_Transform: unreadable outline");
    }
    const auto count = static_cast<std::int16_t>(header.n_points);
    if (count <= 0) return;
    if (!header.points) NotImplemented_nid_no_patch(__func__);
    GuestMatrix factors{};
    if (!GuestCopyIn_nid_no_patch(&factors, matrix, sizeof(factors))) {
        throw std::invalid_argument("FT_Outline_Transform: unreadable matrix");
    }
    std::vector<GuestVector> transformed(static_cast<std::size_t>(count));
    if (!GuestCopyIn_nid_no_patch(transformed.data(), header.points, transformed.size() * sizeof(GuestVector))) {
        throw std::invalid_argument("FT_Outline_Transform: unreadable points");
    }
    for (auto& point : transformed) {
        point = {MulFix(point.x, factors.xx, __func__) + MulFix(point.y, factors.xy, __func__),
                 MulFix(point.x, factors.yx, __func__) + MulFix(point.y, factors.yy, __func__)};
    }
    if (!GuestCopyOut_nid_no_patch(header.points, transformed.data(), transformed.size() * sizeof(GuestVector))) {
        throw std::invalid_argument("FT_Outline_Transform: unwritable points");
    }
}
APS5_EXPORT("0Lv6K1d47jg", sceFreeTypeOtOutlineTransform_nid_no_patch);

int APS5_VABI sceFreeTypeOtOutlineEmboldenXY_nid_no_patch(GuestOutline* outline, std::int64_t xstrength, std::int64_t ystrength) {
    if (!outline) return InvalidArgument;
    xstrength /= 2;
    ystrength /= 2;
    if (xstrength == 0 && ystrength == 0) return 0;
    GuestOutline header{};
    if (!GuestCopyIn_nid_no_patch(&header, outline, sizeof(header))) return InvalidArgument;
    Points points(header, __func__);
    const Contours contourEnds(header, __func__);
    if (!points.Readable() || !contourEnds.Readable()) return InvalidArgument;
    const int orientation = Orientation(header, points, contourEnds, __func__);
    const int contours = static_cast<std::int16_t>(header.n_contours);
    if (orientation == OrientationNone) return contours != 0 ? InvalidArgument : 0;
    int first = 0;
    for (int c = 0; c < contours; ++c) {
        const int last = contourEnds[c];
        const GuestVector vFirst = points[first];
        const GuestVector vPrev = points[last];
        GuestVector vCur = vFirst;
        GuestVector in{Sub(vCur.x, vPrev.x, __func__), Sub(vCur.y, vPrev.y, __func__)};
        std::int64_t lIn = OtVectorLength(in, __func__);
        for (int n = first; n <= last; ++n) {
            const GuestVector vNext = n < last ? points[n + 1] : vFirst;
            const GuestVector out{Sub(vNext.x, vCur.x, __func__), Sub(vNext.y, vCur.y, __func__)};
            const std::int64_t lOut = OtVectorLength(out, __func__);
            const std::int64_t lengths = Mul(lIn, lOut, __func__);
            const std::int64_t d = Add(Add(lengths, Mul(in.x, out.x, __func__), __func__), Mul(in.y, out.y, __func__), __func__);
            GuestVector shift{0, 0};
            if (Mul(16, d, __func__) > lengths) {
                shift.x = Add(Mul(lOut, in.y, __func__), Mul(lIn, out.y, __func__), __func__);
                shift.y = Add(Mul(lOut, in.x, __func__), Mul(lIn, out.x, __func__), __func__);
                if (orientation == OrientationTrueType) {
                    shift.x = Sub(0, shift.x, __func__);
                } else {
                    shift.y = Sub(0, shift.y, __func__);
                }
                const std::int64_t l = std::min(lIn, lOut);
                std::int64_t q = Sub(Mul(out.x, in.y, __func__), Mul(out.y, in.x, __func__), __func__);
                if (orientation == OrientationTrueType) q = Sub(0, q, __func__);
                shift.x = MulDiv(xstrength, q, l, __func__) < d ? MulDiv(shift.x, xstrength, d, __func__) : MulDiv(shift.x, l, q, __func__);
                shift.y = MulDiv(ystrength, q, l, __func__) < d ? MulDiv(shift.y, ystrength, d, __func__) : MulDiv(shift.y, l, q, __func__);
            }
            points[n] = {Add(Add(vCur.x, xstrength, __func__), shift.x, __func__), Add(Add(vCur.y, ystrength, __func__), shift.y, __func__)};
            in = out;
            lIn = lOut;
            vCur = vNext;
        }
        first = last + 1;
    }
    const auto& result = points.All();
    if (!result.empty() && !GuestCopyOut_nid_no_patch(header.points, result.data(), result.size() * sizeof(GuestVector))) {
        return InvalidArgument;
    }
    return 0;
}
APS5_EXPORT("BgUMBGvhATU", sceFreeTypeOtOutlineEmboldenXY_nid_no_patch);

int APS5_VABI sceFreeTypeOtOutlineEmbolden_nid_no_patch(GuestOutline* outline, std::int64_t strength) {
    return sceFreeTypeOtOutlineEmboldenXY_nid_no_patch(outline, strength, strength);
}
APS5_EXPORT("l6Cf9rb9fZQ", sceFreeTypeOtOutlineEmbolden_nid_no_patch);

}
