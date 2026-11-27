#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/GradientExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <new>
#include <stdexcept>
#include <utility>

namespace WebCore {

namespace {

void Destroy(Gradient* self) {
    if (!self) {
        throw std::invalid_argument("WebCore::Gradient::~Gradient: null object");
    }
    self->~Gradient();
}

}  // namespace
}  // namespace WebCore

extern "C" {

WebCore::Gradient* APS5_VABI _ZN7WebCore8Gradient6createEON3WTF7VariantIJNS0_10LinearDataENS0_10RadialDataENS0_9ConicDataEEEE(WebCore::GradientData&& data) {
    void* memory = _ZN3WTF10fastMallocEm(sizeof(WebCore::Gradient));
    if (!memory) {
        throw std::bad_alloc();
    }
    return new (memory) WebCore::Gradient(std::move(data));
}

void APS5_VABI _ZN7WebCore8Gradient12addColorStopEONS0_9ColorStopE(WebCore::Gradient* self, WebCore::Gradient::ColorStop&& stop) {
    if (!self) {
        throw std::invalid_argument("WebCore::Gradient::addColorStop: null object");
    }
    self->addColorStop(std::move(stop));
}

void APS5_VABI _ZN7WebCore8Gradient15setSpreadMethodENS_20GradientSpreadMethodE(WebCore::Gradient* self, WebCore::GradientSpreadMethod method) {
    if (!self) {
        throw std::invalid_argument("WebCore::Gradient::setSpreadMethod: null object");
    }
    self->setSpreadMethod(method);
}

void APS5_VABI _ZN7WebCore8Gradient19setSortedColorStopsEON3WTF6VectorINS0_9ColorStopELm2ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(
    WebCore::Gradient* self, WTF::Vector<WebCore::Gradient::ColorStop, 2>&& stops) {
    if (!self) {
        throw std::invalid_argument("WebCore::Gradient::setSortedColorStops: null object");
    }
    self->setSortedColorStops(std::move(stops));
}

void APS5_VABI _ZN7WebCore8Gradient25setGradientSpaceTransformERKNS_15AffineTransformE(WebCore::Gradient* self, const WebCore::AffineTransform& transform) {
    if (!self) {
        throw std::invalid_argument("WebCore::Gradient::setGradientSpaceTransform: null object");
    }
    self->setGradientSpaceTransform(transform);
}

void APS5_VABI _ZN7WebCore8GradientD0Ev(WebCore::Gradient* self) {
    if (!self) {
        throw std::invalid_argument("WebCore::Gradient::~Gradient: null object");
    }
    WebCore::Destroy(self);
    _ZN3WTF8fastFreeEPv(self);
}

void APS5_VABI _ZN7WebCore8GradientD1Ev(WebCore::Gradient* self) {
    WebCore::Destroy(self);
}

void APS5_VABI _ZN7WebCore8GradientD2Ev(WebCore::Gradient* self) {
    WebCore::Destroy(self);
}

}
