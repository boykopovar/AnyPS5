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

    WKWebsiteDataStoreSetStatisticsMinimumTimeBetweenDataRecordsRemoval(storeRef, 300.0);
    Require(s->minimumTimeBetweenDataRecordsRemoval == 300.0);

    WKWebsiteDataStoreSetStatisticsNotifyPagesWhenDataRecordsWereScanned(storeRef, true);
    Require(s->notifyPagesWhenDataRecordsWereScanned);
    WKWebsiteDataStoreSetStatisticsNotifyPagesWhenDataRecordsWereScanned(storeRef, false);
    Require(!s->notifyPagesWhenDataRecordsWereScanned);

    const void* hostA = WKStringCreateWithUTF8CString("hosta.com");
    const void* hostB = WKStringCreateWithUTF8CString("hostb.com");

    WKWebsiteDataStoreSetStatisticsPrevalentResource(storeRef, hostA, true, &ctx, TestCallback);
    Require(ctx.callCount == 1);
    Require(s->prevalentResourceHosts.count("hosta.com") == 1);
    WKWebsiteDataStoreSetStatisticsPrevalentResource(storeRef, hostA, false, nullptr, nullptr);
    Require(s->prevalentResourceHosts.count("hosta.com") == 0);

    WKWebsiteDataStoreSetStatisticsPruneEntriesDownTo(storeRef, 250);
    Require(s->pruneEntriesDownTo == 250);

    WKWebsiteDataStoreSetStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(storeRef, true);
    Require(s->shouldClassifyResourcesBeforeDataRecordsRemoval);
    WKWebsiteDataStoreSetStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(storeRef, false);
    Require(!s->shouldClassifyResourcesBeforeDataRecordsRemoval);

    WKWebsiteDataStoreSetStatisticsSubframeUnderTopFrameOrigin(storeRef, hostA, hostB);
    Require(s->subframeUnderTopFrameOriginPairs.count({"hosta.com", "hostb.com"}) == 1);

    WKWebsiteDataStoreSetStatisticsSubresourceUnderTopFrameOrigin(storeRef, hostA, hostB);
    Require(s->subresourceUnderTopFrameOriginPairs.count({"hosta.com", "hostb.com"}) == 1);

    WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectFrom(storeRef, hostA, hostB);
    Require(s->subresourceUniqueRedirectFromPairs.count({"hosta.com", "hostb.com"}) == 1);

    WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectTo(storeRef, hostA, hostB);
    Require(s->subresourceUniqueRedirectToPairs.count({"hosta.com", "hostb.com"}) == 1);

    WKWebsiteDataStoreSetStatisticsTimeToLiveUserInteraction(storeRef, 604800.0, &ctx, TestCallback);
    Require(ctx.callCount == 2);
    Require(s->timeToLiveUserInteraction == 604800.0);
    WKWebsiteDataStoreSetStatisticsTimeToLiveUserInteraction(storeRef, 1209600.0, nullptr, nullptr);
    Require(s->timeToLiveUserInteraction == 1209600.0);

    WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectFrom(storeRef, hostA, hostB);
    Require(s->topFrameUniqueRedirectFromPairs.count({"hosta.com", "hostb.com"}) == 1);

    WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectTo(storeRef, hostA, hostB);
    Require(s->topFrameUniqueRedirectToPairs.count({"hosta.com", "hostb.com"}) == 1);

    WKWebsiteDataStoreSetStatisticsVeryPrevalentResource(storeRef, hostA, true, &ctx, TestCallback);
    Require(ctx.callCount == 3);
    Require(s->veryPrevalentResourceHosts.count("hosta.com") == 1);
    WKWebsiteDataStoreSetStatisticsVeryPrevalentResource(storeRef, hostA, false, nullptr, nullptr);
    Require(s->veryPrevalentResourceHosts.count("hosta.com") == 0);

    const void* dummyObj = WKStringCreateWithUTF8CString("dummy");

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsMinimumTimeBetweenDataRecordsRemoval(nullptr, 10.0); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsMinimumTimeBetweenDataRecordsRemoval(dummyObj, 10.0); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsNotifyPagesWhenDataRecordsWereScanned(nullptr, true); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsNotifyPagesWhenDataRecordsWereScanned(dummyObj, true); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsPrevalentResource(nullptr, hostA, true, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsPrevalentResource(dummyObj, hostA, true, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsPrevalentResource(storeRef, nullptr, true, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsPruneEntriesDownTo(nullptr, 10); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsPruneEntriesDownTo(dummyObj, 10); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(nullptr, true); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(dummyObj, true); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsSubframeUnderTopFrameOrigin(nullptr, hostA, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsSubframeUnderTopFrameOrigin(dummyObj, hostA, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsSubframeUnderTopFrameOrigin(storeRef, nullptr, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsSubframeUnderTopFrameOrigin(storeRef, hostA, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsSubresourceUnderTopFrameOrigin(nullptr, hostA, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsSubresourceUnderTopFrameOrigin(dummyObj, hostA, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsSubresourceUnderTopFrameOrigin(storeRef, nullptr, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsSubresourceUnderTopFrameOrigin(storeRef, hostA, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectFrom(nullptr, hostA, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectFrom(dummyObj, hostA, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectFrom(storeRef, nullptr, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectFrom(storeRef, hostA, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectTo(nullptr, hostA, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectTo(dummyObj, hostA, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectTo(storeRef, nullptr, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsSubresourceUniqueRedirectTo(storeRef, hostA, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsTimeToLiveUserInteraction(nullptr, 10.0, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsTimeToLiveUserInteraction(dummyObj, 10.0, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectFrom(nullptr, hostA, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectFrom(dummyObj, hostA, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectFrom(storeRef, nullptr, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectFrom(storeRef, hostA, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectTo(nullptr, hostA, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectTo(dummyObj, hostA, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectTo(storeRef, nullptr, hostB); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsTopFrameUniqueRedirectTo(storeRef, hostA, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsVeryPrevalentResource(nullptr, hostA, true, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsVeryPrevalentResource(dummyObj, hostA, true, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetStatisticsVeryPrevalentResource(storeRef, nullptr, true, nullptr, nullptr); });

    WKRelease(hostA);
    WKRelease(hostB);
    WKRelease(dummyObj);

    return 0;
}
