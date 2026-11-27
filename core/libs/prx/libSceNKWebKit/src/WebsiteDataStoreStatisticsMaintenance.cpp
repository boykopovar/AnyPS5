#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

void APS5_VABI WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStore(const void* dataStore, void* context, void (APS5_VABI *callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->clearInMemoryAndPersistentStoreCalled = true;
    store->clearModifiedSinceHours = std::nullopt;
    store->deletedCookiesHosts.clear();
    store->isolatedSessionHosts.clear();
    store->localStorageHosts.clear();
    store->debugPrevalentResources.clear();
    store->firstPartyHostCNAMEDomains.clear();
    store->thirdPartyCNAMEDomains.clear();
    store->sameSiteStrictCookiesHosts.clear();
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreModifiedSinceHours(const void* dataStore, unsigned hours, void* context, void (APS5_VABI *callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->clearInMemoryAndPersistentStoreCalled = true;
    store->clearModifiedSinceHours = hours;
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreStatisticsClearThroughWebsiteDataRemoval(const void* dataStore, void* context, void (APS5_VABI *callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->clearThroughWebsiteDataRemovalCalled = true;
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreStatisticsDeleteCookiesForTesting(const void* dataStore, const void* hostRef, bool includeHttpOnlyCookies, void* context, void (APS5_VABI *callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* host = NKWebKit::Cast<NKWebKit::String>(hostRef, __func__);
    store->deletedCookiesHosts[host->Utf8()] = includeHttpOnlyCookies;
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreStatisticsHasIsolatedSession(const void* dataStore, const void* hostRef, void* context, void (APS5_VABI *callback)(bool, void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* host = NKWebKit::Cast<NKWebKit::String>(hostRef, __func__);
    bool hasSession = store->isolatedSessionHosts.find(host->Utf8()) != store->isolatedSessionHosts.end();
    if (callback) {
        callback(hasSession, context);
    }
}

void APS5_VABI WKWebsiteDataStoreStatisticsHasLocalStorage(const void* dataStore, const void* hostRef, void* context, void (APS5_VABI *callback)(bool, void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* host = NKWebKit::Cast<NKWebKit::String>(hostRef, __func__);
    bool hasStorage = store->localStorageHosts.find(host->Utf8()) != store->localStorageHosts.end();
    if (callback) {
        callback(hasStorage, context);
    }
}

void APS5_VABI WKWebsiteDataStoreStatisticsProcessStatisticsAndDataRecords(const void* dataStore, void* context, void (APS5_VABI *callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->processStatisticsAndDataRecordsCalled = true;
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreStatisticsResetToConsistentState(const void* dataStore, void* context, void (APS5_VABI *callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->resourceLoadStatisticsEnabled = false;
    store->resourceLoadStatisticsDebugMode = false;
    store->debugPrevalentResources.clear();
    store->blockThirdPartyCookies = false;
    store->thirdPartyCookieBlockingPolicy = 0;
    store->downgradeReferrer = true;
    store->firstPartyHostCNAMEDomains.clear();
    store->thirdPartyCNAMEDomains.clear();
    store->sameSiteStrictCookiesHosts.clear();
    store->firstPartyWebsiteDataRemovalMode = false;
    store->clearInMemoryAndPersistentStoreCalled = false;
    store->clearModifiedSinceHours = std::nullopt;
    store->clearThroughWebsiteDataRemovalCalled = false;
    store->deletedCookiesHosts.clear();
    store->isolatedSessionHosts.clear();
    store->localStorageHosts.clear();
    store->processStatisticsAndDataRecordsCalled = false;
    store->resetToConsistentStateCalled = true;
    store->updateCookieBlockingCalled = false;
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreStatisticsUpdateCookieBlocking(const void* dataStore, void* context, void (APS5_VABI *callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->updateCookieBlockingCalled = true;
    if (callback) {
        callback(context);
    }
}

}
