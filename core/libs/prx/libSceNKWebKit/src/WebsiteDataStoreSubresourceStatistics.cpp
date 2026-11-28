#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

void APS5_VABI WKWebsiteDataStoreSetStatisticsMinimumTimeBetweenDataRecordsRemoval(const void* dataStore, double seconds) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->minimumTimeBetweenDataRecordsRemoval = seconds;
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsNotifyPagesWhenDataRecordsWereScanned(const void* dataStore, bool value) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->notifyPagesWhenDataRecordsWereScanned = value;
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsPrevalentResource(const void* dataStore, const void* hostRef, bool value, void* context, void (APS5_VABI *callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* host = NKWebKit::Cast<NKWebKit::String>(hostRef, __func__);
    if (value) {
        store->prevalentResourceHosts.insert(host->Utf8());
    } else {
        store->prevalentResourceHosts.erase(host->Utf8());
    }
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsPruneEntriesDownTo(const void* dataStore, unsigned entries) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->pruneEntriesDownTo = entries;
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(const void* dataStore, bool value) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->shouldClassifyResourcesBeforeDataRecordsRemoval = value;
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsSubframeUnderTopFrameOrigin(const void* dataStore, const void* hostRef, const void* topFrameHostRef) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* host = NKWebKit::Cast<NKWebKit::String>(hostRef, __func__);
    auto* topHost = NKWebKit::Cast<NKWebKit::String>(topFrameHostRef, __func__);
    store->subframeUnderTopFrameOriginPairs.insert({host->Utf8(), topHost->Utf8()});
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsSubresourceUnderTopFrameOrigin(const void* dataStore, const void* hostRef, const void* topFrameHostRef) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* host = NKWebKit::Cast<NKWebKit::String>(hostRef, __func__);
    auto* topHost = NKWebKit::Cast<NKWebKit::String>(topFrameHostRef, __func__);
    store->subresourceUnderTopFrameOriginPairs.insert({host->Utf8(), topHost->Utf8()});
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectFrom(const void* dataStore, const void* hostRef, const void* hostRedirectedFromRef) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* host = NKWebKit::Cast<NKWebKit::String>(hostRef, __func__);
    auto* fromHost = NKWebKit::Cast<NKWebKit::String>(hostRedirectedFromRef, __func__);
    store->subresourceUniqueRedirectFromPairs.insert({host->Utf8(), fromHost->Utf8()});
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectTo(const void* dataStore, const void* hostRef, const void* hostRedirectedToRef) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* host = NKWebKit::Cast<NKWebKit::String>(hostRef, __func__);
    auto* toHost = NKWebKit::Cast<NKWebKit::String>(hostRedirectedToRef, __func__);
    store->subresourceUniqueRedirectToPairs.insert({host->Utf8(), toHost->Utf8()});
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsTimeToLiveUserInteraction(const void* dataStore, double seconds, void* context, void (APS5_VABI *callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->timeToLiveUserInteraction = seconds;
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectFrom(const void* dataStore, const void* hostRef, const void* hostRedirectedFromRef) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* host = NKWebKit::Cast<NKWebKit::String>(hostRef, __func__);
    auto* fromHost = NKWebKit::Cast<NKWebKit::String>(hostRedirectedFromRef, __func__);
    store->topFrameUniqueRedirectFromPairs.insert({host->Utf8(), fromHost->Utf8()});
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectTo(const void* dataStore, const void* hostRef, const void* hostRedirectedToRef) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* host = NKWebKit::Cast<NKWebKit::String>(hostRef, __func__);
    auto* toHost = NKWebKit::Cast<NKWebKit::String>(hostRedirectedToRef, __func__);
    store->topFrameUniqueRedirectToPairs.insert({host->Utf8(), toHost->Utf8()});
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsVeryPrevalentResource(const void* dataStore, const void* hostRef, bool value, void* context, void (APS5_VABI *callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* host = NKWebKit::Cast<NKWebKit::String>(hostRef, __func__);
    if (value) {
        store->veryPrevalentResourceHosts.insert(host->Utf8());
    } else {
        store->veryPrevalentResourceHosts.erase(host->Utf8());
    }
    if (callback) {
        callback(context);
    }
}

}
