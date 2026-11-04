#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace WebCore {

namespace {

int ClampToInteger(float value) {
    if (value >= static_cast<float>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    if (!(value > static_cast<float>(std::numeric_limits<int>::min()))) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(value);
}

int ClampToInteger(double value) {
    if (value >= static_cast<double>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    if (!(value > static_cast<double>(std::numeric_limits<int>::min()))) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(value);
}

bool AreEssentiallyEqual(float u, float v) {
    if (u == v) {
        return true;
    }
    float delta = std::abs(u - v);
    float eps = std::numeric_limits<float>::epsilon();
    auto safeDiv = [](float num, float den) {
        if (std::abs(num) < std::abs(den) * std::numeric_limits<float>::max()) {
            return num / den;
        }
        return std::numeric_limits<float>::max();
    };
    return safeDiv(delta, std::abs(u)) <= eps && safeDiv(delta, std::abs(v)) <= eps;
}

bool IsPointInTriangle(const FloatPoint& p, const FloatPoint& t1, const FloatPoint& t2, const FloatPoint& t3) {
    float v0x = t3.m_x - t1.m_x;
    float v0y = t3.m_y - t1.m_y;
    float v1x = t2.m_x - t1.m_x;
    float v1y = t2.m_y - t1.m_y;
    float v2x = p.m_x - t1.m_x;
    float v2y = p.m_y - t1.m_y;

    float dot00 = v0x * v0x + v0y * v0y;
    float dot01 = v0x * v1x + v0y * v1y;
    float dot02 = v0x * v2x + v0y * v2y;
    float dot11 = v1x * v1x + v1y * v1y;
    float dot12 = v1x * v2x + v1y * v2y;

    float denom = dot00 * dot11 - dot01 * dot01;
    if (denom == 0.0f) {
        return false;
    }
    float invDenom = 1.0f / denom;
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;
    return (u >= 0.0f) && (v >= 0.0f) && (u + v <= 1.0f);
}

bool EllipseContainsPoint(const FloatPoint& center, float rx, float ry, const FloatPoint& point) {
    if (rx <= 0.0f || ry <= 0.0f) {
        return false;
    }
    FloatPoint transformed{point.m_x - center.m_x, point.m_y - center.m_y};
    if (transformed.m_x < -rx || transformed.m_x > rx || transformed.m_y < -ry || transformed.m_y > ry) {
        return false;
    }
    transformed.m_x *= ry;
    transformed.m_y *= rx;
    float radius = rx * ry;
    if (std::abs(transformed.m_x) + std::abs(transformed.m_y) <= radius) {
        return true;
    }
    return (transformed.m_x * transformed.m_x + transformed.m_y * transformed.m_y) <= (radius * radius);
}

bool IsRoundedRectRenderable(const RoundedRect& rr) {
    int w = rr.m_rect.m_size.m_width.m_value;
    int h = rr.m_rect.m_size.m_height.m_value;
    int tlw = rr.m_radii.m_topLeft.m_width.m_value;
    int tlh = rr.m_radii.m_topLeft.m_height.m_value;
    int trw = rr.m_radii.m_topRight.m_width.m_value;
    int trh = rr.m_radii.m_topRight.m_height.m_value;
    int blw = rr.m_radii.m_bottomLeft.m_width.m_value;
    int blh = rr.m_radii.m_bottomLeft.m_height.m_value;
    int brw = rr.m_radii.m_bottomRight.m_width.m_value;
    int brh = rr.m_radii.m_bottomRight.m_height.m_value;
    return tlw >= 0 && tlh >= 0 && blw >= 0 && blh >= 0
        && trw >= 0 && trh >= 0 && brw >= 0 && brh >= 0
        && (tlw + trw) <= w && (blw + brw) <= w
        && (tlh + blh) <= h && (trh + brh) <= h;
}

}  // namespace
}  // namespace WebCore

extern "C" {
WebCore::FloatPoint APS5_VABI _ZN7WebCore10FloatPoint15narrowPrecisionEdd(double x, double y) {
    return WebCore::FloatPoint{static_cast<float>(x), static_cast<float>(y)};
}

void APS5_VABI _ZN7WebCore10FloatPoint9normalizeEv(WebCore::FloatPoint* self) {
    float length = std::hypot(self->m_x, self->m_y);
    if (length != 0.0f) {
        self->m_x /= length;
        self->m_y /= length;
    }
}

void APS5_VABI _ZN7WebCore10FloatPointC1ERKNS_8IntPointE(WebCore::FloatPoint* self, const WebCore::IntPoint& p) {
    self->m_x = static_cast<float>(p.m_x);
    self->m_y = static_cast<float>(p.m_y);
}

void APS5_VABI _ZN7WebCore10FloatPointC2ERKNS_8IntPointE(WebCore::FloatPoint* self, const WebCore::IntPoint& p) {
    self->m_x = static_cast<float>(p.m_x);
    self->m_y = static_cast<float>(p.m_y);
}

WebCore::FloatPoint APS5_VABI _ZNK7WebCore10FloatPoint15matrixTransformERKNS_15AffineTransformE(
    const WebCore::FloatPoint* self, const WebCore::AffineTransform& t) {
    double x = static_cast<double>(self->m_x);
    double y = static_cast<double>(self->m_y);
    double newX = t.m_transform[0] * x + t.m_transform[2] * y + t.m_transform[4];
    double newY = t.m_transform[1] * x + t.m_transform[3] * y + t.m_transform[5];
    return WebCore::FloatPoint{static_cast<float>(newX), static_cast<float>(newY)};
}

WebCore::FloatPoint APS5_VABI _ZNK7WebCore10FloatPoint15matrixTransformERKNS_20TransformationMatrixE(
    const WebCore::FloatPoint* self, const WebCore::TransformationMatrix& t) {
    double x = static_cast<double>(self->m_x);
    double y = static_cast<double>(self->m_y);
    double resultX = t.m_matrix[3][0] + x * t.m_matrix[0][0] + y * t.m_matrix[1][0];
    double resultY = t.m_matrix[3][1] + x * t.m_matrix[0][1] + y * t.m_matrix[1][1];
    double w = t.m_matrix[3][3] + x * t.m_matrix[0][3] + y * t.m_matrix[1][3];
    if (w != 1.0 && w != 0.0) {
        resultX /= w;
        resultY /= w;
    }
    return WebCore::FloatPoint{static_cast<float>(resultX), static_cast<float>(resultY)};
}

WebCore::FloatPoint APS5_VABI _ZNK7WebCore10FloatPoint18constrainedBetweenERKS0_S2_(
    const WebCore::FloatPoint* self, const WebCore::FloatPoint& min, const WebCore::FloatPoint& max) {
    return WebCore::FloatPoint{
        std::max(min.m_x, std::min(max.m_x, self->m_x)),
        std::max(min.m_y, std::min(max.m_y, self->m_y))
    };
}

WebCore::FloatSize APS5_VABI _ZNK7WebCore9FloatSize18constrainedBetweenERKS0_S2_(
    const WebCore::FloatSize* self, const WebCore::FloatSize& min, const WebCore::FloatSize& max) {
    return WebCore::FloatSize{
        std::max(min.m_width, std::min(max.m_width, self->m_width)),
        std::max(min.m_height, std::min(max.m_height, self->m_height))
    };
}

bool APS5_VABI _ZNK7WebCore9FloatSize6isZeroEv(const WebCore::FloatSize* self) {
    return self->m_width == 0.0f && self->m_height == 0.0f;
}

void APS5_VABI _ZN7WebCore9FloatRect5scaleEff(WebCore::FloatRect* self, float sx, float sy) {
    self->m_location.m_x *= sx;
    self->m_location.m_y *= sy;
    self->m_size.m_width *= sx;
    self->m_size.m_height *= sy;
}

void APS5_VABI _ZN7WebCore9FloatRect5uniteERKS0_(WebCore::FloatRect* self, const WebCore::FloatRect& other) {
    if (other.m_size.m_width <= 0.0f || other.m_size.m_height <= 0.0f) {
        return;
    }
    if (self->m_size.m_width <= 0.0f || self->m_size.m_height <= 0.0f) {
        *self = other;
        return;
    }
    float minX = std::min(self->m_location.m_x, other.m_location.m_x);
    float minY = std::min(self->m_location.m_y, other.m_location.m_y);
    float maxX = std::max(self->m_location.m_x + self->m_size.m_width, other.m_location.m_x + other.m_size.m_width);
    float maxY = std::max(self->m_location.m_y + self->m_size.m_height, other.m_location.m_y + other.m_size.m_height);
    self->m_location = {minX, minY};
    self->m_size = {maxX - minX, maxY - minY};
}

void APS5_VABI _ZN7WebCore9FloatRect6extendERKNS_10FloatPointE(WebCore::FloatRect* self, const WebCore::FloatPoint& p) {
    float minX = std::min(self->m_location.m_x, p.m_x);
    float minY = std::min(self->m_location.m_y, p.m_y);
    float maxX = std::max(self->m_location.m_x + self->m_size.m_width, p.m_x);
    float maxY = std::max(self->m_location.m_y + self->m_size.m_height, p.m_y);
    self->m_location = {minX, minY};
    self->m_size = {maxX - minX, maxY - minY};
}

void APS5_VABI _ZN7WebCore9FloatRect9intersectERKS0_(WebCore::FloatRect* self, const WebCore::FloatRect& other) {
    float l = std::max(self->m_location.m_x, other.m_location.m_x);
    float t = std::max(self->m_location.m_y, other.m_location.m_y);
    float r = std::min(self->m_location.m_x + self->m_size.m_width, other.m_location.m_x + other.m_size.m_width);
    float b = std::min(self->m_location.m_y + self->m_size.m_height, other.m_location.m_y + other.m_size.m_height);
    if (l >= r || t >= b) {
        self->m_location = {0.0f, 0.0f};
        self->m_size = {0.0f, 0.0f};
    } else {
        self->m_location = {l, t};
        self->m_size = {r - l, b - t};
    }
}

void APS5_VABI _ZN7WebCore9FloatRectC1ERKNS_7IntRectE(WebCore::FloatRect* self, const WebCore::IntRect& r) {
    self->m_location.m_x = static_cast<float>(r.m_location.m_x);
    self->m_location.m_y = static_cast<float>(r.m_location.m_y);
    self->m_size.m_width = static_cast<float>(r.m_size.m_width);
    self->m_size.m_height = static_cast<float>(r.m_size.m_height);
}

void APS5_VABI _ZN7WebCore9FloatRectC2ERKNS_7IntRectE(WebCore::FloatRect* self, const WebCore::IntRect& r) {
    self->m_location.m_x = static_cast<float>(r.m_location.m_x);
    self->m_location.m_y = static_cast<float>(r.m_location.m_y);
    self->m_size.m_width = static_cast<float>(r.m_size.m_width);
    self->m_size.m_height = static_cast<float>(r.m_size.m_height);
}

bool APS5_VABI _ZNK7WebCore9FloatRect10intersectsERKS0_(const WebCore::FloatRect* self, const WebCore::FloatRect& other) {
    return self->m_size.m_width > 0.0f && self->m_size.m_height > 0.0f
        && other.m_size.m_width > 0.0f && other.m_size.m_height > 0.0f
        && self->m_location.m_x < other.m_location.m_x + other.m_size.m_width
        && other.m_location.m_x < self->m_location.m_x + self->m_size.m_width
        && self->m_location.m_y < other.m_location.m_y + other.m_size.m_height
        && other.m_location.m_y < self->m_location.m_y + self->m_size.m_height;
}

bool APS5_VABI _ZNK7WebCore9FloatRect21inclusivelyIntersectsERKS0_(const WebCore::FloatRect* self, const WebCore::FloatRect& other) {
    return self->m_size.m_width >= 0.0f && self->m_size.m_height >= 0.0f
        && other.m_size.m_width >= 0.0f && other.m_size.m_height >= 0.0f
        && self->m_location.m_x <= other.m_location.m_x + other.m_size.m_width
        && other.m_location.m_x <= self->m_location.m_x + self->m_size.m_width
        && self->m_location.m_y <= other.m_location.m_y + other.m_size.m_height
        && other.m_location.m_y <= self->m_location.m_y + self->m_size.m_height;
}

bool APS5_VABI _ZNK7WebCore9FloatRect8containsERKNS_10FloatPointENS0_12ContainsModeE(
    const WebCore::FloatRect* self, const WebCore::FloatPoint& point, std::uint32_t containsMode) {
    if (containsMode == 0) {
        return point.m_x >= self->m_location.m_x
            && point.m_x <= self->m_location.m_x + self->m_size.m_width
            && point.m_y >= self->m_location.m_y
            && point.m_y <= self->m_location.m_y + self->m_size.m_height;
    }
    return point.m_x > self->m_location.m_x
        && point.m_x < self->m_location.m_x + self->m_size.m_width
        && point.m_y > self->m_location.m_y
        && point.m_y < self->m_location.m_y + self->m_size.m_height;
}

bool APS5_VABI _ZNK7WebCore9FloatRect8containsERKS0_(const WebCore::FloatRect* self, const WebCore::FloatRect& other) {
    return self->m_location.m_x <= other.m_location.m_x
        && self->m_location.m_x + self->m_size.m_width >= other.m_location.m_x + other.m_size.m_width
        && self->m_location.m_y <= other.m_location.m_y
        && self->m_location.m_y + self->m_size.m_height >= other.m_location.m_y + other.m_size.m_height;
}

WebCore::FloatRect APS5_VABI _ZNK7WebCore9FloatQuad11boundingBoxEv(const WebCore::FloatQuad* self) {
    float left = std::min({self->m_p1.m_x, self->m_p2.m_x, self->m_p3.m_x, self->m_p4.m_x});
    float top = std::min({self->m_p1.m_y, self->m_p2.m_y, self->m_p3.m_y, self->m_p4.m_y});
    float right = std::max({self->m_p1.m_x, self->m_p2.m_x, self->m_p3.m_x, self->m_p4.m_x});
    float bottom = std::max({self->m_p1.m_y, self->m_p2.m_y, self->m_p3.m_y, self->m_p4.m_y});
    return WebCore::FloatRect{{left, top}, {right - left, bottom - top}};
}

bool APS5_VABI _ZNK7WebCore9FloatQuad12containsQuadERKS0_(const WebCore::FloatQuad* self, const WebCore::FloatQuad& other) {
    auto containsPoint = [self](const WebCore::FloatPoint& p) {
        return WebCore::IsPointInTriangle(p, self->m_p1, self->m_p2, self->m_p3)
            || WebCore::IsPointInTriangle(p, self->m_p1, self->m_p3, self->m_p4);
    };
    return containsPoint(other.m_p1) && containsPoint(other.m_p2) && containsPoint(other.m_p3) && containsPoint(other.m_p4);
}

bool APS5_VABI _ZNK7WebCore9FloatQuad13containsPointERKNS_10FloatPointE(const WebCore::FloatQuad* self, const WebCore::FloatPoint& point) {
    return WebCore::IsPointInTriangle(point, self->m_p1, self->m_p2, self->m_p3)
        || WebCore::IsPointInTriangle(point, self->m_p1, self->m_p3, self->m_p4);
}

bool APS5_VABI _ZNK7WebCore9FloatQuad13isRectilinearEv(const WebCore::FloatQuad* self) {
    return (WebCore::AreEssentiallyEqual(self->m_p1.m_x, self->m_p2.m_x)
            && WebCore::AreEssentiallyEqual(self->m_p2.m_y, self->m_p3.m_y)
            && WebCore::AreEssentiallyEqual(self->m_p3.m_x, self->m_p4.m_x)
            && WebCore::AreEssentiallyEqual(self->m_p4.m_y, self->m_p1.m_y))
        || (WebCore::AreEssentiallyEqual(self->m_p1.m_y, self->m_p2.m_y)
            && WebCore::AreEssentiallyEqual(self->m_p2.m_x, self->m_p3.m_x)
            && WebCore::AreEssentiallyEqual(self->m_p3.m_y, self->m_p4.m_y)
            && WebCore::AreEssentiallyEqual(self->m_p4.m_x, self->m_p1.m_x));
}

void APS5_VABI _ZN7WebCore16FloatRoundedRectC1ERKNS_9FloatRectERKNS0_5RadiiE(
    WebCore::FloatRoundedRect* self, const WebCore::FloatRect& rect, const WebCore::FloatRoundedRect::Radii& radii) {
    self->m_rect = rect;
    self->m_radii = radii;
}

void APS5_VABI _ZN7WebCore16FloatRoundedRectC2ERKNS_9FloatRectERKNS0_5RadiiE(
    WebCore::FloatRoundedRect* self, const WebCore::FloatRect& rect, const WebCore::FloatRoundedRect::Radii& radii) {
    self->m_rect = rect;
    self->m_radii = radii;
}

void APS5_VABI _ZN7WebCore8IntPointC1ERKNS_10FloatPointE(WebCore::IntPoint* self, const WebCore::FloatPoint& p) {
    self->m_x = WebCore::ClampToInteger(p.m_x);
    self->m_y = WebCore::ClampToInteger(p.m_y);
}

void APS5_VABI _ZN7WebCore8IntPointC2ERKNS_10FloatPointE(WebCore::IntPoint* self, const WebCore::FloatPoint& p) {
    self->m_x = WebCore::ClampToInteger(p.m_x);
    self->m_y = WebCore::ClampToInteger(p.m_y);
}

WebCore::IntPoint APS5_VABI _ZNK7WebCore8IntPoint18constrainedBetweenERKS0_S2_(
    const WebCore::IntPoint* self, const WebCore::IntPoint& min, const WebCore::IntPoint& max) {
    return WebCore::IntPoint{
        std::max(min.m_x, std::min(max.m_x, self->m_x)),
        std::max(min.m_y, std::min(max.m_y, self->m_y))
    };
}

void APS5_VABI _ZN7WebCore7IntSizeC1ERKNS_9FloatSizeE(WebCore::IntSize* self, const WebCore::FloatSize& s) {
    self->m_width = WebCore::ClampToInteger(s.m_width);
    self->m_height = WebCore::ClampToInteger(s.m_height);
}

void APS5_VABI _ZN7WebCore7IntSizeC2ERKNS_9FloatSizeE(WebCore::IntSize* self, const WebCore::FloatSize& s) {
    self->m_width = WebCore::ClampToInteger(s.m_width);
    self->m_height = WebCore::ClampToInteger(s.m_height);
}

WebCore::IntSize APS5_VABI _ZNK7WebCore7IntSize18constrainedBetweenERKS0_S2_(
    const WebCore::IntSize* self, const WebCore::IntSize& min, const WebCore::IntSize& max) {
    return WebCore::IntSize{
        std::max(min.m_width, std::min(max.m_width, self->m_width)),
        std::max(min.m_height, std::min(max.m_height, self->m_height))
    };
}

void APS5_VABI _ZN7WebCore7IntRect5scaleEf(WebCore::IntRect* self, float s) {
    self->m_location.m_x = static_cast<int>(self->m_location.m_x * s);
    self->m_location.m_y = static_cast<int>(self->m_location.m_y * s);
    self->m_size.m_width = static_cast<int>(self->m_size.m_width * s);
    self->m_size.m_height = static_cast<int>(self->m_size.m_height * s);
}

void APS5_VABI _ZN7WebCore7IntRect5uniteERKS0_(WebCore::IntRect* self, const WebCore::IntRect& other) {
    if (other.m_size.m_width <= 0 || other.m_size.m_height <= 0) {
        return;
    }
    if (self->m_size.m_width <= 0 || self->m_size.m_height <= 0) {
        *self = other;
        return;
    }
    int l = std::min(self->m_location.m_x, other.m_location.m_x);
    int t = std::min(self->m_location.m_y, other.m_location.m_y);
    int r = std::max(self->m_location.m_x + self->m_size.m_width, other.m_location.m_x + other.m_size.m_width);
    int b = std::max(self->m_location.m_y + self->m_size.m_height, other.m_location.m_y + other.m_size.m_height);
    self->m_location = {l, t};
    self->m_size = {r - l, b - t};
}

void APS5_VABI _ZN7WebCore7IntRect9intersectERKS0_(WebCore::IntRect* self, const WebCore::IntRect& other) {
    int l = std::max(self->m_location.m_x, other.m_location.m_x);
    int t = std::max(self->m_location.m_y, other.m_location.m_y);
    int r = std::min(self->m_location.m_x + self->m_size.m_width, other.m_location.m_x + other.m_size.m_width);
    int b = std::min(self->m_location.m_y + self->m_size.m_height, other.m_location.m_y + other.m_size.m_height);
    if (l >= r || t >= b) {
        self->m_location = {0, 0};
        self->m_size = {0, 0};
    } else {
        self->m_location = {l, t};
        self->m_size = {r - l, b - t};
    }
}

void APS5_VABI _ZN7WebCore7IntRectC1ERKNS_10LayoutRectE(WebCore::IntRect* self, const WebCore::LayoutRect& r) {
    self->m_location.m_x = r.m_location.m_x.m_value / 64;
    self->m_location.m_y = r.m_location.m_y.m_value / 64;
    self->m_size.m_width = r.m_size.m_width.m_value / 64;
    self->m_size.m_height = r.m_size.m_height.m_value / 64;
}

void APS5_VABI _ZN7WebCore7IntRectC1ERKNS_9FloatRectE(WebCore::IntRect* self, const WebCore::FloatRect& r) {
    self->m_location.m_x = WebCore::ClampToInteger(r.m_location.m_x);
    self->m_location.m_y = WebCore::ClampToInteger(r.m_location.m_y);
    self->m_size.m_width = WebCore::ClampToInteger(r.m_size.m_width);
    self->m_size.m_height = WebCore::ClampToInteger(r.m_size.m_height);
}

void APS5_VABI _ZN7WebCore7IntRectC2ERKNS_10LayoutRectE(WebCore::IntRect* self, const WebCore::LayoutRect& r) {
    self->m_location.m_x = r.m_location.m_x.m_value / 64;
    self->m_location.m_y = r.m_location.m_y.m_value / 64;
    self->m_size.m_width = r.m_size.m_width.m_value / 64;
    self->m_size.m_height = r.m_size.m_height.m_value / 64;
}

void APS5_VABI _ZN7WebCore7IntRectC2ERKNS_9FloatRectE(WebCore::IntRect* self, const WebCore::FloatRect& r) {
    self->m_location.m_x = WebCore::ClampToInteger(r.m_location.m_x);
    self->m_location.m_y = WebCore::ClampToInteger(r.m_location.m_y);
    self->m_size.m_width = WebCore::ClampToInteger(r.m_size.m_width);
    self->m_size.m_height = WebCore::ClampToInteger(r.m_size.m_height);
}

bool APS5_VABI _ZNK7WebCore7IntRect10intersectsERKS0_(const WebCore::IntRect* self, const WebCore::IntRect& other) {
    return self->m_size.m_width > 0 && self->m_size.m_height > 0
        && other.m_size.m_width > 0 && other.m_size.m_height > 0
        && self->m_location.m_x < other.m_location.m_x + other.m_size.m_width
        && other.m_location.m_x < self->m_location.m_x + self->m_size.m_width
        && self->m_location.m_y < other.m_location.m_y + other.m_size.m_height
        && other.m_location.m_y < self->m_location.m_y + self->m_size.m_height;
}

bool APS5_VABI _ZNK7WebCore7IntRect8containsERKS0_(const WebCore::IntRect* self, const WebCore::IntRect& other) {
    return self->m_location.m_x <= other.m_location.m_x
        && self->m_location.m_x + self->m_size.m_width >= other.m_location.m_x + other.m_size.m_width
        && self->m_location.m_y <= other.m_location.m_y
        && self->m_location.m_y + self->m_size.m_height >= other.m_location.m_y + other.m_size.m_height;
}

void APS5_VABI _ZN7WebCore10LayoutRect5scaleEf(WebCore::LayoutRect* self, float s) {
    constexpr int minVal = std::numeric_limits<int>::min() / 2;
    constexpr int maxVal = std::numeric_limits<int>::max() - 64 / 2;
    if (self->m_location.m_x.m_value == minVal && self->m_location.m_y.m_value == minVal
        && self->m_size.m_width.m_value == maxVal && self->m_size.m_height.m_value == maxVal) {
        return;
    }
    self->m_location.m_x.m_value = WebCore::ClampToInteger(self->m_location.m_x.m_value * s);
    self->m_location.m_y.m_value = WebCore::ClampToInteger(self->m_location.m_y.m_value * s);
    self->m_size.m_width.m_value = WebCore::ClampToInteger(self->m_size.m_width.m_value * s);
    self->m_size.m_height.m_value = WebCore::ClampToInteger(self->m_size.m_height.m_value * s);
}

void APS5_VABI _ZN7WebCore10LayoutRect5uniteERKS0_(WebCore::LayoutRect* self, const WebCore::LayoutRect& other) {
    if (other.m_size.m_width.m_value <= 0 || other.m_size.m_height.m_value <= 0) {
        return;
    }
    if (self->m_size.m_width.m_value <= 0 || self->m_size.m_height.m_value <= 0) {
        *self = other;
        return;
    }
    int minX = std::min(self->m_location.m_x.m_value, other.m_location.m_x.m_value);
    int minY = std::min(self->m_location.m_y.m_value, other.m_location.m_y.m_value);
    int maxX = std::max(self->m_location.m_x.m_value + self->m_size.m_width.m_value,
                        other.m_location.m_x.m_value + other.m_size.m_width.m_value);
    int maxY = std::max(self->m_location.m_y.m_value + self->m_size.m_height.m_value,
                        other.m_location.m_y.m_value + other.m_size.m_height.m_value);
    self->m_location.m_x.m_value = minX;
    self->m_location.m_y.m_value = minY;
    self->m_size.m_width.m_value = maxX - minX;
    self->m_size.m_height.m_value = maxY - minY;
}

void APS5_VABI _ZN7WebCore10LayoutRectC1ERKNS_9FloatRectE(WebCore::LayoutRect* self, const WebCore::FloatRect& r) {
    self->m_location.m_x.m_value = WebCore::ClampToInteger(r.m_location.m_x * 64.0f);
    self->m_location.m_y.m_value = WebCore::ClampToInteger(r.m_location.m_y * 64.0f);
    self->m_size.m_width.m_value = WebCore::ClampToInteger(r.m_size.m_width * 64.0f);
    self->m_size.m_height.m_value = WebCore::ClampToInteger(r.m_size.m_height * 64.0f);
}

void APS5_VABI _ZN7WebCore10LayoutRectC2ERKNS_9FloatRectE(WebCore::LayoutRect* self, const WebCore::FloatRect& r) {
    self->m_location.m_x.m_value = WebCore::ClampToInteger(r.m_location.m_x * 64.0f);
    self->m_location.m_y.m_value = WebCore::ClampToInteger(r.m_location.m_y * 64.0f);
    self->m_size.m_width.m_value = WebCore::ClampToInteger(r.m_size.m_width * 64.0f);
    self->m_size.m_height.m_value = WebCore::ClampToInteger(r.m_size.m_height * 64.0f);
}

bool APS5_VABI _ZNK7WebCore10LayoutRect8containsERKS0_(const WebCore::LayoutRect* self, const WebCore::LayoutRect& other) {
    return self->m_location.m_x.m_value <= other.m_location.m_x.m_value
        && self->m_location.m_x.m_value + self->m_size.m_width.m_value >= other.m_location.m_x.m_value + other.m_size.m_width.m_value
        && self->m_location.m_y.m_value <= other.m_location.m_y.m_value
        && self->m_location.m_y.m_value + self->m_size.m_height.m_value >= other.m_location.m_y.m_value + other.m_size.m_height.m_value;
}

void APS5_VABI _ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(
    WebCore::RoundedRect* self, const WebCore::LayoutRect& rect,
    const WebCore::LayoutSize& topLeft, const WebCore::LayoutSize& topRight,
    const WebCore::LayoutSize& bottomLeft, const WebCore::LayoutSize& bottomRight) {
    self->m_rect = rect;
    self->m_radii.m_topLeft = topLeft;
    self->m_radii.m_topRight = topRight;
    self->m_radii.m_bottomLeft = bottomLeft;
    self->m_radii.m_bottomRight = bottomRight;
}

void APS5_VABI _ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(
    WebCore::RoundedRect* self, const WebCore::LayoutRect& rect,
    const WebCore::LayoutSize& topLeft, const WebCore::LayoutSize& topRight,
    const WebCore::LayoutSize& bottomLeft, const WebCore::LayoutSize& bottomRight) {
    self->m_rect = rect;
    self->m_radii.m_topLeft = topLeft;
    self->m_radii.m_topRight = topRight;
    self->m_radii.m_bottomLeft = bottomLeft;
    self->m_radii.m_bottomRight = bottomRight;
}

bool APS5_VABI _ZNK7WebCore11RoundedRect8containsERKNS_10LayoutRectE(
    const WebCore::RoundedRect* self, const WebCore::LayoutRect& other) {
    bool rectContains = self->m_rect.m_location.m_x.m_value <= other.m_location.m_x.m_value
        && self->m_rect.m_location.m_x.m_value + self->m_rect.m_size.m_width.m_value >= other.m_location.m_x.m_value + other.m_size.m_width.m_value
        && self->m_rect.m_location.m_y.m_value <= other.m_location.m_y.m_value
        && self->m_rect.m_location.m_y.m_value + self->m_rect.m_size.m_height.m_value >= other.m_location.m_y.m_value + other.m_size.m_height.m_value;
    if (!rectContains || !WebCore::IsRoundedRectRenderable(*self)) {
        return false;
    }
    float rx = static_cast<float>(self->m_rect.m_location.m_x.m_value) / 64.0f;
    float ry = static_cast<float>(self->m_rect.m_location.m_y.m_value) / 64.0f;
    float rw = static_cast<float>(self->m_rect.m_size.m_width.m_value) / 64.0f;
    float rh = static_cast<float>(self->m_rect.m_size.m_height.m_value) / 64.0f;
    float ox = static_cast<float>(other.m_location.m_x.m_value) / 64.0f;
    float oy = static_cast<float>(other.m_location.m_y.m_value) / 64.0f;
    float ow = static_cast<float>(other.m_size.m_width.m_value) / 64.0f;
    float oh = static_cast<float>(other.m_size.m_height.m_value) / 64.0f;

    if (self->m_radii.m_topLeft.m_width.m_value > 0 && self->m_radii.m_topLeft.m_height.m_value > 0) {
        float radW = static_cast<float>(self->m_radii.m_topLeft.m_width.m_value) / 64.0f;
        float radH = static_cast<float>(self->m_radii.m_topLeft.m_height.m_value) / 64.0f;
        WebCore::FloatPoint center{rx + radW, ry + radH};
        if (ox <= center.m_x && oy <= center.m_y) {
            if (!WebCore::EllipseContainsPoint(center, radW, radH, WebCore::FloatPoint{ox, oy})) {
                return false;
            }
        }
    }
    if (self->m_radii.m_topRight.m_width.m_value > 0 && self->m_radii.m_topRight.m_height.m_value > 0) {
        float radW = static_cast<float>(self->m_radii.m_topRight.m_width.m_value) / 64.0f;
        float radH = static_cast<float>(self->m_radii.m_topRight.m_height.m_value) / 64.0f;
        WebCore::FloatPoint center{rx + rw - radW, ry + radH};
        if (ox + ow >= center.m_x && oy <= center.m_y) {
            if (!WebCore::EllipseContainsPoint(center, radW, radH, WebCore::FloatPoint{ox + ow, oy})) {
                return false;
            }
        }
    }
    if (self->m_radii.m_bottomLeft.m_width.m_value > 0 && self->m_radii.m_bottomLeft.m_height.m_value > 0) {
        float radW = static_cast<float>(self->m_radii.m_bottomLeft.m_width.m_value) / 64.0f;
        float radH = static_cast<float>(self->m_radii.m_bottomLeft.m_height.m_value) / 64.0f;
        WebCore::FloatPoint center{rx + radW, ry + rh - radH};
        if (ox <= center.m_x && oy + oh >= center.m_y) {
            if (!WebCore::EllipseContainsPoint(center, radW, radH, WebCore::FloatPoint{ox, oy + oh})) {
                return false;
            }
        }
    }
    if (self->m_radii.m_bottomRight.m_width.m_value > 0 && self->m_radii.m_bottomRight.m_height.m_value > 0) {
        float radW = static_cast<float>(self->m_radii.m_bottomRight.m_width.m_value) / 64.0f;
        float radH = static_cast<float>(self->m_radii.m_bottomRight.m_height.m_value) / 64.0f;
        WebCore::FloatPoint center{rx + rw - radW, ry + rh - radH};
        if (ox + ow >= center.m_x && oy + oh >= center.m_y) {
            if (!WebCore::EllipseContainsPoint(center, radW, radH, WebCore::FloatPoint{ox + ow, oy + oh})) {
                return false;
            }
        }
    }
    return true;
}

}
