#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {

const void* APS5_VABI WKViewCreate(const void* pageConfigurationRef) {
    auto view = NKWebKit::Make<NKWebKit::View>();
    if (pageConfigurationRef) {
        auto* config = NKWebKit::Cast<NKWebKit::PageConfiguration>(pageConfigurationRef, __func__);
        view->configuration = NKWebKit::Retained<NKWebKit::Object>::Share(config);
    }
    return NKWebKit::ToRef(view);
}

void APS5_VABI WKViewDidEnterFullScreen(const void* viewRef) {
    NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
}

void APS5_VABI WKViewDidExitFullScreen(const void* viewRef) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    view->isFullScreen = false;
}

void APS5_VABI WKViewGetPunchHoles(const void* viewRef, WKPunchHole* holes, int arrayLengthOfHoles, int* numberOfCopiedPunchHoles, int* numberOfActualPunchHoles) {
    const auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    if (!numberOfCopiedPunchHoles || !numberOfActualPunchHoles) {
        throw std::invalid_argument(std::string(__func__) + ": output pointers cannot be null");
    }
    if (arrayLengthOfHoles < 0) {
        throw std::invalid_argument(std::string(__func__) + ": invalid array length");
    }
    int actual = static_cast<int>(view->punchHoles.size());
    int copied = 0;
    std::vector<WKPunchHole> stagedHoles;
    if (arrayLengthOfHoles > 0) {
        if (!holes) {
            throw std::invalid_argument(std::string(__func__) + ": null holes destination");
        }
        copied = std::min(arrayLengthOfHoles, actual);
        stagedHoles.resize(copied);
        for (int i = 0; i < copied; ++i) {
            std::memcpy(&stagedHoles[i].quad, &view->punchHoles[i].quad, sizeof(stagedHoles[i].quad));
            stagedHoles[i].opacity = view->punchHoles[i].opacity;
            stagedHoles[i].canvasHandle = view->punchHoles[i].canvasHandle;
            stagedHoles[i].flags = view->punchHoles[i].flags;
        }
        if (static_cast<std::size_t>(copied) > std::numeric_limits<std::size_t>::max() / sizeof(WKPunchHole)) {
            throw std::overflow_error(std::string(__func__) + ": array byte size overflow");
        }
        const std::size_t bytes = static_cast<std::size_t>(copied) * sizeof(WKPunchHole);
        if (!GuestCopyOut_nid_no_patch(holes, stagedHoles.data(), bytes)) {
            throw std::invalid_argument(std::string(__func__) + ": unwritable holes destination");
        }
    }
    if (!GuestCopyOut_nid_no_patch(numberOfCopiedPunchHoles, &copied, sizeof(copied))) {
        throw std::invalid_argument(std::string(__func__) + ": unwritable numberOfCopiedPunchHoles destination");
    }
    if (!GuestCopyOut_nid_no_patch(numberOfActualPunchHoles, &actual, sizeof(actual))) {
        throw std::invalid_argument(std::string(__func__) + ": unwritable numberOfActualPunchHoles destination");
    }
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
    NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
}

void APS5_VABI WKViewSetActive(const void* viewRef, bool active) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    view->isActive = active;
}

void APS5_VABI WKViewSetDrawsBackground(const void* viewRef, bool drawsBackground) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    view->drawsBackground = drawsBackground;
}

void APS5_VABI WKViewSetFocus(const void* viewRef, bool focused) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    view->isFocused = focused;
}

void APS5_VABI WKViewSetPageScaleFactor(const void* viewRef, double scale) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    if (!std::isfinite(scale) || scale <= 0.0) throw std::invalid_argument(std::string(__func__) + ": invalid scale factor");
    view->pageScaleFactor = scale;
}

void APS5_VABI WKViewSetSize(const void* viewRef, NKWebKit::SizeValue size) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    if (!std::isfinite(size.width) || size.width < 0.0 || !std::isfinite(size.height) || size.height < 0.0) {
        throw std::invalid_argument(std::string(__func__) + ": invalid size");
    }
    view->size = size;
}

void APS5_VABI WKViewSetViewClient(const void* viewRef, const void* clientRef) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    view->viewClient = clientRef;
}

void APS5_VABI WKViewSetViewPopupMenuClient(const void* viewRef, const void* clientRef) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    view->popupMenuClient = clientRef;
}

void APS5_VABI WKViewSetViewScaleFactor(const void* viewRef, double scale) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    if (!std::isfinite(scale) || scale <= 0.0) throw std::invalid_argument(std::string(__func__) + ": invalid scale factor");
    view->viewScaleFactor = scale;
}

void APS5_VABI WKViewSetVisible(const void* viewRef, bool visible) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    view->isVisible = visible;
}

void APS5_VABI WKViewValueChangedForPopupMenu(const void* viewRef, int selectedIndex) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    view->lastPopupMenuSelectedIndex = selectedIndex;
}

void APS5_VABI WKViewWillEnterFullScreen(const void* viewRef) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    view->isFullScreen = true;
}

void APS5_VABI WKViewWillExitFullScreen(const void* viewRef) {
    NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
}

}
