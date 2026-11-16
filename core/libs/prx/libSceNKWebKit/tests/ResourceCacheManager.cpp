#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ResourceCacheManager check failed at line %u\n", location.line());
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

bool callbackCalled = false;
void OnOrigins(const void* arrayRef, const void* errorRef, void* context) {
    (void)errorRef;
    (void)context;
    callbackCalled = (arrayRef != nullptr);
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

}

int main() {
    auto mgr = NKWebKit::Make<NKWebKit::ResourceCacheManager>();
    const void* mgrRef = mgr.Get();
    mgr->origins.push_back("example.com");

    WKResourceCacheManagerClearCacheForAllOrigins(mgrRef, 1);
    Require(mgr->clearedAll);
    Require(mgr->lastCachesCleared == 1);
    Require(mgr->origins.empty());

    mgr->origins.push_back("example.org");
    auto protoStr = NKWebKit::String::FromUtf8("https");
    auto hostStr = NKWebKit::String::FromUtf8("example.org");
    const void* secOriginRef = WKSecurityOriginCreate(protoStr.Get(), hostStr.Get(), 443);
    WKResourceCacheManagerClearCacheForOrigin(mgrRef, secOriginRef, 2);
    Require(mgr->origins.empty());
    Require(mgr->lastCachesCleared == 2);
    WKRelease(secOriginRef);

    mgr->origins.push_back("example.net");
    auto originStr = NKWebKit::String::FromUtf8("example.net");
    const void* originRef = originStr.Get();
    WKResourceCacheManagerClearCacheForOrigin(mgrRef, originRef, 3);
    Require(mgr->origins.empty());
    Require(mgr->lastCachesCleared == 3);
    WKResourceCacheManagerClearCacheForOrigin(mgrRef, nullptr, 3);

    auto badObj = NKWebKit::Make<NKWebKit::Array>();
    RequireInvalidArgument([&] { WKResourceCacheManagerClearCacheForOrigin(mgrRef, badObj.Get(), 1); });

    mgr->origins.push_back("example.org");
    WKResourceCacheManagerGetCacheOrigins(mgrRef, nullptr, OnOrigins);
    Require(callbackCalled);
    WKResourceCacheManagerGetCacheOrigins(mgrRef, nullptr, nullptr);

    RequireInvalidArgument([] { WKResourceCacheManagerClearCacheForAllOrigins(nullptr, 0); });
    RequireInvalidArgument([&] { WKResourceCacheManagerClearCacheForOrigin(nullptr, originRef, 0); });
    RequireInvalidArgument([] { WKResourceCacheManagerGetCacheOrigins(nullptr, nullptr, OnOrigins); });

    return 0;
}
