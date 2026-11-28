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

static void APS5_VABI TestVoidCallback(void* context) {
    auto* ctx = static_cast<TestContext*>(context);
    ctx->callCount++;
}

static void APS5_VABI TestBoolCallback(bool result, void* context) {
    auto* ctx = static_cast<TestContext*>(context);
    ctx->callCount++;
    ctx->lastResult = result;
}

int main() {
    auto store = NKWebKit::Make<NKWebKit::WebsiteDataStore>();
    const void* storeRef = store.Get();
    Require(storeRef != nullptr);
    auto* s = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(storeRef, __func__);

    TestContext ctx;

    const void* hostA = WKStringCreateWithUTF8CString("hosta.com");
    const void* hostB = WKStringCreateWithUTF8CString("hostb.com");

    s->isolatedSessionHosts.insert("hosta.com");
    s->localStorageHosts.insert("hosta.com");
    s->debugPrevalentResources.push_back("hosta.com");

    WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStore(storeRef, &ctx, TestVoidCallback);
    Require(ctx.callCount == 1);
    Require(s->clearInMemoryAndPersistentStoreCalled);
    Require(!s->clearModifiedSinceHours.has_value());
    Require(s->isolatedSessionHosts.empty());
    Require(s->localStorageHosts.empty());
    Require(s->debugPrevalentResources.empty());

    WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreModifiedSinceHours(storeRef, 24, &ctx, TestVoidCallback);
    Require(ctx.callCount == 2);
    Require(s->clearModifiedSinceHours.value() == 24);

    WKWebsiteDataStoreStatisticsClearThroughWebsiteDataRemoval(storeRef, &ctx, TestVoidCallback);
    Require(ctx.callCount == 3);
    Require(s->clearThroughWebsiteDataRemovalCalled);

    WKWebsiteDataStoreStatisticsDeleteCookiesForTesting(storeRef, hostA, true, &ctx, TestVoidCallback);
    Require(ctx.callCount == 4);
    Require(s->deletedCookiesHosts.count("hosta.com") == 1);
    Require(s->deletedCookiesHosts["hosta.com"]);

    WKWebsiteDataStoreStatisticsHasIsolatedSession(storeRef, hostA, &ctx, TestBoolCallback);
    Require(ctx.callCount == 5);
    Require(!ctx.lastResult);

    s->isolatedSessionHosts.insert("hosta.com");
    WKWebsiteDataStoreStatisticsHasIsolatedSession(storeRef, hostA, &ctx, TestBoolCallback);
    Require(ctx.callCount == 6);
    Require(ctx.lastResult);

    WKWebsiteDataStoreStatisticsHasLocalStorage(storeRef, hostA, &ctx, TestBoolCallback);
    Require(ctx.callCount == 7);
    Require(!ctx.lastResult);

    s->localStorageHosts.insert("hosta.com");
    WKWebsiteDataStoreStatisticsHasLocalStorage(storeRef, hostA, &ctx, TestBoolCallback);
    Require(ctx.callCount == 8);
    Require(ctx.lastResult);

    WKWebsiteDataStoreStatisticsProcessStatisticsAndDataRecords(storeRef, &ctx, TestVoidCallback);
    Require(ctx.callCount == 9);
    Require(s->processStatisticsAndDataRecordsCalled);

    s->debugPrevalentResources.push_back("hostb.com");
    s->resourceLoadStatisticsEnabled = true;
    s->blockThirdPartyCookies = true;
    WKWebsiteDataStoreStatisticsResetToConsistentState(storeRef, &ctx, TestVoidCallback);
    Require(ctx.callCount == 10);
    Require(s->resetToConsistentStateCalled);
    Require(!s->resourceLoadStatisticsEnabled);
    Require(!s->blockThirdPartyCookies);
    Require(s->downgradeReferrer);
    Require(s->debugPrevalentResources.empty());
    Require(!s->clearInMemoryAndPersistentStoreCalled);
    Require(!s->clearThroughWebsiteDataRemovalCalled);
    Require(s->isolatedSessionHosts.empty());
    Require(s->localStorageHosts.empty());
    Require(s->deletedCookiesHosts.empty());

    WKWebsiteDataStoreStatisticsUpdateCookieBlocking(storeRef, &ctx, TestVoidCallback);
    Require(ctx.callCount == 11);
    Require(s->updateCookieBlockingCalled);

    WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStore(storeRef, nullptr, nullptr);
    WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreModifiedSinceHours(storeRef, 1, nullptr, nullptr);
    WKWebsiteDataStoreStatisticsClearThroughWebsiteDataRemoval(storeRef, nullptr, nullptr);
    WKWebsiteDataStoreStatisticsDeleteCookiesForTesting(storeRef, hostB, false, nullptr, nullptr);
    WKWebsiteDataStoreStatisticsHasIsolatedSession(storeRef, hostB, nullptr, nullptr);
    WKWebsiteDataStoreStatisticsHasLocalStorage(storeRef, hostB, nullptr, nullptr);
    WKWebsiteDataStoreStatisticsProcessStatisticsAndDataRecords(storeRef, nullptr, nullptr);
    WKWebsiteDataStoreStatisticsResetToConsistentState(storeRef, nullptr, nullptr);
    WKWebsiteDataStoreStatisticsUpdateCookieBlocking(storeRef, nullptr, nullptr);

    const void* dummyObj = WKStringCreateWithUTF8CString("dummy");

    RequireThrows([&] { WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStore(nullptr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStore(dummyObj, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreModifiedSinceHours(nullptr, 1, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreStatisticsClearInMemoryAndPersistentStoreModifiedSinceHours(dummyObj, 1, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreStatisticsClearThroughWebsiteDataRemoval(nullptr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreStatisticsClearThroughWebsiteDataRemoval(dummyObj, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreStatisticsDeleteCookiesForTesting(nullptr, hostA, true, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreStatisticsDeleteCookiesForTesting(dummyObj, hostA, true, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreStatisticsDeleteCookiesForTesting(storeRef, nullptr, true, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreStatisticsHasIsolatedSession(nullptr, hostA, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreStatisticsHasIsolatedSession(dummyObj, hostA, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreStatisticsHasIsolatedSession(storeRef, nullptr, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreStatisticsHasLocalStorage(nullptr, hostA, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreStatisticsHasLocalStorage(dummyObj, hostA, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreStatisticsHasLocalStorage(storeRef, nullptr, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreStatisticsProcessStatisticsAndDataRecords(nullptr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreStatisticsProcessStatisticsAndDataRecords(dummyObj, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreStatisticsResetToConsistentState(nullptr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreStatisticsResetToConsistentState(dummyObj, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreStatisticsUpdateCookieBlocking(nullptr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreStatisticsUpdateCookieBlocking(dummyObj, nullptr, nullptr); });

    WKRelease(hostA);
    WKRelease(hostB);
    WKRelease(dummyObj);

    return 0;
}
