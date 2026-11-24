#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ContextSettings check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireInvalidArgument(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    auto pool = NKWebKit::Make<NKWebKit::ProcessPool>();
    const void* poolRef = NKWebKit::ToRef(pool);

    Require(WKContextGetCacheModel(poolRef) == 2);
    Require(WKContextGetCacheModel(nullptr) == 2);
    {
        auto other = NKWebKit::Make<NKWebKit::ProcessPool>();
        const void* otherRef = NKWebKit::ToRef(other);
        WKContextSetCacheModel(poolRef, 1);
        Require(WKContextGetCacheModel(poolRef) == 1);
        Require(WKContextGetCacheModel(otherRef) == 1);
        WKContextSetCacheModel(otherRef, 0);
        Require(WKContextGetCacheModel(poolRef) == 0);
        WKRelease(otherRef);
    }
    WKContextSetCacheModel(nullptr, 1);
    Require(WKContextGetCacheModel(poolRef) == 1);
    for (const std::uint32_t unknown : {3u, 7u, 0xFFFFFFFFu}) {
        WKContextSetCacheModel(poolRef, 1);
        WKContextSetCacheModel(poolRef, unknown);
        Require(WKContextGetCacheModel(poolRef) == 0);
    }
    WKContextSetCacheModel(poolRef, 2);
    Require(WKContextGetCacheModel(poolRef) == 2);

    Require(!WKContextGetAccessibilityEnabled(poolRef));
    WKContextSetAccessibilityEnabled(poolRef, true);
    Require(WKContextGetAccessibilityEnabled(poolRef));

    Require(!pool->alwaysUsesComplexTextCodePath);
    WKContextSetAlwaysUsesComplexTextCodePath(poolRef, true);
    Require(pool->alwaysUsesComplexTextCodePath);
    WKContextSetAlwaysUsesComplexTextCodePath(poolRef, false);
    Require(!pool->alwaysUsesComplexTextCodePath);

    Require(!pool->memoryCacheDisabled);
    WKContextSetMemoryCacheDisabled(poolRef, true);
    Require(pool->memoryCacheDisabled);
    WKContextSetMemoryCacheDisabled(poolRef, false);
    Require(!pool->memoryCacheDisabled);

    Require(pool->httpPipeliningEnabled);
    WKContextSetHTTPPipeliningEnabled(poolRef, false);
    Require(!pool->httpPipeliningEnabled);
    WKContextSetHTTPPipeliningEnabled(poolRef, true);
    Require(pool->httpPipeliningEnabled);

    Require(pool->shouldUseFontSmoothing);
    WKContextSetShouldUseFontSmoothing(poolRef, false);
    Require(!pool->shouldUseFontSmoothing);
    WKContextSetShouldUseFontSmoothing(poolRef, true);
    Require(pool->shouldUseFontSmoothing);

    Require(!WKContextJavaScriptConfigurationFileEnabled(poolRef));
    Require(!pool->javaScriptConfigurationFileEnabled);
    WKContextSetJavaScriptConfigurationFileEnabled(poolRef, true);
    Require(WKContextJavaScriptConfigurationFileEnabled(poolRef));
    Require(pool->javaScriptConfigurationFileEnabled);
    WKContextSetJavaScriptConfigurationFileEnabled(poolRef, false);
    Require(!WKContextJavaScriptConfigurationFileEnabled(poolRef));
    Require(!pool->javaScriptConfigurationFileEnabled);

    Require(pool->javaScriptGarbageCollectorTimerEnabled);
    WKContextSetJavaScriptGarbageCollectorTimerEnabled(poolRef, false);
    Require(!pool->javaScriptGarbageCollectorTimerEnabled);
    WKContextSetJavaScriptGarbageCollectorTimerEnabled(poolRef, true);
    Require(pool->javaScriptGarbageCollectorTimerEnabled);

    Require(pool->garbageCollectJavaScriptObjectsCount == 0);
    WKContextGarbageCollectJavaScriptObjects(poolRef);
    Require(pool->garbageCollectJavaScriptObjectsCount == 1);
    WKContextGarbageCollectJavaScriptObjects(poolRef);
    Require(pool->garbageCollectJavaScriptObjectsCount == 2);

    Require(!pool->storageAccessAPIEnabled);
    WKContextSetStorageAccessAPIEnabled(poolRef, true);
    Require(pool->storageAccessAPIEnabled);
    WKContextSetStorageAccessAPIEnabled(poolRef, false);
    Require(!pool->storageAccessAPIEnabled);

    Require(!pool->cachedCredentialsCleared);
    Require(!pool->legacyPrivateBrowsingLocalStorageCleared);
    Require(!pool->localStorageSynced);

    WKContextClearCachedCredentials(poolRef);
    Require(pool->cachedCredentialsCleared);

    WKContextClearLegacyPrivateBrowsingLocalStorage(poolRef);
    Require(pool->legacyPrivateBrowsingLocalStorageCleared);

    WKContextSyncLocalStorage(poolRef);
    Require(pool->localStorageSynced);

    RequireInvalidArgument([] { WKContextGetAccessibilityEnabled(nullptr); });
    RequireInvalidArgument([] { WKContextSetAccessibilityEnabled(nullptr, false); });
    RequireInvalidArgument([] { WKContextSetAlwaysUsesComplexTextCodePath(nullptr, true); });
    RequireInvalidArgument([] { WKContextSetMemoryCacheDisabled(nullptr, true); });
    RequireInvalidArgument([] { WKContextSetHTTPPipeliningEnabled(nullptr, false); });
    RequireInvalidArgument([] { WKContextSetShouldUseFontSmoothing(nullptr, false); });
    RequireInvalidArgument([] { WKContextJavaScriptConfigurationFileEnabled(nullptr); });
    RequireInvalidArgument([] { WKContextSetJavaScriptConfigurationFileEnabled(nullptr, true); });
    RequireInvalidArgument([] { WKContextSetJavaScriptGarbageCollectorTimerEnabled(nullptr, false); });
    RequireInvalidArgument([] { WKContextGarbageCollectJavaScriptObjects(nullptr); });
    RequireInvalidArgument([] { WKContextSetStorageAccessAPIEnabled(nullptr, true); });
    RequireInvalidArgument([] { WKContextClearCachedCredentials(nullptr); });
    RequireInvalidArgument([] { WKContextClearLegacyPrivateBrowsingLocalStorage(nullptr); });
    RequireInvalidArgument([] { WKContextSyncLocalStorage(nullptr); });

    auto pageConfig = NKWebKit::Make<NKWebKit::PageConfiguration>();
    const void* invalidRef = NKWebKit::ToRef(pageConfig);
    RequireInvalidArgument([&] { WKContextClearCachedCredentials(invalidRef); });
    RequireInvalidArgument([&] { WKContextClearLegacyPrivateBrowsingLocalStorage(invalidRef); });
    RequireInvalidArgument([&] { WKContextSyncLocalStorage(invalidRef); });
    WKRelease(invalidRef);

    WKRelease(poolRef);
    return 0;
}
