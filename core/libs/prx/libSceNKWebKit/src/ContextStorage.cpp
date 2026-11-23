#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

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
