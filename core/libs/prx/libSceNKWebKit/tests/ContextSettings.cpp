#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>


int main() {
    auto pool = NKWebKit::Make<NKWebKit::ProcessPool>();
    const void* poolRef = NKWebKit::ToRef(pool);

    Require(WKContextGetCacheModel(poolRef) == 0);
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

    ExpectErr<std::invalid_argument>([] { WKContextGetCacheModel(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKContextSetCacheModel(nullptr, 1); });
    ExpectErr<std::invalid_argument>([] { WKContextGetAccessibilityEnabled(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKContextSetAccessibilityEnabled(nullptr, false); });
    ExpectErr<std::invalid_argument>([] { WKContextSetAlwaysUsesComplexTextCodePath(nullptr, true); });
    ExpectErr<std::invalid_argument>([] { WKContextSetMemoryCacheDisabled(nullptr, true); });
    ExpectErr<std::invalid_argument>([] { WKContextSetHTTPPipeliningEnabled(nullptr, false); });
    ExpectErr<std::invalid_argument>([] { WKContextSetShouldUseFontSmoothing(nullptr, false); });
    ExpectErr<std::invalid_argument>([] { WKContextJavaScriptConfigurationFileEnabled(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKContextSetJavaScriptConfigurationFileEnabled(nullptr, true); });
    ExpectErr<std::invalid_argument>([] { WKContextSetJavaScriptGarbageCollectorTimerEnabled(nullptr, false); });
    ExpectErr<std::invalid_argument>([] { WKContextGarbageCollectJavaScriptObjects(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKContextSetStorageAccessAPIEnabled(nullptr, true); });

    WKRelease(poolRef);
    return 0;
}
