#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename Fn>
static void RequireThrows(Fn&& fn) {
    bool threw = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

struct TestContext {
    int callCount = 0;
};

static void APS5_VABI TestCallback(void* context) {
    auto* ctx = static_cast<TestContext*>(context);
    ctx->callCount++;
}

int main() {
    auto store = NKWebKit::Make<NKWebKit::WebsiteDataStore>();
    const void* storeRef = store.Get();
    Require(storeRef != nullptr);
    auto* s = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(storeRef, __func__);

    TestContext ctx;

    WKWebsiteDataStoreSetStatisticsCacheMaxAgeCap(storeRef, 86400.0, &ctx, TestCallback);
    Require(ctx.callCount == 1);
    Require(s->statisticsCacheMaxAgeCap == 86400.0);
    WKWebsiteDataStoreSetStatisticsCacheMaxAgeCap(storeRef, 3600.0, nullptr, nullptr);
    Require(s->statisticsCacheMaxAgeCap == 3600.0);

    const void* hostA = WKStringCreateWithUTF8CString("hosta.com");
    const void* hostB = WKStringCreateWithUTF8CString("hostb.com");
    const void* hostC = WKStringCreateWithUTF8CString("hostc.com");

    WKWebsiteDataStoreSetStatisticsCrossSiteLoadWithLinkDecoration(storeRef, hostA, hostB, true, &ctx, TestCallback);
    Require(ctx.callCount == 2);
    Require(s->crossSiteLoadsWithLinkDecoration.size() == 1);
    Require(s->crossSiteLoadsWithLinkDecoration[0].fromHost == "hosta.com");
    Require(s->crossSiteLoadsWithLinkDecoration[0].toHost == "hostb.com");
    Require(s->crossSiteLoadsWithLinkDecoration[0].wasFiltered);

    WKWebsiteDataStoreSetStatisticsExpiredStatistic(storeRef, hostA, 7, true, false, true, &ctx, TestCallback);
    Require(ctx.callCount == 3);
    Require(s->expiredStatistics.count("hosta.com") == 1);
    Require(s->expiredStatistics["hosta.com"].numberOfOperatingDaysPassed == 7);
    Require(s->expiredStatistics["hosta.com"].hadUserInteraction);
    Require(!s->expiredStatistics["hosta.com"].isScheduledForAllButCookieDataRemoval);
    Require(s->expiredStatistics["hosta.com"].isPrevalent);

    WKWebsiteDataStoreSetStatisticsGrandfathered(storeRef, hostA, true);
    Require(s->grandfatheredHosts.count("hosta.com") == 1);
    WKWebsiteDataStoreSetStatisticsGrandfathered(storeRef, hostA, false);
    Require(s->grandfatheredHosts.count("hosta.com") == 0);

    WKWebsiteDataStoreSetStatisticsGrandfatheringTime(storeRef, 12345.0);
    Require(s->statisticsGrandfatheringTime == 12345.0);

    WKWebsiteDataStoreSetStatisticsHasHadUserInteraction(storeRef, hostA, true, &ctx, TestCallback);
    Require(ctx.callCount == 4);
    Require(s->userInteractionHosts.count("hosta.com") == 1);
    WKWebsiteDataStoreSetStatisticsHasHadUserInteraction(storeRef, hostA, false, &ctx, TestCallback);
    Require(ctx.callCount == 5);
    Require(s->userInteractionHosts.count("hosta.com") == 0);

    WKWebsiteDataStoreSetStatisticsIsRunningTest(storeRef, true, &ctx, TestCallback);
    Require(ctx.callCount == 6);
    Require(s->statisticsIsRunningTest);
    WKWebsiteDataStoreSetStatisticsIsRunningTest(storeRef, false, nullptr, nullptr);
    Require(!s->statisticsIsRunningTest);

    WKWebsiteDataStoreSetStatisticsLastSeen(storeRef, hostA, 9876.5, &ctx, TestCallback);
    Require(ctx.callCount == 7);
    Require(s->statisticsLastSeen["hosta.com"] == 9876.5);

    WKWebsiteDataStoreSetStatisticsMaxStatisticsEntries(storeRef, 1000);
    Require(s->maxStatisticsEntries == 1000);

    WKWebsiteDataStoreSetStatisticsMergeStatistic(storeRef, hostA, hostB, hostC, 100.0, true, 50.0, true, false, true, 3, &ctx, TestCallback);
    Require(ctx.callCount == 8);
    Require(s->mergedStatistics.count("hosta.com") == 1);
    const auto& merged = s->mergedStatistics["hosta.com"];
    Require(merged.topFrameDomain1 == "hostb.com");
    Require(merged.topFrameDomain2 == "hostc.com");
    Require(merged.lastSeen == 100.0);
    Require(merged.hadUserInteraction);
    Require(merged.mostRecentUserInteraction == 50.0);
    Require(merged.isGrandfathered);
    Require(!merged.isPrevalent);
    Require(merged.isVeryPrevalent);
    Require(merged.dataRecordsRemoved == 3);

    const void* dummyObj = WKStringCreateWithUTF8CString("dummy");

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsCacheMaxAgeCap(nullptr, 1.0, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsCacheMaxAgeCap(dummyObj, 1.0, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsCrossSiteLoadWithLinkDecoration(nullptr, hostA, hostB, true, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsCrossSiteLoadWithLinkDecoration(dummyObj, hostA, hostB, true, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsCrossSiteLoadWithLinkDecoration(storeRef, nullptr, hostB, true, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsCrossSiteLoadWithLinkDecoration(storeRef, hostA, nullptr, true, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsExpiredStatistic(nullptr, hostA, 1, false, false, false, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsExpiredStatistic(dummyObj, hostA, 1, false, false, false, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsExpiredStatistic(storeRef, nullptr, 1, false, false, false, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsGrandfathered(nullptr, hostA, true); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsGrandfathered(dummyObj, hostA, true); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsGrandfathered(storeRef, nullptr, true); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsGrandfatheringTime(nullptr, 1.0); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsGrandfatheringTime(dummyObj, 1.0); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsHasHadUserInteraction(nullptr, hostA, true, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsHasHadUserInteraction(dummyObj, hostA, true, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsHasHadUserInteraction(storeRef, nullptr, true, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsIsRunningTest(nullptr, true, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsIsRunningTest(dummyObj, true, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsLastSeen(nullptr, hostA, 1.0, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsLastSeen(dummyObj, hostA, 1.0, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsLastSeen(storeRef, nullptr, 1.0, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsMaxStatisticsEntries(nullptr, 10); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsMaxStatisticsEntries(dummyObj, 10); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsMergeStatistic(nullptr, hostA, hostB, hostC, 0, false, 0, false, false, false, 0, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsMergeStatistic(dummyObj, hostA, hostB, hostC, 0, false, 0, false, false, false, 0, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsMergeStatistic(storeRef, nullptr, hostB, hostC, 0, false, 0, false, false, false, 0, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsMergeStatistic(storeRef, hostA, nullptr, hostC, 0, false, 0, false, false, false, 0, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsMergeStatistic(storeRef, hostA, hostB, nullptr, 0, false, 0, false, false, false, 0, nullptr, nullptr); });

    WKRelease(hostA);
    WKRelease(hostB);
    WKRelease(hostC);
    WKRelease(dummyObj);

    return 0;
}
