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

}
