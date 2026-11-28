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
 * Derived from Source/WebCore/platform/graphics/transforms/TransformationMatrix.cpp@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
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

FloatPoint InternalMapPoint(const TransformationMatrix& tm, const FloatPoint& p);

template <typename T>
T LoadGuest(const T* source, const char* function) {
    T value;
    if (!GuestCopyIn_nid_no_patch(&value, source, sizeof(T))) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

template <typename T>
void StoreGuest(T* destination, const T& value, const char* function) {
    if (!GuestCopyOut_nid_no_patch(destination, &value, sizeof(T))) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

AffineTransform MultiplyAffine(const AffineTransform& self, const AffineTransform& other) {
    AffineTransform trans;
    trans.m_transform[0] = other.m_transform[0] * self.m_transform[0] + other.m_transform[1] * self.m_transform[2];
    trans.m_transform[1] = other.m_transform[0] * self.m_transform[1] + other.m_transform[1] * self.m_transform[3];
    trans.m_transform[2] = other.m_transform[2] * self.m_transform[0] + other.m_transform[3] * self.m_transform[2];
    trans.m_transform[3] = other.m_transform[2] * self.m_transform[1] + other.m_transform[3] * self.m_transform[3];
    trans.m_transform[4] = other.m_transform[4] * self.m_transform[0] + other.m_transform[5] * self.m_transform[2] + self.m_transform[4];
    trans.m_transform[5] = other.m_transform[4] * self.m_transform[1] + other.m_transform[5] * self.m_transform[3] + self.m_transform[5];
    return trans;
}

TransformationMatrix MultiplyMatrix(const TransformationMatrix& self, const TransformationMatrix& mat) {
    TransformationMatrix result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m_matrix[i][j] = mat.m_matrix[i][0] * self.m_matrix[0][j]
                                  + mat.m_matrix[i][1] * self.m_matrix[1][j]
                                  + mat.m_matrix[i][2] * self.m_matrix[2][j]
                                  + mat.m_matrix[i][3] * self.m_matrix[3][j];
        }
    }
    return result;
}

TransformationMatrix AffineToMatrix(const AffineTransform& t) {
    TransformationMatrix result;
    result.m_matrix[0][0] = t.m_transform[0];
    result.m_matrix[0][1] = t.m_transform[1];
    result.m_matrix[0][2] = 0.0;
    result.m_matrix[0][3] = 0.0;
    result.m_matrix[1][0] = t.m_transform[2];
    result.m_matrix[1][1] = t.m_transform[3];
    result.m_matrix[1][2] = 0.0;
    result.m_matrix[1][3] = 0.0;
    result.m_matrix[2][0] = 0.0;
    result.m_matrix[2][1] = 0.0;
    result.m_matrix[2][2] = 1.0;
    result.m_matrix[2][3] = 0.0;
    result.m_matrix[3][0] = t.m_transform[4];
    result.m_matrix[3][1] = t.m_transform[5];
    result.m_matrix[3][2] = 0.0;
    result.m_matrix[3][3] = 1.0;
    return result;
}

FloatPoint AffineMapPoint(const AffineTransform& t, const FloatPoint& point) {
    double x = static_cast<double>(point.m_x);
    double y = static_cast<double>(point.m_y);
    double x2 = t.m_transform[0] * x + t.m_transform[2] * y + t.m_transform[4];
    double y2 = t.m_transform[1] * x + t.m_transform[3] * y + t.m_transform[5];
    return FloatPoint{static_cast<float>(x2), static_cast<float>(y2)};
}

