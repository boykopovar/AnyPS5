#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

const void* APS5_VABI WKPageCopyCustomTextEncodingName(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->customTextEncodingName ? NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(*page->customTextEncodingName)) : nullptr;
}

void APS5_VABI WKPageSetCustomTextEncodingName(const void* pageRef, const void* stringRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (!stringRef) {
        page->customTextEncodingName = std::nullopt;
        return;
    }
    const auto* str = NKWebKit::Cast<NKWebKit::String>(stringRef, __func__);
    if (str->null) {
        page->customTextEncodingName = std::nullopt;
        return;
    }
    page->customTextEncodingName = str->characters;
}

}
