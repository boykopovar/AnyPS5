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
        std::fprintf(stderr, "WebsiteDataStore check failed at line %u\n", location.line());
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
    auto store = NKWebKit::Make<NKWebKit::WebsiteDataStore>();
    const void* storeRef = NKWebKit::ToRef(store);

    Require(!store->quotaReset);
    Require(!store->diskCacheCleared);
    Require(!store->useITPDatabase);
    Require(store->cacheModel == 0);
    Require(!store->bundleIdentifierInNetworkProcessCleared);
    Require(!store->bundleIdentifierInNetworkProcess);

    WKWebsiteDataStoreResetQuota(storeRef);
    Require(store->quotaReset);

    WKWebsiteDataStoreClearDiskCache(storeRef);
    Require(store->diskCacheCleared);

    WKWebsiteDataStoreSetUseITPDatabase(storeRef, true);
    Require(store->useITPDatabase);
    WKWebsiteDataStoreSetUseITPDatabase(storeRef, false);
    Require(!store->useITPDatabase);

    WKWebsiteDataStoreSetCacheModelSynchronouslyForTesting(storeRef, 2);
    Require(store->cacheModel == 2);
    WKWebsiteDataStoreSetCacheModelSynchronouslyForTesting(storeRef, 0);
    Require(store->cacheModel == 0);

    const void* idStr = NKWebKit::ToRef(NKWebKit::String::FromUtf8("com.playstation.webkit.network"));
    WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(storeRef, idStr);
    Require(store->bundleIdentifierInNetworkProcess);
    Require(store->bundleIdentifierInNetworkProcess->Utf8() == "com.playstation.webkit.network");

    WKWebsiteDataStoreClearBundleIdentifierInNetworkProcess(storeRef);
    Require(store->bundleIdentifierInNetworkProcessCleared);
    Require(!store->bundleIdentifierInNetworkProcess);

    WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(storeRef, idStr);
    Require(store->bundleIdentifierInNetworkProcess);
    WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(storeRef, nullptr);
    Require(!store->bundleIdentifierInNetworkProcess);

    auto wrongType = NKWebKit::Make<NKWebKit::WebsiteDataStoreConfiguration>();
    const void* wrongRef = NKWebKit::ToRef(wrongType);

    RequireInvalidArgument([&] { WKWebsiteDataStoreResetQuota(nullptr); });
    RequireInvalidArgument([&] { WKWebsiteDataStoreResetQuota(wrongRef); });

    RequireInvalidArgument([&] { WKWebsiteDataStoreClearDiskCache(nullptr); });
    RequireInvalidArgument([&] { WKWebsiteDataStoreClearDiskCache(wrongRef); });

    RequireInvalidArgument([&] { WKWebsiteDataStoreSetUseITPDatabase(nullptr, true); });
    RequireInvalidArgument([&] { WKWebsiteDataStoreSetUseITPDatabase(wrongRef, true); });

    RequireInvalidArgument([&] { WKWebsiteDataStoreSetCacheModelSynchronouslyForTesting(nullptr, 1); });
    RequireInvalidArgument([&] { WKWebsiteDataStoreSetCacheModelSynchronouslyForTesting(wrongRef, 1); });

    RequireInvalidArgument([&] { WKWebsiteDataStoreClearBundleIdentifierInNetworkProcess(nullptr); });
    RequireInvalidArgument([&] { WKWebsiteDataStoreClearBundleIdentifierInNetworkProcess(wrongRef); });

    RequireInvalidArgument([&] { WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(nullptr, idStr); });
    RequireInvalidArgument([&] { WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(wrongRef, idStr); });
    RequireInvalidArgument([&] { WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(storeRef, wrongRef); });

    return 0;
}
