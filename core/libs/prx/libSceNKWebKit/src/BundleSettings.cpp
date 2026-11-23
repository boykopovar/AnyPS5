#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

void APS5_VABI WKBundleSetAllowFileAccessFromFileURLs(const void* bundleRef, bool allows) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    bundle->allowFileAccessFromFileURLs = allows;
}

void APS5_VABI WKBundleSetAllowStorageAccessFromFileURLS(const void* bundleRef, bool allows) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    bundle->allowStorageAccessFromFileURLs = allows;
}

void APS5_VABI WKBundleSetAllowUniversalAccessFromFileURLs(const void* bundleRef, bool allows) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    bundle->allowUniversalAccessFromFileURLs = allows;
}

void APS5_VABI WKBundleSetAsynchronousSpellCheckingEnabled(const void* bundleRef, bool enabled) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    bundle->asynchronousSpellCheckingEnabled = enabled;
}

void APS5_VABI WKBundleSetAuthorAndUserStylesEnabled(const void* bundleRef, bool enabled) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    bundle->authorAndUserStylesEnabled = enabled;
}

void APS5_VABI WKBundleSetDatabaseQuota(const void* bundleRef, std::uint64_t quota) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    bundle->databaseQuota = quota;
}

void APS5_VABI WKBundleSetFrameFlatteningEnabled(const void* bundleRef, bool enabled) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    bundle->frameFlatteningEnabled = enabled;
}

void APS5_VABI WKBundleSetJavaScriptCanAccessClipboard(const void* bundleRef, bool enabled) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    bundle->javaScriptCanAccessClipboard = enabled;
}

void APS5_VABI WKBundleSetMinimumLogicalFontSize(const void* bundleRef, int size) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    bundle->minimumLogicalFontSize = size;
}

void APS5_VABI WKBundleSetPopupBlockingEnabled(const void* bundleRef, bool enabled) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    bundle->popupBlockingEnabled = enabled;
}

void APS5_VABI WKBundleSetSpatialNavigationEnabled(const void* bundleRef, bool enabled) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    bundle->spatialNavigationEnabled = enabled;
}

}
