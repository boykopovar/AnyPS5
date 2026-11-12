#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

bool APS5_VABI WKFrameCanProvideSource(const void* frame) {
    const auto* f = NKWebKit::Cast<NKWebKit::Frame>(frame, __func__);
    return f->isDisplayingMarkupDocument;
}

bool APS5_VABI WKFrameIsDisplayingMarkupDocument(const void* frame) {
    const auto* f = NKWebKit::Cast<NKWebKit::Frame>(frame, __func__);
    return f->isDisplayingMarkupDocument;
}

bool APS5_VABI WKFrameIsDisplayingStandaloneImageDocument(const void* frame) {
    const auto* f = NKWebKit::Cast<NKWebKit::Frame>(frame, __func__);
    return f->isDisplayingStandaloneImageDocument;
}

bool APS5_VABI WKFrameIsMainFrame(const void* frame) {
    const auto* f = NKWebKit::Cast<NKWebKit::Frame>(frame, __func__);
    return f->isMainFrame;
}
