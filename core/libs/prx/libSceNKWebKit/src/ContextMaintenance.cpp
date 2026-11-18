#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKContextTerminateNetworkProcess(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    ++context->terminateNetworkProcessCount;
}

void APS5_VABI WKContextSyncLocalStorage(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    ++context->syncLocalStorageCount;
}

void APS5_VABI WKContextClearCachedCredentials(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    ++context->clearCachedCredentialsCount;
}

void APS5_VABI WKContextClearLegacyPrivateBrowsingLocalStorage(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    ++context->clearLegacyPrivateBrowsingLocalStorageCount;
}

void APS5_VABI WKContextUseTestingNetworkSession(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->usesTestingNetworkSession = true;
}

void APS5_VABI WKContextClearCurrentModifierStateForTesting(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    ++context->clearCurrentModifierStateCount;
}

}
