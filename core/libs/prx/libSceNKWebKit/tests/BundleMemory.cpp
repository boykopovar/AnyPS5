#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/JscExports.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKBundle memory check failed at line %u\n", location.line());
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
    auto bundle = NKWebKit::Make<NKWebKit::Bundle>();
    const void* bundleRef = bundle.Get();

    Require(bundle->memoryReleaseCount == 0);
    Require(!bundle->historyClearedForTesting);
    Require(!bundle->resourceLoadStatisticsCleared);
    Require(bundle->resourceLoadStatisticsNotifyCount == 0);

    const std::size_t initialObjects = WKBundleGetJavaScriptObjectsCount(bundleRef);
    Require(initialObjects > 0);
    Require(bundle->context != nullptr);

    NKWebKit::JSObjectRef object = JSObjectMake(bundle->context, nullptr, nullptr);
    Require(object != nullptr);

    const std::size_t countAfterAlloc = WKBundleGetJavaScriptObjectsCount(bundleRef);
    Require(countAfterAlloc > initialObjects);

    bundle->javaScriptObjectsCount = 10;
    Require(WKBundleGetJavaScriptObjectsCount(bundleRef) == countAfterAlloc + 10);

    WKBundleReleaseMemory(bundleRef);
    Require(bundle->memoryReleaseCount == 1);
    WKBundleReleaseMemory(bundleRef);
    Require(bundle->memoryReleaseCount == 2);

    WKBundleClearHistoryForTesting(bundleRef);
    Require(bundle->historyClearedForTesting);

    WKBundleClearResourceLoadStatistics(bundleRef);
    Require(bundle->resourceLoadStatisticsCleared);

    WKBundleResourceLoadStatisticsNotifyObserver(bundleRef);
    Require(bundle->resourceLoadStatisticsNotifyCount == 1);
    WKBundleResourceLoadStatisticsNotifyObserver(bundleRef);
    Require(bundle->resourceLoadStatisticsNotifyCount == 2);

    RequireInvalidArgument([] { WKBundleGetJavaScriptObjectsCount(nullptr); });
    RequireInvalidArgument([] { WKBundleReleaseMemory(nullptr); });
    RequireInvalidArgument([] { WKBundleClearHistoryForTesting(nullptr); });
    RequireInvalidArgument([] { WKBundleClearResourceLoadStatistics(nullptr); });
    RequireInvalidArgument([] { WKBundleResourceLoadStatisticsNotifyObserver(nullptr); });

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    RequireInvalidArgument([&] { WKBundleGetJavaScriptObjectsCount(frame.Get()); });
    RequireInvalidArgument([&] { WKBundleReleaseMemory(frame.Get()); });
    RequireInvalidArgument([&] { WKBundleClearHistoryForTesting(frame.Get()); });
    RequireInvalidArgument([&] { WKBundleClearResourceLoadStatistics(frame.Get()); });
    RequireInvalidArgument([&] { WKBundleResourceLoadStatisticsNotifyObserver(frame.Get()); });

    return 0;
}
