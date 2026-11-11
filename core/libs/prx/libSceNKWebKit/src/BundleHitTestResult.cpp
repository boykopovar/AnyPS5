#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKBundleHitTestResultCopyAbsoluteImageURL(const void* hitTestResultRef) {
    const auto* result = NKWebKit::Cast<NKWebKit::BundleHitTestResult>(hitTestResultRef, __func__);
    return result->absoluteImageURL ? NKWebKit::ToRef(result->absoluteImageURL) : nullptr;
}

const void* APS5_VABI WKBundleHitTestResultCopyAbsoluteLinkURL(const void* hitTestResultRef) {
    const auto* result = NKWebKit::Cast<NKWebKit::BundleHitTestResult>(hitTestResultRef, __func__);
    return result->absoluteLinkURL ? NKWebKit::ToRef(result->absoluteLinkURL) : nullptr;
}

const void* APS5_VABI WKBundleHitTestResultCopyAbsoluteMediaURL(const void* hitTestResultRef) {
    const auto* result = NKWebKit::Cast<NKWebKit::BundleHitTestResult>(hitTestResultRef, __func__);
    return result->absoluteMediaURL ? NKWebKit::ToRef(result->absoluteMediaURL) : nullptr;
}

const void* APS5_VABI WKBundleHitTestResultCopyAbsolutePDFURL(const void* hitTestResultRef) {
    const auto* result = NKWebKit::Cast<NKWebKit::BundleHitTestResult>(hitTestResultRef, __func__);
    return result->absolutePDFURL ? NKWebKit::ToRef(result->absolutePDFURL) : nullptr;
}

const void* APS5_VABI WKBundleHitTestResultCopyLinkLabel(const void* hitTestResultRef) {
    const auto* result = NKWebKit::Cast<NKWebKit::BundleHitTestResult>(hitTestResultRef, __func__);
    return result->linkLabel ? NKWebKit::ToRef(result->linkLabel) : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

const void* APS5_VABI WKBundleHitTestResultCopyLinkTitle(const void* hitTestResultRef) {
    const auto* result = NKWebKit::Cast<NKWebKit::BundleHitTestResult>(hitTestResultRef, __func__);
    return result->linkTitle ? NKWebKit::ToRef(result->linkTitle) : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

const void* APS5_VABI WKBundleHitTestResultCopyLinkSuggestedFilename(const void* hitTestResultRef) {
    const auto* result = NKWebKit::Cast<NKWebKit::BundleHitTestResult>(hitTestResultRef, __func__);
    return result->linkSuggestedFilename ? NKWebKit::ToRef(result->linkSuggestedFilename) : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

bool APS5_VABI WKBundleHitTestResultGetIsSelected(const void* hitTestResultRef) {
    const auto* result = NKWebKit::Cast<NKWebKit::BundleHitTestResult>(hitTestResultRef, __func__);
    return result->isSelected;
}

bool APS5_VABI WKBundleHitTestResultIsDownloadableMedia(const void* hitTestResultRef) {
    const auto* result = NKWebKit::Cast<NKWebKit::BundleHitTestResult>(hitTestResultRef, __func__);
    return result->isDownloadableMedia;
}

bool APS5_VABI WKBundleHitTestResultMediaHasAudio(const void* hitTestResultRef) {
    const auto* result = NKWebKit::Cast<NKWebKit::BundleHitTestResult>(hitTestResultRef, __func__);
    return result->mediaHasAudio;
}

bool APS5_VABI WKBundleHitTestResultMediaIsInFullscreen(const void* hitTestResultRef) {
    const auto* result = NKWebKit::Cast<NKWebKit::BundleHitTestResult>(hitTestResultRef, __func__);
    return result->mediaIsInFullscreen;
}

std::uint32_t APS5_VABI WKBundleHitTestResultGetMediaType(const void* hitTestResultRef) {
    const auto* result = NKWebKit::Cast<NKWebKit::BundleHitTestResult>(hitTestResultRef, __func__);
    return result->mediaType;
}

const void* APS5_VABI WKBundleHitTestResultCopyNodeHandle(const void* hitTestResultRef) {
    const auto* result = NKWebKit::Cast<NKWebKit::BundleHitTestResult>(hitTestResultRef, __func__);
    return result->nodeHandle ? NKWebKit::ToRef(result->nodeHandle) : nullptr;
}

const void* APS5_VABI WKBundleHitTestResultCopyURLElementHandle(const void* hitTestResultRef) {
    const auto* result = NKWebKit::Cast<NKWebKit::BundleHitTestResult>(hitTestResultRef, __func__);
    return result->urlElementHandle ? NKWebKit::ToRef(result->urlElementHandle) : nullptr;
}

NKWebKit::RectValue APS5_VABI WKBundleHitTestResultGetImageRect(const void* hitTestResultRef) {
    const auto* result = NKWebKit::Cast<NKWebKit::BundleHitTestResult>(hitTestResultRef, __func__);
    return result->imageRect;
}


}
