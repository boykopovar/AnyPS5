#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

const void* APS5_VABI WKPageCopyApplicationNameForUserAgent(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->applicationNameForUserAgent ? NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(*page->applicationNameForUserAgent)) : nullptr;
}

void APS5_VABI WKPageSetApplicationNameForUserAgent(const void* pageRef, const void* stringRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (!stringRef) {
        page->applicationNameForUserAgent = std::nullopt;
        return;
    }
    const auto* str = NKWebKit::Cast<NKWebKit::String>(stringRef, __func__);
    if (str->null) {
        page->applicationNameForUserAgent = std::nullopt;
        return;
    }
    page->applicationNameForUserAgent = str->characters;
}

}
