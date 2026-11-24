#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <atomic>

namespace {

std::atomic<std::uint32_t> globalCacheModel{2};

}

extern "C" {

std::uint32_t APS5_VABI WKContextGetCacheModel(const void*) {
    return globalCacheModel.load(std::memory_order_relaxed);
}

void APS5_VABI WKContextSetCacheModel(const void*, std::uint32_t cacheModel) {
    globalCacheModel.store(cacheModel <= 2 ? cacheModel : 0, std::memory_order_relaxed);
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

void APS5_VABI WKContextClearCachedCredentials(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->cachedCredentialsCleared = true;
}

void APS5_VABI WKContextClearLegacyPrivateBrowsingLocalStorage(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->legacyPrivateBrowsingLocalStorageCleared = true;
}

void APS5_VABI WKContextSyncLocalStorage(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->localStorageSynced = true;
}

}
