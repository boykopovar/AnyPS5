#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

void APS5_VABI WKPageGetBytecodeProfile(const void* pageRef, void* context, WKResultErrorFunction callback) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed || !callback) {
        return;
    }
    auto result = NKWebKit::Make<NKWebKit::String>(std::u16string());
    callback(NKWebKit::ToRef(result), nullptr, context);
}

void APS5_VABI WKPageGetContentsAsMHTMLData(const void* pageRef, void* context, WKResultErrorFunction callback) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed || !callback) {
        return;
    }
    auto result = NKWebKit::Make<NKWebKit::Data>(std::vector<std::uint8_t>{});
    callback(NKWebKit::ToRef(result), nullptr, context);
}

void APS5_VABI WKPageGetContentsAsString(const void* pageRef, void* context, WKResultErrorFunction callback) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed || !callback) {
        return;
    }
    auto result = NKWebKit::Make<NKWebKit::String>(std::u16string());
    callback(NKWebKit::ToRef(result), nullptr, context);
}

void APS5_VABI WKPageGetSamplingProfilerOutput(const void* pageRef, void* context, WKResultErrorFunction callback) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed || !callback) {
        return;
    }
    auto result = NKWebKit::Make<NKWebKit::String>(std::u16string());
    callback(NKWebKit::ToRef(result), nullptr, context);
}

void APS5_VABI WKPageGetSelectionAsWebArchiveData(const void* pageRef, void* context, WKResultErrorFunction callback) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed || !callback) {
        return;
    }
    auto result = NKWebKit::Make<NKWebKit::Data>(std::vector<std::uint8_t>{});
    callback(NKWebKit::ToRef(result), nullptr, context);
}

void APS5_VABI WKPageRenderTreeExternalRepresentation(const void* pageRef, void* context, WKResultErrorFunction callback) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed || !callback) {
        return;
    }
    auto result = NKWebKit::Make<NKWebKit::String>(std::u16string());
    callback(NKWebKit::ToRef(result), nullptr, context);
}

}
