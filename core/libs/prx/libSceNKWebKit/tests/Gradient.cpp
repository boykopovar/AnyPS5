#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace {

void Check(bool condition, const char* name, int line) {
    if (!condition) {
        std::fprintf(stderr, "Gradient check failed for %s at line %d\n", name, line);
        std::abort();
    }
}
#define Require(value, name) Check((value), (name), __LINE__)

template <typename F>
void RequireException(F&& func, const char* name) {
    bool threw = false;
    try {
        func();
    } catch (const std::exception&) {
        threw = true;
    }
    if (!threw) {
        std::fprintf(stderr, "Expected exception for %s\n", name);
        std::abort();
    }
}

void TestLifecycleLinear() {
    WebCore::GradientData data{};
    data.linear.point0 = {10.0f, 20.0f};
    data.linear.point1 = {110.0f, 120.0f};
    data.index = 0;

    auto* grad = _ZN7WebCore8Gradient6createEON3WTF7VariantIJNS0_10LinearDataENS0_10RadialDataENS0_9ConicDataEEEE(std::move(data));
    Require(grad != nullptr, "linear gradient created");
    Require(grad->m_data.linear.point0.m_x == 10.0f, "p0.x");
    Require(grad->m_data.linear.point0.m_y == 20.0f, "p0.y");
    Require(grad->m_data.linear.point1.m_x == 110.0f, "p1.x");
    Require(grad->m_data.linear.point1.m_y == 120.0f, "p1.y");
    Require(grad->m_stops.isEmpty(), "linear stops empty");
    Require(!grad->m_stopsSorted, "linear stops not sorted initially");
    Require(grad->m_spreadMethod == WebCore::GradientSpreadMethod::Pad, "default spread pad");
    _ZN7WebCore8GradientD1Ev(grad);
}

void TestLifecycleRadialAndConic() {
    WebCore::GradientData radial{};
    radial.radial.point0 = {5.0f, 6.0f};
    radial.radial.point1 = {50.0f, 60.0f};
    radial.radial.startRadius = 2.5f;
    radial.radial.endRadius = 25.0f;
    radial.radial.aspectRatio = 1.75f;
    radial.index = 1;

    auto* gradR = _ZN7WebCore8Gradient6createEON3WTF7VariantIJNS0_10LinearDataENS0_10RadialDataENS0_9ConicDataEEEE(std::move(radial));
    Require(gradR != nullptr, "radial gradient created");
    Require(gradR->m_data.radial.startRadius == 2.5f, "startRadius");
    Require(gradR->m_data.radial.endRadius == 25.0f, "endRadius");
    Require(gradR->m_data.radial.aspectRatio == 1.75f, "aspectRatio");
    _ZN7WebCore8GradientD2Ev(gradR);

    WebCore::GradientData conic{};
    conic.conic.point0 = {15.0f, 25.0f};
    conic.conic.angleRadians = 3.14159f;
    conic.index = 2;

    auto* gradC = _ZN7WebCore8Gradient6createEON3WTF7VariantIJNS0_10LinearDataENS0_10RadialDataENS0_9ConicDataEEEE(std::move(conic));
    Require(gradC != nullptr, "conic gradient created");
    Require(gradC->m_data.conic.angleRadians == 3.14159f, "angleRadians");
    _ZN7WebCore8GradientD1Ev(gradC);
}

void TestAddColorStop() {
    WebCore::GradientData data{};
    data.index = 0;
    auto* grad = _ZN7WebCore8Gradient6createEON3WTF7VariantIJNS0_10LinearDataENS0_10RadialDataENS0_9ConicDataEEEE(std::move(data));

    _ZN7WebCore8Gradient12addColorStopEONS0_9ColorStopE(grad, WebCore::Gradient::ColorStop{0.1f, WebCore::Color{0x11223344}});
    Require(grad->m_stops.size() == 1, "stops size 1");
    Require(grad->m_stopsSorted, "first stop is sorted");
    Require(grad->m_stops[0].offset == 0.1f, "stop 0 offset");
    Require(grad->m_stops[0].color.m_colorAndFlags == 0x11223344, "stop 0 color");

    _ZN7WebCore8Gradient12addColorStopEONS0_9ColorStopE(grad, WebCore::Gradient::ColorStop{0.5f, WebCore::Color{0x55667788}});
    Require(grad->m_stops.size() == 2, "stops size 2");
    Require(grad->m_stopsSorted, "ascending stop keeps sorted");

    _ZN7WebCore8Gradient12addColorStopEONS0_9ColorStopE(grad, WebCore::Gradient::ColorStop{0.2f, WebCore::Color{0x99AABBCC}});
    Require(grad->m_stops.size() == 3, "stops size 3");
    Require(!grad->m_stopsSorted, "descending stop marks stopsSorted false");

    _ZN7WebCore8GradientD1Ev(grad);
}

