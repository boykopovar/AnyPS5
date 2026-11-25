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
        std::fprintf(stderr, "WebsiteDataStoreFetchCache check failed at line %u\n", location.line());
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

bool originsCallbackCalled = false;
void APS5_VABI OnOrigins(const void* arrayRef, void* context) {
    Require(context == &originsCallbackCalled);
    originsCallbackCalled = true;
    if (arrayRef) {
        Require(WKArrayGetSize(arrayRef) == 1);
        const void* item = WKArrayGetItemAtIndex(arrayRef, 0);
        Require(WKGetTypeID(item) == WKSecurityOriginGetTypeID());
        const void* host = WKSecurityOriginCopyHost(item);
        Require(WKStringIsEqualToUTF8CString(host, "example.org"));
        WKRelease(host);
        WKRelease(arrayRef);
    }
}

bool sizeCallbackCalled = false;
std::uint64_t reportedSize = 0;
void APS5_VABI OnSize(std::uint64_t size, void* context) {
    Require(context == &sizeCallbackCalled);
    sizeCallbackCalled = true;
    reportedSize = size;
}

bool removeCallbackCalled = false;
void APS5_VABI OnRemove(void* context) {
    Require(context == &removeCallbackCalled);
    removeCallbackCalled = true;
}

}

int main() {
    auto store = NKWebKit::Make<NKWebKit::WebsiteDataStore>();
    const void* storeRef = store.Get();

    store->fetchCacheOrigins.push_back("example.org");
    store->fetchCacheSizes["example.org"] = 4096;

    WKWebsiteDataStoreGetFetchCacheOrigins(storeRef, &originsCallbackCalled, OnOrigins);
    Require(originsCallbackCalled);
    WKWebsiteDataStoreGetFetchCacheOrigins(storeRef, nullptr, nullptr);

    auto protoStr = NKWebKit::String::FromUtf8("https");
    auto hostStr = NKWebKit::String::FromUtf8("example.org");
    const void* secOriginRef = WKSecurityOriginCreate(protoStr.Get(), hostStr.Get(), 443);

    WKWebsiteDataStoreGetFetchCacheSizeForOrigin(storeRef, secOriginRef, &sizeCallbackCalled, OnSize);
    Require(sizeCallbackCalled);
    Require(reportedSize == 4096);

    auto missingHostStr = NKWebKit::String::FromUtf8("nonexistent.com");
    const void* missingSecOriginRef = WKSecurityOriginCreate(protoStr.Get(), missingHostStr.Get(), 443);
    sizeCallbackCalled = false;
    reportedSize = 999;
    WKWebsiteDataStoreGetFetchCacheSizeForOrigin(storeRef, missingSecOriginRef, &sizeCallbackCalled, OnSize);
    Require(sizeCallbackCalled);
    Require(reportedSize == 0);
    WKRelease(missingSecOriginRef);

    auto originStr = NKWebKit::String::FromUtf8("example.org");
    sizeCallbackCalled = false;
    reportedSize = 0;
    WKWebsiteDataStoreGetFetchCacheSizeForOrigin(storeRef, originStr.Get(), &sizeCallbackCalled, OnSize);
    Require(sizeCallbackCalled);
    Require(reportedSize == 4096);
    WKWebsiteDataStoreGetFetchCacheSizeForOrigin(storeRef, originStr.Get(), nullptr, nullptr);

    WKWebsiteDataStoreRemoveFetchCacheForOrigin(storeRef, secOriginRef, &removeCallbackCalled, OnRemove);
    Require(removeCallbackCalled);
    Require(store->fetchCacheOrigins.empty());
    Require(store->fetchCacheSizes.find("example.org") == store->fetchCacheSizes.end());
    WKRelease(secOriginRef);

    store->fetchCacheOrigins.push_back("first.org");
    store->fetchCacheOrigins.push_back("second.org");
    store->fetchCacheSizes["first.org"] = 100;
    store->fetchCacheSizes["second.org"] = 200;

    removeCallbackCalled = false;
    WKWebsiteDataStoreRemoveAllFetchCaches(storeRef, &removeCallbackCalled, OnRemove);
    Require(removeCallbackCalled);
    Require(store->removedAllFetchCaches);
    Require(store->fetchCacheOrigins.empty());
    Require(store->fetchCacheSizes.empty());
    WKWebsiteDataStoreRemoveAllFetchCaches(storeRef, nullptr, nullptr);

    auto badObj = NKWebKit::Make<NKWebKit::Array>();
    RequireInvalidArgument([&] { WKWebsiteDataStoreGetFetchCacheSizeForOrigin(storeRef, badObj.Get(), nullptr, OnSize); });
    RequireInvalidArgument([&] { WKWebsiteDataStoreRemoveFetchCacheForOrigin(storeRef, badObj.Get(), nullptr, OnRemove); });
    RequireInvalidArgument([&] { WKWebsiteDataStoreGetFetchCacheSizeForOrigin(storeRef, nullptr, nullptr, OnSize); });
    RequireInvalidArgument([&] { WKWebsiteDataStoreRemoveFetchCacheForOrigin(storeRef, nullptr, nullptr, OnRemove); });

    RequireInvalidArgument([] { WKWebsiteDataStoreGetFetchCacheOrigins(nullptr, nullptr, OnOrigins); });
    RequireInvalidArgument([&] { WKWebsiteDataStoreGetFetchCacheSizeForOrigin(nullptr, originStr.Get(), nullptr, OnSize); });
    RequireInvalidArgument([] { WKWebsiteDataStoreRemoveAllFetchCaches(nullptr, nullptr, OnRemove); });
    RequireInvalidArgument([&] { WKWebsiteDataStoreRemoveFetchCacheForOrigin(nullptr, originStr.Get(), nullptr, OnRemove); });

    return 0;
}
