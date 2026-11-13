/* Derived from src/cairo-arc.c@1.18.0 and src/cairo-matrix.c@1.18.0 */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#include "prx/libcairo/include/Path.hpp"

#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kMaxFullCircles = 65536;
constexpr double kTolerance = 0.1;
constexpr double kScalingEpsilon = 1.0 / 256.0;

double ArcErrorNormalized(double angle) {
    return 2.0 / 27.0 * std::pow(std::sin(angle / 4), 6) / std::pow(std::cos(angle / 4), 2);
}

double MaxAngleForToleranceNormalized(double tolerance) {
    struct Entry {
        double angle;
        double error;
    };
    static const Entry table[] = {
        {kPi / 1.0, 0.0185185185185185036127},
        {kPi / 2.0, 0.000272567143730179811158},
        {kPi / 3.0, 2.38647043651461047433e-05},
        {kPi / 4.0, 4.2455377443222443279e-06},
        {kPi / 5.0, 1.11281001494389081528e-06},
        {kPi / 6.0, 3.72662000942734705475e-07},
        {kPi / 7.0, 1.47783685574284411325e-07},
        {kPi / 8.0, 6.63240432022601149057e-08},
        {kPi / 9.0, 3.2715520137536980553e-08},
        {kPi / 10.0, 1.73863223499021216974e-08},
        {kPi / 11.0, 9.81410988043554039085e-09},
    };
    constexpr int kTableSize = sizeof(table) / sizeof(table[0]);
    constexpr int kMaxSegments = 1000;
    int i = 0;
    for (; i < kTableSize; ++i)
        if (table[i].error < tolerance) return table[i].angle;
    ++i;
    double angle;
    double error;
    do {
        angle = kPi / i++;
        error = ArcErrorNormalized(angle);
    } while (error > tolerance && i < kMaxSegments);
    return angle;
}

bool HasUnityScale(const CairoMatrix& matrix) {
    const double determinant = matrix.xx * matrix.yy - matrix.yx * matrix.xy;
    if (std::fabs(determinant * determinant - 1.0) < kScalingEpsilon) {
        if (std::fabs(matrix.xy) < kScalingEpsilon && std::fabs(matrix.yx) < kScalingEpsilon) return true;
        if (std::fabs(matrix.xx) < kScalingEpsilon && std::fabs(matrix.yy) < kScalingEpsilon) return true;
    }
    return false;
}

double TransformedCircleMajorAxis(const CairoMatrix& matrix, double radius) {
    if (HasUnityScale(matrix)) return radius;
    const double a = matrix.xx, b = matrix.yx, c = matrix.xy, d = matrix.yy;
    const double i = a * a + b * b;
    const double j = c * c + d * d;
    const double f = 0.5 * (i + j);
    const double g = 0.5 * (i - j);
    const double h = a * c + b * d;
    return radius * std::sqrt(f + std::hypot(g, h));
}

int SegmentsNeeded(double angle, double radius, const CairoMatrix& matrix) {
    const double majorAxis = TransformedCircleMajorAxis(matrix, radius);
    const double maxAngle = MaxAngleForToleranceNormalized(kTolerance / majorAxis);
    return static_cast<int>(std::ceil(std::fabs(angle) / maxAngle));
}

void ArcSegment(CairoContext* context, double xc, double yc, double radius, double angleA, double angleB) {
    const double rSinA = radius * std::sin(angleA);
    const double rCosA = radius * std::cos(angleA);
    const double rSinB = radius * std::sin(angleB);
    const double rCosB = radius * std::cos(angleB);
    const double h = 4.0 / 3.0 * std::tan((angleB - angleA) / 4.0);
    cairo_curve_to(context, xc + rCosA - h * rSinA, yc + rSinA + h * rCosA,
                   xc + rCosB + h * rSinB, yc + rSinB - h * rCosB, xc + rCosB, yc + rSinB);
}

void ArcInDirection(CairoContext* context, double xc, double yc, double radius,
                    double angleMin, double angleMax, bool forward) {
    if (CairoContextStatus(context) != 0) return;
    if (!std::isfinite(angleMax) || !std::isfinite(angleMin)) return;
    if (angleMax - angleMin > 2 * kPi * kMaxFullCircles) {
        angleMax = std::fmod(angleMax - angleMin, 2 * kPi);
        angleMin = std::fmod(angleMin, 2 * kPi);
        angleMax += angleMin + 2 * kPi * kMaxFullCircles;
    }
    if (angleMax - angleMin > kPi) {
        const double angleMid = angleMin + (angleMax - angleMin) / 2.0;
        if (forward) {
            ArcInDirection(context, xc, yc, radius, angleMin, angleMid, forward);
            ArcInDirection(context, xc, yc, radius, angleMid, angleMax, forward);
        } else {
            ArcInDirection(context, xc, yc, radius, angleMid, angleMax, forward);
            ArcInDirection(context, xc, yc, radius, angleMin, angleMid, forward);
        }
    } else if (angleMax != angleMin) {
        CairoMatrix matrix{};
        cairo_get_matrix(context, &matrix);
        int segments = SegmentsNeeded(angleMax - angleMin, radius, matrix);
        double step = (angleMax - angleMin) / segments;
        segments -= 1;
        if (!forward) {
            const double swap = angleMin;
            angleMin = angleMax;
            angleMax = swap;
            step = -step;
        }
        cairo_line_to(context, xc + radius * std::cos(angleMin), yc + radius * std::sin(angleMin));
        for (int i = 0; i < segments; i++, angleMin += step)
            ArcSegment(context, xc, yc, radius, angleMin, angleMin + step);
        ArcSegment(context, xc, yc, radius, angleMin, angleMax);
    } else {
        cairo_line_to(context, xc + radius * std::cos(angleMin), yc + radius * std::sin(angleMin));
    }
}

void Arc(CairoContext* context, double xc, double yc, double radius, double angle1, double angle2, bool forward) {
    if (radius <= 0.0) {
        cairo_line_to(context, xc, yc);
        cairo_line_to(context, xc, yc);
        return;
    }
    cairo_line_to(context, xc + radius * std::cos(angle1), yc + radius * std::sin(angle1));
    if (forward) ArcInDirection(context, xc, yc, radius, angle1, angle2, true);
    else ArcInDirection(context, xc, yc, radius, angle2, angle1, false);
}

} // namespace

extern "C" {

void APS5_VABI cairo_arc(CairoContext* context, double xc, double yc, double radius,
                         double angle1, double angle2) {
    if (CairoContextStatus(context) != 0) return;
    if (angle2 < angle1) {
        angle2 = std::fmod(angle2 - angle1, 2 * kPi);
        if (angle2 < 0) angle2 += 2 * kPi;
        angle2 += angle1;
    }
    Arc(context, xc, yc, radius, angle1, angle2, true);
}

void APS5_VABI cairo_arc_negative(CairoContext* context, double xc, double yc, double radius,
                                  double angle1, double angle2) {
    if (CairoContextStatus(context) != 0) return;
    if (angle2 > angle1) {
        angle2 = std::fmod(angle2 - angle1, 2 * kPi);
        if (angle2 > 0) angle2 -= 2 * kPi;
        angle2 += angle1;
    }
    Arc(context, xc, yc, radius, angle1, angle2, false);
}

}
