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
    pattern->extend = type == CairoPatternTypeMesh ? 0 : CairoExtendPad;
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

constexpr int kPathI[12]{0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 2, 1};
constexpr int kPathJ[12]{0, 1, 2, 3, 3, 3, 3, 2, 1, 0, 0, 0};
constexpr int kControlI[4]{1, 1, 2, 2};
constexpr int kControlJ[4]{1, 2, 2, 1};

CairoPattern* Mesh(CairoPattern* pattern) {
    if (!pattern || pattern->status != CairoStatusSuccess) return nullptr;
    if (pattern->type != CairoPatternTypeMesh) {
        SetError(pattern, CairoStatusPatternTypeMismatch);
        return nullptr;
    }
    return pattern;
}

CairoMeshPoint& PathPoint(CairoMeshPatch& patch, int index) {
    return patch.points[kPathI[index]][kPathJ[index]];
}

void CalculateControlPoint(CairoMeshPatch& patch, int corner) {
    const int row = kControlI[corner];
    const int column = kControlJ[corner];
    CairoMeshPoint* p[3][3]{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            p[i][j] = &patch.points[row ^ i][column ^ j];
    p[0][0]->x = (-4 * p[1][1]->x + 6 * (p[1][0]->x + p[0][1]->x) -
                  2 * (p[1][2]->x + p[2][1]->x) + 3 * (p[2][0]->x + p[0][2]->x) -
                  p[2][2]->x) * (1. / 9);
    p[0][0]->y = (-4 * p[1][1]->y + 6 * (p[1][0]->y + p[0][1]->y) -
                  2 * (p[1][2]->y + p[2][1]->y) + 3 * (p[2][0]->y + p[0][2]->y) -
                  p[2][2]->y) * (1. / 9);
}

}

extern "C" {

CairoPattern* APS5_VABI cairo_pattern_create_mesh() {
    const double geometry[6]{};
    return Create(CairoPatternTypeMesh, geometry);
}

void APS5_VABI cairo_mesh_pattern_begin_patch(CairoPattern* pattern) {
    if (!Mesh(pattern)) return;
    if (pattern->patchActive) {
        SetError(pattern, CairoStatusInvalidMeshConstruction);
        return;
    }
    try {
        pattern->patches.emplace_back();
    } catch (const std::bad_alloc&) {
        SetError(pattern, CairoStatusNoMemory);
        return;
    } catch (const std::length_error&) {
        SetError(pattern, CairoStatusNoMemory);
        return;
    }
    pattern->patchActive = true;
    pattern->currentSide = -2;
    for (bool& hasColor : pattern->hasColor) hasColor = false;
}

void APS5_VABI cairo_mesh_pattern_move_to(CairoPattern* pattern, double x, double y) {
    if (!Mesh(pattern)) return;
    if (!pattern->patchActive || pattern->currentSide >= 0) {
        SetError(pattern, CairoStatusInvalidMeshConstruction);
        return;
    }
    pattern->currentSide = -1;
    pattern->patches.back().points[0][0] = {x, y};
}

void APS5_VABI cairo_mesh_pattern_curve_to(CairoPattern* pattern, double x1, double y1,
                                           double x2, double y2, double x3, double y3) {
    if (!Mesh(pattern)) return;
    if (!pattern->patchActive || pattern->currentSide == 3) {
        SetError(pattern, CairoStatusInvalidMeshConstruction);
        return;
    }
    if (pattern->currentSide == -2)
        cairo_mesh_pattern_move_to(pattern, x1, y1);
    ++pattern->currentSide;
    auto& patch = pattern->patches.back();
    const int index = 3 * pattern->currentSide;
    PathPoint(patch, index + 1) = {x1, y1};
    PathPoint(patch, index + 2) = {x2, y2};
    if (index + 3 < 12)
        PathPoint(patch, index + 3) = {x3, y3};
}

void APS5_VABI cairo_mesh_pattern_line_to(CairoPattern* pattern, double x, double y) {
    if (!Mesh(pattern)) return;
    if (!pattern->patchActive || pattern->currentSide == 3) {
        SetError(pattern, CairoStatusInvalidMeshConstruction);
        return;
    }
    if (pattern->currentSide == -2) {
        cairo_mesh_pattern_move_to(pattern, x, y);
        return;
    }
    const CairoMeshPoint start = PathPoint(pattern->patches.back(), 3 * (pattern->currentSide + 1));
    cairo_mesh_pattern_curve_to(pattern,
                               (2 * start.x + x) * (1. / 3),
                               (2 * start.y + y) * (1. / 3),
                               (start.x + 2 * x) * (1. / 3),
                               (start.y + 2 * y) * (1. / 3), x, y);
}

void APS5_VABI cairo_mesh_pattern_set_corner_color_rgba(CairoPattern* pattern, unsigned int corner,
                                                        double red, double green, double blue, double alpha) {
    if (!Mesh(pattern)) return;
    if (corner > 3) {
        SetError(pattern, CairoStatusInvalidIndex);
        return;
    }
    if (!pattern->patchActive) {
        SetError(pattern, CairoStatusInvalidMeshConstruction);
        return;
    }
    pattern->patches.back().colors[corner] = {Restrict(red), Restrict(green), Restrict(blue), Restrict(alpha)};
    pattern->hasColor[corner] = true;
}

void APS5_VABI cairo_mesh_pattern_end_patch(CairoPattern* pattern) {
    if (!Mesh(pattern)) return;
    if (!pattern->patchActive || pattern->currentSide == -2) {
        SetError(pattern, CairoStatusInvalidMeshConstruction);
        return;
    }
    auto& patch = pattern->patches.back();
    while (pattern->currentSide < 3) {
        const CairoMeshPoint start = patch.points[0][0];
        cairo_mesh_pattern_line_to(pattern, start.x, start.y);
        const int corner = pattern->currentSide + 1;
        if (corner < 4 && !pattern->hasColor[corner]) {
            patch.colors[corner] = patch.colors[0];
            pattern->hasColor[corner] = true;
        }
    }
    for (int i = 0; i < 4; ++i)
        CalculateControlPoint(patch, i);
    pattern->patchActive = false;
}

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
