#include "prx/libSceNKWebKit/include/RegionExports.hpp"
#include "prx/libc/include/General.hpp"

#include <memory>
#include <new>
#include <utility>

extern "C" {

void APS5_VABI _ZN7WebCore6RegionC1EOS0_(WebCore::Region* self, WebCore::Region&& other) {
    self->m_bounds = std::move(other.m_bounds);
    new (&self->m_shape) std::unique_ptr<WebCore::Region::Shape>(std::move(other.m_shape));
}

void APS5_VABI _ZN7WebCore6RegionC1ERKNS_7IntRectE(WebCore::Region* self, const WebCore::IntRect& rect) {
    self->m_bounds = rect;
    new (&self->m_shape) std::unique_ptr<WebCore::Region::Shape>(nullptr);
}

void APS5_VABI _ZN7WebCore6RegionC1ERKS0_(WebCore::Region* self, const WebCore::Region& other) {
    self->m_bounds = other.m_bounds;
    new (&self->m_shape) std::unique_ptr<WebCore::Region::Shape>(other.m_shape ? std::make_unique<WebCore::Region::Shape>(*other.m_shape) : nullptr);
}

void APS5_VABI _ZN7WebCore6RegionC1Ev(WebCore::Region* self) {
    self->m_bounds = WebCore::IntRect{WebCore::IntPoint{0, 0}, WebCore::IntSize{0, 0}};
    new (&self->m_shape) std::unique_ptr<WebCore::Region::Shape>(nullptr);
}

void APS5_VABI _ZN7WebCore6RegionC2EOS0_(WebCore::Region* self, WebCore::Region&& other) {
    _ZN7WebCore6RegionC1EOS0_(self, std::move(other));
}

void APS5_VABI _ZN7WebCore6RegionC2ERKNS_7IntRectE(WebCore::Region* self, const WebCore::IntRect& rect) {
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(self, rect);
}

void APS5_VABI _ZN7WebCore6RegionC2ERKS0_(WebCore::Region* self, const WebCore::Region& other) {
    _ZN7WebCore6RegionC1ERKS0_(self, other);
}

void APS5_VABI _ZN7WebCore6RegionC2Ev(WebCore::Region* self) {
    _ZN7WebCore6RegionC1Ev(self);
}

void APS5_VABI _ZN7WebCore6RegionD1Ev(WebCore::Region* self) {
    self->m_shape.~unique_ptr();
}

void APS5_VABI _ZN7WebCore6RegionD2Ev(WebCore::Region* self) {
    self->m_shape.~unique_ptr();
}

WebCore::Region& APS5_VABI _ZN7WebCore6RegionaSEOS0_(WebCore::Region* self, WebCore::Region&& other) {
    self->m_bounds = std::move(other.m_bounds);
    self->m_shape = std::move(other.m_shape);
    return *self;
}

WebCore::Region& APS5_VABI _ZN7WebCore6RegionaSERKS0_(WebCore::Region* self, const WebCore::Region& other) {
    self->m_bounds = other.m_bounds;
    self->m_shape = other.m_shape ? std::make_unique<WebCore::Region::Shape>(*other.m_shape) : nullptr;
    return *self;
}

}
