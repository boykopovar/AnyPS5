/*
 * Derived from src/cairo-path-in-fill.c, src/cairo-spline.c and
 * src/cairo-fixed-private.h (cairo 1.18.0)
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2007 Mozilla Corporation
 * Copyright © 2008 Chris Wilson
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * src/cairo-spline.c:
 * The Initial Developer of the Original Code is University of Southern
 * California.
 * src/cairo-fixed-private.h:
 * The Initial Developer of the Original Code is Mozilla Foundation
 * src/cairo-path-in-fill.c:
 * The Initial Developer of the Original Code is Chris Wilson.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 *	Vladimir Vukicevic <vladimir@pobox.com>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "InFill.hpp"

#include <bit>
#include <initializer_list>
#include <utility>

namespace {

constexpr int kFixedFractionBits = 8;
constexpr double kFixedOne = static_cast<double>(1 << kFixedFractionBits);
constexpr double kFixedMagic = static_cast<double>(std::int64_t{1} << (52 - kFixedFractionBits)) * 1.5;
constexpr double kTolerance = 0.1;

struct SplineKnots {
    CairoFixedPoint a;
    CairoFixedPoint b;
    CairoFixedPoint c;
    CairoFixedPoint d;
};

std::int32_t Wrap(std::uint32_t value) {
    return static_cast<std::int32_t>(value);
}

std::int32_t Difference(std::int32_t left, std::int32_t right) {
    return Wrap(static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right));
}

std::int32_t Sum(std::int32_t left, std::int32_t right) {
    return Wrap(static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
}

double FixedToDouble(std::int32_t value) {
    return static_cast<double>(value) / kFixedOne;
}

bool Same(CairoFixedPoint left, CairoFixedPoint right) {
    return left.x == right.x && left.y == right.y;
}

int EdgeCompareForYAgainstX(CairoFixedPoint p1, CairoFixedPoint p2, std::int32_t y, std::int32_t x) {
    const std::int32_t adx = Difference(p2.x, p1.x);
    const std::int32_t dx = Difference(x, p1.x);
    if (adx == 0) return Difference(0, dx);
    if ((adx ^ dx) < 0) return adx;
    const std::int32_t dy = Difference(y, p1.y);
    const std::int32_t ady = Difference(p2.y, p1.y);
    const std::int64_t left = std::int64_t{dy} * adx;
    const std::int64_t right = std::int64_t{dx} * ady;
    return left == right ? 0 : left < right ? -1 : 1;
}

void AddEdge(CairoInFillState& state, CairoFixedPoint p1, CairoFixedPoint p2) {
    if (state.onEdge) return;
    const std::int32_t x = state.query.x;
    const std::int32_t y = state.query.y;
    int direction = 1;
    if (p2.y < p1.y) {
        std::swap(p1, p2);
        direction = -1;
    }
    if (Same(p1, state.query) || Same(p2, state.query) ||
        (!(p2.y < y || p1.y > y || (p1.x > x && p2.x > x) || (p1.x < x && p2.x < x)) &&
         EdgeCompareForYAgainstX(p1, p2, y, x) == 0)) {
        state.onEdge = true;
        return;
    }
    if (p2.y <= y || p1.y > y) return;
    if (p1.x >= x && p2.x >= x) return;
    if ((p1.x <= x && p2.x <= x) || EdgeCompareForYAgainstX(p1, p2, y, x) < 0)
        state.winding += direction;
}

bool SplineHasTangents(const SplineKnots& knots) {
    if (Same(knots.a, knots.b) && Same(knots.c, knots.d)) return false;
    if (Same(knots.c, knots.d) && Same(knots.b, knots.d)) return false;
    return true;
}

CairoFixedPoint LerpHalf(CairoFixedPoint a, CairoFixedPoint b) {
    return {Sum(a.x, Difference(b.x, a.x) >> 1), Sum(a.y, Difference(b.y, a.y) >> 1)};
}

SplineKnots DeCasteljau(SplineKnots& s1) {
    const CairoFixedPoint ab = LerpHalf(s1.a, s1.b);
    const CairoFixedPoint bc = LerpHalf(s1.b, s1.c);
    const CairoFixedPoint cd = LerpHalf(s1.c, s1.d);
    const CairoFixedPoint abbc = LerpHalf(ab, bc);
    const CairoFixedPoint bccd = LerpHalf(bc, cd);
    const CairoFixedPoint middle = LerpHalf(abbc, bccd);
    const SplineKnots s2{middle, bccd, cd, s1.d};
    s1.b = ab;
    s1.c = abbc;
    s1.d = middle;
    return s2;
}

void SubtractProjection(double& px, double& py, double dx, double dy, double v) {
    const double u = px * dx + py * dy;
    if (u <= 0) return;
    if (u >= v) {
        px -= dx;
        py -= dy;
    } else {
        px -= u / v * dx;
        py -= u / v * dy;
    }
}

double SplineErrorSquared(const SplineKnots& knots) {
    double bdx = FixedToDouble(Difference(knots.b.x, knots.a.x));
    double bdy = FixedToDouble(Difference(knots.b.y, knots.a.y));
    double cdx = FixedToDouble(Difference(knots.c.x, knots.a.x));
    double cdy = FixedToDouble(Difference(knots.c.y, knots.a.y));
    if (!Same(knots.a, knots.d)) {
        const double dx = FixedToDouble(Difference(knots.d.x, knots.a.x));
        const double dy = FixedToDouble(Difference(knots.d.y, knots.a.y));
        const double v = dx * dx + dy * dy;
        SubtractProjection(bdx, bdy, dx, dy, v);
        SubtractProjection(cdx, cdy, dx, dy, v);
    }
    const double berr = bdx * bdx + bdy * bdy;
    const double cerr = cdx * cdx + cdy * cdy;
    return berr > cerr ? berr : cerr;
}

void SplineDecomposeInto(CairoInFillState& state, CairoFixedPoint& lastPoint, SplineKnots& s1) {
    if (SplineErrorSquared(s1) < kTolerance * kTolerance) {
        if (Same(lastPoint, s1.a)) return;
        lastPoint = s1.a;
        CairoInFillLineTo_nid_no_patch(state, s1.a);
        return;
    }
    SplineKnots s2 = DeCasteljau(s1);
    SplineDecomposeInto(state, lastPoint, s1);
    SplineDecomposeInto(state, lastPoint, s2);
}

} // namespace

std::int32_t CairoFixedFromDouble_nid_no_patch(double value) {
    return Wrap(static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(value + kFixedMagic)));
}

CairoInFillState CairoInFillBegin_nid_no_patch(CairoFixedPoint query) {
    return {false, 0, query, false, {0, 0}, {0, 0}};
}

void CairoInFillMoveTo_nid_no_patch(CairoInFillState& state, CairoFixedPoint point) {
    if (state.hasCurrentPoint) AddEdge(state, state.currentPoint, state.firstPoint);
    state.firstPoint = point;
    state.currentPoint = point;
    state.hasCurrentPoint = true;
}

void CairoInFillLineTo_nid_no_patch(CairoInFillState& state, CairoFixedPoint point) {
    if (state.hasCurrentPoint) AddEdge(state, state.currentPoint, point);
    state.currentPoint = point;
    state.hasCurrentPoint = true;
}

void CairoInFillCurveTo_nid_no_patch(CairoInFillState& state, CairoFixedPoint b,
                                     CairoFixedPoint c, CairoFixedPoint d) {
    std::int32_t top = state.currentPoint.y;
    std::int32_t bottom = top;
    std::int32_t left = state.currentPoint.x;
    for (const CairoFixedPoint point : {b, c, d}) {
        if (point.y < top) top = point.y;
        if (point.y > bottom) bottom = point.y;
        if (point.x < left) left = point.x;
    }
    if (bottom < state.query.y || top > state.query.y || left > state.query.x) {
        state.currentPoint = d;
        return;
    }
    SplineKnots knots{state.currentPoint, b, c, d};
    if (!SplineHasTangents(knots)) return;
    CairoFixedPoint lastPoint = knots.a;
    SplineDecomposeInto(state, lastPoint, knots);
    CairoInFillLineTo_nid_no_patch(state, d);
}

void CairoInFillClosePath_nid_no_patch(CairoInFillState& state) {
    if (!state.hasCurrentPoint) return;
    AddEdge(state, state.currentPoint, state.firstPoint);
    state.hasCurrentPoint = false;
}
