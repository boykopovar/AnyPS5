/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 David Reveman
 * Copyright © 2005 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of David
 * Reveman not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. David Reveman makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * DAVID REVEMAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL DAVID REVEMAN BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: David Reveman <davidr@novell.com>
 *	    Keith Packard <keithp@keithp.com>
 *	    Carl Worth <cworth@cworth.org>
 */

#include "prx/libcairo/include/Pattern.hpp"

#include <cstring>
#include <new>
#include <stdexcept>

namespace {

CairoPattern* NilPattern() {
    static CairoPattern pattern;
    static const bool initialized = [] {
        pattern.refCount.store(CairoReferenceInvalid);
        pattern.status = CairoStatusNoMemory;
        return true;
    }();
    (void)initialized;
    return &pattern;
}

bool IsReferenceInvalid(const CairoPattern* pattern) {
    return pattern->refCount.load(std::memory_order_relaxed) == CairoReferenceInvalid;
}

void SetError(CairoPattern* pattern, int status) {
    if (pattern->status == CairoStatusSuccess)
        pattern->status = status;
}

CairoPattern* Create(int type, const double (&geometry)[6]) {
    auto* pattern = new (std::nothrow) CairoPattern;
    if (pattern == nullptr)
        return NilPattern();
    pattern->type = type;
    pattern->extend = CairoExtendPad;
    pattern->filter = CairoFilterGood;
    for (int i = 0; i < 6; ++i)
        pattern->geometry[i] = geometry[i];
    return pattern;
}

double Restrict(double value) {
    if (value < 0.0)
        return 0.0;
    if (value > 1.0)
        return 1.0;
    return value;
}

[[gnu::noinline]] void InsertStop(CairoPattern* pattern, const CairoColorStop& stop) {
    try {
        std::size_t position = 0;
        while (position < pattern->stops.size() && !(stop.offset < pattern->stops[position].offset))
            ++position;
        pattern->stops.insert(pattern->stops.begin() + static_cast<std::ptrdiff_t>(position), stop);
    } catch (const std::bad_alloc&) {
        SetError(pattern, CairoStatusNoMemory);
    } catch (const std::length_error&) {
        SetError(pattern, CairoStatusNoMemory);
    }
}

}

extern "C" {

CairoPattern* APS5_VABI cairo_pattern_create_linear(double x0, double y0, double x1, double y1) {
    const double geometry[6] = {x0, y0, x1, y1, 0.0, 0.0};
    return Create(CairoPatternTypeLinear, geometry);
}

CairoPattern* APS5_VABI cairo_pattern_create_radial(double cx0, double cy0, double radius0, double cx1,
                                                    double cy1, double radius1) {
    const double geometry[6] = {cx0, cy0, radius0, cx1, cy1, radius1};
    return Create(CairoPatternTypeRadial, geometry);
}

CairoPattern* APS5_VABI cairo_pattern_reference(CairoPattern* pattern) {
    if (pattern == nullptr || IsReferenceInvalid(pattern))
        return pattern;
    pattern->refCount.fetch_add(1, std::memory_order_relaxed);
    return pattern;
}

void APS5_VABI cairo_pattern_destroy(CairoPattern* pattern) {
    if (pattern == nullptr || IsReferenceInvalid(pattern))
        return;
    if (pattern->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        delete pattern;
}

void APS5_VABI cairo_pattern_set_extend(CairoPattern* pattern, int extend) {
    if (pattern == nullptr || pattern->status != CairoStatusSuccess)
        return;
    pattern->extend = extend;
}

void APS5_VABI cairo_pattern_set_filter(CairoPattern* pattern, int filter) {
    if (pattern == nullptr || pattern->status != CairoStatusSuccess)
        return;
    pattern->filter = filter;
}

void APS5_VABI cairo_pattern_set_matrix(CairoPattern* pattern, const CairoMatrix* matrix) {
    if (pattern == nullptr || matrix == nullptr || pattern->status != CairoStatusSuccess)
        return;
    if (std::memcmp(&pattern->matrix, matrix, sizeof(CairoMatrix)) == 0)
        return;
    pattern->matrix = *matrix;
    CairoMatrix inverse = *matrix;
    const int status = cairo_matrix_invert(&inverse);
    if (status != CairoStatusSuccess)
        SetError(pattern, status);
}

void APS5_VABI cairo_pattern_add_color_stop_rgba(CairoPattern* pattern, double offset, double red, double green,
                                                 double blue, double alpha) {
    if (pattern == nullptr || pattern->status != CairoStatusSuccess)
        return;
    if (pattern->type != CairoPatternTypeLinear && pattern->type != CairoPatternTypeRadial) {
        SetError(pattern, CairoStatusPatternTypeMismatch);
        return;
    }
    InsertStop(pattern, {Restrict(offset), Restrict(red), Restrict(green), Restrict(blue), Restrict(alpha)});
}

}
