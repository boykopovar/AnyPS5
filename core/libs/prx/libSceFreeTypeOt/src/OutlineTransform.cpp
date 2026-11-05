// Derived from FreeType src/base/ftoutln.c (FreeType 2.4.11, commit c6a66b49e64e9a21c013f23c867d238cf1aa98cb).
// Copyright 1996-2008, 2010, 2012 by David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#include "prx/libSceFreeTypeOt/include/OutlineTransform.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace FreeType;

namespace {

constexpr std::uint64_t Int32Max = 0x7FFFFFFF;

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

}

extern "C" {

void APS5_VABI sceFreeTypeOtOutlineTransform_nid_no_patch(const GuestOutline* outline, const GuestMatrix* matrix) {
    if (!outline || !matrix) return;
    const auto count = static_cast<std::int16_t>(outline->n_points);
    if (count <= 0) return;
    if (!outline->points) NotImplemented_nid_no_patch(__func__);
    std::vector<GuestVector> transformed(static_cast<std::size_t>(count));
    for (std::int16_t i = 0; i < count; ++i) {
        const GuestVector& point = outline->points[i];
        transformed[i] = {MulFix(point.x, matrix->xx, __func__) + MulFix(point.y, matrix->xy, __func__),
                          MulFix(point.x, matrix->yx, __func__) + MulFix(point.y, matrix->yy, __func__)};
    }
    std::copy(transformed.begin(), transformed.end(), outline->points);
}
APS5_EXPORT("0Lv6K1d47jg", sceFreeTypeOtOutlineTransform_nid_no_patch);

}
