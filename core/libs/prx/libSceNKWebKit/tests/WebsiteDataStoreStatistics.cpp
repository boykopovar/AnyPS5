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

int main() {
    auto store = NKWebKit::Make<NKWebKit::WebsiteDataStore>();
    const void* storeRef = store.Get();
    Require(storeRef != nullptr);
    const auto* s = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(storeRef, __func__);

    Require(!s->resourceLoadStatisticsEnabled);
    Require(!s->resourceLoadStatisticsDebugMode);
    Require(s->debugPrevalentResources.empty());
    Require(!s->blockThirdPartyCookies);
    Require(s->thirdPartyCookieBlockingPolicy == 0);
    Require(!s->downgradeReferrer);
    Require(s->firstPartyHostCNAMEDomains.empty());
    Require(s->thirdPartyCNAMEDomains.empty());
    Require(s->sameSiteStrictCookiesHosts.empty());
    Require(!s->firstPartyWebsiteDataRemovalMode);

    WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(storeRef, true);
    Require(s->resourceLoadStatisticsEnabled);
    WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(storeRef, false);
    Require(!s->resourceLoadStatisticsEnabled);

    WKWebsiteDataStoreSetResourceLoadStatisticsDebugMode(storeRef, true);
    Require(s->resourceLoadStatisticsDebugMode);
    WKWebsiteDataStoreSetResourceLoadStatisticsDebugMode(storeRef, false);
    Require(!s->resourceLoadStatisticsDebugMode);

    int callbackCount = 0;
    auto callback = [](void* ctx) {
        (*static_cast<int*>(ctx))++;
    };

    WKWebsiteDataStoreSetResourceLoadStatisticsDebugModeWithCompletionHandler(storeRef, true, &callbackCount, callback);
    Require(callbackCount == 1);
    Require(s->resourceLoadStatisticsDebugMode);
    WKWebsiteDataStoreSetResourceLoadStatisticsDebugModeWithCompletionHandler(storeRef, false, nullptr, nullptr);
    Require(!s->resourceLoadStatisticsDebugMode);

    const void* hostStr = WKStringCreateWithUTF8CString("example.com");
    WKWebsiteDataStoreSetResourceLoadStatisticsPrevalentResourceForDebugMode(storeRef, hostStr, &callbackCount, callback);
    Require(callbackCount == 2);
    Require(s->debugPrevalentResources.size() == 1);
    Require(s->debugPrevalentResources[0] == "example.com");

    WKWebsiteDataStoreSetResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(storeRef, true, 2, &callbackCount, callback);
    Require(callbackCount == 3);
    Require(s->blockThirdPartyCookies);
    Require(s->thirdPartyCookieBlockingPolicy == 2);

    WKWebsiteDataStoreSetResourceLoadStatisticsShouldDowngradeReferrerForTesting(storeRef, true, &callbackCount, callback);
    Require(callbackCount == 4);
    Require(s->downgradeReferrer);

    const void* cnameStr = WKStringCreateWithUTF8CString("cname.example.com");
    WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyHostCNAMEDomainForTesting(storeRef, hostStr, cnameStr, &callbackCount, callback);
    Require(callbackCount == 5);
    Require(s->firstPartyHostCNAMEDomains.size() == 1);
    Require(s->firstPartyHostCNAMEDomains.at("example.com") == "cname.example.com");

    const void* thirdPartyStr = WKStringCreateWithUTF8CString("tracker.cname.com");
    WKWebsiteDataStoreSetResourceLoadStatisticsThirdPartyCNAMEDomainForTesting(storeRef, thirdPartyStr, &callbackCount, callback);
    Require(callbackCount == 6);
    Require(s->thirdPartyCNAMEDomains.size() == 1);
    Require(s->thirdPartyCNAMEDomains[0] == "tracker.cname.com");

    WKWebsiteDataStoreSetResourceLoadStatisticsToSameSiteStrictCookiesForTesting(storeRef, hostStr, &callbackCount, callback);
    Require(callbackCount == 7);
    Require(s->sameSiteStrictCookiesHosts.size() == 1);
    Require(s->sameSiteStrictCookiesHosts[0] == "example.com");

    WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTesting(storeRef, true, &callbackCount, callback);
    Require(callbackCount == 8);
    Require(s->firstPartyWebsiteDataRemovalMode);

    const void* dummyObject = WKStringCreateWithUTF8CString("dummy");

    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(nullptr, true); });
    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(dummyObject, true); });

    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsDebugMode(nullptr, true); });
    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsDebugMode(dummyObject, true); });

    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsDebugModeWithCompletionHandler(nullptr, true, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsDebugModeWithCompletionHandler(dummyObject, true, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsPrevalentResourceForDebugMode(nullptr, hostStr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsPrevalentResourceForDebugMode(dummyObject, hostStr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsPrevalentResourceForDebugMode(storeRef, nullptr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsPrevalentResourceForDebugMode(storeRef, storeRef, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(nullptr, true, 1, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(dummyObject, true, 1, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsShouldDowngradeReferrerForTesting(nullptr, true, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsShouldDowngradeReferrerForTesting(dummyObject, true, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyHostCNAMEDomainForTesting(nullptr, hostStr, cnameStr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyHostCNAMEDomainForTesting(dummyObject, hostStr, cnameStr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyHostCNAMEDomainForTesting(storeRef, nullptr, cnameStr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyHostCNAMEDomainForTesting(storeRef, hostStr, nullptr, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsThirdPartyCNAMEDomainForTesting(nullptr, thirdPartyStr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsThirdPartyCNAMEDomainForTesting(dummyObject, thirdPartyStr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsThirdPartyCNAMEDomainForTesting(storeRef, nullptr, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsToSameSiteStrictCookiesForTesting(nullptr, hostStr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsToSameSiteStrictCookiesForTesting(dummyObject, hostStr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsToSameSiteStrictCookiesForTesting(storeRef, nullptr, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTesting(nullptr, true, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreSetResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTesting(dummyObject, true, nullptr, nullptr); });

    WKRelease(hostStr);
    WKRelease(cnameStr);
    WKRelease(thirdPartyStr);
    WKRelease(dummyObject);

    return 0;
}
