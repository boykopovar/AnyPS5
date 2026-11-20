#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

namespace {

std::optional<std::u16string> NullableString(const void* stringRef, const char* function) {
    const auto* text = NKWebKit::CastOrNull<NKWebKit::String>(stringRef, function);
    if (!text || text->null) return std::nullopt;
    return text->characters;
}

}

extern "C" {

bool APS5_VABI WKPageIsClosed(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__)->isClosed;
}

const void* APS5_VABI WKPageCopyTitle(const void* pageRef) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__)->title);
}

const void* APS5_VABI WKPageCopyActiveURL(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->activeURL ? NKWebKit::ToRef(page->activeURL) : nullptr;
}

const void* APS5_VABI WKPageCopyProvisionalURL(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->provisionalURL ? NKWebKit::ToRef(page->provisionalURL) : nullptr;
}

const void* APS5_VABI WKPageCopyCommittedURL(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->committedURL ? NKWebKit::ToRef(page->committedURL) : nullptr;
}

double APS5_VABI WKPageGetEstimatedProgress(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__)->estimatedProgress;
}

const void* APS5_VABI WKPageCopyApplicationNameForUserAgent(const void* pageRef) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__)->applicationNameForUserAgent);
}

void APS5_VABI WKPageSetApplicationNameForUserAgent(const void* pageRef, const void* applicationNameRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->applicationNameForUserAgent = NullableString(applicationNameRef, __func__);
}

const void* APS5_VABI WKPageCopyCustomUserAgent(const void* pageRef) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__)->customUserAgent);
}

void APS5_VABI WKPageSetCustomUserAgent(const void* pageRef, const void* userAgentRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->customUserAgent = NullableString(userAgentRef, __func__);
}

const void* APS5_VABI WKPageCopyCustomTextEncodingName(const void* pageRef) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__)->customTextEncodingName);
}

void APS5_VABI WKPageSetCustomTextEncodingName(const void* pageRef, const void* encodingNameRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->customTextEncodingName = NullableString(encodingNameRef, __func__);
}

void APS5_VABI WKPageSetBackgroundExtendsBeyondPage(const void* pageRef, bool backgroundExtendsBeyondPage) {
    NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__)->backgroundExtendsBeyondPage = backgroundExtendsBeyondPage;
}

bool APS5_VABI WKPageBackgroundExtendsBeyondPage(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__)->backgroundExtendsBeyondPage;
}

}
