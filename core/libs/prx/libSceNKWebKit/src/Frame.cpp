#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/src/FrameHandle.hpp"
#include "prx/libSceNKWebKit/include/MimeExports.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

bool APS5_VABI WKFrameCanProvideSource(const void* frame) {
    const auto* f = NKWebKit::Cast<NKWebKit::Frame>(frame, __func__);
    return f->isDisplayingMarkupDocument;
}

bool APS5_VABI WKFrameCanShowMIMEType(const void* frame, const void* mimeType) {
    NKWebKit::Cast<NKWebKit::Frame>(frame, __func__);
    const auto* text = NKWebKit::Cast<NKWebKit::String>(mimeType, __func__);
    if (!text || text->null || text->characters.empty()) {
        return false;
    }
    NKWebKit::WtfAbi::String wtfMime;
    _ZN3WTF6StringC1EPKDsj(&wtfMime, text->characters.data(), static_cast<unsigned>(text->characters.size()));
    return _ZN7WebCore16MIMETypeRegistry15canShowMIMETypeERKN3WTF6StringE(wtfMime);
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

bool APS5_VABI WKFrameIsFrameSet(const void* frame) {
    const auto* f = NKWebKit::Cast<NKWebKit::Frame>(frame, __func__);
    return f->isFrameSet;
}

const void* APS5_VABI WKFrameCopyURL(const void* frame) {
    const auto* f = NKWebKit::Cast<NKWebKit::Frame>(frame, __func__);
    return f->url ? NKWebKit::ToRef(f->url) : nullptr;
}

const void* APS5_VABI WKFrameCopyTitle(const void* frame) {
    const auto* f = NKWebKit::Cast<NKWebKit::Frame>(frame, __func__);
    return NKWebKit::ToRef(f->title ? f->title : NKWebKit::String::FromUtf8(""));
}

const void* APS5_VABI WKFrameCopyMIMEType(const void* frame) {
    const auto* f = NKWebKit::Cast<NKWebKit::Frame>(frame, __func__);
    return NKWebKit::ToRef(f->mimeType ? f->mimeType : NKWebKit::String::FromUtf8(""));
}

const void* APS5_VABI WKFrameCopyProvisionalURL(const void* frame) {
    const auto* f = NKWebKit::Cast<NKWebKit::Frame>(frame, __func__);
    return f->provisionalURL ? NKWebKit::ToRef(f->provisionalURL) : nullptr;
}

const void* APS5_VABI WKFrameCopyUnreachableURL(const void* frame) {
    const auto* f = NKWebKit::Cast<NKWebKit::Frame>(frame, __func__);
    return f->unreachableURL ? NKWebKit::ToRef(f->unreachableURL) : nullptr;
}

const void* APS5_VABI WKFrameCreateFrameHandle(const void* frame) {
    const auto* f = NKWebKit::Cast<NKWebKit::Frame>(frame, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::FrameHandle>(f->frameID));
}

std::uint32_t APS5_VABI WKFrameGetFrameLoadState(const void* frame) {
    const auto* f = NKWebKit::Cast<NKWebKit::Frame>(frame, __func__);
    return f->loadState;
}
