#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/FrameHandle.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

const void* APS5_VABI WKPageLookUpFrameFromHandle(const void* pageRef, const void* handleRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* handle = NKWebKit::Cast<NKWebKit::FrameHandle>(handleRef, __func__);
    if (page->isClosed) {
        return nullptr;
    }
    if (page->mainFrame && page->mainFrame->frameID == handle->frameID) {
        return NKWebKit::ToRef(page->mainFrame);
    }
    for (const auto& frame : page->frames) {
        if (frame && frame->frameID == handle->frameID) {
            return NKWebKit::ToRef(frame);
        }
    }
    return nullptr;
}

void APS5_VABI WKPageGetSourceForFrame(const void* pageRef, const void* frameRef, void* context, void (APS5_VABI *callback)(const void*, const void*, void*)) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* frame = NKWebKit::Cast<NKWebKit::Frame>(frameRef, __func__);
    if (!callback) {
        return;
    }
    if (page->isClosed) {
        callback(nullptr, nullptr, context);
        return;
    }
    bool belongsToPage = (page->mainFrame.Get() == frame);
    if (!belongsToPage) {
        for (const auto& f : page->frames) {
            if (f.Get() == frame) {
                belongsToPage = true;
                break;
            }
        }
    }
    if (!belongsToPage) {
        callback(nullptr, nullptr, context);
        return;
    }
    const auto* str = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(frame->source.value_or(std::u16string())));
    callback(str, nullptr, context);
    WKRelease(str);
}

}
