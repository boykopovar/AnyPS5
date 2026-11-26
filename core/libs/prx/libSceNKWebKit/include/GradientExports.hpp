#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/include/RegionExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace WebCore {

enum class GradientSpreadMethod : std::uint8_t {
    Pad = 0,
    Reflect = 1,
    Repeat = 2
};

struct LinearData {
    FloatPoint point0;
    FloatPoint point1;
};

struct RadialData {
    FloatPoint point0;
    FloatPoint point1;
    float startRadius;
    float endRadius;
    float aspectRatio;
};

struct ConicData {
    FloatPoint point0;
    float angleRadians;
};

struct GradientData {
    union {
        LinearData linear;
        RadialData radial;
        ConicData conic;
    };
    std::size_t index;
};

class Gradient {
public:
    struct ColorStop {
        float offset{0.0f};
        Color color{};

        bool operator==(const ColorStop& other) const {
            return offset == other.offset && color.m_colorAndFlags == other.color.m_colorAndFlags;
        }
    };

    unsigned m_refCount{1};
    GradientData m_data{};
    WTF::Vector<ColorStop, 2> m_stops{};
    GradientSpreadMethod m_spreadMethod{GradientSpreadMethod::Pad};
    AffineTransform m_gradientSpaceTransformation{};
    bool m_stopsSorted{false};

    Gradient() = default;
    explicit Gradient(GradientData&& data)
        : m_refCount(1)
        , m_data(std::move(data))
        , m_stops()
        , m_spreadMethod(GradientSpreadMethod::Pad)
        , m_gradientSpaceTransformation()
        , m_stopsSorted(false) {}

    ~Gradient() = default;

    void addColorStop(ColorStop&& stop) {
        if (m_stops.isEmpty()) {
            m_stopsSorted = true;
        } else {
            m_stopsSorted = m_stopsSorted && (m_stops.last().offset <= stop.offset);
        }
        m_stops.append(std::move(stop));
    }

    void setSpreadMethod(GradientSpreadMethod method) {
        m_spreadMethod = method;
    }

    void setSortedColorStops(WTF::Vector<ColorStop, 2>&& stops) {
        m_stops = std::move(stops);
        m_stopsSorted = true;
    }

    void setGradientSpaceTransform(const AffineTransform& transform) {
        m_gradientSpaceTransformation = transform;
    }
};

}  // namespace WebCore

extern "C" {

WebCore::Gradient* APS5_VABI _ZN7WebCore8Gradient6createEON3WTF7VariantIJNS0_10LinearDataENS0_10RadialDataENS0_9ConicDataEEEE(WebCore::GradientData&& data);
void APS5_VABI _ZN7WebCore8Gradient12addColorStopEONS0_9ColorStopE(WebCore::Gradient* self, WebCore::Gradient::ColorStop&& stop);
void APS5_VABI _ZN7WebCore8Gradient15setSpreadMethodENS_20GradientSpreadMethodE(WebCore::Gradient* self, WebCore::GradientSpreadMethod method);
void APS5_VABI _ZN7WebCore8Gradient19setSortedColorStopsEON3WTF6VectorINS0_9ColorStopELm2ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(
    WebCore::Gradient* self, WTF::Vector<WebCore::Gradient::ColorStop, 2>&& stops);
void APS5_VABI _ZN7WebCore8Gradient25setGradientSpaceTransformERKNS_15AffineTransformE(WebCore::Gradient* self, const WebCore::AffineTransform& transform);
void APS5_VABI _ZN7WebCore8GradientD1Ev(WebCore::Gradient* self);
void APS5_VABI _ZN7WebCore8GradientD2Ev(WebCore::Gradient* self);

}
