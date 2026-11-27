#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

void APS5_VABI WKWebsiteDataStoreDoesStatisticsDomainIDExistInDatabase(const void* dataStore, std::uint32_t domainID, void* context, void (APS5_VABI *callback)(bool, void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    bool exists = store->statisticsDomainIDs.find(domainID) != store->statisticsDomainIDs.end();
    if (callback) {
        callback(exists, context);
    }
}

void APS5_VABI WKWebsiteDataStoreDumpResourceLoadStatistics(const void* dataStore, void* context, void (APS5_VABI *callback)(const void*, void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    (void)store;
    if (callback) {
        auto dump = NKWebKit::Make<NKWebKit::String>(std::u16string());
        callback(NKWebKit::ToRef(dump), context);
    }
}

void APS5_VABI WKWebsiteDataStoreIsStatisticsEphemeral(const void* dataStore, void* context, void (APS5_VABI *callback)(bool, void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    bool ephemeral = store->isEphemeral;
    if (callback) {
        callback(ephemeral, context);
    }
}

void APS5_VABI WKWebsiteDataStoreIsStatisticsGrandfathered(const void* dataStore, const void* hostNameRef, void* context, void (APS5_VABI *callback)(bool, void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* hostName = NKWebKit::Cast<NKWebKit::String>(hostNameRef, __func__);
    bool grandfathered = store->grandfatheredHosts.find(hostName->Utf8()) != store->grandfatheredHosts.end();
    if (callback) {
        callback(grandfathered, context);
    }
}

void APS5_VABI WKWebsiteDataStoreIsStatisticsHasHadUserInteraction(const void* dataStore, const void* hostNameRef, void* context, void (APS5_VABI *callback)(bool, void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* hostName = NKWebKit::Cast<NKWebKit::String>(hostNameRef, __func__);
    bool hadInteraction = store->userInteractionHosts.find(hostName->Utf8()) != store->userInteractionHosts.end();
    if (callback) {
        callback(hadInteraction, context);
    }
}

void APS5_VABI WKWebsiteDataStoreIsStatisticsOnlyInDatabaseOnce(const void* dataStore, const void* subHostRef, const void* topHostRef, void* context, void (APS5_VABI *callback)(bool, void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* subHost = NKWebKit::Cast<NKWebKit::String>(subHostRef, __func__);
    auto* topHost = NKWebKit::Cast<NKWebKit::String>(topHostRef, __func__);
    bool onlyOnce = store->onlyInDatabaseOncePairs.find({subHost->Utf8(), topHost->Utf8()}) != store->onlyInDatabaseOncePairs.end();
    if (callback) {
        callback(onlyOnce, context);
    }
}

void APS5_VABI WKWebsiteDataStoreIsStatisticsPrevalentResource(const void* dataStore, const void* hostNameRef, void* context, void (APS5_VABI *callback)(bool, void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* hostName = NKWebKit::Cast<NKWebKit::String>(hostNameRef, __func__);
    bool prevalent = store->prevalentResourceHosts.find(hostName->Utf8()) != store->prevalentResourceHosts.end();
    if (callback) {
        callback(prevalent, context);
    }
}

void APS5_VABI WKWebsiteDataStoreIsStatisticsRegisteredAsRedirectingTo(const void* dataStore, const void* hostRedirectedFromRef, const void* hostRedirectedToRef, void* context, void (APS5_VABI *callback)(bool, void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* fromHost = NKWebKit::Cast<NKWebKit::String>(hostRedirectedFromRef, __func__);
    auto* toHost = NKWebKit::Cast<NKWebKit::String>(hostRedirectedToRef, __func__);
    bool redirecting = store->redirectingToPairs.find({fromHost->Utf8(), toHost->Utf8()}) != store->redirectingToPairs.end();
    if (callback) {
        callback(redirecting, context);
    }
}

void APS5_VABI WKWebsiteDataStoreIsStatisticsRegisteredAsSubFrameUnder(const void* dataStore, const void* subFrameHostRef, const void* topFrameHostRef, void* context, void (APS5_VABI *callback)(bool, void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* subHost = NKWebKit::Cast<NKWebKit::String>(subFrameHostRef, __func__);
    auto* topHost = NKWebKit::Cast<NKWebKit::String>(topFrameHostRef, __func__);
    bool subFrame = store->subFrameUnderPairs.find({subHost->Utf8(), topHost->Utf8()}) != store->subFrameUnderPairs.end();
    if (callback) {
        callback(subFrame, context);
    }
}

void APS5_VABI WKWebsiteDataStoreIsStatisticsRegisteredAsSubresourceUnder(const void* dataStore, const void* subresourceHostRef, const void* topFrameHostRef, void* context, void (APS5_VABI *callback)(bool, void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* subresourceHost = NKWebKit::Cast<NKWebKit::String>(subresourceHostRef, __func__);
    auto* topHost = NKWebKit::Cast<NKWebKit::String>(topFrameHostRef, __func__);
    bool subresource = store->subresourceUnderPairs.find({subresourceHost->Utf8(), topHost->Utf8()}) != store->subresourceUnderPairs.end();
    if (callback) {
        callback(subresource, context);
    }
}

void APS5_VABI WKWebsiteDataStoreIsStatisticsVeryPrevalentResource(const void* dataStore, const void* hostNameRef, void* context, void (APS5_VABI *callback)(bool, void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* hostName = NKWebKit::Cast<NKWebKit::String>(hostNameRef, __func__);
    bool veryPrevalent = store->veryPrevalentResourceHosts.find(hostName->Utf8()) != store->veryPrevalentResourceHosts.end();
    if (callback) {
        callback(veryPrevalent, context);
    }
}

}
