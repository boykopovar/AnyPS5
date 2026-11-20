#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

extern "C" {

void APS5_VABI WKViewDidEnterFullScreen(const void* viewRef) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    view->isFullScreen = true;
}

void APS5_VABI WKViewDidExitFullScreen(const void* viewRef) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    view->isFullScreen = false;
}

double APS5_VABI WKViewGetPageScaleFactor(const void* viewRef) {
    const auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    return view->pageScaleFactor;
}

double APS5_VABI WKViewGetViewScaleFactor(const void* viewRef) {
    const auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    return view->viewScaleFactor;
}

bool APS5_VABI WKViewIsFullScreen(const void* viewRef) {
    const auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    return view->isFullScreen;
}

void APS5_VABI WKViewRequestExitFullScreen(const void* viewRef) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    view->isFullScreen = false;
}

void APS5_VABI WKViewSetDrawsBackground(const void* viewRef, bool drawsBackground) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    view->drawsBackground = drawsBackground;
}

void APS5_VABI WKViewSetPageScaleFactor(const void* viewRef, double scale) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    if (!std::isfinite(scale) || scale <= 0.0) throw std::invalid_argument(std::string(__func__) + ": invalid scale factor");
    view->pageScaleFactor = scale;
}

void APS5_VABI WKViewSetViewScaleFactor(const void* viewRef, double scale) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    if (!std::isfinite(scale) || scale <= 0.0) throw std::invalid_argument(std::string(__func__) + ": invalid scale factor");
    view->viewScaleFactor = scale;
}

void APS5_VABI WKViewWillEnterFullScreen(const void* viewRef) {
    NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
}

void APS5_VABI WKViewWillExitFullScreen(const void* viewRef) {
    NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
}

}
