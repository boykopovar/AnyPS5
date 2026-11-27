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
    bool lastResult = false;
};

static void APS5_VABI TestCallback(bool result, void* context) {
    auto* ctx = static_cast<TestContext*>(context);
    ctx->callCount++;
    ctx->lastResult = result;
}

struct DumpContext {
    int callCount = 0;
    bool hasDump = false;
};

static void APS5_VABI DumpCallback(const void* dump, void* context) {
    auto* ctx = static_cast<DumpContext*>(context);
    ctx->callCount++;
    ctx->hasDump = (dump != nullptr);
}

int main() {
    auto store = NKWebKit::Make<NKWebKit::WebsiteDataStore>();
    const void* storeRef = store.Get();
    Require(storeRef != nullptr);
    auto* s = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(storeRef, __func__);

    TestContext ctx;

    WKWebsiteDataStoreDoesStatisticsDomainIDExistInDatabase(storeRef, 42, &ctx, TestCallback);
    Require(ctx.callCount == 1);
    Require(!ctx.lastResult);

    s->statisticsDomainIDs.insert(42);
    WKWebsiteDataStoreDoesStatisticsDomainIDExistInDatabase(storeRef, 42, &ctx, TestCallback);
    Require(ctx.callCount == 2);
    Require(ctx.lastResult);

    WKWebsiteDataStoreDoesStatisticsDomainIDExistInDatabase(storeRef, 99, &ctx, TestCallback);
    Require(ctx.callCount == 3);
    Require(!ctx.lastResult);

    WKWebsiteDataStoreIsStatisticsEphemeral(storeRef, &ctx, TestCallback);
    Require(ctx.callCount == 4);
    Require(!ctx.lastResult);

    s->isEphemeral = true;
    WKWebsiteDataStoreIsStatisticsEphemeral(storeRef, &ctx, TestCallback);
    Require(ctx.callCount == 5);
    Require(ctx.lastResult);

    const void* hostA = WKStringCreateWithUTF8CString("hosta.com");
    const void* hostB = WKStringCreateWithUTF8CString("hostb.com");

    WKWebsiteDataStoreIsStatisticsGrandfathered(storeRef, hostA, &ctx, TestCallback);
    Require(ctx.callCount == 6);
    Require(!ctx.lastResult);

    s->grandfatheredHosts.insert("hosta.com");
    WKWebsiteDataStoreIsStatisticsGrandfathered(storeRef, hostA, &ctx, TestCallback);
    Require(ctx.callCount == 7);
    Require(ctx.lastResult);

    WKWebsiteDataStoreIsStatisticsHasHadUserInteraction(storeRef, hostA, &ctx, TestCallback);
    Require(ctx.callCount == 8);
    Require(!ctx.lastResult);

    s->userInteractionHosts.insert("hosta.com");
    WKWebsiteDataStoreIsStatisticsHasHadUserInteraction(storeRef, hostA, &ctx, TestCallback);
    Require(ctx.callCount == 9);
    Require(ctx.lastResult);

    WKWebsiteDataStoreIsStatisticsOnlyInDatabaseOnce(storeRef, hostA, hostB, &ctx, TestCallback);
    Require(ctx.callCount == 10);
    Require(!ctx.lastResult);

    s->onlyInDatabaseOncePairs.insert({"hosta.com", "hostb.com"});
    WKWebsiteDataStoreIsStatisticsOnlyInDatabaseOnce(storeRef, hostA, hostB, &ctx, TestCallback);
    Require(ctx.callCount == 11);
    Require(ctx.lastResult);

    WKWebsiteDataStoreIsStatisticsPrevalentResource(storeRef, hostA, &ctx, TestCallback);
    Require(ctx.callCount == 12);
    Require(!ctx.lastResult);

    s->prevalentResourceHosts.insert("hosta.com");
    WKWebsiteDataStoreIsStatisticsPrevalentResource(storeRef, hostA, &ctx, TestCallback);
    Require(ctx.callCount == 13);
    Require(ctx.lastResult);

    WKWebsiteDataStoreIsStatisticsRegisteredAsRedirectingTo(storeRef, hostA, hostB, &ctx, TestCallback);
    Require(ctx.callCount == 14);
    Require(!ctx.lastResult);

    s->redirectingToPairs.insert({"hosta.com", "hostb.com"});
    WKWebsiteDataStoreIsStatisticsRegisteredAsRedirectingTo(storeRef, hostA, hostB, &ctx, TestCallback);
    Require(ctx.callCount == 15);
    Require(ctx.lastResult);

    WKWebsiteDataStoreIsStatisticsRegisteredAsSubFrameUnder(storeRef, hostA, hostB, &ctx, TestCallback);
    Require(ctx.callCount == 16);
    Require(!ctx.lastResult);

    s->subFrameUnderPairs.insert({"hosta.com", "hostb.com"});
    WKWebsiteDataStoreIsStatisticsRegisteredAsSubFrameUnder(storeRef, hostA, hostB, &ctx, TestCallback);
    Require(ctx.callCount == 17);
    Require(ctx.lastResult);

    WKWebsiteDataStoreIsStatisticsRegisteredAsSubresourceUnder(storeRef, hostA, hostB, &ctx, TestCallback);
    Require(ctx.callCount == 18);
    Require(!ctx.lastResult);

    s->subresourceUnderPairs.insert({"hosta.com", "hostb.com"});
    WKWebsiteDataStoreIsStatisticsRegisteredAsSubresourceUnder(storeRef, hostA, hostB, &ctx, TestCallback);
    Require(ctx.callCount == 19);
    Require(ctx.lastResult);

    WKWebsiteDataStoreIsStatisticsVeryPrevalentResource(storeRef, hostA, &ctx, TestCallback);
    Require(ctx.callCount == 20);
    Require(!ctx.lastResult);

    s->veryPrevalentResourceHosts.insert("hosta.com");
    WKWebsiteDataStoreIsStatisticsVeryPrevalentResource(storeRef, hostA, &ctx, TestCallback);
    Require(ctx.callCount == 21);
    Require(ctx.lastResult);

    WKWebsiteDataStoreIsStatisticsVeryPrevalentResource(storeRef, hostA, nullptr, nullptr);

    DumpContext dumpCtx;
    WKWebsiteDataStoreDumpResourceLoadStatistics(storeRef, &dumpCtx, DumpCallback);
    Require(dumpCtx.callCount == 1);
    Require(dumpCtx.hasDump);
    WKWebsiteDataStoreDumpResourceLoadStatistics(storeRef, nullptr, nullptr);

    const void* dummyObj = WKStringCreateWithUTF8CString("dummy");

    RequireThrows([&] { WKWebsiteDataStoreDumpResourceLoadStatistics(nullptr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreDumpResourceLoadStatistics(dummyObj, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreDoesStatisticsDomainIDExistInDatabase(nullptr, 1, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreDoesStatisticsDomainIDExistInDatabase(dummyObj, 1, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsEphemeral(nullptr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsEphemeral(dummyObj, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsGrandfathered(nullptr, hostA, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsGrandfathered(dummyObj, hostA, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsGrandfathered(storeRef, nullptr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsGrandfathered(storeRef, storeRef, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsHasHadUserInteraction(nullptr, hostA, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsHasHadUserInteraction(dummyObj, hostA, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsHasHadUserInteraction(storeRef, nullptr, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsOnlyInDatabaseOnce(nullptr, hostA, hostB, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsOnlyInDatabaseOnce(dummyObj, hostA, hostB, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsOnlyInDatabaseOnce(storeRef, nullptr, hostB, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsOnlyInDatabaseOnce(storeRef, hostA, nullptr, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsPrevalentResource(nullptr, hostA, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsPrevalentResource(dummyObj, hostA, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsPrevalentResource(storeRef, nullptr, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsRegisteredAsRedirectingTo(nullptr, hostA, hostB, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsRegisteredAsRedirectingTo(dummyObj, hostA, hostB, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsRegisteredAsRedirectingTo(storeRef, nullptr, hostB, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsRegisteredAsRedirectingTo(storeRef, hostA, nullptr, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsRegisteredAsSubFrameUnder(nullptr, hostA, hostB, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsRegisteredAsSubFrameUnder(dummyObj, hostA, hostB, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsRegisteredAsSubFrameUnder(storeRef, nullptr, hostB, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsRegisteredAsSubFrameUnder(storeRef, hostA, nullptr, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsRegisteredAsSubresourceUnder(nullptr, hostA, hostB, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsRegisteredAsSubresourceUnder(dummyObj, hostA, hostB, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsRegisteredAsSubresourceUnder(storeRef, nullptr, hostB, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsRegisteredAsSubresourceUnder(storeRef, hostA, nullptr, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsVeryPrevalentResource(nullptr, hostA, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsVeryPrevalentResource(dummyObj, hostA, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreIsStatisticsVeryPrevalentResource(storeRef, nullptr, nullptr, nullptr); });

    WKRelease(hostA);
    WKRelease(hostB);
    WKRelease(dummyObj);

    return 0;
}
