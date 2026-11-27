#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

void APS5_VABI WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(const void* dataStore, bool enable) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->resourceLoadStatisticsEnabled = enable;
}

void APS5_VABI WKWebsiteDataStoreSetResourceLoadStatisticsDebugMode(const void* dataStore, bool enable) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->resourceLoadStatisticsDebugMode = enable;
}

void APS5_VABI WKWebsiteDataStoreSetResourceLoadStatisticsDebugModeWithCompletionHandler(const void* dataStore, bool enable, void* context, void (*callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->resourceLoadStatisticsDebugMode = enable;
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreSetResourceLoadStatisticsPrevalentResourceForDebugMode(const void* dataStore, const void* hostNameRef, void* context, void (*callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* hostName = NKWebKit::Cast<NKWebKit::String>(hostNameRef, __func__);
    store->debugPrevalentResources.push_back(hostName->Utf8());
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreSetResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(const void* dataStore, bool value, std::uint32_t policy, void* context, void (*callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->blockThirdPartyCookies = value;
    store->thirdPartyCookieBlockingPolicy = policy;
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreSetResourceLoadStatisticsShouldDowngradeReferrerForTesting(const void* dataStore, bool value, void* context, void (*callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->downgradeReferrer = value;
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyHostCNAMEDomainForTesting(const void* dataStore, const void* firstPartyURLString, const void* cnameURLString, void* context, void (*callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* firstParty = NKWebKit::Cast<NKWebKit::String>(firstPartyURLString, __func__);
    auto* cname = NKWebKit::Cast<NKWebKit::String>(cnameURLString, __func__);
    store->firstPartyHostCNAMEDomains[firstParty->Utf8()] = cname->Utf8();
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreSetResourceLoadStatisticsThirdPartyCNAMEDomainForTesting(const void* dataStore, const void* cnameURLString, void* context, void (*callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* cname = NKWebKit::Cast<NKWebKit::String>(cnameURLString, __func__);
    store->thirdPartyCNAMEDomains.push_back(cname->Utf8());
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreSetResourceLoadStatisticsToSameSiteStrictCookiesForTesting(const void* dataStore, const void* hostNameRef, void* context, void (*callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* hostName = NKWebKit::Cast<NKWebKit::String>(hostNameRef, __func__);
    store->sameSiteStrictCookiesHosts.push_back(hostName->Utf8());
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTesting(const void* dataStore, bool value, void* context, void (*callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->firstPartyWebsiteDataRemovalMode = value;
    if (callback) {
        callback(context);
    }
}

}
