#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

bool APS5_VABI WKPageGetIsControlledByAutomation(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__)->isControlledByAutomation;
}

void APS5_VABI WKPageSetControlledByAutomation(const void* pageRef, bool controlled) {
    NKWebKit::MutablePage(pageRef, __func__)->isControlledByAutomation = controlled;
}

bool APS5_VABI WKPageGetMediaCaptureEnabled(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__)->mediaCaptureEnabled;
}

void APS5_VABI WKPageSetMediaCaptureEnabled(const void* pageRef, bool enabled) {
    NKWebKit::MutablePage(pageRef, __func__)->mediaCaptureEnabled = enabled;
}

bool APS5_VABI WKPageIsContentEditable(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__)->isContentEditable;
}

bool APS5_VABI WKPageHasSelectedRange(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__)->selectionIsRange;
}

bool APS5_VABI WKPageCanDelete(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->selectionIsRange && page->isContentEditable;
}

bool APS5_VABI WKPageIsPlayingAudio(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__)->isPlayingAudio;
}

bool APS5_VABI WKPageSupportsTextEncoding(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->mainFrame && !page->mainFrame->isDisplayingStandaloneImageDocument;
}

const void* APS5_VABI WKPageGetSessionHistoryURLValueType() {
    static const void* const value = NKWebKit::ToRef(NKWebKit::String::FromUtf8("SessionHistoryURL"));
    return value;
}

const void* APS5_VABI WKPageGetSessionBackForwardListItemValueType() {
    static const void* const value = NKWebKit::ToRef(NKWebKit::String::FromUtf8("SessionBackForwardListItem"));
    return value;
}

}
