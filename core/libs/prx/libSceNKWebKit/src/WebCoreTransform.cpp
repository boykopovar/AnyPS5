/*
 * Copyright (C) 2005, 2006, 2013 Apple Inc.  All rights reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005-2008, 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
/*
 * Derived from Source/WebCore/platform/graphics/transforms/TransformationMatrix.cpp@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/TransformOperation.h@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/TransformOperation.cpp@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/IdentityTransformOperation.h@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/RotateTransformOperation.h@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/RotateTransformOperation.cpp@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/ScaleTransformOperation.h@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/ScaleTransformOperation.cpp@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/SkewTransformOperation.h@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/SkewTransformOperation.cpp@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/MatrixTransformOperation.h@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/MatrixTransformOperation.cpp@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/Matrix3DTransformOperation.h@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/Matrix3DTransformOperation.cpp@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/TransformOperationExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace WebCore {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kSmallNumber = 1.0e-8;

inline double Deg2Rad(double d) {
    return d * (kPi / 180.0);
}

int ClampToInteger(float value) {
    if (value >= static_cast<float>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    if (!(value > static_cast<float>(std::numeric_limits<int>::min()))) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(value);
}

IntRect EnclosingIntRect(const FloatRect& rect) {
    int x = ClampToInteger(std::floor(rect.m_location.m_x));
    int y = ClampToInteger(std::floor(rect.m_location.m_y));
    int maxX = ClampToInteger(std::ceil(rect.m_location.m_x + rect.m_size.m_width));
    int maxY = ClampToInteger(std::ceil(rect.m_location.m_y + rect.m_size.m_height));
    return IntRect{IntPoint{x, y}, IntSize{maxX - x, maxY - y}};
}

bool IsAffineIdentityOrTranslation(const AffineTransform& t) {
    return t.m_transform[0] == 1.0 && t.m_transform[1] == 0.0
        && t.m_transform[2] == 0.0 && t.m_transform[3] == 1.0;
}

enum class MatrixType : std::uint8_t {
    IdentityOrTranslation,
    Affine,
    Other
};

MatrixType GetMatrixType(const TransformationMatrix& tm) {
    if (tm.m_matrix[0][2] == 0.0 && tm.m_matrix[0][3] == 0.0
        && tm.m_matrix[1][2] == 0.0 && tm.m_matrix[1][3] == 0.0
        && tm.m_matrix[2][3] == 0.0 && tm.m_matrix[2][0] == 0.0
        && tm.m_matrix[2][1] == 0.0 && tm.m_matrix[2][2] == 1.0
        && tm.m_matrix[3][3] == 1.0) {
        if (tm.m_matrix[0][1] == 0.0 && tm.m_matrix[1][0] == 0.0
            && tm.m_matrix[0][0] == 1.0 && tm.m_matrix[1][1] == 1.0) {
            return MatrixType::IdentityOrTranslation;
        }
        if (tm.m_matrix[3][2] == 0.0) {
            return MatrixType::Affine;
        }
    }
    return MatrixType::Other;
}

double Determinant2x2(double a, double b, double c, double d) {
    return a * d - b * c;
}

double Determinant3x3(double a1, double a2, double a3,
                      double b1, double b2, double b3,
                      double c1, double c2, double c3) {
    return a1 * Determinant2x2(b2, b3, c2, c3)
         - b1 * Determinant2x2(a2, a3, c2, c3)
         + c1 * Determinant2x2(a2, a3, b2, b3);
}

double Determinant4x4(const double m[4][4]) {
    double a1 = m[0][0], b1 = m[0][1], c1 = m[0][2], d1 = m[0][3];
    double a2 = m[1][0], b2 = m[1][1], c2 = m[1][2], d2 = m[1][3];
    double a3 = m[2][0], b3 = m[2][1], c3 = m[2][2], d3 = m[2][3];
    double a4 = m[3][0], b4 = m[3][1], c4 = m[3][2], d4 = m[3][3];

    return a1 * Determinant3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4)
         - b1 * Determinant3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4)
         + c1 * Determinant3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4)
         - d1 * Determinant3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4);
}

void MultVecMatrix2D(const TransformationMatrix& tm, double x, double y, double& dstX, double& dstY) {
    dstX = tm.m_matrix[3][0] + x * tm.m_matrix[0][0] + y * tm.m_matrix[1][0];
    dstY = tm.m_matrix[3][1] + x * tm.m_matrix[0][1] + y * tm.m_matrix[1][1];
    double w = tm.m_matrix[3][3] + x * tm.m_matrix[0][3] + y * tm.m_matrix[1][3];
    if (w != 1.0 && w != 0.0) {
        dstX /= w;
        dstY /= w;
    }
}

FloatPoint InternalMapPoint(const TransformationMatrix& tm, const FloatPoint& p) {
    double rx = 0.0;
    double ry = 0.0;
    MultVecMatrix2D(tm, static_cast<double>(p.m_x), static_cast<double>(p.m_y), rx, ry);
    return FloatPoint{static_cast<float>(rx), static_cast<float>(ry)};
}

inline double Rad2Deg(double r) {
    return r * (180.0 / kPi);
}

bool IsIdentity(const TransformationMatrix& tm) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (tm.m_matrix[i][j] != (i == j ? 1.0 : 0.0)) {
                return false;
            }
        }
    }
    return true;
}

bool IsAffine(const TransformationMatrix& tm) {
    return tm.m_matrix[0][2] == 0.0 && tm.m_matrix[0][3] == 0.0
        && tm.m_matrix[1][2] == 0.0 && tm.m_matrix[1][3] == 0.0
        && tm.m_matrix[2][0] == 0.0 && tm.m_matrix[2][1] == 0.0
        && tm.m_matrix[2][2] == 1.0 && tm.m_matrix[2][3] == 0.0
        && tm.m_matrix[3][2] == 0.0 && tm.m_matrix[3][3] == 1.0;
}

void Adjoint(const double m[4][4], double result[4][4]) {
    double a1 = m[0][0], b1 = m[0][1], c1 = m[0][2], d1 = m[0][3];
    double a2 = m[1][0], b2 = m[1][1], c2 = m[1][2], d2 = m[1][3];
    double a3 = m[2][0], b3 = m[2][1], c3 = m[2][2], d3 = m[2][3];
    double a4 = m[3][0], b4 = m[3][1], c4 = m[3][2], d4 = m[3][3];

    result[0][0] = Determinant3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4);
    result[1][0] = -Determinant3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4);
    result[2][0] = Determinant3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4);
    result[3][0] = -Determinant3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4);

    result[0][1] = -Determinant3x3(b1, b3, b4, c1, c3, c4, d1, d3, d4);
    result[1][1] = Determinant3x3(a1, a3, a4, c1, c3, c4, d1, d3, d4);
    result[2][1] = -Determinant3x3(a1, a3, a4, b1, b3, b4, d1, d3, d4);
    result[3][1] = Determinant3x3(a1, a3, a4, b1, b3, b4, c1, c3, c4);

    result[0][2] = Determinant3x3(b1, b2, b4, c1, c2, c4, d1, d2, d4);
    result[1][2] = -Determinant3x3(a1, a2, a4, c1, c2, c4, d1, d2, d4);
    result[2][2] = Determinant3x3(a1, a2, a4, b1, b2, b4, d1, d2, d4);
    result[3][2] = -Determinant3x3(a1, a2, a4, b1, b2, b4, c1, c2, c4);

    result[0][3] = -Determinant3x3(b1, b2, b3, c1, c2, c3, d1, d2, d3);
    result[1][3] = Determinant3x3(a1, a2, a3, c1, c2, c3, d1, d2, d3);
    result[2][3] = -Determinant3x3(a1, a2, a3, b1, b2, b3, d1, d2, d3);
    result[3][3] = Determinant3x3(a1, a2, a3, b1, b2, b3, c1, c2, c3);
}

bool InverseMatrix(const double m[4][4], double result[4][4]) {
    Adjoint(m, result);
    double det = Determinant4x4(m);
    if (std::abs(det) < kSmallNumber) {
        return false;
    }
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result[i][j] = result[i][j] / det;
        }
    }
    return true;
}

double V3Length(const double a[3]) {
    return std::hypot(a[0], a[1], a[2]);
}

void V3Scale(double v[3], double desiredLength) {
    double len = V3Length(v);
    if (len != 0.0) {
        double l = desiredLength / len;
        v[0] *= l;
        v[1] *= l;
        v[2] *= l;
    }
}

double V3Dot(const double a[3], const double b[3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void V3Combine(const double a[3], const double b[3], double result[3], double ascl, double bscl) {
    result[0] = ascl * a[0] + bscl * b[0];
    result[1] = ascl * a[1] + bscl * b[1];
    result[2] = ascl * a[2] + bscl * b[2];
}

void V3Cross(const double a[3], const double b[3], double result[3]) {
    result[0] = a[1] * b[2] - a[2] * b[1];
    result[1] = a[2] * b[0] - a[0] * b[2];
    result[2] = a[0] * b[1] - a[1] * b[0];
}

struct Decomposed2 {
    double scaleX, scaleY;
    double translateX, translateY;
    double angle;
    double m11, m12, m21, m22;
};

struct Decomposed4 {
    double scaleX, scaleY, scaleZ;
    double skewXY, skewXZ, skewYZ;
    double quaternion[4];
    double translateX, translateY, translateZ;
    double perspectiveX, perspectiveY, perspectiveZ, perspectiveW;
};

bool Decompose2(const TransformationMatrix& tm, Decomposed2& result) {
    if (IsIdentity(tm)) {
        result = Decomposed2{};
        result.scaleX = 1.0;
        result.scaleY = 1.0;
        result.m11 = 1.0;
        result.m22 = 1.0;
        return true;
    }
    double row0x = tm.m_matrix[0][0];
    double row0y = tm.m_matrix[0][1];
    double row1x = tm.m_matrix[1][0];
    double row1y = tm.m_matrix[1][1];
    result.translateX = tm.m_matrix[3][0];
    result.translateY = tm.m_matrix[3][1];
    result.scaleX = std::hypot(row0x, row0y);
    result.scaleY = std::hypot(row1x, row1y);
    double determinant = row0x * row1y - row0y * row1x;
    if (determinant < 0.0) {
        if (row0x < row1y) {
            result.scaleX = -result.scaleX;
        } else {
            result.scaleY = -result.scaleY;
        }
    }
    if (result.scaleX != 0.0) {
        row0x *= 1.0 / result.scaleX;
        row0y *= 1.0 / result.scaleX;
    }
    if (result.scaleY != 0.0) {
        row1x *= 1.0 / result.scaleY;
        row1y *= 1.0 / result.scaleY;
    }
    result.angle = std::atan2(row0y, row0x);
    if (result.angle != 0.0) {
        double sn = -row0y;
        double cs = row0x;
        double m11 = row0x, m12 = row0y;
        double m21 = row1x, m22 = row1y;
        row0x = cs * m11 + sn * m21;
        row0y = cs * m12 + sn * m22;
        row1x = -sn * m11 + cs * m21;
        row1y = -sn * m12 + cs * m22;
    }
    result.m11 = row0x;
    result.m12 = row0y;
    result.m21 = row1x;
    result.m22 = row1y;
    result.angle = Rad2Deg(result.angle);
    return true;
}

bool Decompose4(const TransformationMatrix& tm, Decomposed4& result) {
    if (IsIdentity(tm)) {
        result = Decomposed4{};
        result.perspectiveW = 1.0;
        result.scaleX = 1.0;
        result.scaleY = 1.0;
        result.scaleZ = 1.0;
        return true;
    }
    double local[4][4];
    std::memcpy(local, tm.m_matrix, sizeof(local));
    if (local[3][3] == 0.0) {
        return false;
    }
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            local[i][j] /= local[3][3];
        }
    }
    double perspective[4][4];
    std::memcpy(perspective, local, sizeof(perspective));
    for (int i = 0; i < 3; ++i) {
        perspective[i][3] = 0.0;
    }
    perspective[3][3] = 1.0;
    if (Determinant4x4(perspective) == 0.0) {
        return false;
    }
    if (local[0][3] != 0.0 || local[1][3] != 0.0 || local[2][3] != 0.0) {
        double rightHandSide[4] = {local[0][3], local[1][3], local[2][3], local[3][3]};
        double inversePerspective[4][4];
        InverseMatrix(perspective, inversePerspective);
        double transposed[4][4];
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                transposed[i][j] = inversePerspective[j][i];
            }
        }
        double point[4];
        for (int j = 0; j < 4; ++j) {
            point[j] = rightHandSide[0] * transposed[0][j] + rightHandSide[1] * transposed[1][j]
                     + rightHandSide[2] * transposed[2][j] + rightHandSide[3] * transposed[3][j];
        }
        result.perspectiveX = point[0];
        result.perspectiveY = point[1];
        result.perspectiveZ = point[2];
        result.perspectiveW = point[3];
        local[0][3] = local[1][3] = local[2][3] = 0.0;
        local[3][3] = 1.0;
    } else {
        result.perspectiveX = result.perspectiveY = result.perspectiveZ = 0.0;
        result.perspectiveW = 1.0;
    }
    result.translateX = local[3][0];
    local[3][0] = 0.0;
    result.translateY = local[3][1];
    local[3][1] = 0.0;
    result.translateZ = local[3][2];
    local[3][2] = 0.0;

    double row[3][3];
    for (int i = 0; i < 3; ++i) {
        row[i][0] = local[i][0];
        row[i][1] = local[i][1];
        row[i][2] = local[i][2];
    }
    result.scaleX = V3Length(row[0]);
    V3Scale(row[0], 1.0);
    result.skewXY = V3Dot(row[0], row[1]);
    V3Combine(row[1], row[0], row[1], 1.0, -result.skewXY);
    result.scaleY = V3Length(row[1]);
    V3Scale(row[1], 1.0);
    result.skewXY /= result.scaleY;
    result.skewXZ = V3Dot(row[0], row[2]);
    V3Combine(row[2], row[0], row[2], 1.0, -result.skewXZ);
    result.skewYZ = V3Dot(row[1], row[2]);
    V3Combine(row[2], row[1], row[2], 1.0, -result.skewYZ);
    result.scaleZ = V3Length(row[2]);
    V3Scale(row[2], 1.0);
    result.skewXZ /= result.scaleZ;
    result.skewYZ /= result.scaleZ;

    double cross[3];
    V3Cross(row[1], row[2], cross);
    if (V3Dot(row[0], cross) < 0.0) {
        result.scaleX *= -1.0;
        result.scaleY *= -1.0;
        result.scaleZ *= -1.0;
        for (int i = 0; i < 3; ++i) {
            row[i][0] *= -1.0;
            row[i][1] *= -1.0;
            row[i][2] *= -1.0;
        }
    }

    double s, x, y, z, w;
    double t = row[0][0] + row[1][1] + row[2][2] + 1.0;
    if (t > 1e-4) {
        s = 0.5 / std::sqrt(t);
        w = 0.25 / s;
        x = (row[2][1] - row[1][2]) * s;
        y = (row[0][2] - row[2][0]) * s;
        z = (row[1][0] - row[0][1]) * s;
    } else if (row[0][0] > row[1][1] && row[0][0] > row[2][2]) {
        s = std::sqrt(1.0 + row[0][0] - row[1][1] - row[2][2]) * 2.0;
        x = 0.25 * s;
        y = (row[0][1] + row[1][0]) / s;
        z = (row[0][2] + row[2][0]) / s;
        w = (row[2][1] - row[1][2]) / s;
    } else if (row[1][1] > row[2][2]) {
        s = std::sqrt(1.0 + row[1][1] - row[0][0] - row[2][2]) * 2.0;
        x = (row[0][1] + row[1][0]) / s;
        y = 0.25 * s;
        z = (row[1][2] + row[2][1]) / s;
        w = (row[0][2] - row[2][0]) / s;
    } else {
        s = std::sqrt(1.0 + row[2][2] - row[0][0] - row[1][1]) * 2.0;
        x = (row[0][2] + row[2][0]) / s;
        y = (row[1][2] + row[2][1]) / s;
        z = 0.25 * s;
        w = (row[1][0] - row[0][1]) / s;
    }
    result.quaternion[0] = x;
    result.quaternion[1] = y;
    result.quaternion[2] = z;
    result.quaternion[3] = w;
    return true;
}

void Slerp(double qa[4], const double qb[4], double t) {
    double ax = qa[0], ay = qa[1], az = qa[2], aw = qa[3];
    double bx = qb[0], by = qb[1], bz = qb[2], bw = qb[3];
    double scale, invscale;
    double angle = ax * bx + ay * by + az * bz + aw * bw;
    if (angle < 0.0) {
        ax = -ax;
        ay = -ay;
        az = -az;
        aw = -aw;
        angle = -angle;
    }
    if (angle + 1.0 > .05) {
        if (1.0 - angle >= .05) {
            double th = std::acos(angle);
            double invth = 1.0 / std::sin(th);
            scale = std::sin(th * (1.0 - t)) * invth;
            invscale = std::sin(th * t) * invth;
        } else {
            scale = 1.0 - t;
            invscale = t;
        }
    } else {
        bx = -ay;
        by = ax;
        bz = -aw;
        bw = az;
        scale = std::sin(kPi * (.5 - t));
        invscale = std::sin(kPi * t);
    }
    qa[0] = ax * scale + bx * invscale;
    qa[1] = ay * scale + by * invscale;
    qa[2] = az * scale + bz * invscale;
    qa[3] = aw * scale + bw * invscale;
}

void BlendFloat(double& from, double to, double progress) {
    if (from != to) {
        from = from + (to - from) * progress;
    }
}

}  // namespace
}  // namespace WebCore

extern "C" {
void APS5_VABI _ZN7WebCore15AffineTransform12makeIdentityEv(WebCore::AffineTransform* self) {
    self->m_transform = {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform15scaleNonUniformEdd(
    WebCore::AffineTransform* self, double sx, double sy) {
    self->m_transform[0] *= sx;
    self->m_transform[1] *= sx;
    self->m_transform[2] *= sy;
    self->m_transform[3] *= sy;
    return *self;
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform4skewEdd(
    WebCore::AffineTransform* self, double angleX, double angleY) {
    double sx = std::tan(WebCore::Deg2Rad(angleX));
    double sy = std::tan(WebCore::Deg2Rad(angleY));
    double a = self->m_transform[0];
    double b = self->m_transform[1];
    self->m_transform[0] += sy * self->m_transform[2];
    self->m_transform[1] += sy * self->m_transform[3];
    self->m_transform[2] += sx * a;
    self->m_transform[3] += sx * b;
    return *self;
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform5flipXEv(WebCore::AffineTransform* self) {
    self->m_transform[0] = -self->m_transform[0];
    self->m_transform[1] = -self->m_transform[1];
    return *self;
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform5flipYEv(WebCore::AffineTransform* self) {
    self->m_transform[2] = -self->m_transform[2];
    self->m_transform[3] = -self->m_transform[3];
    return *self;
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform5scaleERKNS_9FloatSizeE(
    WebCore::AffineTransform* self, const WebCore::FloatSize& s) {
    return _ZN7WebCore15AffineTransform15scaleNonUniformEdd(self, static_cast<double>(s.m_width), static_cast<double>(s.m_height));
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform5scaleEd(WebCore::AffineTransform* self, double s) {
    return _ZN7WebCore15AffineTransform15scaleNonUniformEdd(self, s, s);
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform5shearEdd(
    WebCore::AffineTransform* self, double sx, double sy) {
    double a = self->m_transform[0];
    double b = self->m_transform[1];
    self->m_transform[0] += sy * self->m_transform[2];
    self->m_transform[1] += sy * self->m_transform[3];
    self->m_transform[2] += sx * a;
    self->m_transform[3] += sx * b;
    return *self;
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform6rotateEd(WebCore::AffineTransform* self, double a) {
    double rad = WebCore::Deg2Rad(a);
    double cosAngle = std::cos(rad);
    double sinAngle = std::sin(rad);
    WebCore::AffineTransform rot;
    rot.m_transform = {cosAngle, sinAngle, -sinAngle, cosAngle, 0.0, 0.0};
    return _ZN7WebCore15AffineTransform8multiplyERKS0_(self, rot);
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform8multiplyERKS0_(
    WebCore::AffineTransform* self, const WebCore::AffineTransform& other) {
    WebCore::AffineTransform trans;
    trans.m_transform[0] = other.m_transform[0] * self->m_transform[0] + other.m_transform[1] * self->m_transform[2];
    trans.m_transform[1] = other.m_transform[0] * self->m_transform[1] + other.m_transform[1] * self->m_transform[3];
    trans.m_transform[2] = other.m_transform[2] * self->m_transform[0] + other.m_transform[3] * self->m_transform[2];
    trans.m_transform[3] = other.m_transform[2] * self->m_transform[1] + other.m_transform[3] * self->m_transform[3];
    trans.m_transform[4] = other.m_transform[4] * self->m_transform[0] + other.m_transform[5] * self->m_transform[2] + self->m_transform[4];
    trans.m_transform[5] = other.m_transform[4] * self->m_transform[1] + other.m_transform[5] * self->m_transform[3] + self->m_transform[5];
    self->m_transform = trans.m_transform;
    return *self;
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform9translateERKNS_10FloatPointE(
    WebCore::AffineTransform* self, const WebCore::FloatPoint& p) {
    return _ZN7WebCore15AffineTransform9translateEdd(self, static_cast<double>(p.m_x), static_cast<double>(p.m_y));
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform9translateERKNS_9FloatSizeE(
    WebCore::AffineTransform* self, const WebCore::FloatSize& s) {
    return _ZN7WebCore15AffineTransform9translateEdd(self, static_cast<double>(s.m_width), static_cast<double>(s.m_height));
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform9translateEdd(
    WebCore::AffineTransform* self, double tx, double ty) {
    if (WebCore::IsAffineIdentityOrTranslation(*self)) {
        self->m_transform[4] += tx;
        self->m_transform[5] += ty;
        return *self;
    }
    self->m_transform[4] += tx * self->m_transform[0] + ty * self->m_transform[2];
    self->m_transform[5] += tx * self->m_transform[1] + ty * self->m_transform[3];
    return *self;
}

bool APS5_VABI _ZNK7WebCore15AffineTransform10isIdentityEv(const WebCore::AffineTransform* self) {
    return self->m_transform[0] == 1.0 && self->m_transform[1] == 0.0
        && self->m_transform[2] == 0.0 && self->m_transform[3] == 1.0
        && self->m_transform[4] == 0.0 && self->m_transform[5] == 0.0;
}

WebCore::TransformationMatrix APS5_VABI _ZNK7WebCore15AffineTransform22toTransformationMatrixEv(
    const WebCore::AffineTransform* self) {
    WebCore::TransformationMatrix result;
    result.m_matrix[0][0] = self->m_transform[0];
    result.m_matrix[0][1] = self->m_transform[1];
    result.m_matrix[0][2] = 0.0;
    result.m_matrix[0][3] = 0.0;
    result.m_matrix[1][0] = self->m_transform[2];
    result.m_matrix[1][1] = self->m_transform[3];
    result.m_matrix[1][2] = 0.0;
    result.m_matrix[1][3] = 0.0;
    result.m_matrix[2][0] = 0.0;
    result.m_matrix[2][1] = 0.0;
    result.m_matrix[2][2] = 1.0;
    result.m_matrix[2][3] = 0.0;
    result.m_matrix[3][0] = self->m_transform[4];
    result.m_matrix[3][1] = self->m_transform[5];
    result.m_matrix[3][2] = 0.0;
    result.m_matrix[3][3] = 1.0;
    return result;
}

double APS5_VABI _ZNK7WebCore15AffineTransform6xScaleEv(const WebCore::AffineTransform* self) {
    return std::hypot(self->m_transform[0], self->m_transform[1]);
}

double APS5_VABI _ZNK7WebCore15AffineTransform6yScaleEv(const WebCore::AffineTransform* self) {
    return std::hypot(self->m_transform[2], self->m_transform[3]);
}

WebCore::FloatQuad APS5_VABI _ZNK7WebCore15AffineTransform7mapQuadERKNS_9FloatQuadE(
    const WebCore::AffineTransform* self, const WebCore::FloatQuad& q) {
    if (WebCore::IsAffineIdentityOrTranslation(*self)) {
        float tx = static_cast<float>(self->m_transform[4]);
        float ty = static_cast<float>(self->m_transform[5]);
        return WebCore::FloatQuad{
            WebCore::FloatPoint{q.m_p1.m_x + tx, q.m_p1.m_y + ty},
            WebCore::FloatPoint{q.m_p2.m_x + tx, q.m_p2.m_y + ty},
            WebCore::FloatPoint{q.m_p3.m_x + tx, q.m_p3.m_y + ty},
            WebCore::FloatPoint{q.m_p4.m_x + tx, q.m_p4.m_y + ty}
        };
    }
    return WebCore::FloatQuad{
        _ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE(self, q.m_p1),
        _ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE(self, q.m_p2),
        _ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE(self, q.m_p3),
        _ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE(self, q.m_p4)
    };
}

WebCore::IntRect APS5_VABI _ZNK7WebCore15AffineTransform7mapRectERKNS_7IntRectE(
    const WebCore::AffineTransform* self, const WebCore::IntRect& r) {
    WebCore::FloatRect fr{
        WebCore::FloatPoint{static_cast<float>(r.m_location.m_x), static_cast<float>(r.m_location.m_y)},
        WebCore::FloatSize{static_cast<float>(r.m_size.m_width), static_cast<float>(r.m_size.m_height)}
    };
    WebCore::FloatRect mapped = _ZNK7WebCore15AffineTransform7mapRectERKNS_9FloatRectE(self, fr);
    return WebCore::EnclosingIntRect(mapped);
}

WebCore::FloatRect APS5_VABI _ZNK7WebCore15AffineTransform7mapRectERKNS_9FloatRectE(
    const WebCore::AffineTransform* self, const WebCore::FloatRect& rect) {
    if (WebCore::IsAffineIdentityOrTranslation(*self)) {
        return WebCore::FloatRect{
            WebCore::FloatPoint{rect.m_location.m_x + static_cast<float>(self->m_transform[4]),
                                rect.m_location.m_y + static_cast<float>(self->m_transform[5])},
            rect.m_size
        };
    }
    WebCore::FloatPoint p1 = _ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE(self, rect.m_location);
    WebCore::FloatPoint p2 = _ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE(
        self, WebCore::FloatPoint{rect.m_location.m_x + rect.m_size.m_width, rect.m_location.m_y});
    WebCore::FloatPoint p3 = _ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE(
        self, WebCore::FloatPoint{rect.m_location.m_x + rect.m_size.m_width, rect.m_location.m_y + rect.m_size.m_height});
    WebCore::FloatPoint p4 = _ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE(
        self, WebCore::FloatPoint{rect.m_location.m_x, rect.m_location.m_y + rect.m_size.m_height});
    float left = std::min({p1.m_x, p2.m_x, p3.m_x, p4.m_x});
    float top = std::min({p1.m_y, p2.m_y, p3.m_y, p4.m_y});
    float right = std::max({p1.m_x, p2.m_x, p3.m_x, p4.m_x});
    float bottom = std::max({p1.m_y, p2.m_y, p3.m_y, p4.m_y});
    return WebCore::FloatRect{WebCore::FloatPoint{left, top}, WebCore::FloatSize{right - left, bottom - top}};
}

WebCore::IntSize APS5_VABI _ZNK7WebCore15AffineTransform7mapSizeERKNS_7IntSizeE(
    const WebCore::AffineTransform* self, const WebCore::IntSize& size) {
    double width2 = static_cast<double>(size.m_width) * _ZNK7WebCore15AffineTransform6xScaleEv(self);
    double height2 = static_cast<double>(size.m_height) * _ZNK7WebCore15AffineTransform6yScaleEv(self);
    return WebCore::IntSize{
        static_cast<int>(std::lround(width2)),
        static_cast<int>(std::lround(height2))
    };
}

WebCore::FloatSize APS5_VABI _ZNK7WebCore15AffineTransform7mapSizeERKNS_9FloatSizeE(
    const WebCore::AffineTransform* self, const WebCore::FloatSize& size) {
    double width2 = static_cast<double>(size.m_width) * _ZNK7WebCore15AffineTransform6xScaleEv(self);
    double height2 = static_cast<double>(size.m_height) * _ZNK7WebCore15AffineTransform6yScaleEv(self);
    return WebCore::FloatSize{static_cast<float>(width2), static_cast<float>(height2)};
}

WebCore::FloatPoint APS5_VABI _ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE(
    const WebCore::AffineTransform* self, const WebCore::FloatPoint& point) {
    double x = static_cast<double>(point.m_x);
    double y = static_cast<double>(point.m_y);
    double x2 = self->m_transform[0] * x + self->m_transform[2] * y + self->m_transform[4];
    double y2 = self->m_transform[1] * x + self->m_transform[3] * y + self->m_transform[5];
    return WebCore::FloatPoint{static_cast<float>(x2), static_cast<float>(y2)};
}

WebCore::IntPoint APS5_VABI _ZNK7WebCore15AffineTransform8mapPointERKNS_8IntPointE(
    const WebCore::AffineTransform* self, const WebCore::IntPoint& point) {
    double x = static_cast<double>(point.m_x);
    double y = static_cast<double>(point.m_y);
    double x2 = self->m_transform[0] * x + self->m_transform[2] * y + self->m_transform[4];
    double y2 = self->m_transform[1] * x + self->m_transform[3] * y + self->m_transform[5];
    return WebCore::IntPoint{
        static_cast<int>(std::lround(x2)),
        static_cast<int>(std::lround(y2))
    };
}

void APS5_VABI _ZN7WebCore20TransformationMatrix10makeAffineEv(WebCore::TransformationMatrix* self) {
    self->m_matrix[0][2] = 0.0;
    self->m_matrix[0][3] = 0.0;
    self->m_matrix[1][2] = 0.0;
    self->m_matrix[1][3] = 0.0;
    self->m_matrix[2][0] = 0.0;
    self->m_matrix[2][1] = 0.0;
    self->m_matrix[2][2] = 1.0;
    self->m_matrix[2][3] = 0.0;
    self->m_matrix[3][2] = 0.0;
    self->m_matrix[3][3] = 1.0;
}

WebCore::TransformationMatrix APS5_VABI _ZN7WebCore20TransformationMatrix10rectToRectERKNS_9FloatRectES3_(
    const WebCore::FloatRect& from, const WebCore::FloatRect& to) {
    WebCore::TransformationMatrix result;
    result.m_matrix[0][0] = static_cast<double>(to.m_size.m_width / from.m_size.m_width);
    result.m_matrix[0][1] = 0.0;
    result.m_matrix[0][2] = 0.0;
    result.m_matrix[0][3] = 0.0;
    result.m_matrix[1][0] = 0.0;
    result.m_matrix[1][1] = static_cast<double>(to.m_size.m_height / from.m_size.m_height);
    result.m_matrix[1][2] = 0.0;
    result.m_matrix[1][3] = 0.0;
    result.m_matrix[2][0] = 0.0;
    result.m_matrix[2][1] = 0.0;
    result.m_matrix[2][2] = 1.0;
    result.m_matrix[2][3] = 0.0;
    result.m_matrix[3][0] = static_cast<double>(to.m_location.m_x - from.m_location.m_x);
    result.m_matrix[3][1] = static_cast<double>(to.m_location.m_y - from.m_location.m_y);
    result.m_matrix[3][2] = 0.0;
    result.m_matrix[3][3] = 1.0;
    return result;
}

WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix11translate3dEddd(
    WebCore::TransformationMatrix* self, double tx, double ty, double tz) {
    self->m_matrix[3][0] += tx * self->m_matrix[0][0] + ty * self->m_matrix[1][0] + tz * self->m_matrix[2][0];
    self->m_matrix[3][1] += tx * self->m_matrix[0][1] + ty * self->m_matrix[1][1] + tz * self->m_matrix[2][1];
    self->m_matrix[3][2] += tx * self->m_matrix[0][2] + ty * self->m_matrix[1][2] + tz * self->m_matrix[2][2];
    self->m_matrix[3][3] += tx * self->m_matrix[0][3] + ty * self->m_matrix[1][3] + tz * self->m_matrix[2][3];
    return *self;
}

WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix15scaleNonUniformEdd(
    WebCore::TransformationMatrix* self, double sx, double sy) {
    self->m_matrix[0][0] *= sx;
    self->m_matrix[0][1] *= sx;
    self->m_matrix[0][2] *= sx;
    self->m_matrix[0][3] *= sx;

    self->m_matrix[1][0] *= sy;
    self->m_matrix[1][1] *= sy;
    self->m_matrix[1][2] *= sy;
    self->m_matrix[1][3] *= sy;
    return *self;
}

WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix4skewEdd(
    WebCore::TransformationMatrix* self, double sx, double sy) {
    double radX = WebCore::Deg2Rad(sx);
    double radY = WebCore::Deg2Rad(sy);
    WebCore::TransformationMatrix mat;
    mat.m_matrix[0][1] = std::tan(radY);
    mat.m_matrix[1][0] = std::tan(radX);
    return _ZN7WebCore20TransformationMatrix8multiplyERKS0_(self, mat);
}

WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix5flipXEv(WebCore::TransformationMatrix* self) {
    return _ZN7WebCore20TransformationMatrix15scaleNonUniformEdd(self, -1.0, 1.0);
}

WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix5flipYEv(WebCore::TransformationMatrix* self) {
    return _ZN7WebCore20TransformationMatrix15scaleNonUniformEdd(self, 1.0, -1.0);
}

WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix5scaleEd(
    WebCore::TransformationMatrix* self, double s) {
    return _ZN7WebCore20TransformationMatrix15scaleNonUniformEdd(self, s, s);
}

WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix6rotateEd(
    WebCore::TransformationMatrix* self, double a) {
    return _ZN7WebCore20TransformationMatrix8rotate3dEddd(self, 0.0, 0.0, a);
}

WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix8multiplyERKS0_(
    WebCore::TransformationMatrix* self, const WebCore::TransformationMatrix& mat) {
    double result[4][4];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result[i][j] = mat.m_matrix[i][0] * self->m_matrix[0][j]
                         + mat.m_matrix[i][1] * self->m_matrix[1][j]
                         + mat.m_matrix[i][2] * self->m_matrix[2][j]
                         + mat.m_matrix[i][3] * self->m_matrix[3][j];
        }
    }
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            self->m_matrix[i][j] = result[i][j];
        }
    }
    return *self;
}

WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix16applyPerspectiveEd(
    WebCore::TransformationMatrix* self, double perspective) {
    WebCore::TransformationMatrix mat;
    if (perspective != 0.0) mat.m_matrix[2][3] = -1.0 / perspective;
    return _ZN7WebCore20TransformationMatrix8multiplyERKS0_(self, mat);
}

WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix8rotate3dEddd(
    WebCore::TransformationMatrix* self, double rx, double ry, double rz) {
    double radX = WebCore::Deg2Rad(rx);
    double radY = WebCore::Deg2Rad(ry);
    double radZ = WebCore::Deg2Rad(rz);

    WebCore::TransformationMatrix rmat;
    double sinTheta = std::sin(radZ);
    double cosTheta = std::cos(radZ);
    rmat.m_matrix[0][0] = cosTheta;
    rmat.m_matrix[0][1] = sinTheta;
    rmat.m_matrix[1][0] = -sinTheta;
    rmat.m_matrix[1][1] = cosTheta;

    WebCore::TransformationMatrix matY;
    sinTheta = std::sin(radY);
    cosTheta = std::cos(radY);
    matY.m_matrix[0][0] = cosTheta;
    matY.m_matrix[0][2] = -sinTheta;
    matY.m_matrix[2][0] = sinTheta;
    matY.m_matrix[2][2] = cosTheta;
    _ZN7WebCore20TransformationMatrix8multiplyERKS0_(&rmat, matY);

    WebCore::TransformationMatrix matX;
    sinTheta = std::sin(radX);
    cosTheta = std::cos(radX);
    matX.m_matrix[1][1] = cosTheta;
    matX.m_matrix[1][2] = sinTheta;
    matX.m_matrix[2][1] = -sinTheta;
    matX.m_matrix[2][2] = cosTheta;
    _ZN7WebCore20TransformationMatrix8multiplyERKS0_(&rmat, matX);

    return _ZN7WebCore20TransformationMatrix8multiplyERKS0_(self, rmat);
}

WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix9translateEdd(
    WebCore::TransformationMatrix* self, double tx, double ty) {
    return _ZN7WebCore20TransformationMatrix11translate3dEddd(self, tx, ty, 0.0);
}

void APS5_VABI _ZN7WebCore20TransformationMatrixC1ERKNS_15AffineTransformE(
    WebCore::TransformationMatrix* self, const WebCore::AffineTransform& t) {
    self->m_matrix[0][0] = t.m_transform[0];
    self->m_matrix[0][1] = t.m_transform[1];
    self->m_matrix[0][2] = 0.0;
    self->m_matrix[0][3] = 0.0;
    self->m_matrix[1][0] = t.m_transform[2];
    self->m_matrix[1][1] = t.m_transform[3];
    self->m_matrix[1][2] = 0.0;
    self->m_matrix[1][3] = 0.0;
    self->m_matrix[2][0] = 0.0;
    self->m_matrix[2][1] = 0.0;
    self->m_matrix[2][2] = 1.0;
    self->m_matrix[2][3] = 0.0;
    self->m_matrix[3][0] = t.m_transform[4];
    self->m_matrix[3][1] = t.m_transform[5];
    self->m_matrix[3][2] = 0.0;
    self->m_matrix[3][3] = 1.0;
}

void APS5_VABI _ZN7WebCore20TransformationMatrixC2ERKNS_15AffineTransformE(
    WebCore::TransformationMatrix* self, const WebCore::AffineTransform& t) {
    _ZN7WebCore20TransformationMatrixC1ERKNS_15AffineTransformE(self, t);
}

bool APS5_VABI _ZNK7WebCore20TransformationMatrix12isInvertibleEv(const WebCore::TransformationMatrix* self) {
    auto type = WebCore::GetMatrixType(*self);
    if (type == WebCore::MatrixType::IdentityOrTranslation) {
        return true;
    }
    double det = (type == WebCore::MatrixType::Affine)
        ? (self->m_matrix[0][0] * self->m_matrix[1][1] - self->m_matrix[0][1] * self->m_matrix[1][0])
        : WebCore::Determinant4x4(self->m_matrix);
    return std::abs(det) >= WebCore::kSmallNumber;
}

WebCore::AffineTransform APS5_VABI _ZNK7WebCore20TransformationMatrix17toAffineTransformEv(
    const WebCore::TransformationMatrix* self) {
    WebCore::AffineTransform result;
    result.m_transform[0] = self->m_matrix[0][0];
    result.m_transform[1] = self->m_matrix[0][1];
    result.m_transform[2] = self->m_matrix[1][0];
    result.m_transform[3] = self->m_matrix[1][1];
    result.m_transform[4] = self->m_matrix[3][0];
    result.m_transform[5] = self->m_matrix[3][1];
    return result;
}

WebCore::FloatQuad APS5_VABI _ZNK7WebCore20TransformationMatrix7mapQuadERKNS_9FloatQuadE(
    const WebCore::TransformationMatrix* self, const WebCore::FloatQuad& q) {
    if (WebCore::GetMatrixType(*self) == WebCore::MatrixType::IdentityOrTranslation) {
        float tx = static_cast<float>(self->m_matrix[3][0]);
        float ty = static_cast<float>(self->m_matrix[3][1]);
        return WebCore::FloatQuad{
            WebCore::FloatPoint{q.m_p1.m_x + tx, q.m_p1.m_y + ty},
            WebCore::FloatPoint{q.m_p2.m_x + tx, q.m_p2.m_y + ty},
            WebCore::FloatPoint{q.m_p3.m_x + tx, q.m_p3.m_y + ty},
            WebCore::FloatPoint{q.m_p4.m_x + tx, q.m_p4.m_y + ty}
        };
    }
    return WebCore::FloatQuad{
        WebCore::InternalMapPoint(*self, q.m_p1),
        WebCore::InternalMapPoint(*self, q.m_p2),
        WebCore::InternalMapPoint(*self, q.m_p3),
        WebCore::InternalMapPoint(*self, q.m_p4)
    };
}

WebCore::IntRect APS5_VABI _ZNK7WebCore20TransformationMatrix7mapRectERKNS_7IntRectE(
    const WebCore::TransformationMatrix* self, const WebCore::IntRect& r) {
    WebCore::FloatRect fr{
        WebCore::FloatPoint{static_cast<float>(r.m_location.m_x), static_cast<float>(r.m_location.m_y)},
        WebCore::FloatSize{static_cast<float>(r.m_size.m_width), static_cast<float>(r.m_size.m_height)}
    };
    WebCore::FloatRect mapped = _ZNK7WebCore20TransformationMatrix7mapRectERKNS_9FloatRectE(self, fr);
    return WebCore::EnclosingIntRect(mapped);
}

WebCore::FloatRect APS5_VABI _ZNK7WebCore20TransformationMatrix7mapRectERKNS_9FloatRectE(
    const WebCore::TransformationMatrix* self, const WebCore::FloatRect& r) {
    auto type = WebCore::GetMatrixType(*self);
    if (type == WebCore::MatrixType::IdentityOrTranslation) {
        return WebCore::FloatRect{
            WebCore::FloatPoint{r.m_location.m_x + static_cast<float>(self->m_matrix[3][0]),
                                r.m_location.m_y + static_cast<float>(self->m_matrix[3][1])},
            r.m_size
        };
    }

    float minX = r.m_location.m_x;
    float minY = r.m_location.m_y;
    float maxX = r.m_location.m_x + r.m_size.m_width;
    float maxY = r.m_location.m_y + r.m_size.m_height;

    if (type == WebCore::MatrixType::Affine) {
        double a = self->m_matrix[0][0];
        double b = self->m_matrix[0][1];
        double c = self->m_matrix[1][0];
        double d = self->m_matrix[1][1];

        double minResultX = (a > 0.0) ? (a * minX) : (a * maxX);
        double maxResultX = (a > 0.0) ? (a * maxX) : (a * minX);
        double minResultY = (b > 0.0) ? (b * minX) : (b * maxX);
        double maxResultY = (b > 0.0) ? (b * maxX) : (b * minX);

        if (c > 0.0) {
            maxResultX += c * maxY;
            minResultX += c * minY;
        } else {
            maxResultX += c * minY;
            minResultX += c * maxY;
        }

        if (d > 0.0) {
            maxResultY += d * maxY;
            minResultY += d * minY;
        } else {
            maxResultY += d * minY;
            minResultY += d * maxY;
        }

        return WebCore::FloatRect{
            WebCore::FloatPoint{static_cast<float>(minResultX + self->m_matrix[3][0]),
                                static_cast<float>(minResultY + self->m_matrix[3][1])},
            WebCore::FloatSize{static_cast<float>(maxResultX - minResultX),
                               static_cast<float>(maxResultY - minResultY)}
        };
    }

    WebCore::FloatPoint p1 = WebCore::InternalMapPoint(*self, WebCore::FloatPoint{minX, minY});
    WebCore::FloatPoint p2 = WebCore::InternalMapPoint(*self, WebCore::FloatPoint{maxX, minY});
    WebCore::FloatPoint p3 = WebCore::InternalMapPoint(*self, WebCore::FloatPoint{maxX, maxY});
    WebCore::FloatPoint p4 = WebCore::InternalMapPoint(*self, WebCore::FloatPoint{minX, maxY});
    float left = std::min({p1.m_x, p2.m_x, p3.m_x, p4.m_x});
    float top = std::min({p1.m_y, p2.m_y, p3.m_y, p4.m_y});
    float right = std::max({p1.m_x, p2.m_x, p3.m_x, p4.m_x});
    float bottom = std::max({p1.m_y, p2.m_y, p3.m_y, p4.m_y});
    return WebCore::FloatRect{WebCore::FloatPoint{left, top}, WebCore::FloatSize{right - left, bottom - top}};
}

WebCore::FloatPoint APS5_VABI _ZNK7WebCore20TransformationMatrix8mapPointERKNS_10FloatPointE(
    const WebCore::TransformationMatrix* self, const WebCore::FloatPoint& p) {
    if (WebCore::GetMatrixType(*self) == WebCore::MatrixType::IdentityOrTranslation) {
        return WebCore::FloatPoint{
            p.m_x + static_cast<float>(self->m_matrix[3][0]),
            p.m_y + static_cast<float>(self->m_matrix[3][1])
        };
    }
    return WebCore::InternalMapPoint(*self, p);
}

WebCore::TransformationMatrix APS5_VABI _ZNK7WebCore24MatrixTransformOperation6matrixEv(
    const WebCore::MatrixTransformOperation* self) {
    WebCore::TransformationMatrix result;
    result.m_matrix[0][0] = self->m_a;
    result.m_matrix[0][1] = self->m_b;
    result.m_matrix[0][2] = 0.0;
    result.m_matrix[0][3] = 0.0;
    result.m_matrix[1][0] = self->m_c;
    result.m_matrix[1][1] = self->m_d;
    result.m_matrix[1][2] = 0.0;
    result.m_matrix[1][3] = 0.0;
    result.m_matrix[2][0] = 0.0;
    result.m_matrix[2][1] = 0.0;
    result.m_matrix[2][2] = 1.0;
    result.m_matrix[2][3] = 0.0;
    result.m_matrix[3][0] = self->m_e;
    result.m_matrix[3][1] = self->m_f;
    result.m_matrix[3][2] = 0.0;
    result.m_matrix[3][3] = 1.0;
    return result;
}


WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix7scale3dEddd(
    WebCore::TransformationMatrix* self, double sx, double sy, double sz) {
    _ZN7WebCore20TransformationMatrix15scaleNonUniformEdd(self, sx, sy);
    self->m_matrix[2][0] *= sz;
    self->m_matrix[2][1] *= sz;
    self->m_matrix[2][2] *= sz;
    self->m_matrix[2][3] *= sz;
    return *self;
}

WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix8rotate3dEdddd(
    WebCore::TransformationMatrix* self, double x, double y, double z, double angle) {
    double length = std::hypot(x, y, z);
    if (length == 0.0) {
        return *self;
    }
    if (length != 1.0) {
        x /= length;
        y /= length;
        z /= length;
    }
    angle = WebCore::Deg2Rad(angle);
    double sinTheta = std::sin(angle);
    double cosTheta = std::cos(angle);

    WebCore::TransformationMatrix mat;
    if (x == 1.0 && y == 0.0 && z == 0.0) {
        mat.m_matrix[1][1] = cosTheta;
        mat.m_matrix[1][2] = sinTheta;
        mat.m_matrix[2][1] = -sinTheta;
        mat.m_matrix[2][2] = cosTheta;
    } else if (x == 0.0 && y == 1.0 && z == 0.0) {
        mat.m_matrix[0][0] = cosTheta;
        mat.m_matrix[0][2] = -sinTheta;
        mat.m_matrix[2][0] = sinTheta;
        mat.m_matrix[2][2] = cosTheta;
    } else if (x == 0.0 && y == 0.0 && z == 1.0) {
        mat.m_matrix[0][0] = cosTheta;
        mat.m_matrix[0][1] = sinTheta;
        mat.m_matrix[1][0] = -sinTheta;
        mat.m_matrix[1][1] = cosTheta;
    } else {
        double oneMinusCosTheta = 1.0 - cosTheta;
        mat.m_matrix[0][0] = cosTheta + x * x * oneMinusCosTheta;
        mat.m_matrix[0][1] = y * x * oneMinusCosTheta + z * sinTheta;
        mat.m_matrix[0][2] = z * x * oneMinusCosTheta - y * sinTheta;
        mat.m_matrix[1][0] = x * y * oneMinusCosTheta - z * sinTheta;
        mat.m_matrix[1][1] = cosTheta + y * y * oneMinusCosTheta;
        mat.m_matrix[1][2] = z * y * oneMinusCosTheta + x * sinTheta;
        mat.m_matrix[2][0] = x * z * oneMinusCosTheta + y * sinTheta;
        mat.m_matrix[2][1] = y * z * oneMinusCosTheta - x * sinTheta;
        mat.m_matrix[2][2] = cosTheta + z * z * oneMinusCosTheta;
    }
    return _ZN7WebCore20TransformationMatrix8multiplyERKS0_(self, mat);
}

WebCore::OptionalTransformationMatrix APS5_VABI _ZNK7WebCore20TransformationMatrix7inverseEv(
    const WebCore::TransformationMatrix* self) {
    WebCore::OptionalTransformationMatrix result;
    auto type = WebCore::GetMatrixType(*self);
    if (type == WebCore::MatrixType::IdentityOrTranslation) {
        result.m_engaged = true;
        if (self->m_matrix[3][0] != 0.0 || self->m_matrix[3][1] != 0.0 || self->m_matrix[3][2] != 0.0) {
            result.m_value.m_matrix[3][0] = -self->m_matrix[3][0];
            result.m_value.m_matrix[3][1] = -self->m_matrix[3][1];
            result.m_value.m_matrix[3][2] = -self->m_matrix[3][2];
        }
        return result;
    }
    if (type == WebCore::MatrixType::Affine) {
        double a = self->m_matrix[0][0];
        double b = self->m_matrix[0][1];
        double c = self->m_matrix[1][0];
        double d = self->m_matrix[1][1];
        double e = self->m_matrix[3][0];
        double f = self->m_matrix[3][1];
        double determinant = a * d - b * c;
        if (std::abs(determinant) < WebCore::kSmallNumber) {
            return result;
        }
        double inverseDeterminant = 1.0 / determinant;
        result.m_engaged = true;
        result.m_value.m_matrix[0][0] = d * inverseDeterminant;
        result.m_value.m_matrix[0][1] = -b * inverseDeterminant;
        result.m_value.m_matrix[1][0] = -c * inverseDeterminant;
        result.m_value.m_matrix[1][1] = a * inverseDeterminant;
        result.m_value.m_matrix[3][0] = (c * f - d * e) * inverseDeterminant;
        result.m_value.m_matrix[3][1] = (b * e - a * f) * inverseDeterminant;
        return result;
    }
    if (!WebCore::InverseMatrix(self->m_matrix, result.m_value.m_matrix)) {
        result.m_value = WebCore::TransformationMatrix{};
        return result;
    }
    result.m_engaged = true;
    return result;
}

void APS5_VABI _ZN7WebCore20TransformationMatrix6blend2ERKS0_d(
    WebCore::TransformationMatrix* self, const WebCore::TransformationMatrix& from, double progress) {
    WebCore::Decomposed2 fromDecomp;
    WebCore::Decomposed2 toDecomp;
    if (!WebCore::Decompose2(from, fromDecomp) || !WebCore::Decompose2(*self, toDecomp)) {
        if (progress < 0.5) {
            *self = from;
        }
        return;
    }
    if ((fromDecomp.scaleX < 0.0 && toDecomp.scaleY < 0.0) || (fromDecomp.scaleY < 0.0 && toDecomp.scaleX < 0.0)) {
        fromDecomp.scaleX = -fromDecomp.scaleX;
        fromDecomp.scaleY = -fromDecomp.scaleY;
        fromDecomp.angle += fromDecomp.angle < 0.0 ? 180.0 : -180.0;
    }
    if (fromDecomp.angle == 0.0) {
        fromDecomp.angle = 360.0;
    }
    if (toDecomp.angle == 0.0) {
        toDecomp.angle = 360.0;
    }
    if (std::abs(fromDecomp.angle - toDecomp.angle) > 180.0) {
        if (fromDecomp.angle > toDecomp.angle) {
            fromDecomp.angle -= 360.0;
        } else {
            toDecomp.angle -= 360.0;
        }
    }
    WebCore::BlendFloat(fromDecomp.m11, toDecomp.m11, progress);
    WebCore::BlendFloat(fromDecomp.m12, toDecomp.m12, progress);
    WebCore::BlendFloat(fromDecomp.m21, toDecomp.m21, progress);
    WebCore::BlendFloat(fromDecomp.m22, toDecomp.m22, progress);
    WebCore::BlendFloat(fromDecomp.translateX, toDecomp.translateX, progress);
    WebCore::BlendFloat(fromDecomp.translateY, toDecomp.translateY, progress);
    WebCore::BlendFloat(fromDecomp.scaleX, toDecomp.scaleX, progress);
    WebCore::BlendFloat(fromDecomp.scaleY, toDecomp.scaleY, progress);
    WebCore::BlendFloat(fromDecomp.angle, toDecomp.angle, progress);

    *self = WebCore::TransformationMatrix{};
    self->m_matrix[0][0] = fromDecomp.m11;
    self->m_matrix[0][1] = fromDecomp.m12;
    self->m_matrix[1][0] = fromDecomp.m21;
    self->m_matrix[1][1] = fromDecomp.m22;
    _ZN7WebCore20TransformationMatrix11translate3dEddd(self, fromDecomp.translateX, fromDecomp.translateY, 0.0);
    double angle = WebCore::Deg2Rad(fromDecomp.angle);
    WebCore::TransformationMatrix rotation;
    rotation.m_matrix[0][0] = std::cos(angle);
    rotation.m_matrix[0][1] = std::sin(angle);
    rotation.m_matrix[1][0] = -std::sin(angle);
    rotation.m_matrix[1][1] = std::cos(angle);
    _ZN7WebCore20TransformationMatrix8multiplyERKS0_(self, rotation);
    _ZN7WebCore20TransformationMatrix7scale3dEddd(self, fromDecomp.scaleX, fromDecomp.scaleY, 1.0);
}

void APS5_VABI _ZN7WebCore20TransformationMatrix6blend4ERKS0_d(
    WebCore::TransformationMatrix* self, const WebCore::TransformationMatrix& from, double progress) {
    WebCore::Decomposed4 fromDecomp;
    WebCore::Decomposed4 toDecomp;
    if (!WebCore::Decompose4(from, fromDecomp) || !WebCore::Decompose4(*self, toDecomp)) {
        if (progress < 0.5) {
            *self = from;
        }
        return;
    }
    WebCore::BlendFloat(fromDecomp.scaleX, toDecomp.scaleX, progress);
    WebCore::BlendFloat(fromDecomp.scaleY, toDecomp.scaleY, progress);
    WebCore::BlendFloat(fromDecomp.scaleZ, toDecomp.scaleZ, progress);
    WebCore::BlendFloat(fromDecomp.skewXY, toDecomp.skewXY, progress);
    WebCore::BlendFloat(fromDecomp.skewXZ, toDecomp.skewXZ, progress);
    WebCore::BlendFloat(fromDecomp.skewYZ, toDecomp.skewYZ, progress);
    WebCore::BlendFloat(fromDecomp.translateX, toDecomp.translateX, progress);
    WebCore::BlendFloat(fromDecomp.translateY, toDecomp.translateY, progress);
    WebCore::BlendFloat(fromDecomp.translateZ, toDecomp.translateZ, progress);
    WebCore::BlendFloat(fromDecomp.perspectiveX, toDecomp.perspectiveX, progress);
    WebCore::BlendFloat(fromDecomp.perspectiveY, toDecomp.perspectiveY, progress);
    WebCore::BlendFloat(fromDecomp.perspectiveZ, toDecomp.perspectiveZ, progress);
    WebCore::BlendFloat(fromDecomp.perspectiveW, toDecomp.perspectiveW, progress);
    WebCore::Slerp(fromDecomp.quaternion, toDecomp.quaternion, progress);

    *self = WebCore::TransformationMatrix{};
    self->m_matrix[0][3] = fromDecomp.perspectiveX;
    self->m_matrix[1][3] = fromDecomp.perspectiveY;
    self->m_matrix[2][3] = fromDecomp.perspectiveZ;
    self->m_matrix[3][3] = fromDecomp.perspectiveW;
    _ZN7WebCore20TransformationMatrix11translate3dEddd(self, fromDecomp.translateX, fromDecomp.translateY, fromDecomp.translateZ);

    const double* q = fromDecomp.quaternion;
    double xx = q[0] * q[0];
    double xy = q[0] * q[1];
    double xz = q[0] * q[2];
    double xw = q[0] * q[3];
    double yy = q[1] * q[1];
    double yz = q[1] * q[2];
    double yw = q[1] * q[3];
    double zz = q[2] * q[2];
    double zw = q[2] * q[3];
    WebCore::TransformationMatrix rotation;
    rotation.m_matrix[0][0] = 1.0 - 2.0 * (yy + zz);
    rotation.m_matrix[0][1] = 2.0 * (xy - zw);
    rotation.m_matrix[0][2] = 2.0 * (xz + yw);
    rotation.m_matrix[1][0] = 2.0 * (xy + zw);
    rotation.m_matrix[1][1] = 1.0 - 2.0 * (xx + zz);
    rotation.m_matrix[1][2] = 2.0 * (yz - xw);
    rotation.m_matrix[2][0] = 2.0 * (xz - yw);
    rotation.m_matrix[2][1] = 2.0 * (yz + xw);
    rotation.m_matrix[2][2] = 1.0 - 2.0 * (xx + yy);
    _ZN7WebCore20TransformationMatrix8multiplyERKS0_(self, rotation);

    if (fromDecomp.skewYZ != 0.0) {
        WebCore::TransformationMatrix skew;
        skew.m_matrix[2][1] = fromDecomp.skewYZ;
        _ZN7WebCore20TransformationMatrix8multiplyERKS0_(self, skew);
    }
    if (fromDecomp.skewXZ != 0.0) {
        WebCore::TransformationMatrix skew;
        skew.m_matrix[2][0] = fromDecomp.skewXZ;
        _ZN7WebCore20TransformationMatrix8multiplyERKS0_(self, skew);
    }
    if (fromDecomp.skewXY != 0.0) {
        WebCore::TransformationMatrix skew;
        skew.m_matrix[1][0] = fromDecomp.skewXY;
        _ZN7WebCore20TransformationMatrix8multiplyERKS0_(self, skew);
    }
    _ZN7WebCore20TransformationMatrix7scale3dEddd(self, fromDecomp.scaleX, fromDecomp.scaleY, fromDecomp.scaleZ);
}

void APS5_VABI _ZN7WebCore20TransformationMatrix5blendERKS0_d(
    WebCore::TransformationMatrix* self, const WebCore::TransformationMatrix& from, double progress) {
    if (WebCore::IsIdentity(from) && WebCore::IsIdentity(*self)) {
        return;
    }
    if (WebCore::IsAffine(from) && WebCore::IsAffine(*self)) {
        _ZN7WebCore20TransformationMatrix6blend2ERKS0_d(self, from, progress);
    } else {
        _ZN7WebCore20TransformationMatrix6blend4ERKS0_d(self, from, progress);
    }
}

}

namespace WebCore {

namespace {

using NKWebKit::TransformAbi::TransformOperationRef;
using NKWebKit::TransformAbi::TransformOperationVtable;
using NKWebKit::TransformAbi::TransformOperationVtableStorage;
using NKWebKit::WtfAbi::TextStream;
using OperationType = TransformOperation::OperationType;

constexpr unsigned NumberRespectingIntegersFlag = 1u << 1;

double BlendDouble(double from, double to, double progress) {
    return from + (to - from) * progress;
}

bool IsSameType(const TransformOperation& first, const TransformOperation& second) {
    return first.m_type == second.m_type;
}

void InitializeOperation(TransformOperation& operation, const TransformOperationVtableStorage& vtable, OperationType type) {
    operation.m_vptr = const_cast<TransformOperationVtable*>(&vtable.dispatch);
    operation.m_refCount = 1;
    operation.m_type = type;
}

template<typename T> T* AllocateOperation() {
    return static_cast<T*>(_ZN3WTF10fastMallocEm(sizeof(T)));
}

TransformOperationRef RefSelf(TransformOperation* self) {
    NKWebKit::TransformAbi::RefOperation(self);
    return TransformOperationRef(self);
}

TransformationMatrix AffineMatrix(double a, double b, double c, double d, double e, double f) {
    TransformationMatrix matrix;
    matrix.m_matrix[0][0] = a;
    matrix.m_matrix[0][1] = b;
    matrix.m_matrix[1][0] = c;
    matrix.m_matrix[1][1] = d;
    matrix.m_matrix[3][0] = e;
    matrix.m_matrix[3][1] = f;
    return matrix;
}

bool MatricesEqual(const TransformationMatrix& first, const TransformationMatrix& second) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (first.m_matrix[i][j] != second.m_matrix[i][j]) {
                return false;
            }
        }
    }
    return true;
}

const char* OperationTypeName(OperationType type) {
    switch (type) {
    case OperationType::ScaleX: return "scaleX";
    case OperationType::ScaleY: return "scaleY";
    case OperationType::Scale: return "scale";
    case OperationType::TranslateX: return "translateX";
    case OperationType::TranslateY: return "translateY";
    case OperationType::Translate: return "translate";
    case OperationType::Rotate: return "rotate";
    case OperationType::SkewX: return "skewX";
    case OperationType::SkewY: return "skewY";
    case OperationType::Skew: return "skew";
    case OperationType::Matrix: return "matrix";
    case OperationType::ScaleZ: return "scaleX";
    case OperationType::Scale3D: return "scale3d";
    case OperationType::TranslateZ: return "translateZ";
    case OperationType::Translate3D: return "translate3d";
    case OperationType::RotateX: return "rotateX";
    case OperationType::RotateY: return "rotateY";
    case OperationType::Rotate3D: return "rotate3d";
    case OperationType::Matrix3D: return "matrix3d";
    case OperationType::Perspective: return "perspective";
    case OperationType::Identity: return "identity";
    case OperationType::NoOperation: return "none";
    }
    return "";
}

TextStream& Text(TextStream& stream, const char* text) {
    return _ZN3WTF10TextStreamlsEPKc(&stream, text);
}

TextStream& TypeName(TextStream& stream, OperationType type) {
    return Text(stream, OperationTypeName(type));
}

TextStream& Number(TextStream& stream, double value) {
    struct Restore {
        TextStream& stream;
        unsigned flags;
        ~Restore() { stream.formattingFlags = flags; }
    } restore{stream, stream.formattingFlags};
    stream.formattingFlags |= NumberRespectingIntegersFlag;
    return _ZN3WTF10TextStreamlsEd(&stream, value);
}

template<typename T, void (APS5_VABI* Function)(T*)>
void APS5_VABI DestructorSlot(TransformOperation* self) {
    Function(static_cast<T*>(self));
}

template<typename T, TransformOperationRef (APS5_VABI* Function)(const T*)>
TransformOperationRef APS5_VABI CloneSlot(const TransformOperation* self) {
    return Function(static_cast<const T*>(self));
}

template<typename T, bool (APS5_VABI* Function)(const T*, const TransformOperation&)>
bool APS5_VABI EqualsSlot(const TransformOperation* self, const TransformOperation& other) {
    return Function(static_cast<const T*>(self), other);
}

template<typename T, bool (APS5_VABI* Function)(const T*)>
bool APS5_VABI PredicateSlot(const TransformOperation* self) {
    return Function(static_cast<const T*>(self));
}

template<typename T, bool (APS5_VABI* Function)(const T*, TransformationMatrix&, const FloatSize&)>
bool APS5_VABI ApplySlot(const TransformOperation* self, TransformationMatrix& transform, const FloatSize& borderBoxSize) {
    return Function(static_cast<const T*>(self), transform, borderBoxSize);
}

template<typename T, TransformOperationRef (APS5_VABI* Function)(T*, const TransformOperation*, double, bool)>
TransformOperationRef APS5_VABI BlendSlot(TransformOperation* self, const TransformOperation* from, double progress, bool blendToIdentity) {
    return Function(static_cast<T*>(self), from, progress, blendToIdentity);
}

template<typename T, void (APS5_VABI* Function)(const T*, TextStream&)>
void APS5_VABI DumpSlot(const TransformOperation* self, TextStream& stream) {
    Function(static_cast<const T*>(self), stream);
}

bool APS5_VABI NotAffectedByTransformOriginSlot(const TransformOperation*) {
    return false;
}

bool APS5_VABI RepresentableIn2DSlot(const TransformOperation*) {
    return true;
}

const TransformOperationVtableStorage IdentityVtable{0, nullptr, {
    DestructorSlot<IdentityTransformOperation, _ZN7WebCore26IdentityTransformOperationD1Ev>,
    DestructorSlot<IdentityTransformOperation, _ZN7WebCore26IdentityTransformOperationD0Ev>,
    CloneSlot<IdentityTransformOperation, _ZNK7WebCore26IdentityTransformOperation5cloneEv>,
    EqualsSlot<IdentityTransformOperation, _ZNK7WebCore26IdentityTransformOperationeqERKNS_18TransformOperationE>,
    PredicateSlot<IdentityTransformOperation, _ZNK7WebCore26IdentityTransformOperation10isIdentityEv>,
    ApplySlot<IdentityTransformOperation, _ZNK7WebCore26IdentityTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE>,
    BlendSlot<IdentityTransformOperation, _ZN7WebCore26IdentityTransformOperation5blendEPKNS_18TransformOperationEdb>,
    NotAffectedByTransformOriginSlot,
    RepresentableIn2DSlot,
    DumpSlot<IdentityTransformOperation, _ZNK7WebCore26IdentityTransformOperation4dumpERN3WTF10TextStreamE>,
}};

const TransformOperationVtableStorage RotateVtable{0, nullptr, {
    DestructorSlot<RotateTransformOperation, _ZN7WebCore24RotateTransformOperationD1Ev>,
    DestructorSlot<RotateTransformOperation, _ZN7WebCore24RotateTransformOperationD0Ev>,
    CloneSlot<RotateTransformOperation, _ZNK7WebCore24RotateTransformOperation5cloneEv>,
    EqualsSlot<RotateTransformOperation, _ZNK7WebCore24RotateTransformOperationeqERKNS_18TransformOperationE>,
    PredicateSlot<RotateTransformOperation, _ZNK7WebCore24RotateTransformOperation10isIdentityEv>,
    ApplySlot<RotateTransformOperation, _ZNK7WebCore24RotateTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE>,
    BlendSlot<RotateTransformOperation, _ZN7WebCore24RotateTransformOperation5blendEPKNS_18TransformOperationEdb>,
    PredicateSlot<RotateTransformOperation, _ZNK7WebCore24RotateTransformOperation27isAffectedByTransformOriginEv>,
    PredicateSlot<RotateTransformOperation, _ZNK7WebCore24RotateTransformOperation19isRepresentableIn2DEv>,
    DumpSlot<RotateTransformOperation, _ZNK7WebCore24RotateTransformOperation4dumpERN3WTF10TextStreamE>,
}};

const TransformOperationVtableStorage ScaleVtable{0, nullptr, {
    DestructorSlot<ScaleTransformOperation, _ZN7WebCore23ScaleTransformOperationD1Ev>,
    DestructorSlot<ScaleTransformOperation, _ZN7WebCore23ScaleTransformOperationD0Ev>,
    CloneSlot<ScaleTransformOperation, _ZNK7WebCore23ScaleTransformOperation5cloneEv>,
    EqualsSlot<ScaleTransformOperation, _ZNK7WebCore23ScaleTransformOperationeqERKNS_18TransformOperationE>,
    PredicateSlot<ScaleTransformOperation, _ZNK7WebCore23ScaleTransformOperation10isIdentityEv>,
    ApplySlot<ScaleTransformOperation, _ZNK7WebCore23ScaleTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE>,
    BlendSlot<ScaleTransformOperation, _ZN7WebCore23ScaleTransformOperation5blendEPKNS_18TransformOperationEdb>,
    PredicateSlot<ScaleTransformOperation, _ZNK7WebCore23ScaleTransformOperation27isAffectedByTransformOriginEv>,
    PredicateSlot<ScaleTransformOperation, _ZNK7WebCore23ScaleTransformOperation19isRepresentableIn2DEv>,
    DumpSlot<ScaleTransformOperation, _ZNK7WebCore23ScaleTransformOperation4dumpERN3WTF10TextStreamE>,
}};

const TransformOperationVtableStorage SkewVtable{0, nullptr, {
    DestructorSlot<SkewTransformOperation, _ZN7WebCore22SkewTransformOperationD1Ev>,
    DestructorSlot<SkewTransformOperation, _ZN7WebCore22SkewTransformOperationD0Ev>,
    CloneSlot<SkewTransformOperation, _ZNK7WebCore22SkewTransformOperation5cloneEv>,
    EqualsSlot<SkewTransformOperation, _ZNK7WebCore22SkewTransformOperationeqERKNS_18TransformOperationE>,
    PredicateSlot<SkewTransformOperation, _ZNK7WebCore22SkewTransformOperation10isIdentityEv>,
    ApplySlot<SkewTransformOperation, _ZNK7WebCore22SkewTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE>,
    BlendSlot<SkewTransformOperation, _ZN7WebCore22SkewTransformOperation5blendEPKNS_18TransformOperationEdb>,
    PredicateSlot<SkewTransformOperation, _ZNK7WebCore22SkewTransformOperation27isAffectedByTransformOriginEv>,
    RepresentableIn2DSlot,
    DumpSlot<SkewTransformOperation, _ZNK7WebCore22SkewTransformOperation4dumpERN3WTF10TextStreamE>,
}};

const TransformOperationVtableStorage MatrixVtable{0, nullptr, {
    DestructorSlot<MatrixTransformOperation, _ZN7WebCore24MatrixTransformOperationD1Ev>,
    DestructorSlot<MatrixTransformOperation, _ZN7WebCore24MatrixTransformOperationD0Ev>,
    CloneSlot<MatrixTransformOperation, _ZNK7WebCore24MatrixTransformOperation5cloneEv>,
    EqualsSlot<MatrixTransformOperation, _ZNK7WebCore24MatrixTransformOperationeqERKNS_18TransformOperationE>,
    PredicateSlot<MatrixTransformOperation, _ZNK7WebCore24MatrixTransformOperation10isIdentityEv>,
    ApplySlot<MatrixTransformOperation, _ZNK7WebCore24MatrixTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE>,
    BlendSlot<MatrixTransformOperation, _ZN7WebCore24MatrixTransformOperation5blendEPKNS_18TransformOperationEdb>,
    PredicateSlot<MatrixTransformOperation, _ZNK7WebCore24MatrixTransformOperation27isAffectedByTransformOriginEv>,
    RepresentableIn2DSlot,
    DumpSlot<MatrixTransformOperation, _ZNK7WebCore24MatrixTransformOperation4dumpERN3WTF10TextStreamE>,
}};

const TransformOperationVtableStorage Matrix3DVtable{0, nullptr, {
    DestructorSlot<Matrix3DTransformOperation, _ZN7WebCore26Matrix3DTransformOperationD1Ev>,
    DestructorSlot<Matrix3DTransformOperation, _ZN7WebCore26Matrix3DTransformOperationD0Ev>,
    CloneSlot<Matrix3DTransformOperation, _ZNK7WebCore26Matrix3DTransformOperation5cloneEv>,
    EqualsSlot<Matrix3DTransformOperation, _ZNK7WebCore26Matrix3DTransformOperationeqERKNS_18TransformOperationE>,
    PredicateSlot<Matrix3DTransformOperation, _ZNK7WebCore26Matrix3DTransformOperation10isIdentityEv>,
    ApplySlot<Matrix3DTransformOperation, _ZNK7WebCore26Matrix3DTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE>,
    BlendSlot<Matrix3DTransformOperation, _ZN7WebCore26Matrix3DTransformOperation5blendEPKNS_18TransformOperationEdb>,
    PredicateSlot<Matrix3DTransformOperation, _ZNK7WebCore26Matrix3DTransformOperation27isAffectedByTransformOriginEv>,
    PredicateSlot<Matrix3DTransformOperation, _ZNK7WebCore26Matrix3DTransformOperation19isRepresentableIn2DEv>,
    DumpSlot<Matrix3DTransformOperation, _ZNK7WebCore26Matrix3DTransformOperation4dumpERN3WTF10TextStreamE>,
}};

}  // namespace
}  // namespace WebCore

extern "C" {

void APS5_VABI _ZN7WebCore26IdentityTransformOperationC1Ev(WebCore::IdentityTransformOperation* self) {
    WebCore::InitializeOperation(*self, WebCore::IdentityVtable, WebCore::OperationType::Identity);
}

void APS5_VABI _ZN7WebCore26IdentityTransformOperationC2Ev(WebCore::IdentityTransformOperation* self) {
    _ZN7WebCore26IdentityTransformOperationC1Ev(self);
}

void APS5_VABI _ZN7WebCore26IdentityTransformOperationD1Ev(WebCore::IdentityTransformOperation*) {
}

void APS5_VABI _ZN7WebCore26IdentityTransformOperationD2Ev(WebCore::IdentityTransformOperation* self) {
    _ZN7WebCore26IdentityTransformOperationD1Ev(self);
}

void APS5_VABI _ZN7WebCore26IdentityTransformOperationD0Ev(WebCore::IdentityTransformOperation* self) {
    _ZN7WebCore26IdentityTransformOperationD1Ev(self);
    _ZN3WTF8fastFreeEPv(self);
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore26IdentityTransformOperation6createEv() {
    auto* operation = WebCore::AllocateOperation<WebCore::IdentityTransformOperation>();
    _ZN7WebCore26IdentityTransformOperationC1Ev(operation);
    return NKWebKit::TransformAbi::TransformOperationRef(operation);
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZNK7WebCore26IdentityTransformOperation5cloneEv(const WebCore::IdentityTransformOperation*) {
    return _ZN7WebCore26IdentityTransformOperation6createEv();
}

bool APS5_VABI _ZNK7WebCore26IdentityTransformOperation10isIdentityEv(const WebCore::IdentityTransformOperation*) {
    return true;
}

bool APS5_VABI _ZNK7WebCore26IdentityTransformOperationeqERKNS_18TransformOperationE(
    const WebCore::IdentityTransformOperation* self, const WebCore::TransformOperation& other) {
    return WebCore::IsSameType(*self, other);
}

bool APS5_VABI _ZNK7WebCore26IdentityTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(
    const WebCore::IdentityTransformOperation*, WebCore::TransformationMatrix&, const WebCore::FloatSize&) {
    return false;
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore26IdentityTransformOperation5blendEPKNS_18TransformOperationEdb(
    WebCore::IdentityTransformOperation* self, const WebCore::TransformOperation*, double, bool) {
    return WebCore::RefSelf(self);
}

void APS5_VABI _ZNK7WebCore26IdentityTransformOperation4dumpERN3WTF10TextStreamE(
    const WebCore::IdentityTransformOperation* self, NKWebKit::WtfAbi::TextStream& stream) {
    WebCore::TypeName(stream, self->m_type);
}

void APS5_VABI _ZN7WebCore24RotateTransformOperationC1EddddNS_18TransformOperation13OperationTypeE(
    WebCore::RotateTransformOperation* self, double x, double y, double z, double angle, WebCore::TransformOperation::OperationType type) {
    WebCore::InitializeOperation(*self, WebCore::RotateVtable, type);
    self->m_x = x;
    self->m_y = y;
    self->m_z = z;
    self->m_angle = angle;
}

void APS5_VABI _ZN7WebCore24RotateTransformOperationC2EddddNS_18TransformOperation13OperationTypeE(
    WebCore::RotateTransformOperation* self, double x, double y, double z, double angle, WebCore::TransformOperation::OperationType type) {
    _ZN7WebCore24RotateTransformOperationC1EddddNS_18TransformOperation13OperationTypeE(self, x, y, z, angle, type);
}

void APS5_VABI _ZN7WebCore24RotateTransformOperationD1Ev(WebCore::RotateTransformOperation*) {
}

void APS5_VABI _ZN7WebCore24RotateTransformOperationD2Ev(WebCore::RotateTransformOperation* self) {
    _ZN7WebCore24RotateTransformOperationD1Ev(self);
}

void APS5_VABI _ZN7WebCore24RotateTransformOperationD0Ev(WebCore::RotateTransformOperation* self) {
    _ZN7WebCore24RotateTransformOperationD1Ev(self);
    _ZN3WTF8fastFreeEPv(self);
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore24RotateTransformOperation6createEddddNS_18TransformOperation13OperationTypeE(
    double x, double y, double z, double angle, WebCore::TransformOperation::OperationType type) {
    auto* operation = WebCore::AllocateOperation<WebCore::RotateTransformOperation>();
    _ZN7WebCore24RotateTransformOperationC1EddddNS_18TransformOperation13OperationTypeE(operation, x, y, z, angle, type);
    return NKWebKit::TransformAbi::TransformOperationRef(operation);
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore24RotateTransformOperation6createEdNS_18TransformOperation13OperationTypeE(
    double angle, WebCore::TransformOperation::OperationType type) {
    return _ZN7WebCore24RotateTransformOperation6createEddddNS_18TransformOperation13OperationTypeE(0.0, 0.0, 1.0, angle, type);
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZNK7WebCore24RotateTransformOperation5cloneEv(const WebCore::RotateTransformOperation* self) {
    return _ZN7WebCore24RotateTransformOperation6createEddddNS_18TransformOperation13OperationTypeE(
        self->m_x, self->m_y, self->m_z, self->m_angle, self->m_type);
}

double APS5_VABI _ZNK7WebCore24RotateTransformOperation1xEv(const WebCore::RotateTransformOperation* self) {
    return self->m_x;
}

double APS5_VABI _ZNK7WebCore24RotateTransformOperation1yEv(const WebCore::RotateTransformOperation* self) {
    return self->m_y;
}

double APS5_VABI _ZNK7WebCore24RotateTransformOperation1zEv(const WebCore::RotateTransformOperation* self) {
    return self->m_z;
}

double APS5_VABI _ZNK7WebCore24RotateTransformOperation5angleEv(const WebCore::RotateTransformOperation* self) {
    return self->m_angle;
}

bool APS5_VABI _ZNK7WebCore24RotateTransformOperation10isIdentityEv(const WebCore::RotateTransformOperation* self) {
    return self->m_angle == 0.0;
}

bool APS5_VABI _ZNK7WebCore24RotateTransformOperation27isAffectedByTransformOriginEv(const WebCore::RotateTransformOperation* self) {
    return !_ZNK7WebCore24RotateTransformOperation10isIdentityEv(self);
}

bool APS5_VABI _ZNK7WebCore24RotateTransformOperation19isRepresentableIn2DEv(const WebCore::RotateTransformOperation* self) {
    return (self->m_x == 0.0 && self->m_y == 0.0) || self->m_angle == 0.0;
}

bool APS5_VABI _ZNK7WebCore24RotateTransformOperationeqERKNS_18TransformOperationE(
    const WebCore::RotateTransformOperation* self, const WebCore::TransformOperation& other) {
    if (!WebCore::IsSameType(*self, other)) {
        return false;
    }
    const auto& rotate = static_cast<const WebCore::RotateTransformOperation&>(other);
    return self->m_x == rotate.m_x && self->m_y == rotate.m_y && self->m_z == rotate.m_z && self->m_angle == rotate.m_angle;
}

bool APS5_VABI _ZNK7WebCore24RotateTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(
    const WebCore::RotateTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize&) {
    if (self->m_type == WebCore::OperationType::Rotate) {
        _ZN7WebCore20TransformationMatrix6rotateEd(&transform, self->m_angle);
    } else {
        _ZN7WebCore20TransformationMatrix8rotate3dEdddd(&transform, self->m_x, self->m_y, self->m_z, self->m_angle);
    }
    return false;
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore24RotateTransformOperation5blendEPKNS_18TransformOperationEdb(
    WebCore::RotateTransformOperation* self, const WebCore::TransformOperation* from, double progress, bool blendToIdentity) {
    if (from && !WebCore::IsSameType(*from, *self)) {
        return WebCore::RefSelf(self);
    }
    if (blendToIdentity) {
        return _ZN7WebCore24RotateTransformOperation6createEddddNS_18TransformOperation13OperationTypeE(
            self->m_x, self->m_y, self->m_z, self->m_angle - self->m_angle * progress, self->m_type);
    }
    const auto* fromOp = static_cast<const WebCore::RotateTransformOperation*>(from);
    if (!fromOp || (fromOp->m_x == 0.0 && fromOp->m_y == 0.0 && fromOp->m_z == 1.0)
        || (fromOp->m_x == 0.0 && fromOp->m_y == 1.0 && fromOp->m_z == 0.0)
        || (fromOp->m_x == 1.0 && fromOp->m_y == 0.0 && fromOp->m_z == 0.0)) {
        double fromAngle = fromOp ? fromOp->m_angle : 0.0;
        return _ZN7WebCore24RotateTransformOperation6createEddddNS_18TransformOperation13OperationTypeE(
            fromOp ? fromOp->m_x : self->m_x, fromOp ? fromOp->m_y : self->m_y, fromOp ? fromOp->m_z : self->m_z,
            WebCore::BlendDouble(fromAngle, self->m_angle, progress), self->m_type);
    }
    WebCore::TransformationMatrix fromT;
    WebCore::TransformationMatrix toT;
    _ZN7WebCore20TransformationMatrix8rotate3dEdddd(&fromT, fromOp->m_x, fromOp->m_y, fromOp->m_z, fromOp->m_angle);
    _ZN7WebCore20TransformationMatrix8rotate3dEdddd(&toT, self->m_x, self->m_y, self->m_z, self->m_angle);
    _ZN7WebCore20TransformationMatrix5blendERKS0_d(&toT, fromT, progress);
    WebCore::Decomposed4 decomp{};
    WebCore::Decompose4(toT, decomp);
    double x = -decomp.quaternion[0];
    double y = -decomp.quaternion[1];
    double z = -decomp.quaternion[2];
    double length = std::hypot(x, y, z);
    double angle = 0.0;
    if (length > 0.00001) {
        x /= length;
        y /= length;
        z /= length;
        angle = WebCore::Rad2Deg(std::acos(decomp.quaternion[3]) * 2.0);
    } else {
        x = 0.0;
        y = 0.0;
        z = 1.0;
    }
    return _ZN7WebCore24RotateTransformOperation6createEddddNS_18TransformOperation13OperationTypeE(
        x, y, z, angle, WebCore::OperationType::Rotate3D);
}

void APS5_VABI _ZNK7WebCore24RotateTransformOperation4dumpERN3WTF10TextStreamE(
    const WebCore::RotateTransformOperation* self, NKWebKit::WtfAbi::TextStream& stream) {
    WebCore::TypeName(stream, self->m_type);
    WebCore::Text(stream, "(");
    WebCore::Number(stream, self->m_x);
    WebCore::Text(stream, ", ");
    WebCore::Number(stream, self->m_y);
    WebCore::Text(stream, ", ");
    WebCore::Number(stream, self->m_z);
    WebCore::Text(stream, ", ");
    WebCore::Number(stream, self->m_angle);
    WebCore::Text(stream, "deg)");
}

void APS5_VABI _ZN7WebCore23ScaleTransformOperationC1EdddNS_18TransformOperation13OperationTypeE(
    WebCore::ScaleTransformOperation* self, double sx, double sy, double sz, WebCore::TransformOperation::OperationType type) {
    WebCore::InitializeOperation(*self, WebCore::ScaleVtable, type);
    self->m_x = sx;
    self->m_y = sy;
    self->m_z = sz;
}

void APS5_VABI _ZN7WebCore23ScaleTransformOperationC2EdddNS_18TransformOperation13OperationTypeE(
    WebCore::ScaleTransformOperation* self, double sx, double sy, double sz, WebCore::TransformOperation::OperationType type) {
    _ZN7WebCore23ScaleTransformOperationC1EdddNS_18TransformOperation13OperationTypeE(self, sx, sy, sz, type);
}

void APS5_VABI _ZN7WebCore23ScaleTransformOperationD1Ev(WebCore::ScaleTransformOperation*) {
}

void APS5_VABI _ZN7WebCore23ScaleTransformOperationD2Ev(WebCore::ScaleTransformOperation* self) {
    _ZN7WebCore23ScaleTransformOperationD1Ev(self);
}

void APS5_VABI _ZN7WebCore23ScaleTransformOperationD0Ev(WebCore::ScaleTransformOperation* self) {
    _ZN7WebCore23ScaleTransformOperationD1Ev(self);
    _ZN3WTF8fastFreeEPv(self);
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore23ScaleTransformOperation6createEdddNS_18TransformOperation13OperationTypeE(
    double sx, double sy, double sz, WebCore::TransformOperation::OperationType type) {
    auto* operation = WebCore::AllocateOperation<WebCore::ScaleTransformOperation>();
    _ZN7WebCore23ScaleTransformOperationC1EdddNS_18TransformOperation13OperationTypeE(operation, sx, sy, sz, type);
    return NKWebKit::TransformAbi::TransformOperationRef(operation);
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore23ScaleTransformOperation6createEddNS_18TransformOperation13OperationTypeE(
    double sx, double sy, WebCore::TransformOperation::OperationType type) {
    return _ZN7WebCore23ScaleTransformOperation6createEdddNS_18TransformOperation13OperationTypeE(sx, sy, 1.0, type);
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZNK7WebCore23ScaleTransformOperation5cloneEv(const WebCore::ScaleTransformOperation* self) {
    return _ZN7WebCore23ScaleTransformOperation6createEdddNS_18TransformOperation13OperationTypeE(self->m_x, self->m_y, self->m_z, self->m_type);
}

double APS5_VABI _ZNK7WebCore23ScaleTransformOperation1xEv(const WebCore::ScaleTransformOperation* self) {
    return self->m_x;
}

double APS5_VABI _ZNK7WebCore23ScaleTransformOperation1yEv(const WebCore::ScaleTransformOperation* self) {
    return self->m_y;
}

double APS5_VABI _ZNK7WebCore23ScaleTransformOperation1zEv(const WebCore::ScaleTransformOperation* self) {
    return self->m_z;
}

bool APS5_VABI _ZNK7WebCore23ScaleTransformOperation10isIdentityEv(const WebCore::ScaleTransformOperation* self) {
    return self->m_x == 1.0 && self->m_y == 1.0 && self->m_z == 1.0;
}

bool APS5_VABI _ZNK7WebCore23ScaleTransformOperation27isAffectedByTransformOriginEv(const WebCore::ScaleTransformOperation* self) {
    return !_ZNK7WebCore23ScaleTransformOperation10isIdentityEv(self);
}

bool APS5_VABI _ZNK7WebCore23ScaleTransformOperation19isRepresentableIn2DEv(const WebCore::ScaleTransformOperation* self) {
    return self->m_z == 1.0;
}

bool APS5_VABI _ZNK7WebCore23ScaleTransformOperationeqERKNS_18TransformOperationE(
    const WebCore::ScaleTransformOperation* self, const WebCore::TransformOperation& other) {
    if (!WebCore::IsSameType(*self, other)) {
        return false;
    }
    const auto& scale = static_cast<const WebCore::ScaleTransformOperation&>(other);
    return self->m_x == scale.m_x && self->m_y == scale.m_y && self->m_z == scale.m_z;
}

bool APS5_VABI _ZNK7WebCore23ScaleTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(
    const WebCore::ScaleTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize&) {
    _ZN7WebCore20TransformationMatrix7scale3dEddd(&transform, self->m_x, self->m_y, self->m_z);
    return false;
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore23ScaleTransformOperation5blendEPKNS_18TransformOperationEdb(
    WebCore::ScaleTransformOperation* self, const WebCore::TransformOperation* from, double progress, bool blendToIdentity) {
    if (from && !WebCore::IsSameType(*from, *self)) {
        return WebCore::RefSelf(self);
    }
    if (blendToIdentity) {
        return _ZN7WebCore23ScaleTransformOperation6createEdddNS_18TransformOperation13OperationTypeE(
            WebCore::BlendDouble(self->m_x, 1.0, progress), WebCore::BlendDouble(self->m_y, 1.0, progress),
            WebCore::BlendDouble(self->m_z, 1.0, progress), self->m_type);
    }
    const auto* fromOp = static_cast<const WebCore::ScaleTransformOperation*>(from);
    double fromX = fromOp ? fromOp->m_x : 1.0;
    double fromY = fromOp ? fromOp->m_y : 1.0;
    double fromZ = fromOp ? fromOp->m_z : 1.0;
    return _ZN7WebCore23ScaleTransformOperation6createEdddNS_18TransformOperation13OperationTypeE(
        WebCore::BlendDouble(fromX, self->m_x, progress), WebCore::BlendDouble(fromY, self->m_y, progress),
        WebCore::BlendDouble(fromZ, self->m_z, progress), self->m_type);
}

void APS5_VABI _ZNK7WebCore23ScaleTransformOperation4dumpERN3WTF10TextStreamE(
    const WebCore::ScaleTransformOperation* self, NKWebKit::WtfAbi::TextStream& stream) {
    WebCore::TypeName(stream, self->m_type);
    WebCore::Text(stream, "(");
    WebCore::Number(stream, self->m_x);
    WebCore::Text(stream, ", ");
    WebCore::Number(stream, self->m_y);
    WebCore::Text(stream, ", ");
    WebCore::Number(stream, self->m_z);
    WebCore::Text(stream, ")");
}

void APS5_VABI _ZN7WebCore22SkewTransformOperationC1EddNS_18TransformOperation13OperationTypeE(
    WebCore::SkewTransformOperation* self, double angleX, double angleY, WebCore::TransformOperation::OperationType type) {
    WebCore::InitializeOperation(*self, WebCore::SkewVtable, type);
    self->m_angleX = angleX;
    self->m_angleY = angleY;
}

void APS5_VABI _ZN7WebCore22SkewTransformOperationC2EddNS_18TransformOperation13OperationTypeE(
    WebCore::SkewTransformOperation* self, double angleX, double angleY, WebCore::TransformOperation::OperationType type) {
    _ZN7WebCore22SkewTransformOperationC1EddNS_18TransformOperation13OperationTypeE(self, angleX, angleY, type);
}

void APS5_VABI _ZN7WebCore22SkewTransformOperationD1Ev(WebCore::SkewTransformOperation*) {
}

void APS5_VABI _ZN7WebCore22SkewTransformOperationD2Ev(WebCore::SkewTransformOperation* self) {
    _ZN7WebCore22SkewTransformOperationD1Ev(self);
}

void APS5_VABI _ZN7WebCore22SkewTransformOperationD0Ev(WebCore::SkewTransformOperation* self) {
    _ZN7WebCore22SkewTransformOperationD1Ev(self);
    _ZN3WTF8fastFreeEPv(self);
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore22SkewTransformOperation6createEddNS_18TransformOperation13OperationTypeE(
    double angleX, double angleY, WebCore::TransformOperation::OperationType type) {
    auto* operation = WebCore::AllocateOperation<WebCore::SkewTransformOperation>();
    _ZN7WebCore22SkewTransformOperationC1EddNS_18TransformOperation13OperationTypeE(operation, angleX, angleY, type);
    return NKWebKit::TransformAbi::TransformOperationRef(operation);
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZNK7WebCore22SkewTransformOperation5cloneEv(const WebCore::SkewTransformOperation* self) {
    return _ZN7WebCore22SkewTransformOperation6createEddNS_18TransformOperation13OperationTypeE(self->m_angleX, self->m_angleY, self->m_type);
}

double APS5_VABI _ZNK7WebCore22SkewTransformOperation6angleXEv(const WebCore::SkewTransformOperation* self) {
    return self->m_angleX;
}

double APS5_VABI _ZNK7WebCore22SkewTransformOperation6angleYEv(const WebCore::SkewTransformOperation* self) {
    return self->m_angleY;
}

bool APS5_VABI _ZNK7WebCore22SkewTransformOperation10isIdentityEv(const WebCore::SkewTransformOperation* self) {
    return self->m_angleX == 0.0 && self->m_angleY == 0.0;
}

bool APS5_VABI _ZNK7WebCore22SkewTransformOperation27isAffectedByTransformOriginEv(const WebCore::SkewTransformOperation* self) {
    return !_ZNK7WebCore22SkewTransformOperation10isIdentityEv(self);
}

bool APS5_VABI _ZNK7WebCore22SkewTransformOperationeqERKNS_18TransformOperationE(
    const WebCore::SkewTransformOperation* self, const WebCore::TransformOperation& other) {
    if (!WebCore::IsSameType(*self, other)) {
        return false;
    }
    const auto& skew = static_cast<const WebCore::SkewTransformOperation&>(other);
    return self->m_angleX == skew.m_angleX && self->m_angleY == skew.m_angleY;
}

bool APS5_VABI _ZNK7WebCore22SkewTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(
    const WebCore::SkewTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize&) {
    _ZN7WebCore20TransformationMatrix4skewEdd(&transform, self->m_angleX, self->m_angleY);
    return false;
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore22SkewTransformOperation5blendEPKNS_18TransformOperationEdb(
    WebCore::SkewTransformOperation* self, const WebCore::TransformOperation* from, double progress, bool blendToIdentity) {
    if (from && !WebCore::IsSameType(*from, *self)) {
        return WebCore::RefSelf(self);
    }
    if (blendToIdentity) {
        return _ZN7WebCore22SkewTransformOperation6createEddNS_18TransformOperation13OperationTypeE(
            WebCore::BlendDouble(self->m_angleX, 0.0, progress), WebCore::BlendDouble(self->m_angleY, 0.0, progress), self->m_type);
    }
    const auto* fromOp = static_cast<const WebCore::SkewTransformOperation*>(from);
    double fromAngleX = fromOp ? fromOp->m_angleX : 0.0;
    double fromAngleY = fromOp ? fromOp->m_angleY : 0.0;
    return _ZN7WebCore22SkewTransformOperation6createEddNS_18TransformOperation13OperationTypeE(
        WebCore::BlendDouble(fromAngleX, self->m_angleX, progress), WebCore::BlendDouble(fromAngleY, self->m_angleY, progress), self->m_type);
}

void APS5_VABI _ZNK7WebCore22SkewTransformOperation4dumpERN3WTF10TextStreamE(
    const WebCore::SkewTransformOperation* self, NKWebKit::WtfAbi::TextStream& stream) {
    WebCore::TypeName(stream, self->m_type);
    WebCore::Text(stream, "(");
    WebCore::Number(stream, self->m_angleX);
    WebCore::Text(stream, "deg, ");
    WebCore::Number(stream, self->m_angleY);
    WebCore::Text(stream, "deg)");
}

void APS5_VABI _ZN7WebCore24MatrixTransformOperationC1Edddddd(
    WebCore::MatrixTransformOperation* self, double a, double b, double c, double d, double e, double f) {
    WebCore::InitializeOperation(*self, WebCore::MatrixVtable, WebCore::OperationType::Matrix);
    self->m_a = a;
    self->m_b = b;
    self->m_c = c;
    self->m_d = d;
    self->m_e = e;
    self->m_f = f;
}

void APS5_VABI _ZN7WebCore24MatrixTransformOperationC2Edddddd(
    WebCore::MatrixTransformOperation* self, double a, double b, double c, double d, double e, double f) {
    _ZN7WebCore24MatrixTransformOperationC1Edddddd(self, a, b, c, d, e, f);
}

void APS5_VABI _ZN7WebCore24MatrixTransformOperationC1ERKNS_20TransformationMatrixE(
    WebCore::MatrixTransformOperation* self, const WebCore::TransformationMatrix& matrix) {
    _ZN7WebCore24MatrixTransformOperationC1Edddddd(self, matrix.m_matrix[0][0], matrix.m_matrix[0][1],
        matrix.m_matrix[1][0], matrix.m_matrix[1][1], matrix.m_matrix[3][0], matrix.m_matrix[3][1]);
}

void APS5_VABI _ZN7WebCore24MatrixTransformOperationC2ERKNS_20TransformationMatrixE(
    WebCore::MatrixTransformOperation* self, const WebCore::TransformationMatrix& matrix) {
    _ZN7WebCore24MatrixTransformOperationC1ERKNS_20TransformationMatrixE(self, matrix);
}

void APS5_VABI _ZN7WebCore24MatrixTransformOperationD1Ev(WebCore::MatrixTransformOperation*) {
}

void APS5_VABI _ZN7WebCore24MatrixTransformOperationD2Ev(WebCore::MatrixTransformOperation* self) {
    _ZN7WebCore24MatrixTransformOperationD1Ev(self);
}

void APS5_VABI _ZN7WebCore24MatrixTransformOperationD0Ev(WebCore::MatrixTransformOperation* self) {
    _ZN7WebCore24MatrixTransformOperationD1Ev(self);
    _ZN3WTF8fastFreeEPv(self);
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore24MatrixTransformOperation6createEdddddd(
    double a, double b, double c, double d, double e, double f) {
    auto* operation = WebCore::AllocateOperation<WebCore::MatrixTransformOperation>();
    _ZN7WebCore24MatrixTransformOperationC1Edddddd(operation, a, b, c, d, e, f);
    return NKWebKit::TransformAbi::TransformOperationRef(operation);
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore24MatrixTransformOperation6createERKNS_20TransformationMatrixE(
    const WebCore::TransformationMatrix& matrix) {
    auto* operation = WebCore::AllocateOperation<WebCore::MatrixTransformOperation>();
    _ZN7WebCore24MatrixTransformOperationC1ERKNS_20TransformationMatrixE(operation, matrix);
    return NKWebKit::TransformAbi::TransformOperationRef(operation);
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZNK7WebCore24MatrixTransformOperation5cloneEv(const WebCore::MatrixTransformOperation* self) {
    return _ZN7WebCore24MatrixTransformOperation6createERKNS_20TransformationMatrixE(_ZNK7WebCore24MatrixTransformOperation6matrixEv(self));
}

bool APS5_VABI _ZNK7WebCore24MatrixTransformOperation10isIdentityEv(const WebCore::MatrixTransformOperation* self) {
    return self->m_a == 1.0 && self->m_b == 0.0 && self->m_c == 0.0 && self->m_d == 1.0 && self->m_e == 0.0 && self->m_f == 0.0;
}

bool APS5_VABI _ZNK7WebCore24MatrixTransformOperation27isAffectedByTransformOriginEv(const WebCore::MatrixTransformOperation* self) {
    return !_ZNK7WebCore24MatrixTransformOperation10isIdentityEv(self);
}

bool APS5_VABI _ZNK7WebCore24MatrixTransformOperationeqERKNS_18TransformOperationE(
    const WebCore::MatrixTransformOperation* self, const WebCore::TransformOperation& other) {
    if (!WebCore::IsSameType(*self, other)) {
        return false;
    }
    const auto& m = static_cast<const WebCore::MatrixTransformOperation&>(other);
    return self->m_a == m.m_a && self->m_b == m.m_b && self->m_c == m.m_c && self->m_d == m.m_d && self->m_e == m.m_e && self->m_f == m.m_f;
}

bool APS5_VABI _ZNK7WebCore24MatrixTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(
    const WebCore::MatrixTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize&) {
    WebCore::TransformationMatrix matrix = WebCore::AffineMatrix(self->m_a, self->m_b, self->m_c, self->m_d, self->m_e, self->m_f);
    _ZN7WebCore20TransformationMatrix8multiplyERKS0_(&transform, matrix);
    return false;
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore24MatrixTransformOperation5blendEPKNS_18TransformOperationEdb(
    WebCore::MatrixTransformOperation* self, const WebCore::TransformOperation* from, double progress, bool blendToIdentity) {
    if (from && !WebCore::IsSameType(*from, *self)) {
        return WebCore::RefSelf(self);
    }
    WebCore::TransformationMatrix fromT;
    WebCore::TransformationMatrix toT = WebCore::AffineMatrix(self->m_a, self->m_b, self->m_c, self->m_d, self->m_e, self->m_f);
    if (from) {
        const auto& m = static_cast<const WebCore::MatrixTransformOperation&>(*from);
        fromT = WebCore::AffineMatrix(m.m_a, m.m_b, m.m_c, m.m_d, m.m_e, m.m_f);
    }
    if (blendToIdentity) {
        _ZN7WebCore20TransformationMatrix5blendERKS0_d(&fromT, toT, progress);
        return _ZN7WebCore24MatrixTransformOperation6createERKNS_20TransformationMatrixE(fromT);
    }
    _ZN7WebCore20TransformationMatrix5blendERKS0_d(&toT, fromT, progress);
    return _ZN7WebCore24MatrixTransformOperation6createERKNS_20TransformationMatrixE(toT);
}

void APS5_VABI _ZNK7WebCore24MatrixTransformOperation4dumpERN3WTF10TextStreamE(
    const WebCore::MatrixTransformOperation* self, NKWebKit::WtfAbi::TextStream& stream) {
    WebCore::Text(stream, "(");
    WebCore::Number(stream, self->m_a);
    WebCore::Text(stream, ", ");
    WebCore::Number(stream, self->m_b);
    WebCore::Text(stream, ", ");
    WebCore::Number(stream, self->m_c);
    WebCore::Text(stream, ", ");
    WebCore::Number(stream, self->m_d);
    WebCore::Text(stream, ", ");
    WebCore::Number(stream, self->m_e);
    WebCore::Text(stream, ", ");
    WebCore::Number(stream, self->m_f);
    WebCore::Text(stream, ")");
}

void APS5_VABI _ZN7WebCore26Matrix3DTransformOperationC1ERKNS_20TransformationMatrixE(
    WebCore::Matrix3DTransformOperation* self, const WebCore::TransformationMatrix& matrix) {
    WebCore::InitializeOperation(*self, WebCore::Matrix3DVtable, WebCore::OperationType::Matrix3D);
    self->m_matrix = matrix;
}

void APS5_VABI _ZN7WebCore26Matrix3DTransformOperationC2ERKNS_20TransformationMatrixE(
    WebCore::Matrix3DTransformOperation* self, const WebCore::TransformationMatrix& matrix) {
    _ZN7WebCore26Matrix3DTransformOperationC1ERKNS_20TransformationMatrixE(self, matrix);
}

void APS5_VABI _ZN7WebCore26Matrix3DTransformOperationD1Ev(WebCore::Matrix3DTransformOperation*) {
}

void APS5_VABI _ZN7WebCore26Matrix3DTransformOperationD2Ev(WebCore::Matrix3DTransformOperation* self) {
    _ZN7WebCore26Matrix3DTransformOperationD1Ev(self);
}

void APS5_VABI _ZN7WebCore26Matrix3DTransformOperationD0Ev(WebCore::Matrix3DTransformOperation* self) {
    _ZN7WebCore26Matrix3DTransformOperationD1Ev(self);
    _ZN3WTF8fastFreeEPv(self);
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore26Matrix3DTransformOperation6createERKNS_20TransformationMatrixE(
    const WebCore::TransformationMatrix& matrix) {
    auto* operation = WebCore::AllocateOperation<WebCore::Matrix3DTransformOperation>();
    _ZN7WebCore26Matrix3DTransformOperationC1ERKNS_20TransformationMatrixE(operation, matrix);
    return NKWebKit::TransformAbi::TransformOperationRef(operation);
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZNK7WebCore26Matrix3DTransformOperation5cloneEv(const WebCore::Matrix3DTransformOperation* self) {
    return _ZN7WebCore26Matrix3DTransformOperation6createERKNS_20TransformationMatrixE(self->m_matrix);
}

WebCore::TransformationMatrix APS5_VABI _ZNK7WebCore26Matrix3DTransformOperation6matrixEv(const WebCore::Matrix3DTransformOperation* self) {
    return self->m_matrix;
}

bool APS5_VABI _ZNK7WebCore26Matrix3DTransformOperation10isIdentityEv(const WebCore::Matrix3DTransformOperation* self) {
    return WebCore::IsIdentity(self->m_matrix);
}

bool APS5_VABI _ZNK7WebCore26Matrix3DTransformOperation27isAffectedByTransformOriginEv(const WebCore::Matrix3DTransformOperation* self) {
    return !_ZNK7WebCore26Matrix3DTransformOperation10isIdentityEv(self);
}

bool APS5_VABI _ZNK7WebCore26Matrix3DTransformOperation19isRepresentableIn2DEv(const WebCore::Matrix3DTransformOperation* self) {
    return WebCore::IsAffine(self->m_matrix);
}

bool APS5_VABI _ZNK7WebCore26Matrix3DTransformOperationeqERKNS_18TransformOperationE(
    const WebCore::Matrix3DTransformOperation* self, const WebCore::TransformOperation& other) {
    return WebCore::IsSameType(*self, other)
        && WebCore::MatricesEqual(self->m_matrix, static_cast<const WebCore::Matrix3DTransformOperation&>(other).m_matrix);
}

bool APS5_VABI _ZNK7WebCore26Matrix3DTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(
    const WebCore::Matrix3DTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize&) {
    _ZN7WebCore20TransformationMatrix8multiplyERKS0_(&transform, self->m_matrix);
    return false;
}

NKWebKit::TransformAbi::TransformOperationRef APS5_VABI _ZN7WebCore26Matrix3DTransformOperation5blendEPKNS_18TransformOperationEdb(
    WebCore::Matrix3DTransformOperation* self, const WebCore::TransformOperation* from, double progress, bool blendToIdentity) {
    if (from && !WebCore::IsSameType(*from, *self)) {
        return WebCore::RefSelf(self);
    }
    WebCore::FloatSize size;
    WebCore::TransformationMatrix fromT;
    WebCore::TransformationMatrix toT;
    if (from) {
        NKWebKit::TransformAbi::Dispatch(*from).apply(from, fromT, size);
    }
    _ZNK7WebCore26Matrix3DTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(self, toT, size);
    if (blendToIdentity) {
        _ZN7WebCore20TransformationMatrix5blendERKS0_d(&fromT, toT, progress);
        return _ZN7WebCore26Matrix3DTransformOperation6createERKNS_20TransformationMatrixE(fromT);
    }
    _ZN7WebCore20TransformationMatrix5blendERKS0_d(&toT, fromT, progress);
    return _ZN7WebCore26Matrix3DTransformOperation6createERKNS_20TransformationMatrixE(toT);
}

void APS5_VABI _ZNK7WebCore26Matrix3DTransformOperation4dumpERN3WTF10TextStreamE(
    const WebCore::Matrix3DTransformOperation* self, NKWebKit::WtfAbi::TextStream& stream) {
    WebCore::TypeName(stream, self->m_type);
    WebCore::Text(stream, "(");
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_20TransformationMatrixE(stream, self->m_matrix);
    WebCore::Text(stream, ")");
}

}
