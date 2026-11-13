#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

std::uint32_t APS5_VABI WKContextGetCacheModel(const void* contextRef) {
    const auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    return context->cacheModel;
}

void APS5_VABI WKContextSetCacheModel(const void* contextRef, std::uint32_t cacheModel) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->cacheModel = cacheModel;
}

bool APS5_VABI WKContextGetAccessibilityEnabled(const void* contextRef) {
    const auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    return context->accessibilityEnabled;
}

void APS5_VABI WKContextSetAccessibilityEnabled(const void* contextRef, bool enabled) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->accessibilityEnabled = enabled;
}

void APS5_VABI WKContextSetAlwaysUsesComplexTextCodePath(const void* contextRef, bool alwaysUseComplexText) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->alwaysUsesComplexTextCodePath = alwaysUseComplexText;
}

void APS5_VABI WKContextSetMemoryCacheDisabled(const void* contextRef, bool disabled) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->memoryCacheDisabled = disabled;
}

void APS5_VABI WKContextSetHTTPPipeliningEnabled(const void* contextRef, bool enabled) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->httpPipeliningEnabled = enabled;
}

void APS5_VABI WKContextSetShouldUseFontSmoothing(const void* contextRef, bool useFontSmoothing) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->shouldUseFontSmoothing = useFontSmoothing;
}

bool APS5_VABI WKContextJavaScriptConfigurationFileEnabled(const void* contextRef) {
    const auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    return context->javaScriptConfigurationFileEnabled;
}

void APS5_VABI WKContextSetJavaScriptConfigurationFileEnabled(const void* contextRef, bool enable) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->javaScriptConfigurationFileEnabled = enable;
}

void APS5_VABI WKContextSetJavaScriptGarbageCollectorTimerEnabled(const void* contextRef, bool enable) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->javaScriptGarbageCollectorTimerEnabled = enable;
}

void APS5_VABI WKContextGarbageCollectJavaScriptObjects(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    ++context->garbageCollectJavaScriptObjectsCount;
}

void APS5_VABI WKContextSetStorageAccessAPIEnabled(const void* contextRef, bool enable) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->storageAccessAPIEnabled = enable;
}

}
