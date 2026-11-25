#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

void APS5_VABI WKWebsiteDataStoreResetQuota(const void* dataStoreRef) {
    auto* dataStore = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStoreRef, __func__);
    dataStore->quotaReset = true;
}

void APS5_VABI WKWebsiteDataStoreClearDiskCache(const void* dataStoreRef) {
    auto* dataStore = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStoreRef, __func__);
    dataStore->diskCacheCleared = true;
}

void APS5_VABI WKWebsiteDataStoreSetUseITPDatabase(const void* dataStoreRef, bool value) {
    auto* dataStore = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStoreRef, __func__);
    dataStore->useITPDatabase = value;
}

void APS5_VABI WKWebsiteDataStoreSetCacheModelSynchronouslyForTesting(const void* dataStoreRef, std::uint32_t cacheModel) {
    auto* dataStore = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStoreRef, __func__);
    dataStore->cacheModel = cacheModel;
}

void APS5_VABI WKWebsiteDataStoreClearBundleIdentifierInNetworkProcess(const void* dataStoreRef) {
    auto* dataStore = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStoreRef, __func__);
    dataStore->bundleIdentifierInNetworkProcessCleared = true;
    dataStore->bundleIdentifierInNetworkProcess = nullptr;
}

void APS5_VABI WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(const void* dataStoreRef, const void* bundleIdentifierRef) {
    auto* dataStore = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStoreRef, __func__);
    if (!bundleIdentifierRef) {
        dataStore->bundleIdentifierInNetworkProcess = nullptr;
        return;
    }
    dataStore->bundleIdentifierInNetworkProcess = NKWebKit::Retained<NKWebKit::String>::Share(
        NKWebKit::Cast<NKWebKit::String>(bundleIdentifierRef, __func__));
}
