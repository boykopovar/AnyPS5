#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

const void* APS5_VABI WKPageCopyCustomUserAgent(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->customUserAgent ? NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(*page->customUserAgent)) : nullptr;
}

void APS5_VABI WKPageSetCustomUserAgent(const void* pageRef, const void* stringRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (!stringRef) {
        page->customUserAgent = std::nullopt;
        return;
    }
    const auto* str = NKWebKit::Cast<NKWebKit::String>(stringRef, __func__);
    if (str->null) {
        page->customUserAgent = std::nullopt;
        return;
    }
    page->customUserAgent = str->characters;
}

}