void TestSetSortedColorStops() {
    WebCore::GradientData data{};
    data.index = 0;
    auto* grad = _ZN7WebCore8Gradient6createEON3WTF7VariantIJNS0_10LinearDataENS0_10RadialDataENS0_9ConicDataEEEE(std::move(data));

    WTF::Vector<WebCore::Gradient::ColorStop, 2> stops;
    stops.append(WebCore::Gradient::ColorStop{0.0f, WebCore::Color{0x01}});
    stops.append(WebCore::Gradient::ColorStop{0.5f, WebCore::Color{0x02}});
    stops.append(WebCore::Gradient::ColorStop{1.0f, WebCore::Color{0x03}});

    _ZN7WebCore8Gradient19setSortedColorStopsEON3WTF6VectorINS0_9ColorStopELm2ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(grad, std::move(stops));
    Require(grad->m_stops.size() == 3, "replaced stops size");
    Require(grad->m_stopsSorted, "setSortedColorStops marks sorted");
    Require(grad->m_stops[0].offset == 0.0f, "stop 0 offset");
    Require(grad->m_stops[1].offset == 0.5f, "stop 1 offset");
    Require(grad->m_stops[2].offset == 1.0f, "stop 2 offset");

    _ZN7WebCore8GradientD1Ev(grad);
}

void TestSpreadMethod() {
    WebCore::GradientData data{};
    data.index = 0;
    auto* grad = _ZN7WebCore8Gradient6createEON3WTF7VariantIJNS0_10LinearDataENS0_10RadialDataENS0_9ConicDataEEEE(std::move(data));

    _ZN7WebCore8Gradient15setSpreadMethodENS_20GradientSpreadMethodE(grad, WebCore::GradientSpreadMethod::Reflect);
    Require(grad->m_spreadMethod == WebCore::GradientSpreadMethod::Reflect, "spread reflect");

    _ZN7WebCore8Gradient15setSpreadMethodENS_20GradientSpreadMethodE(grad, WebCore::GradientSpreadMethod::Repeat);
    Require(grad->m_spreadMethod == WebCore::GradientSpreadMethod::Repeat, "spread repeat");

    _ZN7WebCore8Gradient15setSpreadMethodENS_20GradientSpreadMethodE(grad, WebCore::GradientSpreadMethod::Pad);
    Require(grad->m_spreadMethod == WebCore::GradientSpreadMethod::Pad, "spread pad");

    _ZN7WebCore8GradientD1Ev(grad);
}

void TestGradientSpaceTransform() {
    WebCore::GradientData data{};
    data.index = 0;
    auto* grad = _ZN7WebCore8Gradient6createEON3WTF7VariantIJNS0_10LinearDataENS0_10RadialDataENS0_9ConicDataEEEE(std::move(data));

    WebCore::AffineTransform xform{{2.0, 0.0, 0.0, 3.0, 10.0, 20.0}};
    _ZN7WebCore8Gradient25setGradientSpaceTransformERKNS_15AffineTransformE(grad, xform);
    Require(grad->m_gradientSpaceTransformation.m_transform[0] == 2.0, "xform a");
    Require(grad->m_gradientSpaceTransformation.m_transform[3] == 3.0, "xform d");
    Require(grad->m_gradientSpaceTransformation.m_transform[4] == 10.0, "xform tx");
    Require(grad->m_gradientSpaceTransformation.m_transform[5] == 20.0, "xform ty");

    _ZN7WebCore8GradientD1Ev(grad);
}

void TestNullGuards() {
    RequireException([] {
        _ZN7WebCore8Gradient12addColorStopEONS0_9ColorStopE(nullptr, WebCore::Gradient::ColorStop{});
    }, "null addColorStop");

    RequireException([] {
        _ZN7WebCore8Gradient15setSpreadMethodENS_20GradientSpreadMethodE(nullptr, WebCore::GradientSpreadMethod::Pad);
    }, "null setSpreadMethod");

    RequireException([] {
        WTF::Vector<WebCore::Gradient::ColorStop, 2> s;
        _ZN7WebCore8Gradient19setSortedColorStopsEON3WTF6VectorINS0_9ColorStopELm2ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(nullptr, std::move(s));
    }, "null setSortedColorStops");

    RequireException([] {
        WebCore::AffineTransform t;
        _ZN7WebCore8Gradient25setGradientSpaceTransformERKNS_15AffineTransformE(nullptr, t);
    }, "null setGradientSpaceTransform");

    RequireException([] {
        _ZN7WebCore8GradientD1Ev(nullptr);
    }, "null D1");

    RequireException([] {
        _ZN7WebCore8GradientD2Ev(nullptr);
    }, "null D2");
}

}  // namespace

int main() {
    TestLifecycleLinear();
    TestLifecycleRadialAndConic();
    TestAddColorStop();
    TestSetSortedColorStops();
    TestSpreadMethod();
    TestGradientSpaceTransform();
    TestNullGuards();
    return 0;
}
