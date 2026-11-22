#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

namespace {

const void* UrlOrNull(const std::string& url) {
    return url.empty() ? nullptr : NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(url));
}

}

extern "C" {

const void* APS5_VABI WKHitTestResultCopyAbsoluteImageURL(const void* hitTestResultRef) {
    const auto* hitTest = NKWebKit::Cast<NKWebKit::HitTestResult>(hitTestResultRef, __func__);
    return UrlOrNull(hitTest->absoluteImageURL);
}

const void* APS5_VABI WKHitTestResultCopyAbsoluteLinkURL(const void* hitTestResultRef) {
    const auto* hitTest = NKWebKit::Cast<NKWebKit::HitTestResult>(hitTestResultRef, __func__);
    return UrlOrNull(hitTest->absoluteLinkURL);
}

const void* APS5_VABI WKHitTestResultCopyAbsoluteMediaURL(const void* hitTestResultRef) {
    const auto* hitTest = NKWebKit::Cast<NKWebKit::HitTestResult>(hitTestResultRef, __func__);
    return UrlOrNull(hitTest->absoluteMediaURL);
}

const void* APS5_VABI WKHitTestResultCopyAbsolutePDFURL(const void* hitTestResultRef) {
    const auto* hitTest = NKWebKit::Cast<NKWebKit::HitTestResult>(hitTestResultRef, __func__);
    return UrlOrNull(hitTest->absolutePDFURL);
}

const void* APS5_VABI WKHitTestResultCopyLinkLabel(const void* hitTestResultRef) {
    const auto* hitTest = NKWebKit::Cast<NKWebKit::HitTestResult>(hitTestResultRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(hitTest->linkLabel));
}

const void* APS5_VABI WKHitTestResultCopyLinkTitle(const void* hitTestResultRef) {
    const auto* hitTest = NKWebKit::Cast<NKWebKit::HitTestResult>(hitTestResultRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(hitTest->linkTitle));
}

const void* APS5_VABI WKHitTestResultCopyLookupText(const void* hitTestResultRef) {
    const auto* hitTest = NKWebKit::Cast<NKWebKit::HitTestResult>(hitTestResultRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(hitTest->lookupText));
}

NKWebKit::RectValue APS5_VABI WKHitTestResultGetElementBoundingBox(const void* hitTestResultRef) {
    const auto* hitTest = NKWebKit::Cast<NKWebKit::HitTestResult>(hitTestResultRef, __func__);
    return hitTest->elementBoundingBox;
}

bool APS5_VABI WKHitTestResultIsContentEditable(const void* hitTestResultRef) {
    const auto* hitTest = NKWebKit::Cast<NKWebKit::HitTestResult>(hitTestResultRef, __func__);
    return hitTest->isContentEditable;
}

}