FloatRect AffineMapRect(const AffineTransform& t, const FloatRect& rect) {
    if (IsAffineIdentityOrTranslation(t)) {
        return FloatRect{
            FloatPoint{rect.m_location.m_x + static_cast<float>(t.m_transform[4]),
                       rect.m_location.m_y + static_cast<float>(t.m_transform[5])},
            rect.m_size
        };
    }
    FloatPoint p1 = AffineMapPoint(t, rect.m_location);
    FloatPoint p2 = AffineMapPoint(t, FloatPoint{rect.m_location.m_x + rect.m_size.m_width, rect.m_location.m_y});
    FloatPoint p3 = AffineMapPoint(t, FloatPoint{rect.m_location.m_x + rect.m_size.m_width, rect.m_location.m_y + rect.m_size.m_height});
    FloatPoint p4 = AffineMapPoint(t, FloatPoint{rect.m_location.m_x, rect.m_location.m_y + rect.m_size.m_height});
    float left = std::min({p1.m_x, p2.m_x, p3.m_x, p4.m_x});
    float top = std::min({p1.m_y, p2.m_y, p3.m_y, p4.m_y});
    float right = std::max({p1.m_x, p2.m_x, p3.m_x, p4.m_x});
    float bottom = std::max({p1.m_y, p2.m_y, p3.m_y, p4.m_y});
    return FloatRect{FloatPoint{left, top}, FloatSize{right - left, bottom - top}};
}

