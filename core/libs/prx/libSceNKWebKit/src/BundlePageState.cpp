#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

double APS5_VABI WKBundlePageGetBackingScaleFactor(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__)->backingScaleFactor;
}

bool APS5_VABI WKBundlePageIsControlledByAutomation(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__)->isControlledByAutomation;
}

bool APS5_VABI WKBundlePageIsSuspended(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__)->isSuspended;
}

bool APS5_VABI WKBundlePageIsUsingEphemeralSession(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__)->usesEphemeralSession;
}

std::uint64_t APS5_VABI WKBundlePageGetRenderTreeSize(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__)->renderTreeSize;
}

bool APS5_VABI WKBundlePageHasComposition(const void* pageRef) {
    return NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__)->hasComposition;
}

}
