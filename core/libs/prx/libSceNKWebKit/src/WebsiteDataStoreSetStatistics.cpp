#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

void APS5_VABI WKWebsiteDataStoreSetStatisticsCacheMaxAgeCap(const void* dataStore, double seconds, void* context, void (APS5_VABI *callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->statisticsCacheMaxAgeCap = seconds;
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsCrossSiteLoadWithLinkDecoration(const void* dataStore, const void* fromHostRef, const void* toHostRef, bool wasFiltered, void* context, void (APS5_VABI *callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* fromHost = NKWebKit::Cast<NKWebKit::String>(fromHostRef, __func__);
    auto* toHost = NKWebKit::Cast<NKWebKit::String>(toHostRef, __func__);
    store->crossSiteLoadsWithLinkDecoration.push_back({fromHost->Utf8(), toHost->Utf8(), wasFiltered});
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsExpiredStatistic(const void* dataStore, const void* hostRef, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, void* context, void (APS5_VABI *callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* host = NKWebKit::Cast<NKWebKit::String>(hostRef, __func__);
    store->expiredStatistics[host->Utf8()] = {numberOfOperatingDaysPassed, hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent};
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsGrandfathered(const void* dataStore, const void* hostRef, bool value) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* host = NKWebKit::Cast<NKWebKit::String>(hostRef, __func__);
    if (value) {
        store->grandfatheredHosts.insert(host->Utf8());
    } else {
        store->grandfatheredHosts.erase(host->Utf8());
    }
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsGrandfatheringTime(const void* dataStore, double seconds) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->statisticsGrandfatheringTime = seconds;
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsHasHadUserInteraction(const void* dataStore, const void* hostRef, bool value, void* context, void (APS5_VABI *callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* host = NKWebKit::Cast<NKWebKit::String>(hostRef, __func__);
    if (value) {
        store->userInteractionHosts.insert(host->Utf8());
    } else {
        store->userInteractionHosts.erase(host->Utf8());
    }
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsIsRunningTest(const void* dataStore, bool value, void* context, void (APS5_VABI *callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->statisticsIsRunningTest = value;
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsLastSeen(const void* dataStore, const void* hostRef, double seconds, void* context, void (APS5_VABI *callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* host = NKWebKit::Cast<NKWebKit::String>(hostRef, __func__);
    store->statisticsLastSeen[host->Utf8()] = seconds;
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsMaxStatisticsEntries(const void* dataStore, unsigned entries) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    store->maxStatisticsEntries = entries;
}

void APS5_VABI WKWebsiteDataStoreSetStatisticsMergeStatistic(const void* dataStore, const void* hostRef, const void* topFrameDomain1Ref, const void* topFrameDomain2Ref, double lastSeen, bool hadUserInteraction, double mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, int dataRecordsRemoved, void* context, void (APS5_VABI *callback)(void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStore, __func__);
    auto* host = NKWebKit::Cast<NKWebKit::String>(hostRef, __func__);
    auto* domain1 = NKWebKit::Cast<NKWebKit::String>(topFrameDomain1Ref, __func__);
    auto* domain2 = NKWebKit::Cast<NKWebKit::String>(topFrameDomain2Ref, __func__);
    store->mergedStatistics[host->Utf8()] = {
        domain1->Utf8(),
        domain2->Utf8(),
        lastSeen,
        hadUserInteraction,
        mostRecentUserInteraction,
        isGrandfathered,
        isPrevalent,
        isVeryPrevalent,
        dataRecordsRemoved
    };
    if (callback) {
        callback(context);
    }
}

}