FloatRect MatrixMapRect(const TransformationMatrix& m, const FloatRect& r) {
    auto type = GetMatrixType(m);
    if (type == MatrixType::IdentityOrTranslation) {
        return FloatRect{
            FloatPoint{r.m_location.m_x + static_cast<float>(m.m_matrix[3][0]),
                       r.m_location.m_y + static_cast<float>(m.m_matrix[3][1])},
            r.m_size
        };
    }

    float minX = r.m_location.m_x;
    float minY = r.m_location.m_y;
    float maxX = r.m_location.m_x + r.m_size.m_width;
    float maxY = r.m_location.m_y + r.m_size.m_height;

    if (type == MatrixType::Affine) {
        double a = m.m_matrix[0][0];
        double b = m.m_matrix[0][1];
        double c = m.m_matrix[1][0];
        double d = m.m_matrix[1][1];

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

        return FloatRect{
            FloatPoint{static_cast<float>(minResultX + m.m_matrix[3][0]),
                       static_cast<float>(minResultY + m.m_matrix[3][1])},
            FloatSize{static_cast<float>(maxResultX - minResultX),
                      static_cast<float>(maxResultY - minResultY)}
        };
    }

    FloatPoint p1 = InternalMapPoint(m, FloatPoint{minX, minY});
    FloatPoint p2 = InternalMapPoint(m, FloatPoint{maxX, minY});
    FloatPoint p3 = InternalMapPoint(m, FloatPoint{maxX, maxY});
    FloatPoint p4 = InternalMapPoint(m, FloatPoint{minX, maxY});
    float left = std::min({p1.m_x, p2.m_x, p3.m_x, p4.m_x});
    float top = std::min({p1.m_y, p2.m_y, p3.m_y, p4.m_y});
    float right = std::max({p1.m_x, p2.m_x, p3.m_x, p4.m_x});
    float bottom = std::max({p1.m_y, p2.m_y, p3.m_y, p4.m_y});
    return FloatRect{FloatPoint{left, top}, FloatSize{right - left, bottom - top}};
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
    WebCore::StoreGuest(self, WebCore::AffineTransform{{1.0, 0.0, 0.0, 1.0, 0.0, 0.0}}, __func__);
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform15scaleNonUniformEdd(
    WebCore::AffineTransform* self, double sx, double sy) {
    auto t = WebCore::LoadGuest(self, __func__);
    t.m_transform[0] *= sx;
    t.m_transform[1] *= sx;
    t.m_transform[2] *= sy;
    t.m_transform[3] *= sy;
    WebCore::StoreGuest(self, t, __func__);
    return *self;
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform4skewEdd(
    WebCore::AffineTransform* self, double angleX, double angleY) {
    auto t = WebCore::LoadGuest(self, __func__);
    double sx = std::tan(WebCore::Deg2Rad(angleX));
    double sy = std::tan(WebCore::Deg2Rad(angleY));
    double a = t.m_transform[0];
    double b = t.m_transform[1];
    t.m_transform[0] += sy * t.m_transform[2];
    t.m_transform[1] += sy * t.m_transform[3];
    t.m_transform[2] += sx * a;
    t.m_transform[3] += sx * b;
    WebCore::StoreGuest(self, t, __func__);
    return *self;
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform5flipXEv(WebCore::AffineTransform* self) {
    auto t = WebCore::LoadGuest(self, __func__);
    t.m_transform[0] = -t.m_transform[0];
    t.m_transform[1] = -t.m_transform[1];
    WebCore::StoreGuest(self, t, __func__);
    return *self;
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform5flipYEv(WebCore::AffineTransform* self) {
    auto t = WebCore::LoadGuest(self, __func__);
    t.m_transform[2] = -t.m_transform[2];
    t.m_transform[3] = -t.m_transform[3];
    WebCore::StoreGuest(self, t, __func__);
    return *self;
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform5scaleERKNS_9FloatSizeE(
    WebCore::AffineTransform* self, const WebCore::FloatSize& s) {
    const auto size = WebCore::LoadGuest(&s, __func__);
    return _ZN7WebCore15AffineTransform15scaleNonUniformEdd(self, static_cast<double>(size.m_width), static_cast<double>(size.m_height));
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform5scaleEd(WebCore::AffineTransform* self, double s) {
    return _ZN7WebCore15AffineTransform15scaleNonUniformEdd(self, s, s);
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform5shearEdd(
    WebCore::AffineTransform* self, double sx, double sy) {
    auto t = WebCore::LoadGuest(self, __func__);
    double a = t.m_transform[0];
    double b = t.m_transform[1];
    t.m_transform[0] += sy * t.m_transform[2];
    t.m_transform[1] += sy * t.m_transform[3];
    t.m_transform[2] += sx * a;
    t.m_transform[3] += sx * b;
    WebCore::StoreGuest(self, t, __func__);
    return *self;
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform6rotateEd(WebCore::AffineTransform* self, double a) {
    double rad = WebCore::Deg2Rad(a);
    double cosAngle = std::cos(rad);
    double sinAngle = std::sin(rad);
    WebCore::AffineTransform rot;
    rot.m_transform = {cosAngle, sinAngle, -sinAngle, cosAngle, 0.0, 0.0};
    auto t = WebCore::LoadGuest(self, __func__);
    WebCore::StoreGuest(self, WebCore::MultiplyAffine(t, rot), __func__);
    return *self;
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform8multiplyERKS0_(
    WebCore::AffineTransform* self, const WebCore::AffineTransform& other) {
    const auto right = WebCore::LoadGuest(&other, __func__);
    const auto t = WebCore::LoadGuest(self, __func__);
    WebCore::StoreGuest(self, WebCore::MultiplyAffine(t, right), __func__);
    return *self;
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform9translateERKNS_10FloatPointE(
    WebCore::AffineTransform* self, const WebCore::FloatPoint& p) {
    const auto point = WebCore::LoadGuest(&p, __func__);
    return _ZN7WebCore15AffineTransform9translateEdd(self, static_cast<double>(point.m_x), static_cast<double>(point.m_y));
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform9translateERKNS_9FloatSizeE(
    WebCore::AffineTransform* self, const WebCore::FloatSize& s) {
    const auto size = WebCore::LoadGuest(&s, __func__);
    return _ZN7WebCore15AffineTransform9translateEdd(self, static_cast<double>(size.m_width), static_cast<double>(size.m_height));
}

WebCore::AffineTransform& APS5_VABI _ZN7WebCore15AffineTransform9translateEdd(
    WebCore::AffineTransform* self, double tx, double ty) {
    auto t = WebCore::LoadGuest(self, __func__);
    if (WebCore::IsAffineIdentityOrTranslation(t)) {
        t.m_transform[4] += tx;
        t.m_transform[5] += ty;
    } else {
        t.m_transform[4] += tx * t.m_transform[0] + ty * t.m_transform[2];
        t.m_transform[5] += tx * t.m_transform[1] + ty * t.m_transform[3];
    }
    WebCore::StoreGuest(self, t, __func__);
    return *self;
}

bool APS5_VABI _ZNK7WebCore15AffineTransform10isIdentityEv(const WebCore::AffineTransform* self) {
    const auto t = WebCore::LoadGuest(self, __func__);
    return t.m_transform[0] == 1.0 && t.m_transform[1] == 0.0
        && t.m_transform[2] == 0.0 && t.m_transform[3] == 1.0
        && t.m_transform[4] == 0.0 && t.m_transform[5] == 0.0;
}

WebCore::TransformationMatrix APS5_VABI _ZNK7WebCore15AffineTransform22toTransformationMatrixEv(
    const WebCore::AffineTransform* self) {
    return WebCore::AffineToMatrix(WebCore::LoadGuest(self, __func__));
}

double APS5_VABI _ZNK7WebCore15AffineTransform6xScaleEv(const WebCore::AffineTransform* self) {
    const auto t = WebCore::LoadGuest(self, __func__);
    return std::hypot(t.m_transform[0], t.m_transform[1]);
}

double APS5_VABI _ZNK7WebCore15AffineTransform6yScaleEv(const WebCore::AffineTransform* self) {
    const auto t = WebCore::LoadGuest(self, __func__);
    return std::hypot(t.m_transform[2], t.m_transform[3]);
}

WebCore::FloatQuad APS5_VABI _ZNK7WebCore15AffineTransform7mapQuadERKNS_9FloatQuadE(
    const WebCore::AffineTransform* self, const WebCore::FloatQuad& quad) {
    const auto t = WebCore::LoadGuest(self, __func__);
    const auto q = WebCore::LoadGuest(&quad, __func__);
    if (WebCore::IsAffineIdentityOrTranslation(t)) {
        float tx = static_cast<float>(t.m_transform[4]);
        float ty = static_cast<float>(t.m_transform[5]);
        return WebCore::FloatQuad{
            WebCore::FloatPoint{q.m_p1.m_x + tx, q.m_p1.m_y + ty},
            WebCore::FloatPoint{q.m_p2.m_x + tx, q.m_p2.m_y + ty},
            WebCore::FloatPoint{q.m_p3.m_x + tx, q.m_p3.m_y + ty},
            WebCore::FloatPoint{q.m_p4.m_x + tx, q.m_p4.m_y + ty}
        };
    }
    return WebCore::FloatQuad{
        WebCore::AffineMapPoint(t, q.m_p1),
        WebCore::AffineMapPoint(t, q.m_p2),
        WebCore::AffineMapPoint(t, q.m_p3),
        WebCore::AffineMapPoint(t, q.m_p4)
    };
}

WebCore::IntRect APS5_VABI _ZNK7WebCore15AffineTransform7mapRectERKNS_7IntRectE(
    const WebCore::AffineTransform* self, const WebCore::IntRect& rect) {
    WebCore::LoadGuest(self, __func__);
    const auto r = WebCore::LoadGuest(&rect, __func__);
    WebCore::FloatRect fr{
        WebCore::FloatPoint{static_cast<float>(r.m_location.m_x), static_cast<float>(r.m_location.m_y)},
        WebCore::FloatSize{static_cast<float>(r.m_size.m_width), static_cast<float>(r.m_size.m_height)}
    };
    WebCore::FloatRect mapped = _ZNK7WebCore15AffineTransform7mapRectERKNS_9FloatRectE(self, fr);
    return WebCore::EnclosingIntRect(mapped);
}

WebCore::FloatRect APS5_VABI _ZNK7WebCore15AffineTransform7mapRectERKNS_9FloatRectE(
    const WebCore::AffineTransform* self, const WebCore::FloatRect& rect) {
    return WebCore::AffineMapRect(WebCore::LoadGuest(self, __func__), WebCore::LoadGuest(&rect, __func__));
}

WebCore::IntSize APS5_VABI _ZNK7WebCore15AffineTransform7mapSizeERKNS_7IntSizeE(
    const WebCore::AffineTransform* self, const WebCore::IntSize& intSize) {
    const auto t = WebCore::LoadGuest(self, __func__);
    const auto size = WebCore::LoadGuest(&intSize, __func__);
    double width2 = static_cast<double>(size.m_width) * std::hypot(t.m_transform[0], t.m_transform[1]);
    double height2 = static_cast<double>(size.m_height) * std::hypot(t.m_transform[2], t.m_transform[3]);
    return WebCore::IntSize{
        static_cast<int>(std::lround(width2)),
        static_cast<int>(std::lround(height2))
    };
}

WebCore::FloatSize APS5_VABI _ZNK7WebCore15AffineTransform7mapSizeERKNS_9FloatSizeE(
    const WebCore::AffineTransform* self, const WebCore::FloatSize& floatSize) {
    const auto t = WebCore::LoadGuest(self, __func__);
    const auto size = WebCore::LoadGuest(&floatSize, __func__);
    double width2 = static_cast<double>(size.m_width) * std::hypot(t.m_transform[0], t.m_transform[1]);
    double height2 = static_cast<double>(size.m_height) * std::hypot(t.m_transform[2], t.m_transform[3]);
    return WebCore::FloatSize{static_cast<float>(width2), static_cast<float>(height2)};
}

WebCore::FloatPoint APS5_VABI _ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE(
    const WebCore::AffineTransform* self, const WebCore::FloatPoint& point) {
    return WebCore::AffineMapPoint(WebCore::LoadGuest(self, __func__), WebCore::LoadGuest(&point, __func__));
}

WebCore::IntPoint APS5_VABI _ZNK7WebCore15AffineTransform8mapPointERKNS_8IntPointE(
    const WebCore::AffineTransform* self, const WebCore::IntPoint& intPoint) {
    const auto t = WebCore::LoadGuest(self, __func__);
    const auto point = WebCore::LoadGuest(&intPoint, __func__);
    double x = static_cast<double>(point.m_x);
    double y = static_cast<double>(point.m_y);
    double x2 = t.m_transform[0] * x + t.m_transform[2] * y + t.m_transform[4];
    double y2 = t.m_transform[1] * x + t.m_transform[3] * y + t.m_transform[5];
    return WebCore::IntPoint{
        static_cast<int>(std::lround(x2)),
        static_cast<int>(std::lround(y2))
    };
}

void APS5_VABI _ZN7WebCore20TransformationMatrix10makeAffineEv(WebCore::TransformationMatrix* self) {
    auto m = WebCore::LoadGuest(self, __func__);
    m.m_matrix[0][2] = 0.0;
    m.m_matrix[0][3] = 0.0;
    m.m_matrix[1][2] = 0.0;
    m.m_matrix[1][3] = 0.0;
    m.m_matrix[2][0] = 0.0;
    m.m_matrix[2][1] = 0.0;
    m.m_matrix[2][2] = 1.0;
    m.m_matrix[2][3] = 0.0;
    m.m_matrix[3][2] = 0.0;
    m.m_matrix[3][3] = 1.0;
    WebCore::StoreGuest(self, m, __func__);
}

WebCore::TransformationMatrix APS5_VABI _ZN7WebCore20TransformationMatrix10rectToRectERKNS_9FloatRectES3_(
    const WebCore::FloatRect& fromRect, const WebCore::FloatRect& toRect) {
    const auto from = WebCore::LoadGuest(&fromRect, __func__);
    const auto to = WebCore::LoadGuest(&toRect, __func__);
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
    auto m = WebCore::LoadGuest(self, __func__);
    m.m_matrix[3][0] += tx * m.m_matrix[0][0] + ty * m.m_matrix[1][0] + tz * m.m_matrix[2][0];
    m.m_matrix[3][1] += tx * m.m_matrix[0][1] + ty * m.m_matrix[1][1] + tz * m.m_matrix[2][1];
    m.m_matrix[3][2] += tx * m.m_matrix[0][2] + ty * m.m_matrix[1][2] + tz * m.m_matrix[2][2];
    m.m_matrix[3][3] += tx * m.m_matrix[0][3] + ty * m.m_matrix[1][3] + tz * m.m_matrix[2][3];
    WebCore::StoreGuest(self, m, __func__);
    return *self;
}

WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix15scaleNonUniformEdd(
    WebCore::TransformationMatrix* self, double sx, double sy) {
    auto m = WebCore::LoadGuest(self, __func__);
    m.m_matrix[0][0] *= sx;
    m.m_matrix[0][1] *= sx;
    m.m_matrix[0][2] *= sx;
    m.m_matrix[0][3] *= sx;

    m.m_matrix[1][0] *= sy;
    m.m_matrix[1][1] *= sy;
    m.m_matrix[1][2] *= sy;
    m.m_matrix[1][3] *= sy;
    WebCore::StoreGuest(self, m, __func__);
    return *self;
}

WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix4skewEdd(
    WebCore::TransformationMatrix* self, double sx, double sy) {
    double radX = WebCore::Deg2Rad(sx);
    double radY = WebCore::Deg2Rad(sy);
    WebCore::TransformationMatrix mat;
    mat.m_matrix[0][1] = std::tan(radY);
    mat.m_matrix[1][0] = std::tan(radX);
    WebCore::StoreGuest(self, WebCore::MultiplyMatrix(WebCore::LoadGuest(self, __func__), mat), __func__);
    return *self;
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
    if (std::fmod(a, 360.0) == 0.0) return *self;
    return _ZN7WebCore20TransformationMatrix8rotate3dEddd(self, 0.0, 0.0, a);
}

WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix8multiplyERKS0_(
    WebCore::TransformationMatrix* self, const WebCore::TransformationMatrix& mat) {
    const auto right = WebCore::LoadGuest(&mat, __func__);
    WebCore::StoreGuest(self, WebCore::MultiplyMatrix(WebCore::LoadGuest(self, __func__), right), __func__);
    return *self;
}

WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix16applyPerspectiveEd(
    WebCore::TransformationMatrix* self, double perspective) {
    WebCore::TransformationMatrix mat;
    if (perspective != 0.0) mat.m_matrix[2][3] = -1.0 / perspective;
    WebCore::StoreGuest(self, WebCore::MultiplyMatrix(WebCore::LoadGuest(self, __func__), mat), __func__);
    return *self;
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
    rmat = WebCore::MultiplyMatrix(rmat, matY);

    WebCore::TransformationMatrix matX;
    sinTheta = std::sin(radX);
    cosTheta = std::cos(radX);
    matX.m_matrix[1][1] = cosTheta;
    matX.m_matrix[1][2] = sinTheta;
    matX.m_matrix[2][1] = -sinTheta;
    matX.m_matrix[2][2] = cosTheta;
    rmat = WebCore::MultiplyMatrix(rmat, matX);

    WebCore::StoreGuest(self, WebCore::MultiplyMatrix(WebCore::LoadGuest(self, __func__), rmat), __func__);
    return *self;
}

WebCore::TransformationMatrix& APS5_VABI _ZN7WebCore20TransformationMatrix9translateEdd(
    WebCore::TransformationMatrix* self, double tx, double ty) {
    return _ZN7WebCore20TransformationMatrix11translate3dEddd(self, tx, ty, 0.0);
}

void APS5_VABI _ZN7WebCore20TransformationMatrixC1ERKNS_15AffineTransformE(
    WebCore::TransformationMatrix* self, const WebCore::AffineTransform& t) {
    WebCore::StoreGuest(self, WebCore::AffineToMatrix(WebCore::LoadGuest(&t, __func__)), __func__);
}

void APS5_VABI _ZN7WebCore20TransformationMatrixC2ERKNS_15AffineTransformE(
    WebCore::TransformationMatrix* self, const WebCore::AffineTransform& t) {
    _ZN7WebCore20TransformationMatrixC1ERKNS_15AffineTransformE(self, t);
}

bool APS5_VABI _ZNK7WebCore20TransformationMatrix12isInvertibleEv(const WebCore::TransformationMatrix* self) {
    const auto m = WebCore::LoadGuest(self, __func__);
    auto type = WebCore::GetMatrixType(m);
    if (type == WebCore::MatrixType::IdentityOrTranslation) {
        return true;
    }
    double det = (type == WebCore::MatrixType::Affine)
        ? (m.m_matrix[0][0] * m.m_matrix[1][1] - m.m_matrix[0][1] * m.m_matrix[1][0])
        : WebCore::Determinant4x4(m.m_matrix);
    return std::abs(det) >= WebCore::kSmallNumber;
}

WebCore::AffineTransform APS5_VABI _ZNK7WebCore20TransformationMatrix17toAffineTransformEv(
    const WebCore::TransformationMatrix* self) {
    const auto m = WebCore::LoadGuest(self, __func__);
    WebCore::AffineTransform result;
    result.m_transform[0] = m.m_matrix[0][0];
    result.m_transform[1] = m.m_matrix[0][1];
    result.m_transform[2] = m.m_matrix[1][0];
    result.m_transform[3] = m.m_matrix[1][1];
    result.m_transform[4] = m.m_matrix[3][0];
    result.m_transform[5] = m.m_matrix[3][1];
    return result;
}

WebCore::FloatQuad APS5_VABI _ZNK7WebCore20TransformationMatrix7mapQuadERKNS_9FloatQuadE(
    const WebCore::TransformationMatrix* self, const WebCore::FloatQuad& quad) {
    const auto m = WebCore::LoadGuest(self, __func__);
    const auto q = WebCore::LoadGuest(&quad, __func__);
    if (WebCore::GetMatrixType(m) == WebCore::MatrixType::IdentityOrTranslation) {
        float tx = static_cast<float>(m.m_matrix[3][0]);
        float ty = static_cast<float>(m.m_matrix[3][1]);
        return WebCore::FloatQuad{
            WebCore::FloatPoint{q.m_p1.m_x + tx, q.m_p1.m_y + ty},
            WebCore::FloatPoint{q.m_p2.m_x + tx, q.m_p2.m_y + ty},
            WebCore::FloatPoint{q.m_p3.m_x + tx, q.m_p3.m_y + ty},
            WebCore::FloatPoint{q.m_p4.m_x + tx, q.m_p4.m_y + ty}
        };
    }
    return WebCore::FloatQuad{
        WebCore::InternalMapPoint(m, q.m_p1),
        WebCore::InternalMapPoint(m, q.m_p2),
        WebCore::InternalMapPoint(m, q.m_p3),
        WebCore::InternalMapPoint(m, q.m_p4)
    };
}

WebCore::IntRect APS5_VABI _ZNK7WebCore20TransformationMatrix7mapRectERKNS_7IntRectE(
    const WebCore::TransformationMatrix* self, const WebCore::IntRect& rect) {
    WebCore::LoadGuest(self, __func__);
    const auto r = WebCore::LoadGuest(&rect, __func__);
    WebCore::FloatRect fr{
        WebCore::FloatPoint{static_cast<float>(r.m_location.m_x), static_cast<float>(r.m_location.m_y)},
        WebCore::FloatSize{static_cast<float>(r.m_size.m_width), static_cast<float>(r.m_size.m_height)}
    };
    WebCore::FloatRect mapped = _ZNK7WebCore20TransformationMatrix7mapRectERKNS_9FloatRectE(self, fr);
    return WebCore::EnclosingIntRect(mapped);
}

WebCore::FloatRect APS5_VABI _ZNK7WebCore20TransformationMatrix7mapRectERKNS_9FloatRectE(
    const WebCore::TransformationMatrix* self, const WebCore::FloatRect& r) {
    return WebCore::MatrixMapRect(WebCore::LoadGuest(self, __func__), WebCore::LoadGuest(&r, __func__));
}

WebCore::FloatPoint APS5_VABI _ZNK7WebCore20TransformationMatrix8mapPointERKNS_10FloatPointE(
    const WebCore::TransformationMatrix* self, const WebCore::FloatPoint& point) {
    const auto m = WebCore::LoadGuest(self, __func__);
    const auto p = WebCore::LoadGuest(&point, __func__);
    if (WebCore::GetMatrixType(m) == WebCore::MatrixType::IdentityOrTranslation) {
        return WebCore::FloatPoint{
            p.m_x + static_cast<float>(m.m_matrix[3][0]),
            p.m_y + static_cast<float>(m.m_matrix[3][1])
        };
    }
    return WebCore::InternalMapPoint(m, p);
}

WebCore::TransformationMatrix APS5_VABI _ZNK7WebCore24MatrixTransformOperation6matrixEv(
    const WebCore::MatrixTransformOperation* self) {
    const auto operation = WebCore::LoadGuest(self, __func__);
    WebCore::TransformationMatrix result;
    result.m_matrix[0][0] = operation.m_a;
    result.m_matrix[0][1] = operation.m_b;
    result.m_matrix[0][2] = 0.0;
    result.m_matrix[0][3] = 0.0;
    result.m_matrix[1][0] = operation.m_c;
    result.m_matrix[1][1] = operation.m_d;
    result.m_matrix[1][2] = 0.0;
    result.m_matrix[1][3] = 0.0;
    result.m_matrix[2][0] = 0.0;
    result.m_matrix[2][1] = 0.0;
    result.m_matrix[2][2] = 1.0;
    result.m_matrix[2][3] = 0.0;
    result.m_matrix[3][0] = operation.m_e;
    result.m_matrix[3][1] = operation.m_f;
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
